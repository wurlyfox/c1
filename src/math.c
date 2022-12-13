#include "math.h"
#include "geom.h"

/* .sbss */
uint32_t seed_a, seed_b; /* 800564D8, 800564DC; gp[0xDC], gp[0xE0] */

/**
 * compute the approximate (manhattan) distance between 2 3d points
 */
//----- (80029B90) --------------------------------------------------------
uint32_t ApxDist(vec *v1, vec *v2) {
  vec da;
  int max_da;

  da.x = abs(v1->x - v2->x);
  da.y = abs(v1->y - v2->y);
  da.z = abs(v1->z - v2->z);
  max_da = max(da.x, max(da.y, da.z));
  if (max_da == da.x)
    return ((da.y + da.z) / 4) + da.x;
  else if (max_da == da.y)
    return ((da.x + da.z) / 4) + da.y;
  else
    return ((da.x + da.y) / 4) + da.z;
}

/**
 * test if 2 points are 'out of range' of each other
 * that is, if abs(v1.x - v2.x) > x, abs(v1.y - v2.y) > y, or abs(v1.z - v2.z) > z
 */
 //----- (80029C90) --------------------------------------------------------
int OutOfRange(gool_object *obj, vec *v1, vec *v2, int32_t x, int32_t y, int32_t z) {
  vec d;

  d.x = v1->x - v2->x;
  d.y = v1->y - v2->y;
  d.z = v1->z - v2->z;
  if (x < d.x || d.x < -x
    || y < d.y || d.y < -y
    || z < d.z || d.z < -z) {
    if (obj)
      obj->status_a |= GOOL_FLAG_LBOUND_INVALID;
    return 1;
  }
  return 0;
}

/**
 * compute the (euclidean) distance between two [3d] points
 */
//----- (80029D30) --------------------------------------------------------
uint32_t EucDist(vec *v1, vec *v2) {
  vec d;

  d.x = (v1->x - v2->x) >> 8;
  d.y = (v1->y - v2->y) >> 8;
  d.z = (v1->z - v2->z) >> 8;
  return sqrt(d.x*d.x+d.y*d.y+d.z*d.z) << 8; /* sqrt */
}

/**
 * compute the xz plane distance between two [3d] points
 */
//----- (80029DB0) --------------------------------------------------------
uint32_t EucDistXZ(vec *v1, vec *v2) {
  vec d;

  d.x = (v1->x - v2->x) >> 8;
  d.z = (v1->z - v2->z) >> 8;
  return sqrt(d.x*d.x+d.z*d.z) << 8; /* sqrt */
}

/**
 * compute the xz plane angle between two points
 * (i.e. angle between the points when projected into the xz plane)
 */
//----- (80029E10) --------------------------------------------------------
int32_t AngDistXZ(vec *v1, vec *v2) {
  return angle12(atan2(v2->x-v1->x,v2->z-v1->z));
}

/**
 * compute the xy plane angle between two points
 * (i.e. angle between the points when projected into the xy plane)
 */
//----- (80029E48) --------------------------------------------------------
int32_t AngDistXY(vec *v1, vec *v2) {
  return angle12(atan2(v2->x-v1->x, v2->y-v1->y));
}

// unreferenced; TODO: does this exist as inline code?

/**
 * compute [angular] distance between two angles
 * and map the result as follows:
 *
 * -360 to -180 => 0 to 180
 * -180 to 0    => -180 to 0
 * 0 to 180     => 0 to 180
 * 180 to 360   => -180 to 0
 *
 * ...and then limit that result between -aa_max and aa_max
 */
int sub_80029E80(int32_t a1, int32_t a2, uint32_t aa_max) {
  int32_t d, a;
  uint32_t da, aa;

  d = angle12(a2) - angle12(a1);
  da = abs(d);
  if (da > 0x800) {
    if (d <= 0)
      a = d + 0x1000;
    else
      a = d - 0x1000;
  }
  aa = abs(a);
  if (aa > aa_max) {
    aa = abs(aa_max);
    return a > 0 ? aa: -aa;
  }
  return a;
}

int sub_80029F04(int a1, int a2) {
  return (sin((a1 << 10) / a2)*a2) >> 12;
}

static inline uint32_t _rand(uint32_t max, uint32_t *seed) {
  uint32_t res, resb, resc;
  if (max==0) { return 0; }
  res = (0x41C64E6D * (*seed)) + 12345;
  *seed = res;
  res /= 15;
  resb = (res + (((uint64_t)res*33) >> 32)) >> 1;
  resc = abs(res - ((int32_t)((resb & 0x7C000000) << 1) - (resb >> 26))) % max;
  return resc;
}

void sranda(uint32_t seed) {
  seed_a = seed;
}

void sranda2() {
  seed_a = 12345;
}

uint32_t randa(uint32_t max) {
  return _rand(max, &seed_a);
}

void srandb(uint32_t seed) {
  seed_b = seed;
}

void srandb2() {
  seed_b = 12345;
}

uint32_t randb(uint32_t max) {
  return _rand(max, &seed_b);
}

uint32_t absdiff(int32_t a, int32_t b) {
  return abs(a-b);
}
