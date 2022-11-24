#ifndef _UTIL_H_
#define _UTIL_H_

#include "common.h"

extern int ADPCMToPCM16(uint8_t *adpcm, size_t size, uint8_t *pcm);

extern int VabToSf2(uint8_t *vab, uint8_t *sf2);
extern int SeqToMid(uint8_t *seq, uint8_t *mid);

#endif /* _UTIL_H_ */
