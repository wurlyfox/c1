#ifndef _F_SVTX_H_
#define _F_SVTX_H_

#include "common.h"
#include "ns.h"

typedef struct {
  uint8_t x;
  uint8_t y;
  uint8_t z;
  int8_t normal_x;
  int8_t normal_y;
  int8_t normal_z;
} svtx_vertex;

typedef struct {
  uint32_t length; // in uint32_t
  eid_t tgeo;
  int32_t x;
  int32_t y;
  int32_t z;
  int32_t bound_x1;
  int32_t bound_y1;
  int32_t bound_z1;
  int32_t bound_x2;
  int32_t bound_y2;
  int32_t bound_z2;
  int32_t col_x;
  int32_t col_y;
  int32_t col_z;
  svtx_vertex vertices[];
} svtx_frame;

#endif /* _F_SVTX_H_ */
