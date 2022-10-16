#ifndef _F_TGEO_H_
#define _F_TGEO_H_

#include "common.h"
#include "ns.h"
#include "geom.h"

typedef texinfo tgeo_texinfo;
typedef colinfo tgeo_colinfo;

typedef struct {
  uint32_t poly_count;
  int32_t scale_x;
  int32_t scale_y;
  int32_t scale_z;
  uint32_t texinfo_count;
  tgeo_texinfo texinfos[];
} tgeo_header;

typedef struct {
  uint16_t a_idx; // should be a_offs
  uint16_t b_idx; // should be b_offs
  uint16_t c_idx; // should be c_offs
BITFIELDS_02(
  uint16_t is_flat_shaded:1,
  uint16_t texinfo_idx:15
)
} tgeo_polygon;

#endif /* _F_TGEO_H_ */
