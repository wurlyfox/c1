/*
 * opengl pc gfx backend
 */
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glcorearb.h>
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

typedef struct {
  int id;
  int count;
  int stride;
  int vert_count;
  int gl_type;
  uint32_t verts_offs;
  uint32_t colors_offs;
  uint32_t uvs_offs;
} gl_vboinfo;

int vbo_count = 1;
gl_vboinfo vboinfos[1] = {
  {
    .id = 0,
    .count = 0,
    .stride = 30,
    .vert_count = 3,
    .gl_type = GL_TRIANGLES,
    .verts_offs = offsetof(poly3i, verts),
    .colors_offs = offsetof(poly3i, colors),
    .uvs_offs = offsetof(poly3i, uvs)
  }
/*,
  {
    .id = 0,
    .count = 0,
    .stride = 27,
    .vert_count = 4,
    .gl_type = GL_QUADS,
    .verts_offs = offsetof(poly4i, verts),
    .colors_offs = offsetof(poly4i, colors),
    .uvs_offs = offsetof(poly4i, uvs)
  }
*/
};

rect2 screen = { .x = -256, .y = -120, .w = 512, .h = 240 };
int image_texid = 0;
prim_struct prim_links[2048];

static int GLCreateTexture(dim2 dim, uint8_t *buf) {
  int texid;

  glGenTextures(1, &texid);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dim.w, dim.h, 0,
    GL_RGBA, GL_UNSIGNED_BYTE, buf);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texid;
}

static void GLDeleteTexture(int texid) {
  glDeleteTextures(1, &texid);
}

static void GLUpdateTexture(int texid, rect2 rect, uint8_t *buf) {
  glBindTexture(GL_TEXTURE_2D, texid);
  glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.w, rect.h,
    GL_RGBA, GL_UNSIGNED_BYTE, buf);
  glBindTexture(GL_TEXTURE_2D, 0);
}

gl_context context;
gl_callbacks callbacks = { 0 };

extern ns_struct ns;
extern entry *cur_zone;
extern int paused;
extern int draw_count;
extern int rcnt_stopped;
extern uint32_t ticks_elapsed;
extern rgb8 vram_fill_color, next_vram_fill_color;
extern pbak_frame *cur_pbak_frame;

/* gl extensions */
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBUFFERDATAPROC glBufferData;

static void GLInitExt() {
  if (!callbacks.ext_supported)
    exit(-1);
  if ((*callbacks.ext_supported)("GL_ARB_vertex_buffer_object")) {
    if (!callbacks.proc_addr)
      exit(-1);
    glBindBuffer = (*callbacks.proc_addr)("glBindBufferARB");
    glGenBuffers = (*callbacks.proc_addr)("glGenBuffersARB");
    glBufferData = (*callbacks.proc_addr)("glBufferDataARB");
  }
  else
    exit(-1);
}

#ifdef CFLAGS_GUI
void GLInitGui() {
  GuiInit();
  ImGui_ImplOpenGL2_Init();
}

void GLKillGui() {
  ImGui_ImplOpenGL2_Shutdown();
}
#endif

/* gl */
int GLInit(gl_callbacks *_callbacks) {
  int i;

  callbacks = *_callbacks;
  GLInitExt();
#ifdef CFLAGS_GUI
  GLInitGui();
#endif
  TexturesInit(GLCreateTexture, GLDeleteTexture, GLUpdateTexture);
  for (i=0;i<vbo_count;i++)
    glGenBuffers(1, &vboinfos[i].id);
  screen.x = -256;
  screen.y = -120;
  screen.w = 512;
  screen.h = 240;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  return SUCCESS;
}

int GLKill() {
  if (context.prims_head) {
    free(context.prims_head);
    context.prims_head = 0;
  }
  TexturesKill();
  if (image_texid) {
    GLDeleteTexture(image_texid);
    image_texid = 0;
  }
#ifdef CFLAGS_GUI
  // GLKillGui(); /* removed for now; may
#endif
  return SUCCESS;
}

int GLSetupPrims() {
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
  glColor3ub(r,g,b);
  glBegin(GL_QUADS);
  for (i=0;i<4;i++)
    glVertex3i(q.p[i].x, q.p[i].y, -1);
  glEnd();
  glColor3ub(0,0,0);
}

