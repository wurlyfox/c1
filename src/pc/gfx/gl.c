/*
 * opengl pc gfx backend
 */
#ifdef __EMSCRIPTEN__
/* web build: WebGL has no fixed-function pipeline, so the primitives are
   batched into a vertex array and drawn with a single GLSL ES 1.00 program */
#define C1_GLES2
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>

/* Yields to the browser until the next display refresh, so frames are
   presented in step with the screen and DOM events / audio callbacks get a
   chance to run. A hidden tab gets no animation frames: poll slowly instead. */
EM_ASYNC_JS(void, WebWaitFrame, (), {
  await new Promise(function(resolve) {
    if (document.hidden) { setTimeout(resolve, 100); return; }
    requestAnimationFrame(function() { resolve(); });
  });
});
#elif defined(_WIN32)
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

#ifdef CFLAGS_GUI
#include "ext/gui.h"
#define CIMGUI_USE_OPENGL2
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"
#endif

#ifdef _WIN32
/* temporary while testing in vm */
#define glActiveTexture(...)
#define glBlendEquation(...)
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

#ifdef C1_GLES2
typedef struct {
  float x, y;
  float r, g, b, a;
  float u, v;
} gl_vertex;

#define GL_BATCH_MAX 3072 /* vertices per draw call */

static const char *vs_src =
  "attribute vec2 a_pos;\n"
  "attribute vec4 a_color;\n"
  "attribute vec2 a_uv;\n"
  "uniform vec2 u_scale;\n"
  "uniform vec2 u_offset;\n"
  "varying vec4 v_color;\n"
  "varying vec2 v_uv;\n"
  "void main() {\n"
  "  v_color = a_color;\n"
  "  v_uv = a_uv;\n"
  "  gl_Position = vec4((a_pos - u_offset) * u_scale, 0.0, 1.0);\n"
  "}\n";

/* u_texscale: 0 = untextured, otherwise the texture is modulated by the
   vertex color and its rgb is scaled by this factor (GL_RGB_SCALE equivalent) */
static const char *fs_src =
  "precision mediump float;\n"
  "uniform sampler2D u_tex;\n"
  "uniform float u_texscale;\n"
  "varying vec4 v_color;\n"
  "varying vec2 v_uv;\n"
  "void main() {\n"
  "  vec4 c = v_color;\n"
  "  if (u_texscale > 0.0) {\n"
  "    vec4 t = texture2D(u_tex, v_uv);\n"
  "    c = vec4(min(t.rgb * c.rgb * u_texscale, 1.0), t.a * c.a);\n"
  "  }\n"
  "  gl_FragColor = c;\n"
  "}\n";

static GLuint gl_prog = 0, gl_vbo = 0;
static GLint gl_u_scale, gl_u_offset, gl_u_tex, gl_u_texscale;
static gl_vertex gl_batch[GL_BATCH_MAX];
static int gl_batch_count = 0;

static GLuint GLCompileShader(GLenum type, const char *src) {
  GLuint shader;
  GLint ok;
  char log[512];

  shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, 0);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glGetShaderInfoLog(shader, sizeof(log), 0, log);
    printf("Error compiling shader: %s\n", log);
  }
  return shader;
}

static void GLInitProgram() {
  GLuint vs, fs;
  GLint ok;
  char log[512];

  vs = GLCompileShader(GL_VERTEX_SHADER, vs_src);
  fs = GLCompileShader(GL_FRAGMENT_SHADER, fs_src);
  gl_prog = glCreateProgram();
  glAttachShader(gl_prog, vs);
  glAttachShader(gl_prog, fs);
  glBindAttribLocation(gl_prog, 0, "a_pos");
  glBindAttribLocation(gl_prog, 1, "a_color");
  glBindAttribLocation(gl_prog, 2, "a_uv");
  glLinkProgram(gl_prog);
  glGetProgramiv(gl_prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    glGetProgramInfoLog(gl_prog, sizeof(log), 0, log);
    printf("Error linking program: %s\n", log);
  }
  glUseProgram(gl_prog);
  gl_u_scale = glGetUniformLocation(gl_prog, "u_scale");
  gl_u_offset = glGetUniformLocation(gl_prog, "u_offset");
  gl_u_tex = glGetUniformLocation(gl_prog, "u_tex");
  gl_u_texscale = glGetUniformLocation(gl_prog, "u_texscale");
  glUniform1i(gl_u_tex, 0);
  glUniform1f(gl_u_texscale, 0.0f);
  glGenBuffers(1, &gl_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(gl_batch), 0, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), (void*)offsetof(gl_vertex, x));
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), (void*)offsetof(gl_vertex, r));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), (void*)offsetof(gl_vertex, u));
}

static void GLBatchFlush(GLenum mode) {
  if (gl_batch_count == 0) { return; }
  glBufferSubData(GL_ARRAY_BUFFER, 0, gl_batch_count*sizeof(gl_vertex), gl_batch);
  glDrawArrays(mode, 0, gl_batch_count);
  gl_batch_count = 0;
}

static inline void GLBatchVertex(float x, float y,
  float r, float g, float b, float a, float u, float v) {
  gl_vertex *vert;

  vert = &gl_batch[gl_batch_count++];
  vert->x = x; vert->y = y;
  vert->r = r; vert->g = g; vert->b = b; vert->a = a;
  vert->u = u; vert->v = v;
}

