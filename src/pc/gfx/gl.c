/*
 * opengl pc gfx backend
 */
#ifdef _WIN32
#include <windows.h>
#include <SDL_opengl.h>
#else
#include <GL/gl.h>
#endif

#include "gl.h"
#include "pcgfx.h"
#include "globals.h"
#include "level.h"
#include "pbak.h"
#include "audio.h"
#include "pc/gfx/tex.h"
#include "pc/time.h"

#ifdef CFLAGS_EXTENSIONS
#include "ext/ext.h"
#include "ext/gui.h"
#include "title.h"
#define CIMGUI_USE_OPENGL2
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"
#endif

int GLCreateTexture(dim2 dim, uint8_t *buf) {
  GLuint texid;

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
  return (int)texid;
}

void GLDeleteTexture(int texid) {
  GLuint gl_texid = (GLuint)texid;
  glDeleteTextures(1, &gl_texid);
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
extern rgb8 sky_color, next_sky_color;
extern pbak_frame *cur_pbak_frame;

#ifdef CFLAGS_EXTENSIONS
hq_context_t hq = { 0 };

void GLInitGui() {
  GuiInit();
  ImGui_ImplOpenGL2_Init();
}

void GLKillGui() {
  GuiFontClear();
  ImGui_ImplOpenGL2_Shutdown();
}
#endif

#ifdef _WIN32
PFNGLACTIVETEXTUREPROC pfnGlActiveTexture = 0;
PFNGLBLENDEQUATIONPROC pfnGlBlendEquation = 0;
#define glActiveTexture pfnGlActiveTexture
#define glBlendEquation pfnGlBlendEquation
#endif

int textures_inited=0;
static void GLInitTextures(void) {
  int max_size;
  
  max_size = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
  TexturesInit(GLCreateTexture, GLDeleteTexture, GLUpdateTexture, max_size);
  textures_inited = 1;
}

/* gl */
int GLInit(gl_callbacks *_callbacks) {
  

  callbacks = *_callbacks;
#ifdef _WIN32
  pfnGlActiveTexture = callbacks.proc_addr("glActiveTexture");
  pfnGlBlendEquation = callbacks.proc_addr("glBlendEquation");
#endif
#ifdef CFLAGS_EXTENSIONS
  GLInitGui();
#endif
  GLInitTextures();
  screen.x = -256;
  screen.y = -120;
  screen.w = 512;
  screen.h = 240;
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthFunc(GL_LEQUAL);
  glAlphaFunc(GL_GREATER, 0.0f);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
  glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
  glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
  glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
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
#ifdef CFLAGS_EXTENSIONS
  // GLKillGui(); /* removed for now */
#endif
  return SUCCESS;
}

int GLFullscreen(int fullscreen) {
  int state, width, height;

  if (!callbacks.fullscreen) return -1;
  state = (*callbacks.fullscreen)(fullscreen, &width, &height);
  if (state >= 0 && width > 0 && height > 0)
    glViewport(0, 0, width, height);
  return state;
}

int GLSetupPrims() {
  if (!textures_inited) {
    GLInitTextures();
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

#ifdef CFLAGS_EXTENSIONS
static void GLSWFrustum();
static void GLHWFrustum();
#endif

void GLDrawRect(int x, int y, int w, int h, int r, int g, int b) {
  quad q;
  int i;
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp) GLSWFrustum();
#endif
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
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp) GLHWFrustum();
#endif
}

void GLDrawOverlay(int brightness) {
  quad q;
  int i, adj[16];
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp) GLSWFrustum();
#endif
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
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp) GLHWFrustum();
#endif
}

void GLDrawImage(dim2 *dim, uint8_t *buf, pnt2 *loc) {
  quad q;
  rect2 rect;
  pnt2 _loc;
  int i;
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp) GLSWFrustum();
#endif
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
  glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
  glBegin(GL_QUADS);
  glColor4ub(255, 255, 255, 255);
  for (i=0;i<4;i++) {
    glTexCoord2f((i%3)?1.0:0, (i/2)?1.0:0);
    glVertex3i(q.p[i].x, -q.p[i].y, -1);
  }
  glEnd();
  glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp) GLHWFrustum();
#endif
}

typedef struct { 
  int flags;
  int texid;
  int wire; 
} gl_prim_state;

