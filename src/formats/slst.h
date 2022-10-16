#ifndef _F_SLST_H_
#define _F_SLST_H_

#include "common.h"
#include "ns.h"

typedef struct {
BITFIELDS_03(
  uint16_t flag:1,
  uint16_t world_idx:3,
  uint16_t poly_idx:12
)
} poly_id;

typedef struct {
  union {
    struct {
      uint16_t len;
      uint16_t type;
      poly_id ids[0];
    };
    poly_id data[0];
  };
} poly_id_list;

typedef struct {
  uint16_t split_idx;
  uint16_t swaps_idx;
  uint16_t data[];
} slst_delta;

typedef struct {
  uint16_t length;
  uint16_t type;
  union {
    poly_id poly_ids[0];
    slst_delta delta;
  };
} slst_item;

typedef struct {
BITFIELDS_03(
  uint16_t fmt:1,
  uint16_t offset:4,
  uint16_t idx:11
)
} slst_swap_fmta;

typedef struct {
BITFIELDS_03(
  uint16_t fmt:2,
  uint16_t :2,
  uint16_t idx:12
);
BITFIELDS_03(
  uint16_t fmt_2:2,
  uint16_t :2,
  uint16_t offset:12
);
} slst_swap_fmtb;

typedef struct {
BITFIELDS_03(
  uint16_t fmt:2,
  uint16_t idx:9,
  uint16_t offset:5
)
} slst_swap_fmtc;

#endif /* _F_SLST_H_ */
