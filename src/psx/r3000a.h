#ifndef _R3000A_H_
#define _R3000A_H_

#include "common.h"
#include "geom.h"
#include "zdat.h"

typedef struct {
  /*  */
  char reserved[0x40];
  uint32_t consts[2];
  union {
    uint32_t vars[46];
    struct {
      rgb far_color1;
      int far_t1;
      rgb far_color2;
      int far_t2;
    };
    struct {
      rgb dark_illum;
      int dark_shamt_add;
      int dark_shamt_sub;
      int dark_amb_fx0;
      int dark_amb_fx1;
    };
  };
  union {
    zone_world worlds[8];
    struct {
      uint32_t wall_bitmap[32];
      uint32_t wall_cache[2*64];
      uint32_t circle_bitmap[32];
    };
  };
} scratch;

#endif /* _R3000A_H_ */