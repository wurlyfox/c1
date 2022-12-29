#ifndef _UTIL_H_
#define _UTIL_H_

#include "common.h"

extern size_t ADPCMToPCM16(uint8_t *adpcm, size_t size, uint8_t *pcm, int *loop);

extern size_t VabToSf2(uint8_t *vab, uint8_t *sf2);
extern size_t SeqToMid(uint8_t *seq, uint8_t *mid, size_t *seq_size);

#endif /* _UTIL_H_ */