void GLDrawOverlay(int brightness) {
  quad q;
  int i, adj[16];
  adj[0] = 12; adj[1] = 25; adj[2] = 38; adj[3] = 51;
  adj[4] = 64; adj[5] = 77; adj[6] = 91; adj[7] =105;
  adj[8] =120; adj[9] =135; adj[10]=151; adj[11]=167;
  adj[12]=185; adj[13]=203; adj[14]=225; adj[15]=255;
  if (brightness) {
    brightness = adj[brightness >> 4];
    q.p[0].x = -256; q.p[0].y = -120;
    q.p[1].x =  256; q.p[1].y = -120;
    q.p[3].x = -256; q.p[2].y =  120;
    q.p[2].x =  256; q.p[3].y =  120;
    glColor4ub(0, 0, 0, brightness);
    glBegin(GL_QUADS);
    for (i=0;i<4;i++)
      glVertex3i(q.p[i].x, q.p[i].y, -1);
    glEnd();
    glColor4ub(255, 255, 255, 255);
    //glDisable(GL_BLEND);
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
  for (i=0;i<4;i++) {
    glTexCoord2f((i%3)?1.0:0, (i/2)?0:1.0);
    glVertex3i(q.p[i].x, q.p[i].y, -1);
  }
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GLDrawTex(dim2 *dim, uint32_t id, pnt2 *loc) {
  quad q;
  rect2 rect;
  pnt2 _loc;
  int i;

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
  glBindTexture(GL_TEXTURE_2D, id);
  glEnable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  for (i=0;i<4;i++) {
    glTexCoord2f((i%3)?1.0:0, (i/2)?0:1.0);
    glVertex3i(q.p[i].x, q.p[i].y, -1);
  }
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void GLUpload(int idx, void *data, int count) {
  gl_vboinfo *info;

  info = &vboinfos[idx];
  info->count = count;
  glBindBuffer(GL_ARRAY_BUFFER, info->id);
  glBufferData(GL_ARRAY_BUFFER, count*info->stride*4, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void GLBindPointers(int idx) {
  gl_vboinfo *info;

  info = &vboinfos[idx];
  glBindBuffer(GL_ARRAY_BUFFER, info->id);
  glVertexPointer(3, GL_INT, 12, (void*)info->verts_offs);
  glColorPointer(3, GL_UNSIGNED_INT, 12, (void*)info->colors_offs);
  glTexCoordPointer(2, GL_FLOAT, 12, (void*)info->uvs_offs);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLDrawPrims(int idx, void *data, int count) {
  gl_vboinfo *info;
  poly3i *poly;
  int i, offs, texid;
  int mode = GL_MODULATE;

  texid = -1;
  info = &vboinfos[idx];
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  if (data)
    GLUpload(idx, data, count);
  GLBindPointers(idx);
  glBindBuffer(GL_ARRAY_BUFFER, info->id);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  for (i=0,offs=0;i<info->count;i++) {
    poly = (poly3i*)((uint8_t*)data+(offs*3*sizeof(uint32_t)));
    if (poly->type == 3)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (poly->texid != texid) {
      if (poly->texid == -1) { glDisable(GL_TEXTURE_2D); }
      else {
        if (texid == -1) { glEnable(GL_TEXTURE_2D); }
        glBindTexture(GL_TEXTURE_2D, poly->texid);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD_SIGNED);
      }
      texid = poly->texid;
    }
    glDrawArrays(info->gl_type, offs, info->vert_count);
    offs += info->stride/3;
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
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
  /* allocate tris */
  *tris = (poly3i*)malloc(size);
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
          tri.uvs[k] = ((poly4i*)src)->uvs[idx];
        }
        *(poly3i*)dst = tri;
        dst += sizeof(poly3i);
        ++(*count);
      }
    }
    if (prim->type == 0) { ot_idx++; }
    prim = (prim_struct*)PRIM_NEXT(prim);
  }
  dst = (uint8_t*)*tris;
  for (i=0;i<*count;i++) {
    for (j=0;j<3;j++)
      ((poly3i*)dst)->verts[j].y = -((poly3i*)dst)->verts[j].y;
    dst += sizeof(poly3i);
  }
}

static void GLFreeTris(poly3i **tris) {
  free(*tris);
  *tris = 0;
}

static void GLDraw(void *ot) {
  poly3i *tris;
  int count;

  GLConvertToTris(ot, &tris, &count);
  GLDrawPrims(0, tris, count);
  GLFreeTris(&tris);
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
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  // glViewport(0, 0, screen.w, screen.h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(screen.x, screen.x+screen.w, screen.y, screen.y+screen.h,
            1.0, 100000.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
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
  vram_fill_color = next_vram_fill_color;
  if (!paused && (cur_display_flags & 0x1000))
    draw_count++;
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
  if (cur_display_flags & 0x80000) {
    if (cur_display_flags & 0x2000) {
      header = (zone_header*)cur_zone->items[0];
      fill = vram_fill_color;
      fh = header->vram_fill_height;
      GLDrawRect(0,  0, 512,     fh, fill.r, fill.g, fill.b);
      GLDrawRect(0, fh, 512, 216-fh, fill.r, fill.g, fill.b);
    }
  }
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
#ifdef CFLAGS_GUI
  GuiUpdate();
  GuiDraw();
  ImGui_ImplOpenGL2_RenderDrawData(igGetDrawData());
#endif
  GLResetPrims(&context);
  cur_display_flags = next_display_flags;  /* copy display/animate flags */
  if (callbacks.post_update)
    (*callbacks.post_update)();
}
