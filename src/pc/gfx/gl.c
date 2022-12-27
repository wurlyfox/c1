/*
 * opengl pc gfx backend
 */
#include <GL/gl.h>
#include "gl.h"
#include "pcgfx.h"
#include "globals.h"
#include "level.h"
#include "pbak.h"
#include "audio.h"
#include "pc/gfx/tex.h"
#include "pc/time.h"

#ifdef CFLAGS_GUI
#include "ext/gui.h"
#define CIMGUI_USE_OPENGL2
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"
#endif

int GLCreateTexture(dim2 dim, uint8_t *buf) {
  int texid;

  glGenTextures(1, &texid);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dim.w, dim.h, 0,
    GL_RGBA, GL_UNSIGNED_BYTE, buf);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texid;
}

void GLDeleteTexture(int texid) {
  glDeleteTextures(1, &texid);
}

void GLUpdateTexture(int texid, rect2 rect, uint8_t *buf) {
  glBindTexture(GL_TEXTURE_2D, texid);
  glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.w, rect.h,
    GL_RGBA, GL_UNSIGNED_BYTE, buf);
  glBindTexture(GL_TEXTURE_2D, 0);
}

gl_context context = { 0 };
gl_callbacks callbacks = { 0 };
rect2 screen = { .x = -256, .y = -120, .w = 512, .h = 240 };
prim_struct prim_links[2048];
int image_texid = 0;

extern ns_struct ns;
extern entry *cur_zone;
extern int paused;
extern int draw_count;
extern int rcnt_stopped;
extern uint32_t ticks_elapsed;
extern rgb8 vram_fill_color, next_vram_fill_color;
extern pbak_frame *cur_pbak_frame;

#ifdef CFLAGS_GUI
void GLInitGui() {
  GuiInit();
  ImGui_ImplOpenGL2_Init();
}

void GLKillGui() {
  ImGui_ImplOpenGL2_Shutdown();
}
#endif

int textures_inited=0;
/* gl */
int GLInit(gl_callbacks *_callbacks) {
  rect2 window;
  int i;

  callbacks = *_callbacks;
#ifdef CFLAGS_GUI
  GLInitGui();
#endif
  TexturesInit(GLCreateTexture, GLDeleteTexture, GLUpdateTexture);
  textures_inited=1;
  screen.x = -256;
  screen.y = -120;
  screen.w = 512;
  screen.h = 240;
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  return SUCCESS;
}

int GLKill() {
  if (context.prims_head) {
    free(context.prims_head);
    context.prims_head = 0;
  }
  TexturesKill();
  textures_inited=0;
  if (image_texid) {
    GLDeleteTexture(image_texid);
    image_texid = 0;
  }
#ifdef CFLAGS_GUI
  // GLKillGui(); /* removed for now */
#endif
  return SUCCESS;
}

int GLSetupPrims() {
  if (!textures_inited) {
    TexturesInit(GLCreateTexture, GLDeleteTexture, GLUpdateTexture);
    textures_inited=1;
  }
  if (!context.prims_head) // not in orig
    context.prims_head = calloc(1,0x800000);
  GLResetPrims(&context);
  return SUCCESS;
}

void GLResetPrims(gl_context *gc) {
  gc->prims_tail = gc->prims_head;
  GLResetOT(gc->ot, 2048);
}

void GLDrawRect(int x, int y, int w, int h, int r, int g, int b) {
  quad q;
  int i;
  q.p[0].x =   x; q.p[0].y =   y;
  q.p[1].x = x+w; q.p[1].y =   y;
  q.p[3].x =   x; q.p[2].y = y+h;
  q.p[2].x = x+w; q.p[3].y = y+h;
  for (i=0;i<4;i++) {
    q.p[i].x += screen.x;
    q.p[i].y += screen.y;
  }
  glColor4ub(r, g, b, 255);
  glBegin(GL_QUADS);
  for (i=0;i<4;i++)
    glVertex3i(q.p[i].x, -q.p[i].y, -1);
  glEnd();
  glColor4ub(255, 255, 255, 255);
}

