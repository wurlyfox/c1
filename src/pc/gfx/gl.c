/*
 * opengl pc gfx backend
 */
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glcorearb.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "gl.h"
#include "pcgfx.h"
#include "globals.h"
#include "level.h"
#include "pbak.h"
#include "audio.h"
#include "pc/gfx/tex.h"
#include "pc/time.h"
#include "ext/gui.h"

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

typedef struct {
  int texid;
  vec2 bearing;
  dim2 size;
  int advance;
} gl_glyph;

gl_glyph glyph_map[128];
dim2 glyph_dim = { .w = 0, .h = 0 };
int32_t glyph_y = 0;

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

/* freetype */
void GLInitFt() {
  FT_Library ft;
  FT_Face face;
  gl_glyph *glyph;
  int i, ii, texid;
  int ymin, ymax;
  int buf[10000];

  FT_Init_FreeType(&ft);
  FT_New_Face(ft, "DejaVuSansMono.ttf", 0, &face);
  FT_Set_Pixel_Sizes(face, 0, 128);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (i=0;i<128;i++) {
    FT_Load_Char(face, i, FT_LOAD_RENDER);
    glyph = &glyph_map[i];
    glyph->size.w = face->glyph->bitmap.width;
    glyph->size.h = face->glyph->bitmap.rows;
    glyph->bearing.x = face->glyph->bitmap_left;
    glyph->bearing.y = face->glyph->bitmap_top;
    glyph->advance = face->glyph->advance.x>>6;
    for (ii=0;ii<glyph->size.w*glyph->size.h;ii++) {
      buf[ii] = face->glyph->bitmap.buffer[ii] ? -1: 0;
    }
    texid = GLCreateTexture(glyph->size, (uint8_t*)buf);
    glyph->texid = texid;
  }
  ymin = 999999999; ymax = -999999999;
  glyph_dim.h = 0;
  for (i=0;i<128;i++) {
    glyph = &glyph_map[i];
    if (glyph->size.w > glyph_dim.w)
      glyph_dim.w = glyph->size.w;
    /* gives the tallest character; however we want line height */
    if (glyph->size.h > glyph_dim.h)
      glyph_dim.h = glyph->size.h;
    if (-glyph->bearing.y < ymin)
      ymin = -glyph->bearing.y;
    //if (-glyph->bearing.y + glyph->size.h > ymax)
    //  ymax = -glyph->bearing.y + glyph->size.h;
  }
  glyph_y = ymin;
  //glyph_dim.h = ymax - ymin;
}

void GLKillFt() {
  int i;

  for (i=0;i<128;i++) {
    GLDeleteTexture(glyph_map[i].texid);
    glyph_map[i].texid = 0;
  }
}

/* gui */
void GLDrawRect2(rect2 *rect, rgb8 *color);
bound2 GLTextExtents(char *str, dim2 size, int flags, fdim2 *scale, int chars);
bound2 GLCharExtents(char *str, dim2 size, int flags, fdim2 *scale, int idx);
void GLPrint(char *str, rect2 rect, int flags, rgb8 *color, rect2 *win);

void GLInputToGui(gl_input *gl_input, gui_input *gui_input) {
  int i;

  gui_input->screen = screen;
  gui_input->window = gl_input->window;
  for (i=0;i<512;i++)
    gui_input->keys[i] = gl_input->keys[i];
  gui_input->click = gl_input->click;
  gui_input->mouse = gl_input->mouse;
  gui_input->key = 0;
  gui_input->key_time = 0;
  gui_input->click_time = 0;
}

void GLDrawGuiRect(gui_rect *rect) {
  GLDrawRect2(&rect->rect2, (rgb8*)&rect->rgba8);
}

void GLDrawGuiText(char *str, rect2 *rect, rect2 *win, rgba8 *color) {
  GLPrint(str, *rect, 1, (rgb8*)color, win);
}

bound2 GLGuiTextExtents(char *str, dim2 *dim, int chars) {
  bound2 extents;
  int flags;

  flags = 0;
  if (dim->w == 0) { flags |= 1; }
  else if (dim->h == 0) { flags |= 3; }
  flags |= 8;
  extents = GLTextExtents(str, *dim, flags, 0, chars);
  return extents;
}

bound2 GLGuiCharExtents(char *str, dim2 *dim, int idx) {
  bound2 extents;
  int flags;

  flags = 0;
  if (dim->w == 0) { flags |= 1; }
  else if (dim->h == 0) { flags |= 3; }
  flags |= 16;
  extents = GLCharExtents(str, *dim, flags, 0, idx);
  return extents;
}

