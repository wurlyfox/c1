#ifndef _TIME_H_
#define _TIME_H_

#include "common.h"

extern void SetTicksElapsed(ticks_t ticks);
extern ticks_t GetTicksElapsed();
extern ticks_t GetTicksMonotonic();

#endif /* _TIME_H_ */
