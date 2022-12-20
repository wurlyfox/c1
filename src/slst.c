#include "slst.h"

/* .sbss */
poly_id_list *poly_id_buf1;
poly_id_list *poly_id_buf2;
poly_id_list *next_poly_ids;

extern lid_t cur_lid;

//----- (80029A58) --------------------------------------------------------
int SlstInit() {
  size_t size;
  int poly_id_count;

  if (cur_lid == LID_TITLE || cur_lid == LID_INTRO)
    poly_id_count = 2610;
  else
    poly_id_count = 1460;
  size = sizeof(poly_id_list) + poly_id_count*sizeof(poly_id);
  poly_id_buf1 = malloc(size); /* gp[0xAF] */
  if (!poly_id_buf1) { return ERROR_MALLOC_FAILED; }
  poly_id_buf1->len = 0;
  poly_id_buf2 = malloc(size); /* gp[0xB0] */
  if (!poly_id_buf2) { return ERROR_MALLOC_FAILED; }
  poly_id_buf2->len = 0;
  next_poly_ids = poly_id_buf1; /* gp[0xC1] */
  return SUCCESS;
}

//----- (80029AD8) --------------------------------------------------------
int SlstKill() {
  free(poly_id_buf1);
  free(poly_id_buf2);
  return SUCCESS;
}

#ifdef PSX
#include "psx/r3000a.h"
poly_id_list *SlstUpdate(slst_item *item, poly_id_list *src, poly_id_list *dst, int dir) {
  if (item->type & 1) {
    if (dir == 1)
      RSlstDecodeForward(item, src, dst);
    else
      RSlstDecodeBackward(item, src, dst);
  }
  else {
    dst->len = item->length;
    RMemCpy(item->poly_ids, dst->ids, dst->len);
  }
  return dst;
}
#else
typedef struct {
  int i;
  int start;
  int end;
  uint16_t node;
  poly_id poly;
  int idx;
  int len;
  int count;
} slst_context;

static const int16_t null_poly_id_val = -1;

static inline void SlstRemNext(slst_context *rem, uint16_t *nodes) {
  if (rem->len > 0) {
    rem->poly = *(poly_id*)&nodes[rem->i++];
    rem->idx += rem->poly.flag; /* advances by 1 if set */
  }
  else { /* all rem.len ids specified by the node have been removed */
    rem->node = nodes[rem->i++];
    if (rem->i < rem->end && rem->node != 0xFFFF) { /* process the next removing node */
      rem->idx = rem->node & 0xFFF;
      rem->len = ((rem->node & 0xF000) >> 12) + 1; /* min 1 */
    }
    else {
      rem->idx = 0xFFFF00; /* null index */
      rem->len = 0; /* no removal */
    }
    if (rem->len > 0)
      rem->poly = *(poly_id*)&nodes[rem->i++];
  }
}

static inline void SlstAddNext(slst_context *add, uint16_t *nodes) {
  if (add->len > 0) {
    add->poly = *(poly_id*)&nodes[add->i++];
    add->idx += add->poly.flag; /* advances by 1 if set */
  }
  else { /* all add.len ids specified by the node have been added */
    add->node = nodes[add->i++];
    if (add->i < add->end && add->node != 0xFFFF) { /* process the next adding node */
      add->idx = add->node & 0xFFF;
      add->len = ((add->node & 0xF000) >> 12) + 1; /* min 1 */
    }
    else {
      add->idx = 0xFFFF00;
      add->len = 0;
    }
    if (add->len > 0)
      add->poly = *(poly_id*)&nodes[add->i++];
  }
}

