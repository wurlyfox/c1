#ifndef _MATH_H_
#define _MATH_H_

#include "common.h"
#include "geom.h"
#include "gool.h"

#ifdef PSX
#include <LIBGTE.H>
#define sin rsin
#define cos rcos
#define atan2 ratan2
#define sqrt SquareRoot0
#else
#include "pc/math.h"
#define sin msin
#define cos mcos
#define atan2 matan2
#define sqrt msqrt
#endif

extern uint32_t ApxDist(vec *v1, vec *v2);
extern int OutOfRange(gool_object* obj, vec *v1, vec *v2, int32_t x, int32_t y, int32_t z);
extern uint32_t EucDist(vec *v1, vec *v2);
extern uint32_t EucDistXZ(vec *v1, vec *v2);
extern int32_t AngDistXZ(vec *v1, vec *v2);
extern int32_t AngDistXY(vec *v1, vec *v2);
extern int sub_80029E80(int32_t a1, int32_t a2, uint32_t aa_max);
extern int sub_80029F04(int a1, int a2);
extern void sranda(uint32_t seed);
extern void sranda2();
extern uint32_t randa(uint32_t max);
extern void srandb(uint32_t seed);
extern void srandb2();
extern uint32_t randb(uint32_t max);
extern uint32_t absdiff(int32_t a, int32_t b);
 
#endif /* _MATH_H_ */
