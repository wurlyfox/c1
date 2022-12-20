#ifndef _F_WGEO_H_
#define _F_WGEO_H_

#include "common.h"
#include "ns.h"
#include "geom.h"

typedef texinfo2 wgeo_texinfo;

typedef struct {
  int32_t trans_x;
  int32_t trans_y;
  int32_t trans_z;
  uint32_t poly_count;
  uint32_t vert_count;
  uint32_t texinfo_count;
  uint32_t tpag_count;
  uint32_t is_backdrop;
  eid_t tpags[8];
  wgeo_texinfo texinfos[];
} wgeo_header;

typedef struct {
BITFIELDS_09(
  uint32_t a_idx:12,
  uint32_t b_idx:12,
  uint32_t anim_period:3,
  uint32_t anim_mask:4,
  uint32_t :1,
  uint32_t c_idx:12,
  uint32_t tinf_idx:12,
  uint32_t tpag_idx:3,
  uint32_t anim_phase:5
)
} wgeo_polygon;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t z8;
BITFIELDS_05(
  int16_t y:13,
  int16_t z3:3,
  int16_t x:13,
  uint16_t z2:2,
  uint16_t fx:1
)
} wgeo_vertex;

#endif /* _F_WGEO_H_ */
