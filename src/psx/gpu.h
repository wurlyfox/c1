#ifndef _GPU_H_
#define _GPU_H_

#include "common.h"
#include <LIBGPU.H>

typedef struct {
  int idx;
  DISPENV disp;
  DRAWENV draw;
  void *prims_head;
  void *prims_tail;
  uint32_t draw_stamp;
  uint32_t sync_stamp;
  uint32_t ticks_per_frame;
  void *ot[2048];
} gfx_context;

typedef struct {
  int count;
  gfx_context *c1_p;
  gfx_context *c2_p;
  gfx_context *cur;
  gfx_context c1;
  gfx_context c2;
} gfx_context_db; /* double buffered context */

#endif /* _GPU_H_ */