#ifndef _GL_H_
#define _GL_H_

#include "common.h"
#include "geom.h"
#include "pcgfx.h"

#define GL_PRIMS_CAPACITY 0x800000

typedef struct {
  uint8_t keys[512];
  vec2 mouse;
  int click;
  int buttons;
  int wheel;  
  rect2 window;
  rect2 screen;
} gl_input;

typedef struct {
  void (*pre_update)();
  void (*post_update)();
  int (*ext_supported)(const char*);
  void *(*proc_addr)(const char*);
  void (*input)(gl_input*);
  int (*fullscreen)(int, int *, int *);
} gl_callbacks;

typedef struct {
  void *prims_head;
  void *prims_tail;
  ticks_t draw_stamp;
  ticks_t sync_stamp;
  ticks_t ticks_per_frame;
  void *ot[2048];
  vec2 draw_clip;
} gl_context;

extern int GLInit();
extern int GLKill();
extern int GLFullscreen(int fullscreen);
extern int GLSetupPrims();
extern void GLResetPrims(gl_context *gc);
extern void GLDrawRect(int x, int y, int w, int h, int r, int g, int b);
extern void GLDrawOverlay(int brightness);
extern void GLDrawImage(dim2 *dim, uint8_t *buf, pnt2 *loc);
extern void **GLGetPrimsTail();
extern void GLResetOT(void *ot, int len);
extern void GLClear();
extern void GLUpdate();

#ifdef CFLAGS_EXTENSIONS
extern int GLCreateTexture(dim2 dim, uint8_t *buf);
extern void GLDeleteTexture(int texid);
extern void GLUpdateTexture(int texid, rect2 rect, uint8_t *buf);
#endif

#endif /* _GL_H_ */