void GLDrawOverlay(int brightness) {
  quad q;
  int i, adj[16];
  adj[0] = 12; adj[1] = 25; adj[2] = 38; adj[3] = 51;
  adj[4] = 64; adj[5] = 77; adj[6] = 91; adj[7] =105;
  adj[8] =120; adj[9] =135; adj[10]=151; adj[11]=167;
  adj[12]=185; adj[13]=203; adj[14]=225; adj[15]=255;
  if (brightness) {
    brightness = limit(brightness>>4,1,16)-1;
    brightness = adj[brightness];
    q.p[0].x = -256; q.p[0].y = -120;
    q.p[1].x =  256; q.p[1].y = -120;
    q.p[3].x = -256; q.p[2].y =  120;
    q.p[2].x =  256; q.p[3].y =  120;
    glEnable(GL_BLEND);
    glColor4ub(0, 0, 0, brightness);
    glBegin(GL_QUADS);
    for (i=0;i<4;i++)
      glVertex3i(q.p[i].x, -q.p[i].y, -1);
    glEnd();
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
  }
}

void GLDrawImage(dim2 *dim, uint8_t *buf, pnt2 *loc) {
  quad q;
  rect2 rect;
  pnt2 _loc;
  int i;

  rect.x=0;rect.y=0;
  rect.dim = *dim;
  if (image_texid)
    GLDeleteTexture(image_texid);
  image_texid = GLCreateTexture(rect.dim, buf);
  q.p[0].x =      0; q.p[0].y =      0;
  q.p[1].x = dim->w; q.p[1].y =      0;
  q.p[3].x =      0; q.p[2].y = dim->h;
  q.p[2].x = dim->w; q.p[3].y = dim->h;
  if (!loc) {
    loc = &_loc;
    loc->x = (int32_t)-dim->w/2; /* centered */
    loc->y = (int32_t)-dim->h/2;
  }
  for (i=0;i<4;i++) {
    q.p[i].x += loc->x;
    q.p[i].y += loc->y;
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, image_texid);
  glEnable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  glColor4ub(255, 255, 255, 255);
  for (i=0;i<4;i++) {
    glTexCoord2f((i%3)?1.0:0, (i/2)?1.0:0);
    glVertex3i(q.p[i].x, -q.p[i].y, -1);
  }
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GLDrawPrims(void *data, int count) {
  poly3i *poly;
  int i, ii, texid, flags;

  texid = -1;
  flags = 3;
  poly = (poly3i*)data;
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  for (i=0;i<count;i++,poly++) {
    if (poly->type == 3)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (poly->flags != flags) {
      if (poly->flags == 2) { glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); }
      else if (flags == 2)  { glBlendEquation(GL_FUNC_ADD); }
      switch (poly->flags) {
      case 0:
      case 3:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
      case 1:
        glBlendFunc(GL_ONE, GL_ONE);
        break;
      case 2:
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        break;
      }
      flags = poly->flags;
    }
    if (poly->texid != texid) {
      if (poly->texid == -1) { glDisable(GL_TEXTURE_2D); }
      else {
        if (texid == -1) { glEnable(GL_TEXTURE_2D); }
        glBindTexture(GL_TEXTURE_2D, poly->texid);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
      }
      texid = poly->texid;
    }
    glBegin(GL_TRIANGLES);
    for (ii=0;ii<3;ii++) {
      glColor3ui(poly->colors[ii].r, poly->colors[ii].g, poly->colors[ii].b);
      glTexCoord2f(poly->uvs[ii].x, poly->uvs[ii].y);
      glVertex3i(poly->verts[ii].x, poly->verts[ii].y, -1);
    }
    glEnd();
  }
  glDisable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void **GLGetPrimsTail() {
  return &context.prims_tail;
}

void GLResetOT(void *ot, int len) {
  prim_struct *link;
  int i;

  link = (prim_struct*)ot;
  for (i=0;i<len;i++,link++) {
    link->type = 0;
    link->next = link+1;
  }
}

void GLAddPrim(void *prim, int idx) {
  prim_struct *ps, *it, *next;

  ps = (prim_struct*)prim;
  if (!context.ot[idx]) { return; }
  for (it=context.ot[idx];(next=PRIM_NEXT(it))&&next->type;it=next);
  PRIM_SETNEXT(it,ps);
  PRIM_SETNEXT(ps,next);
}

/*
 * until another solution is found, for now it is necessary to convert all primitives
 * which are processed by glDrawArrays to triangles, in their order in the OT.
 *
 * alternatives are to
 * 1) a) iterate the OT and group contiguous primitives of the same type;
 *    b) perform a glDrawArrays call for each group
 * 2) manually generate tris instead of quads in sw gfx code
 */
void *trimem=0;
static void GLConvertToTris(void *ot, poly3i **tris, int *count) {
  prim_struct *prim;
  poly3i tri;
  size_t size;
  uint8_t *src, *dst;
  int i, j, k;
  int ot_idx, idx;

  if (count)
    *count = 0;
  context.prims_tail = context.prims_head;
  /* iterate ot and compute total size of converted tris */
  src = (uint8_t*)ot;
  prim = (prim_struct*)src;
  size = 0;
  while (prim) {
    if (prim->type == 1) { size += sizeof(poly3i); }
    else if (prim->type >= 2) { size += sizeof(poly3i)*2; }
    prim = (prim_struct*)PRIM_NEXT(prim);
  }
  if (trimem==0)
    trimem=calloc(1,0x800000);
  *tris=trimem;
  prim = (prim_struct*)src;
  src = (uint8_t*)prim;
  dst = (uint8_t*)*tris;
  ot_idx = 0;
  while (src = (uint8_t*)prim) {
    if (prim->type == 1) {
      *(poly3i*)dst = *(poly3i*)src;
#ifdef CFLAGS_GFX_SW_PERSP
      for (i=0;i<3;i++)
        ((poly3i*)dst)->verts[i].z = -1;
#endif
      dst += sizeof(poly3i);
      ++(*count);
    }
    else if (prim->type >= 2) {
      int idxs[4] = {0,1,3,2};
      for (j=0;j<2;j++) {
        for (k=0;k<3;k++) {
          idx = idxs[((j*2)+k)%4];
          tri.type = prim->type == 2 ? 1: 3;
          tri.next = 0;
          tri.verts[k] = ((poly4i*)src)->verts[idx];
#ifdef CFLAGS_GFX_SW_PERSP
          tri.verts[k].z = -1;
#endif
          tri.colors[k] = ((poly4i*)src)->colors[idx];
          tri.texid = ((poly4i*)src)->texid;
          if (prim->type == 3) {
            tri.texid = -1;
            tri.type = 3;
          }
          tri.flags = ((poly4i*)src)->flags;
          tri.uvs[k] = ((poly4i*)src)->uvs[idx];
        }
        *(poly3i*)dst = tri;
        dst += sizeof(poly3i);
        ++(*count);
      }
    }
    else if (prim->type == 0) { ot_idx++; }
    prim = (prim_struct*)PRIM_NEXT(prim);
  }
  dst = (uint8_t*)*tris;
  for (i=0;i<*count;i++) {
    for (j=0;j<3;j++) {
      ((poly3i*)dst)->verts[j].y = -((poly3i*)dst)->verts[j].y;
      ((poly3i*)dst)->colors[j].a = ~0; /* for now */
    }
    dst += sizeof(poly3i);
  }
}

static void GLDraw(void *ot) {
  poly3i *tris;
  int count;

  GLConvertToTris(ot, &tris, &count);
  GLDrawPrims(tris, count);
}

int GLRoundTicks(int ticks) {
  if (ticks < 0)
    return 34;
  if (ticks < 19)
    return 17; /* 1/2 frame */
  if (ticks < 36)
    return 34; /* 1 frame */
  if (ticks < 53)
    return 51; /* 1 1/2 frames */
  return ticks;
}

void GLClear() {
  zone_header *header;
  rgb8 fill;
  int fh;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  // glViewport(0, 0, screen.w, screen.h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(screen.x, screen.x+screen.w, screen.y, screen.y+screen.h,
            1.0, 100000.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  if (!(cur_display_flags & 0x80000)) {
    /* fill bg color(s) */
    header = (zone_header*)cur_zone->items[0];
    fh = header->vram_fill_height;
    if (cur_display_flags & 0x2000) {
      fill = vram_fill_color;
      GLDrawRect(0,  12+0, 512,     fh, fill.r, fill.g, fill.b); /* top color */
      fill = header->vram_fill;
      GLDrawRect(0, 12+fh, 512, 216-fh, fill.r, fill.g, fill.b); /* bottom color */
    }
    else {
      GLDrawRect(0,  12+0, 512, 216, 0, 0, 0);
    }
  }
}

void GLUpdate() {
  zone_header *header;
  rgb8 fill;
  int fh;
  int ticks_elapsed, elapsed_since;
  gl_input gl_input;

  if (callbacks.pre_update)
    (*callbacks.pre_update)();
  if (callbacks.input) {
    (*callbacks.input)(&gl_input);
  }
#ifdef CFLAGS_GUI
  ImGui_ImplOpenGL2_NewFrame();
#endif
  cur_display_flags = next_display_flags;  /* copy display/animate flags */
  vram_fill_color = next_vram_fill_color;
  if (!paused && (cur_display_flags & 0x1000))
    draw_count++;
  AudioUpdate();
  ticks_elapsed = GetTicksElapsed();
  context.sync_stamp = ticks_elapsed;
  if (pbak_state == 0)
    ticks_cur_frame = ticks_elapsed - context.draw_stamp; /* time elapsed between current sync and previous draw  */
  else
    ticks_cur_frame = 17; /* default to 17 */
  while (ticks_elapsed - context.draw_stamp < 34) {
    ticks_elapsed = GetTicksElapsed();
  }
  if (pbak_state == 2)
    SetTicksElapsed(cur_pbak_frame->ticks_elapsed);
  ticks_elapsed = GetTicksElapsed();
  elapsed_since = ticks_elapsed - context.draw_stamp; /* time elapsed between draws */
  context.draw_stamp = ticks_elapsed;
  context.ticks_per_frame = GLRoundTicks(elapsed_since); /* record rounded tick count */
  TexturesUpdate();
  if (ns.draw_skip_counter > 0)
    ns.draw_skip_counter--;
  if (ns.draw_skip_counter == 0)
    GLDraw(context.ot);
  GLResetPrims(&context);
  if (fade_counter != 0) { /* brightness */
    if (fade_counter < -2) {
      fade_counter += fade_step;
      GLDrawOverlay(fade_counter + 256);
      if (fade_counter == 0 && !(cur_display_flags & 0x200000))
        fade_counter = -2;
    }
    else if (fade_counter < 0) {
      GLDrawOverlay(256);
      fade_counter = -1;
    }
    else {
      fade_counter -= fade_step;
      GLDrawOverlay(fade_counter);
    }
  }
  /* simulate draw area clipping behavior in orig impl */
  GLDrawRect(0, 0, 512, 12, 0, 0, 0);
  GLDrawRect(0, 228, 512, 12, 0, 0, 0);
#ifdef CFLAGS_GUI
  GuiUpdate();
  GuiDraw();
  ImGui_ImplOpenGL2_RenderDrawData(igGetDrawData());
#endif
  if (callbacks.post_update)
    (*callbacks.post_update)();
}
