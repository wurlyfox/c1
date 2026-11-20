#ifndef _EXT_MATH_H_
#define _EXT_MATH_H_

#define TWO_PI 6.283185307179586

/* the project math.h shadows functions in the c standard library
   via macro redefinitions; undefine them for extension purposes */
#undef sin
#undef cos
#undef atan2
#undef sqrt
#if defined(_MSC_VER)
#undef abs
#endif
#include_next <math.h>

#endif /* _EXT_MATH_H_ */
