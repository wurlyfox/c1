#ifndef _PC_PAD_H_
#define _PC_PAD_H_

#include "common.h"
#include "../pad.h"

#define PAD_SELECT      0x100
#define PAD_L3          0x200
#define PAD_R3          0x400
#define PAD_START       0x800
#define PAD_UP          0x1000
#define PAD_RIGHT       0x2000
#define PAD_DOWN        0x4000
#define PAD_LEFT        0x8000
#define PAD_L2          0x1
#define PAD_R2          0x2
#define PAD_L1          0x4
#define PAD_R1          0x8
#define PAD_TRIANGLE    0x10
#define PAD_CIRCLE      0x20
#define PAD_CROSS       0x40
#define PAD_SQUARE      0x80

uint32_t SwPadRead(int idx);

#endif /* _PC_PAD_H_ */