static inline int GLPrimWire(poly3i *poly) {
#ifdef CFLAGS_EXTENSIONS
  return poly->type == 3 || ext_config.enable_wireframe;
#else
  return 0;
#endif
}

static void GLSetPrimState(gl_prim_state *st, poly3i *poly) {
  int wire;

  wire = GLPrimWire(poly);
  if (wire != st->wire) {
    glPolygonMode(GL_FRONT_AND_BACK, wire ? GL_LINE : GL_FILL);
    st->wire = wire;
  }
  if (poly->flags != st->flags) {
    if ((poly->flags & 3) == 2) { glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); }
    else if ((st->flags & 3) == 2) { glBlendEquation(GL_FUNC_ADD); }
#ifdef CFLAGS_EXTENSIONS
    if (ext_config.enable_hwpersp 
     && !!((poly->flags ^ st->flags) & PRIM_FLAG_SCREEN)) {
      if (poly->flags & PRIM_FLAG_SCREEN) GLSWFrustum();
      else GLHWFrustum();
    }
    if (poly->flags & PRIM_FLAG_HIGHLIGHT)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
#endif
    switch (poly->flags&3) {
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
#ifdef CFLAGS_EXTENSIONS
    if (ext_config.enable_hwpersp)
      glDepthMask(!((poly->flags & 28) || ((poly->flags & 3) != 3)));
#endif
    st->flags = poly->flags;
  }
#ifdef CFLAGS_EXTENSIONS
  if (!ext_config.enable_textures) { glDisable(GL_TEXTURE_2D); } else
#endif
  if (poly->texid != st->texid) {
    if (poly->texid == -1) { glDisable(GL_TEXTURE_2D); }
    else {
      if (st->texid == -1) { glEnable(GL_TEXTURE_2D); }
      glBindTexture(GL_TEXTURE_2D, poly->texid);
    }
    st->texid = poly->texid; 
  }
}

static void GLDrawPrimsArrays(poly3i *tris, int count) {
  static float *va = 0, *ta = 0;
  static uint32_t *ca = 0;
  static int cap = 0;
  gl_prim_state st;
  poly3i *poly;
  float *vp, *tp;
  uint32_t *cp;
  int hwpersp, gouraud;
  int i, ii, start;

  if (count > cap) {
    cap = count + count / 2 + 1024;
    va = realloc(va, cap * 9 * sizeof(float));
    ta = realloc(ta, cap * 6 * sizeof(float));
    ca = realloc(ca, cap * 12 * sizeof(uint32_t));
  }
  hwpersp = 0;
  gouraud = 1;
#ifdef CFLAGS_EXTENSIONS
  hwpersp = ext_config.enable_hwpersp;
  gouraud = ext_config.enable_gouraud;
#endif
  poly = tris;
  vp = va;
  tp = ta;
  cp = ca;
  for (i=0;i<count;i++,poly++) {
    for (ii=0;ii<3;ii++) {
      *(vp++) = (float)poly->verts[ii].x;
      *(vp++) = (float)poly->verts[ii].y;
      *(vp++) = hwpersp ? (float)-poly->verts[ii].z : -1.0f;
      *(tp++) = poly->uvs[ii].x;
      *(tp++) = poly->uvs[ii].y;
      if (gouraud) {
        *(cp++) = poly->colors[ii].r;
        *(cp++) = poly->colors[ii].g;
        *(cp++) = poly->colors[ii].b;
      }
      else {
        *(cp++) = ~0u;
        *(cp++) = ~0u;
        *(cp++) = ~0u;
      }
      *(cp++) = ~0u;
    }
  }
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, va);
  glColorPointer(4, GL_UNSIGNED_INT, 0, ca);
  glTexCoordPointer(2, GL_FLOAT, 0, ta);
  st.flags = -1; st.texid = -1; st.wire = -1;
  start = 0;
  poly = tris;
  for (i=0;i<count;i++,poly++) {
    if (i == 0 || poly->flags != st.flags || poly->texid != st.texid 
     || GLPrimWire(poly) != st.wire) {
      if (i > start)
        glDrawArrays(GL_TRIANGLES, start*3, (i-start)*3);
      start = i;
      GLSetPrimState(&st, poly);
    }
  }
  if (count > start)
    glDrawArrays(GL_TRIANGLES, start*3, (count-start)*3);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