void GLInitGui() {
  gui_callbacks gui_callbacks;

  gui_callbacks.draw_rect = GLDrawGuiRect;
  gui_callbacks.draw_text = GLDrawGuiText;
  gui_callbacks.text_extents = GLGuiTextExtents;
  gui_callbacks.char_extents = GLGuiCharExtents;
  GuiInit(&gui_callbacks);
}

/* gl */
int GLInit(gl_callbacks *_callbacks) {
  int i;

  callbacks = *_callbacks;
  GLInitExt();
  GLInitFt();
  GLInitGui();
  TexturesInit(GLCreateTexture, GLDeleteTexture, GLUpdateTexture);
  GLSetupPrims();
  for (i=0;i<vbo_count;i++)
    glGenBuffers(1, &vboinfos[i].id);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  screen.x = -256;
  screen.y = -120;
  screen.w = 512;
  screen.h = 240;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  return SUCCESS;
}

int GLKill() {
  if (context.prims_head)
    free(context.prims_head);
  TexturesKill();
  if (image_texid)
    GLDeleteTexture(image_texid);
  GLKillFt();
  return SUCCESS;
}

int GLSetupPrims() {
  context.prims_head = malloc(0x800000);
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
  info->count = count; /* or should this be incremental? */
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
    //glColor3ub(255,255,255);
    glDrawArrays(info->gl_type, offs, info->vert_count);
    offs += info->stride/3;
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
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

/* draw extensions */

static inline void print_vec(FILE *s, vec *v) {
  fprintf(s, "(%x, %x, %x) ", v->x, v->y, v->z);
}
static inline void print_vec2(FILE *s, vec2 *v) {
  fprintf(s, "(%x, %x) ", v->x, v->y);
}
static inline void print_fvec(FILE *s, fvec *v) {
  fprintf(s, "(%f, %f, %f) ", v->x, v->y, v->z);
}
static inline void print_fvec2(FILE *s, fvec2 *v) {
  fprintf(s, "(%f, %f) ", v->x, v->y);
}
static inline void print_color(FILE *s, rgb *c) {
  fprintf(s, "(%x, %x, %x) ", c->r, c->g, c->b);
}
static inline void print_poly(FILE *s, poly3i *poly) {
  int i;
  for (i=0;i<3;i++) {
    fprintf(s, "vert %i: ", i);
    fprintf(s, "loc: ");
    print_vec(s, &poly->verts[i]);
    fprintf(s, "color: ");
    print_color(s, &poly->colors[i]);
    fprintf(s, "uvs: ");
    print_fvec(s, &poly->uvs[i]);
    fprintf(s, "\n");
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
#ifdef GFX_SW_PERSP
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
#ifdef GFX_SW_PERSP
          tri.verts[k].z = -1;
#endif
          tri.colors[k] = ((poly4i*)src)->colors[idx];
          tri.texid = ((poly4i*)src)->texid;
          if (prim->type == 3) {
            tri.texid = -1;
            tri.verts[k].y = -tri.verts[k].y;
            tri.verts[k].z = -1;
            tri.colors[k].r = 0xFFFFFFFF;
            tri.colors[k].g = 0xFFFFFFFF;
            tri.colors[k].b = 0xFFFFFFFF;
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

/**
 * non-immediate version of GLDrawRect
 */
void GLDrawRect2(rect2 *rect, rgb8 *color) {
  void **prims_tail;
  poly4i *prim;
  int i;

  prims_tail = GLGetPrimsTail();
  prim = (poly4i*)*prims_tail;
  prim->verts[0].x = rect->x;
  prim->verts[0].y = rect->y;
  prim->verts[0].z = -1;
  prim->verts[1].x = rect->x+rect->w;
  prim->verts[1].y = rect->y;
  prim->verts[1].z = -1;
  prim->verts[3].x = rect->x+rect->w;
  prim->verts[3].y = rect->y+rect->h;
  prim->verts[3].z = -1;
  prim->verts[2].x = rect->x;
  prim->verts[2].y = rect->y+rect->h;
  prim->verts[2].z = -1;
  for (i=0;i<4;i++) {
    prim->colors[i].r = color->r*16843009;
    prim->colors[i].g = color->g*16843009;
    prim->colors[i].b = color->b*16843009;
    // alpha not implemented yet...
  }
  prim->texid = -1;
  prim->type = 2;
  GLAddPrim(prim, 2046);
  prim->type=2;
  *prims_tail+=sizeof(poly4i);
}

/**
 * gets the extents, with respect to the origin, of a string of text
 * to be rendered with the current font
 *
 * str - string
 * size - desired size of the rendered text string,
 *        from leftmost and topmost pixel,
 *          to rightmost and bottommost pixel,
 *        or at least that for one dimension (will maintain aspect ratio);
 *        if bit 4 is set the height is the desired height
 *        for a single line of the rendered text string
 * flags - determines how the size argument is used:
 *         bit 1 - clear if both width and height are specified, else set
 *         bit 2 - used for bit 1 set:
 *                 set if only height specified, clear if only width specified
 *         bit 3 - height of the result is height of the tallest glyph amongst
 *                 set - glyphs for characters in the input string
 *                 clear - all possible glyphs
 *         bit 4 - clear if size.h is height of full rendered text block
 *                 set if size.h is line height
 *         bit 5 - set to disable bearing offset of extents (UL will be origin)
 *         internal flags
 *         bit 6 - clear if scale is an output
 *                 set if scale is an input with dimensions by which to scale
 *                 glyphs when determining extents
 * scale - optional output; dimensions by which to scale glyphs when
 *         rendering so as to fill the desired size
 * chars - optional number of characters of the text string that constitute
 *         the part to be rendered; a value of -1 defaults to all characters
 * returns the extents.
 *
 * when rendering a string to a particular location, subtract the upper
 * left extent returned and add the desired location-to the locations for
 * each glyph
 */
bound2 GLTextExtents(char *str, dim2 size, int flags, fdim2 *scale, int chars) {
  gl_glyph *glyph;
  bound2 extents;
  fdim2 _scale;
  rect2 ri;
  int32_t x, y, h, n;
  int i, len;

  if (!scale)
    scale = &_scale;
  if (flags & 8) {
    len = strlen(str);
    for (i=0,n=0;i<(chars==-1?len:chars);i++) {
      if (str[i] == '\n') { ++n; }
    }
    size.h *= (n+1);
  }
  if (!(flags & 32)) { scale->w = 1; scale->h = 1; } /* default scale */
  len = strlen(str);
  /* get extents of the rendered text at current scale */
  extents.p1.x =  99999999; extents.p1.y =  99999999;
  extents.p2.x = -99999999; extents.p2.y = -99999999;
  x = 0; y = 0;
  for (i=0;i<(chars==-1?len:chars);i++) {
    if (str[i] == '\n') {
      ri.x = x;
      ri.y = y;
      ri.w = glyph_dim.w*scale->w;
      ri.h = glyph_dim.h*scale->h;
      x = 0;
      y += glyph_dim.h*scale->h;
    }
    else {
      glyph = &glyph_map[str[i]];
      // ri.x = ((x + glyph->bearing.x)*scale->w);
      ri.x = x + (glyph->bearing.x*scale->w);
      ri.y = y + ((-glyph->bearing.y)*scale->h);
      ri.w = ((i==len-1)?glyph->size.w:(glyph->advance-glyph->bearing.x))*scale->w;
      ri.h = glyph->size.h*scale->h;
      if (str[i] == ' ') {
        ri.w = glyph->advance*scale->w;
        ri.h = glyph_dim.h*scale->h;
      }
    }
    if (ri.x < extents.p1.x)
      extents.p1.x = ri.x;
    if (ri.y < extents.p1.y)
      extents.p1.y = ri.y;
    if (ri.x + (int32_t)ri.w > extents.p2.x)
      extents.p2.x = ri.x + (int32_t)ri.w;
    if (ri.y + (int32_t)ri.h > extents.p2.y)
      extents.p2.y = ri.y + (int32_t)ri.h;
    if (str[i] != '\n')
      x += (glyph->advance*scale->w);
  }
  if (chars != -1) { /* include x bearing of the chars+1'th char if retrieving extents for 'chars' count chars */
    glyph = &glyph_map[str[i]];
    extents.p2.x += glyph->bearing.x*scale->w;
  }
  if (!(flags & 4)) { /* use largest height of all glyphs * # of lines */
    extents.p1.y = glyph_y*scale->h;
    extents.p2.y = (glyph_y+glyph_dim.h)*scale->h;
    for (i=0;i<(chars==-1?len:chars);i++) {
      if (str[i] == '\n') { extents.p2.y+=(glyph_dim.h*scale->h); }
    }
  }
  if (flags & 16) {
    extents.p2.x -= extents.p1.x;
    extents.p2.y -= extents.p1.y;
    extents.p1.x = 0; extents.p1.y = 0;
  }
  if (flags & 32) { return extents; } /* return scaled extents */
  scale->w = extents.p2.x - extents.p1.x;
  scale->h = extents.p2.y - extents.p1.y;
  if (flags & 1) { /* maintain aspect? */
   if (!(flags & 2)) { /* fit to h? */
     scale->w = (float)size.h / scale->h;
     scale->h = (float)size.h / scale->h;
   }
   else { /* fit to w */
     scale->h = (float)size.w / scale->w;
     scale->w = (float)size.w / scale->w;
   }
  }
  else { /* no maintain aspect */
   scale->w = (float)size.w / scale->w;
   scale->h = (float)size.h / scale->h;
  }
  /* get extents of the rendered text at adjusted scale */
  extents = GLTextExtents(str, size, flags | 32, scale, chars);
  return extents;
}

/**
 * gets the extents, with respect to the origin, of a character
 * within a string of text to be rendered with the current font
 *
 * str - string
 * size - desired size of the rendered text string,
 *        from leftmost and topmost pixel,
 *          to rightmost and bottommost pixel,
 *        or at least that for one dimension (will maintain aspect ratio)
 * flags - determines how the size argument is used:
 *         see GLTextExtents
 * scale - optional output; dimensions by which to scale glyphs when
 *         rendering so as to fill the desired size
 * idx - index of the character to be rendered
 * returns the extents.
 */
bound2 GLCharExtents(char *str, dim2 size, int flags, fdim2 *scale, int idx) {
  gl_glyph *glyph;
  bound2 extents;
  fdim2 _scale;
  rect2 ri;
  int i, len, x, y;

  if (!scale)
    scale = &_scale;
  extents = GLTextExtents(str, size, flags, scale, -1);
  len = strlen(str);
  ri.x = 0; ri.y = 0; ri.w = 0; ri.h = 0;
  x = 0; y = 0;
  for (i=0;i<idx;i++) {
    if (str[i] == '\n') { x=0; y+=scale->h*glyph_dim.h; continue; }
    glyph = &glyph_map[str[i]];
    x += glyph->advance*scale->w;
  }
  if (i == -1 || i == len || str[i] == '\n') {
    ri.x = x;
    ri.y = y;
    ri.w = glyph_dim.w*scale->w;
    ri.h = glyph_dim.h*scale->h;
  }
  else {
    glyph = &glyph_map[str[i]];
    ri.x = x + (glyph->bearing.x*scale->w);
    ri.y = y + ((int32_t)glyph->size.h - glyph->bearing.y)*scale->h;
    ri.w = glyph->size.w*scale->w;
    ri.h = glyph->size.h*scale->h; // glyph->size.h*scale->h;
    if (str[i] == ' ') {
      ri.w = glyph->advance*scale->w;
      ri.h = glyph_dim.h*scale->h;
    }
  }
  extents.p1.x += ri.x;
  extents.p1.y += ri.y;
  extents.p2.x = extents.p1.x + ri.w;
  extents.p2.y = extents.p1.y + ri.h;
  return extents;
}

void GLPrint(char *str, rect2 rect, int flags, rgb8 *color, rect2 *win) {
  gl_glyph *glyph;
  vec2 trans;
  fdim2 scale;
  fbound2 uvb;
  rect2 ri;
  bound2 extents, bi, bc;
  poly4i *prim, *next, *tail;
  void *ot, **prims_tail;
  int32_t x, y;
  int i, ii, len;
  rgb8 white;

  white.r=255;white.g=255;white.b=255;
  prims_tail = GLGetPrimsTail();
  ot = context.ot;
  extents = GLTextExtents(str, rect.dim, flags, &scale, -1);
  trans.x = rect.x - extents.p1.x;
  trans.y = rect.y - extents.p1.y;
  /* create quad prims for each str char */
  ri.x = 0; ri.y = 0; ri.w = 0; ri.h = 0;
  x = 0; y = 0;
  len = strlen(str);
  for (i=0;i<len;i++) {
    if (str[i] == '\n') { x=0; y+=scale.h*glyph_dim.h; continue; }
    prim = (poly4i*)*prims_tail;
    glyph = &glyph_map[str[i]];
    ri.x = x + (glyph->bearing.x*scale.w);
    ri.y = y + ((int32_t)glyph->size.h - glyph->bearing.y)*scale.h;
    ri.w = glyph->size.w*scale.w;
    ri.h = glyph->size.h*scale.h;
    //printf("GLPrint:\n%s\nchar %i (%c): x: %i, y: %i, w: %i, h: %i\n",
    //  str, i, str[i], ri.x, ri.y, ri.w, ri.h);
    //printf("scale->w: %f\n", scale.w);
    //printf("flags: %i\n", flags);
    bi.p1.x = ri.x + trans.x;
    bi.p1.y = ri.y + trans.y + (-ri.h);
    bi.p2.x = bi.p1.x + ri.w;
    bi.p2.y = bi.p1.y + ri.h;
    // if ((flags & 4) && (bi.p1.x < rect.x || bi.p2.x > rect.x + (int32_t)rect.w))
    //  continue;
    // if ((flags & 8) && (bi.p1.y < rect.y || bi.p2.y > rect.y + (int32_t)rect.h))
    //  continue;
    uvb.p1.x = 0.0; uvb.p1.y = 0.0;
    uvb.p2.x = 1.0; uvb.p2.y = 1.0;
    if (win) {
      if (bi.p1.x < win->x && bi.p2.x < win->x) {
        x += glyph->advance*scale.w;
        continue;
      }
      if (bi.p1.x > (win->x+(int32_t)win->w) && bi.p2.x > (win->x+(int32_t)win->w)) {
        x += glyph->advance*scale.w;
        continue;
      }
      if (bi.p1.y < win->y && bi.p2.y < win->y) {
        x += glyph->advance*scale.w;
        continue;
      }
      if (bi.p1.y > (win->y+(int32_t)win->h) && bi.p2.y > (win->y+(int32_t)win->h)) {
        x += glyph->advance*scale.w;
        continue;
      }
      bc.p1.x = max(bi.p1.x, win->x);
      bc.p2.x = min(bi.p2.x, (win->x+(int32_t)win->w));
      bc.p1.y = max(bi.p1.y, win->y);
      bc.p2.y = min(bi.p2.y, (win->y+(int32_t)win->h));
      uvb.p1.x = ((float)bc.p1.x - bi.p1.x)/((float)bi.p2.x - bi.p1.x);
      uvb.p2.x = ((float)bc.p2.x - bi.p1.x)/((float)bi.p2.x - bi.p1.x);
      uvb.p1.y = ((float)bc.p1.y - bi.p1.y)/((float)bi.p2.y - bi.p1.y);
      uvb.p2.y = ((float)bc.p2.y - bi.p1.y)/((float)bi.p2.y - bi.p1.y);
      bi = bc;
    }
    prim->verts[0].x = bi.p1.x;
    prim->verts[0].y = bi.p1.y;
    prim->verts[0].z = -1;
    prim->verts[1].x = bi.p2.x;
    prim->verts[1].y = bi.p1.y;
    prim->verts[1].z = -1;
    prim->verts[3].x = bi.p2.x;
    prim->verts[3].y = bi.p2.y;
    prim->verts[3].z = -1;
    prim->verts[2].x = bi.p1.x;
    prim->verts[2].y = bi.p2.y;
    prim->verts[2].z = -1;
    if (!color)
      color = &white;
    for (ii=0;ii<4;ii++) {
      prim->colors[ii].r = color->r*16843009;
      prim->colors[ii].g = color->g*16843009;
      prim->colors[ii].b = color->b*16843009;
    }
    prim->uvs[0].x = uvb.p1.x; prim->uvs[0].y = uvb.p1.y;
    prim->uvs[1].x = uvb.p2.x; prim->uvs[1].y = uvb.p1.y;
    prim->uvs[3].x = uvb.p2.x; prim->uvs[3].y = uvb.p2.y;
    prim->uvs[2].x = uvb.p1.x, prim->uvs[2].y = uvb.p2.y;
    prim->texid = glyph->texid;
    prim->type=2;
    GLAddPrim(prim, 2046);
    prim->type=2;
    *prims_tail+=sizeof(poly4i);
    x += glyph->advance*scale.w;
  }
}

/* ------------ */

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
  gui_input gui_input;

  if (callbacks.pre_update)
    (*callbacks.pre_update)();
  if (callbacks.input) {
    (*callbacks.input)(&gl_input);
    GLInputToGui(&gl_input, &gui_input);
  }
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
  if (callbacks.input)
    GuiUpdate(&gui_input);
  //GLResetPrims(&context);
  GuiDraw();
  if (ns.draw_skip_counter == 0)
    GLDraw(context.ot);
  // GLDrawWallMap();
  GLResetPrims(&context);
  cur_display_flags = next_display_flags;  /* copy display/animate flags */
  if (callbacks.post_update)
    (*callbacks.post_update)();
}
