#ifndef _F_PBAK_H_
#define _F_PBAK_H_

#include "common.h"
#include "gfx.h"
#include "level.h"

typedef struct {
  int ticks_elapsed;
  uint32_t held;
} pbak_frame;

typedef struct {
  int frame_count;
  uint32_t seed;
  int unk;
  int draw_count;
  level_state savestate;
  uint32_t draw_stamp;
  bound crash_bound;
  int ticks_per_frame;
  pbak_frame frames[];
} pbak_header;

#endif /* _F_PBAK_H_ */
