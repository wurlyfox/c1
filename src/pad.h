#ifndef _PAD_H_
#define _PAD_H_

#include "common.h"

typedef struct {
  uint32_t tapped;
  uint32_t held;
  uint32_t held_prev;
  uint32_t tapped_prev;
  uint32_t held_prev2;
} pad;

extern void PadInit(int count);
extern void PadUpdate();

#endif /* _PAD_H_ */