static int gl_use_arrays = 1;

void GLDrawPrims(void *data, int count) {
  poly3i *poly;
  gl_prim_state st;
  int i, ii;

  poly = (poly3i*)data;
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  st.flags = -1; st.texid = -1; st.wire = -1;
  if (gl_use_arrays)
    GLDrawPrimsArrays((poly3i*)data, count);
  else
  for (i=0;i<count;i++,poly++) {
    GLSetPrimState(&st, poly);
    glBegin(GL_TRIANGLES);
    for (ii=0;ii<3;ii++) {
#ifdef CFLAGS_EXTENSIONS
      if (!ext_config.enable_gouraud) { glColor3ub(255, 255, 255); }
      else
#endif
      glColor3ui(poly->colors[ii].r, poly->colors[ii].g, poly->colors[ii].b);
      glTexCoord2f(poly->uvs[ii].x, poly->uvs[ii].y);
#ifdef CFLAGS_EXTENSIONS
      if (ext_config.enable_hwpersp)
        glVertex3d(poly->verts[ii].x, poly->verts[ii].y, (double)-poly->verts[ii].z);
      else
#endif
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
  glDepthMask(GL_TRUE);
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp)
    glDisable(GL_ALPHA_TEST);
#endif
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
  for (it=context.ot[idx];(next=PRIM_NEXT(it))&&next->type;it=next) { /* empty */ }
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
static size_t trimem_size=0;
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
  if (size < 0x800000) size = 0x800000;
  if (size > trimem_size) {
    void *newmem = realloc(trimem, size);
    trimem = newmem;
    trimem_size = size;
  }
  *tris=trimem;
  prim = (prim_struct*)src;
  src = (uint8_t*)prim;
  dst = (uint8_t*)*tris;
  ot_idx = 0;
  while ((src = (uint8_t*)prim)) {
    if (prim->type == 1) {
      *(poly3i*)dst = *(poly3i*)src;
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
  if (!tris) return;
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp)
    ExtAppendTris(&ext_ctxt, tris, &count);
#endif
  GLDrawPrims(tris, count);
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.enable_hwpersp) {
    GLDrawExtOverlay(&ext_ctxt);
    GLDrawCamOverlay();
  }
  GLDrawObjectHighlights(&ext_ctxt);
#endif
}

ticks_t GLRoundTicks(ticks_t ticks) {
#ifdef CFLAGS_EXTENSIONS
  if (ext_config.fps_mode)
    return ticks;
#endif
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

static void GLSWFrustum() {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(screen.x, screen.x+screen.w, screen.y, screen.y+screen.h,
            1.0, 100000.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

#ifdef CFLAGS_EXTENSIONS
extern uint32_t screen_proj;
static void GLHWFrustum() {
  double sp, hw, hh;
  double aspect, mult, zoom;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  sp = (screen_proj > 0) ? (double)screen_proj : (double)screen.w;
  hw = (double)screen.w / (2.0 * sp);
  hh = (double)screen.h / (2.0 * sp);
  if (ext_config.aspect_mode) {
    switch (ext_config.aspect_mode) {
    case 1:  aspect = 16.0 / 9.0; break;
    case 2:  aspect = 10.0 / 7.0; break;
    default: aspect = 4.0 / 3.0;  break;
    }
    mult = aspect / (4.0 / 3.0);
    hw *= mult;
  }
  zoom = ExtViewZoom();
  hw *= zoom;
  hh *= zoom;
  glFrustum(-hw, hw, -hh, hh, 1.0, 100000.0);
  glMatrixMode(GL_MODELVIEW);
}
#endif

void GLClear() {
  zone_header *header;
  rgb8 fill;
  int fh;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  GLSWFrustum();
  if (!(cur_display_flags & 0x80000)) {
    /* fill bg color(s) */
    header = (zone_header*)cur_zone->items[0];
    fh = header->sky_color_fill_height;
    if (cur_display_flags & 0x2000) {
      fill = sky_color;
      GLDrawRect(0,  12+0, 512,     fh, fill.r, fill.g, fill.b); /* top color */
      fill = header->ground_color;
      GLDrawRect(0, 12+fh, 512, 216-fh, fill.r, fill.g, fill.b); /* bottom color */
    }
    else {
      GLDrawRect(0,  12+0, 512, 216, 0, 0, 0);
    }
  }
#ifdef CFLAGS_EXTENSIONS
  extern title_struct *title;
  int depth;
  if (ext_config.enable_hwpersp) {
    GLHWFrustum();
    depth = !(title && title->at_title 
      && (cur_display_flags & GOOL_FLAG_DISPLAY_IMAGES));
    if (depth) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
  }
#endif
}

#ifndef CFLAGS_EXTENSIONS
static inline void GLDrawTick() {
  draw_count++;
}
static inline ticks_t GLWaitTime() {
  return 34;
}
static inline int GLFadeStep() {
  return fade_step;
}
#else
static inline double GLTickMult() {
  if (ext_config.fps_mode == 3)
    return context.ticks_per_frame / 34.0;
  return 30.5 / ext_config.target_fps;
}

static inline ticks_t GLWaitTime() {
  if (ext_config.fps_mode == 3)
    return 0;
  return 34.0 * GLTickMult();
}

static inline void GLDrawTick() {
  if ((int)hq.draw_count != draw_count)
    hq.draw_count = draw_count;
  hq.draw_count += GLTickMult();
  draw_count = (int)hq.draw_count;
}

static inline int GLFadeStep() {
  int step;

  hq.fade_rem += fade_step * GLTickMult();
  step = (int)hq.fade_rem;
  hq.fade_rem -= step;
  return step;
}
#endif

void GLUpdate() {
  gl_input gl_input;
  ticks_t ticks_elapsed, elapsed_since;
  int step;

  if (callbacks.pre_update)
    (*callbacks.pre_update)();
  if (callbacks.input) {
    (*callbacks.input)(&gl_input);
  }
#ifdef CFLAGS_EXTENSIONS
  ExtUpdate(&ext_ctxt, &gl_input);
  ImGui_ImplOpenGL2_NewFrame();
  switch (ext_config.fps_mode) {
  case 1: ext_config.target_fps = 60.0;  break;
  case 2: ext_config.target_fps = 120.0; break;
  case 3: ext_config.target_fps = 0.0;   break;
  default: 
    ext_config.fps_mode = 0; 
    ext_config.target_fps = 30.5; 
    break;
  }
#endif
  cur_display_flags = next_display_flags;  /* copy display/animate flags */
  sky_color = next_sky_color;
  if (!paused && (cur_display_flags & 0x1000)) {
    GLDrawTick();
  }
  AudioUpdate();
  ticks_elapsed = GetTicksElapsed();
  context.sync_stamp = ticks_elapsed;
  if (pbak_state == 0)
    ticks_cur_frame = ticks_elapsed - context.draw_stamp; /* time elapsed between current sync and previous draw  */
  else
    ticks_cur_frame = 17; /* default to 17 */
  while (ticks_elapsed - context.draw_stamp < GLWaitTime()) {
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
    step = GLFadeStep();
    if (fade_counter < -2) {
      fade_counter += step;
#ifdef CFLAGS_EXTENSIONS
      if (fade_counter >= -2) fade_counter = 0;
#endif
      GLDrawOverlay(fade_counter + 256);
      if (fade_counter == 0 && !(cur_display_flags & 0x200000))
        fade_counter = -2;
    }
    else if (fade_counter < 0) {
      GLDrawOverlay(256);
      fade_counter = -1;
    }
    else {
      fade_counter -= step;
#ifdef CFLAGS_EXTENSIONS
      if (fade_counter < 0) fade_counter = 0;
#endif
      GLDrawOverlay(fade_counter);
    }
  }
#ifdef CFLAGS_EXTENSIONS
  else hq.fade_rem = 0;
#endif
  /* simulate draw area clipping behavior in orig impl */
  GLDrawRect(0, 0, 512, 12, 0, 0, 0);
  GLDrawRect(0, 228, 512, 12, 0, 0, 0);
#ifdef CFLAGS_EXTENSIONS
  GuiUpdate();
  GuiDraw();
  ImGui_ImplOpenGL2_RenderDrawData(igGetDrawData());
#endif
  if (callbacks.post_update)
    (*callbacks.post_update)();
}
