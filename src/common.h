#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef PSX
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#endif

/* compilation settings */
#define LID_BOOTLEVEL             LID_NSANITYBEACH
#ifndef PSX                       /* non-PSX target options */
#define GFX_SW_PERSP
#endif

/* codes */
#define SUCCESS                   -255
#define ERROR_ALLOC_FAILED        -254
#define ERROR_COLLISION_OVERRIDE  -28
#define ERROR_INVALID_STATE       -27
#define ERROR_INVALID_STATERETURN -26
#define ERROR_INVALID_RETURN      -25
#define ERROR_NO_NEIGHBORS_FOUND  -23
#define ERROR_OBJECT_POOL_FULL    -22
#define ERROR_INVALID_MAGIC       -18
#define ERROR_READ_FAILED         -16
#define ERROR_MALLOC_FAILED       -15
#define ERROR                     -14
#define ERROR_NO_FREE_PAGES       -12
#define ERROR_INVALID_REF         -10

#define ISERRORCODE(v)   ((int)(v) < 0 && (int)(v) > -255)
#define ISSUCCESSCODE(v) ((int)(v)==-255)
#define ISCODE(v)        ((int)(v) < 0 && (int)(v) >= -255)

/* aliases */
#define NULL_PAGE                 -18

/* macros for platform independent handling of bitfield layout */
#define BITFIELDS_02(a01, a02) \
  a02; a01;
#define BITFIELDS_03(a01, a02, a03) \
  a03; a02; a01;
#define BITFIELDS_04(a01, a02, a03, a04) \
  a04; a03; a02; a01;
#define BITFIELDS_05(a01, a02, a03, a04, a05) \
  a05; a04; a03; a02; a01;
#define BITFIELDS_06(a01, a02, a03, a04, a05, a06) \
  a06; a05; a04; a03; a02; a01;
#define BITFIELDS_07(a01, a02, a03, a04, a05, a06, a07) \
  a07; a06; a05; a04; a03; a02; a01;
#define BITFIELDS_08(a01, a02, a03, a04, a05, a06, a07, a08) \
  a08; a07; a06; a05; a04; a03; a02; a01;
#define BITFIELDS_09(a01, a02, a03, a04, a05, a06, a07, a08, a09) \
  a09; a08; a07; a06; a05; a04; a03; a02; a01;

/* misc */
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define min3(a,b,c)   min(min(a,b),c)
#define min4(a,b,c,d) min(min3(a,b,c),d)
#define max3(a,b,c)   max(max(a,b),c)
#define max4(a,b,c,d) max(max3(a,b,c),d)
#define swap(a,b) tmp=a;a=b;b=tmp;
#define sign(v) ((v)>0?1:(v)<0?-1:0)
#define limit(v,a,b) max(min(v,b),a)
#define shr_floor(v,a) (((v)+((v)<0?((1<<(a))-1):0))>>(a))
#define shr_ceil(v,a) (((v)+((1<<(a))-1)+((v)<0?((1<<(a))-1):0))>>(a))
#define shr_mul(a,b,m,s,t) ((((a)>>((b)>(m)?(t):0))*b)>>(s-((b)>(m)?(t):0)))

/* level ids. TODO: could they go elsewhere? */
#define LID_TEST              0x1
#define LID_LAVACAVE          0x2
#define LID_CORTEXPOWER       0x3
#define LID_CAVE              0x4
#define LID_GENERATORROOM     0x5
#define LID_HEAVYMACHINERY    0x6
#define LID_TOXICWASTE        0x7
#define LID_PINSTRIPE         0x8
#define LID_NSANITYBEACH      0x9
#define LID_PAPUPAPU          0xA
#define LID_JUNGLEROLLERS     0xC
#define LID_BOULDERS          0xE
#define LID_UPSTREAM          0xF
#define LID_HOGWILD           0x11
#define LID_THEGREATGATE      0x12
#define LID_BOULDERDASH       0x13
#define LID_ROADTONOWHERE     0x14
#define LID_ROLLINGSTONES     0x15
#define LID_THEHIGHROAD       0x16
#define LID_RIPPERROO         0x17
#define LID_UPTHECREEK        0x18
#define LID_TITLE             0x19
#define LID_NATIVEFORTRESS    0x1A
#define LID_DRNBRIO           0x1B
#define LID_TEMPLERUINS       0x1C
#define LID_JAWSOFDARKNESS    0x1D
#define LID_WHOLEHOG          0x1E
#define LID_DRNEOCORTEX       0x1F
#define LID_THELOSTCITY       0x20
#define LID_KOALAKONG         0x21
#define LID_STORMYASCENT      0x22
#define LID_SUNSETVISTA       0x23
#define LID_BONUSTAWNA1       0x24
#define LID_BONUSBRIO         0x25
#define LID_BONUS             0x26
#define LID_LIGHTSOUT         0x28
#define LID_THELAB            0x29
#define LID_FUMBLINGINTHEDARK 0x2A
#define LID_THEGREATHALL      0x2C
#define LID_LEVELEND          0x2D
#define LID_SLIPPERYCLIMB     0x2E
#define LID_BONUSTAWNA2       0x33
#define LID_BONUSCORTEX       0x34
#define LID_CASTLEMACHINERY   0x37
#define LID_INTRO             0x38
#define LID_GAMEWIN           0x39

#endif /* _COMMON_H_ */
