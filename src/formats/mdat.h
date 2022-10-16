#ifndef _F_MDAT_H_
#define _F_MDAT_H_

#include "common.h"
#include "ns.h"

typedef struct { /* location and height (clut_count) of a vertically stacked group of 256x1 cluts */
  uint32_t clut_x;
  uint32_t clut_y;
  uint32_t clut_count;
} clut_line;

typedef struct { /* describes a title card/state, including tiles and cluts for a splash screen image and object models (also entity count) */
  int w_idx;
  int h_idx;
  int ipal_count;
  int entity_count;
  int unk_4;
  int tgeo_count;
  clut_line clut_lines[8];
  eid_t ipals[46];
  eid_t tgeos[32];
  eid_t imags[32];
} mdat_header;

#include "formats/zdat.h"
typedef zone_entity mdat_entity;

#endif /* _F_MDAT_H_ */
