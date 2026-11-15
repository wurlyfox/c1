#ifndef _PC_CARD_H_
#define _PC_CARD_H_

#include "common.h"

/* virtual memory card (see card.c) */
extern int CardControl(int op, int part_idx);
extern void CardUpdate();

#endif /* _PC_CARD_H_ */
