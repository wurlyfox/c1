#ifndef _F_CVTX_H_
#define _F_CVTX_H_

#include "common.h"
#include "ns.h"

typedef struct {
  uint8_t x;
  uint8_t y;
  uint8_t z;
  uint8_t r;
  uint8_t g;
  uint8_t b;
} cvtx_vertex;

typedef struct {
  uint32_t length; // in 4-byte words
  eid_t tgeo;
  int32_t x;
  int32_t y;
  int32_t z;
  uint32_t bound_x1;
  uint32_t bound_y1;
  uint32_t bound_z1;
  uint32_t bound_x2;
  uint32_t bound_y2;
  uint32_t bound_z2;
  int32_t col_x;
  int32_t col_y;
  int32_t col_z;
  cvtx_vertex vertices[];
} cvtx_frame;

#endif /* _F_CVTX_H_ */

