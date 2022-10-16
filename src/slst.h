#ifndef _SLST_H_
#define _SLST_H_

#include "common.h"
#include "formats/slst.h"

extern poly_id_list *poly_id_buf1;
extern poly_id_list *poly_id_buf2;
extern poly_id_list *next_poly_ids;

#define cur_poly_ids poly_id_buf1
#define prev_poly_ids poly_id_buf2

extern int SlstInit();
extern int SlstKill();
extern poly_id_list *SlstUpdate(slst_item *item,
  poly_id_list *src, poly_id_list *dst, int dir);

#endif /* _SLST_H_ */