//----- (800334A0) --------------------------------------------------------
static void SlstDecodeForward(slst_item *item, poly_id_list *src, poly_id_list *dst) {
  uint16_t *nodes;
  slst_delta *delta;
  slst_context add, rem, swap;
  int isrc, idst, i, count_copy;
  int node_len;
  poly_id tmp;

  delta = &item->delta;
  nodes = (uint16_t*)delta;
  rem.start = offsetof(slst_delta, data)/sizeof(delta->data[0]);
  add.start = rem.end = delta->split_idx;
  swap.start = add.end = delta->swaps_idx;
  swap.end = item->length;
  rem.i = rem.start;
  add.i = add.start;
  rem.len = 0; /* done so the initial calls to add/rem_next  */
  add.len = 0; /* run only the else block */
  SlstRemNext(&rem, nodes);
  SlstAddNext(&add, nodes);
  isrc = 0;
  idst = 0;
  add.count = 0;
  rem.count = 0;
  do {
    if ((isrc < src->len) &&
      (0 < rem.len) &&
      (((rem.idx-1)-isrc) == 0)) {
      isrc++; /* advance the source point (this is the 'removal') */
      rem.len--;
      rem.count++; /* skips/removes only one id at a time */
      SlstRemNext(&rem, nodes);
    }
    else if ((add.len > 0) &&
      (((add.idx + rem.count) - isrc) == 0)) {
      add.poly.flag = 0;
      dst->ids[idst++] = add.poly; /* append the next poly id at insertion pt */
      add.len--;
      add.count++; /* unused */
      SlstAddNext(&add, nodes);
    }
    else if (isrc < src->len) {
      count_copy = min3((add.idx + rem.count) - isrc,
        rem.idx - (isrc+1),
        src->len - (isrc+1));
      if (count_copy == 0) { count_copy = 1; }
      for (i=0;i<count_copy;i++) {
        dst->ids[idst++] = src->ids[isrc++];
      }
    }
  } while (isrc < src->len ||
    add.len > 0 ||
    rem.len > 0);
  dst->len = idst;
  swap.i = swap.start;
  while (swap.i < swap.end) {
    swap.node = nodes[swap.i];
    if (swap.node == 0xFFFF) { break; }
    if (swap.node & 0x8000) { /* fmt A: 1BBBBAAA AAAAAAAA */
      swap.idx = ((slst_swap_fmta*)&nodes[swap.i])->idx;
      swap.len = ((slst_swap_fmta*)&nodes[swap.i])->offset;
      node_len = sizeof(slst_swap_fmta)/sizeof(uint16_t);
    } /* fmt C */
    else if ((swap.node & 0xC000) == 0x4000) { /* is the follow-up node of the preceding node */
      swap.idx += ((slst_swap_fmtc*)&nodes[swap.i])->idx;
      swap.len = ((slst_swap_fmtc*)&nodes[swap.i])->offset + 16;
      node_len = sizeof(slst_swap_fmtc)/sizeof(uint16_t);
    }
    else { /* fmt B: 0000AAAA AAAAAAAA 0000BBBB BBBBBBBB */
      swap.idx = ((slst_swap_fmtb*)&nodes[swap.i])->idx;
      swap.len = ((slst_swap_fmtb*)&nodes[swap.i])->offset;
      node_len = sizeof(slst_swap_fmtb)/sizeof(uint16_t);
    }
    tmp = dst->ids[swap.idx];
    dst->ids[swap.idx] = dst->ids[swap.idx+swap.len+1];
    dst->ids[swap.idx+swap.len+1] = tmp;
    swap.i += node_len;
  }
}