/* draw a quad with vertices in fan order; uvs are those used by GLDrawImage */
static void GLDrawQuad(quad *q, int r, int g, int b, int a) {
  static const int idxs[6] = { 0, 1, 2, 0, 2, 3 };
  int i, idx;

  for (i=0;i<6;i++) {
    idx = idxs[i];
    GLBatchVertex(q->p[idx].x, -q->p[idx].y,
      r/255.0f, g/255.0f, b/255.0f, a/255.0f,
      (idx%3)?1.0f:0.0f, (idx/2)?1.0f:0.0f);
  }
  GLBatchFlush(GL_TRIANGLES);
}
#endif /* C1_GLES2 */

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
  

  callbacks = *_callbacks;
#ifdef CFLAGS_GUI
  GLInitGui();
#endif
#ifdef C1_GLES2
  GLInitProgram();
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
#ifdef C1_GLES2
  GLDrawQuad(&q, r, g, b, 255);
#else
  glColor4ub(r, g, b, 255);
  glBegin(GL_QUADS);
  for (i=0;i<4;i++)
    glVertex3i(q.p[i].x, -q.p[i].y, -1);
  glEnd();
  glColor4ub(255, 255, 255, 255);
#endif
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
#ifdef C1_GLES2
    (void)i;
    GLDrawQuad(&q, 0, 0, 0, brightness);
    glDisable(GL_BLEND);
#else
    glColor4ub(0, 0, 0, brightness);
    glBegin(GL_QUADS);
    for (i=0;i<4;i++)
      glVertex3i(q.p[i].x, -q.p[i].y, -1);
    glEnd();
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
#endif
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
#ifdef C1_GLES2
  glUniform1f(gl_u_texscale, 1.0f);
  GLDrawQuad(&q, 255, 255, 255, 255);
  glUniform1f(gl_u_texscale, 0.0f);
#else
  glEnable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  glColor4ub(255, 255, 255, 255);
  for (i=0;i<4;i++) {
    glTexCoord2f((i%3)?1.0:0, (i/2)?1.0:0);
    glVertex3i(q.p[i].x, -q.p[i].y, -1);
  }
  glEnd();
  glDisable(GL_TEXTURE_2D);
#endif
  glBindTexture(GL_TEXTURE_2D, 0);
}

#ifdef C1_GLES2
void GLDrawPrims(void *data, int count) {
  static const int line_idxs[6] = { 0, 1, 1, 2, 2, 0 };
  poly3i *poly;
  GLenum mode, poly_mode;
  int i, ii, idx, texid, flags, vert_count;

  texid = -1;
  flags = 3;
  mode = GL_TRIANGLES;
  poly = (poly3i*)data;
  glActiveTexture(GL_TEXTURE0);
  glUniform1f(gl_u_texscale, 0.0f);
  glEnable(GL_BLEND);
  for (i=0;i<count;i++,poly++) {
    poly_mode = poly->type == 3 ? GL_LINES : GL_TRIANGLES; /* type 3 = wireframe */
    vert_count = poly->type == 3 ? 6 : 3;
    if (poly->flags != flags || poly->texid != texid || poly_mode != mode
     || gl_batch_count + vert_count > GL_BATCH_MAX)
      GLBatchFlush(mode);
    mode = poly_mode;
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
      if (poly->texid == -1) { glUniform1f(gl_u_texscale, 0.0f); }
      else {
        if (texid == -1) { glUniform1f(gl_u_texscale, 2.0f); }
        glBindTexture(GL_TEXTURE_2D, poly->texid);
      }
      texid = poly->texid;
    }
    for (ii=0;ii<vert_count;ii++) {
      idx = poly->type == 3 ? line_idxs[ii] : ii;
      /* colors are full-range 32 bit values (as passed to glColor3ui) */
      GLBatchVertex(poly->verts[idx].x, poly->verts[idx].y,
        poly->colors[idx].r / 4294967295.0f,
        poly->colors[idx].g / 4294967295.0f,
        poly->colors[idx].b / 4294967295.0f,
        1.0f,
        poly->uvs[idx].x, poly->uvs[idx].y);
    }
  }
  GLBatchFlush(mode);
  glDisable(GL_BLEND);
  glUniform1f(gl_u_texscale, 0.0f);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
#else
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
#endif /* C1_GLES2 */

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
  while ((src = (uint8_t*)prim)) {
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

#ifdef C1_GLES2
  {
    /* the page resizes the canvas to match the screen (device pixel ratio,
       rotations, fullscreen): follow it */
    int cw, ch;
    if (emscripten_get_canvas_element_size("#canvas", &cw, &ch) == EMSCRIPTEN_RESULT_SUCCESS)
      glViewport(0, 0, cw, ch);
  }
  glClear(GL_COLOR_BUFFER_BIT);
  /* equivalent of the glFrustum call below for vertices at z=-1 */
  glUniform2f(gl_u_scale, 2.0f/screen.w, 2.0f/screen.h);
  glUniform2f(gl_u_offset, screen.x+(screen.w/2.0f), screen.y+(screen.h/2.0f));
#else
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  // glViewport(0, 0, screen.w, screen.h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(screen.x, screen.x+screen.w, screen.y, screen.y+screen.h,
            1.0, 100000.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
#endif
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
#ifdef __EMSCRIPTEN__
    WebWaitFrame(); /* a busy wait would freeze the browser tab */
#endif
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