//----- (80033878) --------------------------------------------------------
static void SlstDecodeBackward(slst_item *item, poly_id_list *src, poly_id_list *dst) {
  uint16_t *nodes;
  slst_delta *delta;
  slst_context add, rem, swap;
  int isrc, idst, i, count_copy;
  poly_id tmp;

  delta = &item->delta;
  nodes = (uint16_t*)delta;
  add.start = offsetof(slst_delta, data)/sizeof(delta->data[0]);
  rem.start = add.end = delta->split_idx;
  swap.start = rem.end = delta->swaps_idx;
  swap.end = item->length;
  swap.i = swap.end-1;
  while (swap.i >= swap.start) {
    swap.node = nodes[swap.i];
    if (swap.node == 0xFFFF) {
      swap.i--;
      continue;
    }
    if (swap.node & 0x8000) { /* fmt A: 1BBBBAAA AAAAAAAA */
      swap.i -= sizeof(slst_swap_fmta)/sizeof(uint16_t)-1;
      swap.idx = ((slst_swap_fmta*)&nodes[swap.i])->idx;
      swap.len = ((slst_swap_fmta*)&nodes[swap.i])->offset;
    } /* fmt C */
    else if ((swap.node & 0xC000) == 0x4000) {
      swap.i -= sizeof(slst_swap_fmtc)/sizeof(uint16_t)-1;
      swap.idx = ((slst_swap_fmtc*)&nodes[swap.i])->idx;
      swap.len = ((slst_swap_fmtc*)&nodes[swap.i])->offset + 16;
      if (nodes[swap.i-1] & 0x8000) /* lookback a short */
        swap.idx += (((slst_swap_fmta*)&nodes[swap.i-1]))->idx;
      else
        swap.idx += (((slst_swap_fmtb*)&nodes[swap.i-2]))->idx;
    }
    else { /* fmt B: 0000AAAA AAAAAAAA 0000BBBB BBBBBBBB */
      swap.i -= sizeof(slst_swap_fmtb)/sizeof(uint16_t)-1;
      swap.idx = ((slst_swap_fmtb*)&nodes[swap.i])->idx;
      swap.len = ((slst_swap_fmtb*)&nodes[swap.i])->offset;
    }
    tmp = src->ids[swap.idx];
    src->ids[swap.idx] = src->ids[swap.idx+swap.len+1];
    src->ids[swap.idx+swap.len+1] = tmp;
    swap.i--; /* go back a short; we don't know the specific struct yet */
  }
  rem.i = rem.start;
  add.i = add.start;
  rem.len = 0; /* done so the initial calls to add/rem_next */
  add.len = 0; /* run only the else block */
  SlstRemNext(&rem, nodes);
  SlstAddNext(&add, nodes);
  isrc = 0;
  idst = 0;
  add.count = 0;
  rem.count = 0;
  do {
    if (isrc < src->len &&
      0 < rem.len &&
      ((rem.idx+rem.count)-isrc) == 0) {
      isrc++; /* advance the source point (this is the 'removal') */
      rem.len--;
      rem.count++; /* skips/removes only one id at a time */
      SlstRemNext(&rem, nodes);
    }
    else if (add.len > 0 &&
      (((add.idx + rem.count) - (add.count+1)) - isrc) == 0) {
      add.poly.flag = 0;
      dst->ids[idst++] = add.poly; /* append the next poly id at insertion pt */
      add.len--;
      add.count++;
      SlstAddNext(&add, nodes);
    }
    else if (isrc < src->len) {
      count_copy = min3((add.idx + (rem.count-add.count)) - (isrc+1),
        ((rem.idx + rem.count) - isrc),
        (src->len - (isrc+1)));
      if (count_copy <= 0) { count_copy = 1; }
      for (i=0;i<count_copy;i++) {
        dst->ids[idst++] = src->ids[isrc++];
      }
    }
  } while (isrc < src->len ||
    add.len > 0 ||
    rem.len > 0);
  dst->len = idst;
}

//----- (80029B0C) --------------------------------------------------------
poly_id_list *SlstUpdate(slst_item *item, poly_id_list *src, poly_id_list *dst, int dir) {
  if (item->type & 1) {
    if (dir == 1)
      SlstDecodeForward(item, src, dst);
    else
      SlstDecodeBackward(item, src, dst);
  }
  else {
    memcpy(dst, item, sizeof(poly_id_list)+(item->length*sizeof(poly_id)));
  }
  return dst;
}
#endif
