#ifndef _GEOM_H_
#define _GEOM_H_

#include "common.h"

/*
   note: most of the types here are unions.
         we want the option to access vector components by name as well as index
         and, for structs like rect, to access x,y,z,w,h,d directly as well as
         via the vec/dim substructures

   naming convention used is

   [u]<name>[<dimensions>][<component_bitlen>]

   <dimensions> defaults to 3 and <component_bitlen> defaults to 32.
   u indicates unsigned component values.

   there are structs for the following <name>s:
   - vec - vector
   - pnt - point
   - ang - euler/tait-bryan angle
   - dim - dimensions (width, height, depth)
   - bound - boundary (range)
   - rect - rectangle (location and dimensions)
   - mat - matrix (plus trans vector)
   - rgb - color
   - tri - triangle
   - quad - quad
*/

/* point/vector types */

typedef union {
  struct {
    int32_t x, y;
  };
  int32_t c[2];
} vec2;

typedef union {
  struct {
    int16_t x, y;
  };
  int16_t c[2];
} vec216;

typedef union {
  struct {
    int8_t x, y;
  };
  int8_t c[2];
} vec28;

typedef union {
  struct {
    uint32_t x, y;
  };
  uint32_t c[2];
} uvec2;

typedef union {
  struct {
    uint16_t x, y;
  };
  uint16_t c[2];
} uvec216;

typedef union {
  struct {
    uint8_t x, y;
  };
  uint8_t c[2];
} uvec28;

typedef union {
  struct {
    int32_t x, y, z;
  };
  int32_t c[3];
} vec;

typedef union {
  struct {
    union {
      struct {
        int16_t x, y;
      };
      vec216 xy;
    };
    int16_t z;
  };
  int16_t c[3];
} vec16;

typedef union {
  struct {
    union {
      struct {
        int8_t x, y;
      };
      vec28 xy;
    };
    int8_t z;
  };
  int8_t c[3];
} vec8;

typedef union {
  struct {
    uint32_t x, y, z;
  };
  uint32_t c[3];
} uvec;

typedef union {
  struct {
    union {
      struct {
        uint16_t x, y;
      };
      uvec216 xy;
    };
    uint16_t z;
  };
  uint16_t c[3];
} uvec16;

typedef union {
  struct {
    union {
      struct {
        uint8_t x, y;
      };
      uvec28 xy;
    };
    uint8_t z;
  };
  uint8_t c[3];
} uvec8;

typedef union {
  struct {
    float x, y;
  };
  float c[2];
} fvec2;

typedef union {
  struct {
    float x, y, z;
  };
  float c[3];
} fvec;

typedef vec pnt;
typedef vec16 pnt16;
typedef vec8 pnt8;
typedef uvec upnt;
typedef uvec16 upnt16;
typedef uvec8 upnt8;
typedef vec2 pnt2;
typedef vec216 pnt216;
typedef vec28 pnt28;
typedef uvec2 upnt2;
typedef uvec216 upnt216;
typedef uvec28 upnt28;
typedef fvec fpnt;
typedef fvec2 fpnt2;

/* euler/tait-bryan angle types */
#define angle12(c) ((c)&0xFFF)

typedef union {
  struct {
    int32_t y, x, z;
  };
  int32_t c[3];
} ang; /* 3 component, 12 bit-valued euler or tait-bryan angle */

typedef union {
  struct {
    int16_t y, x, z;
  };
  int16_t c[3];
} ang16; /* 3 component, 12 bit-valued euler or tait-bryan angle, 16 bit packed */

typedef union {
  struct {
    int32_t x, y;
  };
  int32_t c[2];
} ang2; /* 2 component, 12 bit-valued euler or tait-bryan angle */

typedef union {
  struct {
    int32_t x, y;
  };
  int32_t c[2];
} ang216; /* 2 component, 12 bit-valued euler or tait-bryan angle, 16 bit packed */

typedef union {
  struct {
    float y, x, z;
  };
  float c[3];
} fang; /* 3 component, float-valued euler or tait-bryan angle */

typedef union {
  struct {
    float x, y;
  };
  float c[2];
} fang2; /* 2 component, float-valued euler or tait-bryan angle */

/* dimensions types */
typedef union {
  struct {
    uint32_t w, h;
  };
  uint32_t c[2];
} dim2;

typedef struct {
  struct {
    uint16_t w, h;
  };
  uint16_t c[2];
} dim216;

typedef union {
  struct {
    uint8_t w, h;
  };
  uint8_t c[2];
} dim28;

typedef union {
  struct {
    uint32_t w, h, d;
  };
  uint32_t c[3];
} dim;

typedef union {
  struct {
    union {
      struct {
        uint16_t w, h;
      };
      dim216 wh;
    };
    uint16_t d;
  };
  uint16_t c[3];
} dim16;

typedef union {
  struct {
    union {
      struct {
        uint8_t w, h;
      };
      dim28 wh;
    };
    uint8_t d;
  };
  uint8_t c[3];
} dim8;

typedef union {
  struct {
    float w, h;
  };
  float c[2];
} fdim2;

typedef union {
  struct {
    float w, h, d;
  };
  float c[3];
} fdim;

/* bound types */
typedef struct {
  vec p1;
  vec p2;
} bound;

typedef struct {
  vec16 p1;
  vec16 p2;
} bound16;

typedef struct {
  vec8 p1;
  vec8 p2;
} bound8;

typedef struct {
  vec2 p1;
  vec2 p2;
} bound2;

typedef struct {
  vec216 p1;
  vec216 p2;
} bound216;

typedef struct {
  vec28 p1;
  vec28 p2;
} bound28;

typedef struct {
  fvec p1;
  fvec p2;
} fbound;

typedef struct {
  fvec2 p1;
  fvec2 p2;
} fbound2;

/* rect types */
typedef union {
  struct {
    int32_t x, y, z;
    uint32_t w, h, d;
  };
  struct {
    vec loc;
    dim dim;
  };
} rect;

typedef union {
  struct {
    int16_t x, y, z;
    uint16_t w, h, d;
  };
  struct {
    vec16 loc;
    dim16 dim;
  };
} rect16;

typedef union {
  struct {
    int8_t x, y, z;
    uint8_t w, h, d;
  };
  struct {
    vec8 loc;
    dim8 dim;
  };
} rect8;

typedef union {
  struct {
    int32_t x, y;
    uint32_t w, h;
  };
  struct {
    vec2 loc;
    dim2 dim;
  };
} rect2;

typedef union {
  struct {
    int16_t x, y;
    uint16_t w, h;
  };
  struct {
    vec216 loc;
    dim216 dim;
  };
} rect216;

typedef union {
  struct {
    int8_t x;
    int8_t y;
    uint8_t w;
    uint8_t h;
  };
  struct {
    vec28 loc;
    dim28 dim;
  };
} rect28;

typedef union {
  struct {
    float x, y, z;
    float w, h, d;
  };
  struct {
    fvec loc;
    fdim dim;
  };
} frect;

typedef union {
  struct {
    float x, y;
    float w, h;
  };
  struct {
    fvec2 loc;
    fdim2 dim;
  };
} frect2;

/* color types */
typedef union {
  struct {
    uint32_t r, g, b;
  };
  uint32_t c[3];
} rgb;

typedef union {
  struct {
    uint16_t r, g, b;
  };
  uint16_t c[3];
} rgb16;

typedef union {
  struct {
    uint8_t r, g, b;
  };
  uint8_t c[3];
} rgb8;

typedef union {
  struct {
    uint8_t r, g, b, a;
  };
  uint8_t c[4];
} rgba8;

typedef union {
  struct {
    uint16_t r, g, b, a;
  };
  uint16_t c[4];
} rgba16;

typedef union {
  struct {
    uint32_t r, g, b, a;
  };
  uint32_t c[4];
} rgba;

typedef union {
  struct {
    int32_t r, g, b;
  };
  int32_t c[3];
} srgb;

/* matrix types */
typedef int16_t mat16_t[3][3];
typedef int32_t mat_t[3][3];

/* matrix (plus trans vector) types */
typedef struct {
  union {
    int16_t m[3][3];
    vec16 v[3];
    ang16 a[3];
    rgb16 c[3];
  };
  vec t;
} mat16;

typedef struct {
  union {
    int32_t m[3][3];
    vec v[3];
    ang a[3];
    rgb c[3];
  };
  vec t;
} mat;

/* matrix (no trans vector) types */
typedef union {
  int32_t m[3][3];
  vec v[3];
  ang a[3];
  rgb c[3];
} matn;

typedef union {
  int16_t m[3][3];
  vec16 v[3];
  ang16 a[3];
  rgb16 c[3];
} matn16;

/* triangle types */
typedef vec tri_t[3];
typedef vec16 tri16_t[3];
typedef vec8 tri8_t[3];
typedef vec2 tri2_t[3];
typedef vec216 tri216_t[3];
typedef vec28 tri28_t[3];
typedef fvec ftri_t[3];
typedef fvec2 ftri2_t[3];

typedef struct { vec p[3]; } tri;
typedef struct { vec16 p[3]; } tri16;
typedef struct { vec8 p[3]; } tri8;
typedef struct { vec2 p[3]; } tri2;
typedef struct { vec216 p[3]; } tri216;
typedef struct { vec28 p[3]; } tri28;
typedef struct { fvec p[3]; } ftri;
typedef struct { fvec2 p[3]; } ftri2;

/* quad types */
typedef vec qudad_t[4];
typedef vec16 quad16_t[4];
typedef vec8 quad8_t[4];
typedef vec2 quad2_t[4];
typedef vec216 quad216_t[4];
typedef vec28 quad28_t[4];
typedef fvec fquad_t[4];
typedef fvec2 fquad2_t[4];

typedef struct { vec p[4]; } quad;
typedef struct { vec16 p[4]; } quad16;
typedef struct { vec8 p[4]; } quad8;
typedef struct { vec2 p[4]; } quad2;
typedef struct { vec216 p[4]; } quad216;
typedef struct { vec28 p[4]; } quad28;
typedef struct { fvec p[4]; } fquad;
typedef struct { fvec2 p[4]; } fquad2;

/* texture types */
typedef struct { /* idx_x, idx_y, y_half stored twice to ease conversion to tpage id and clut id, respectively */
  uint32_t:1;
  uint32_t is_info:1;
  uint32_t idx_x:2;
  uint32_t idx_y:1;
  uint32_t :2;
  uint32_t y_half:1;
  uint32_t idx_x2:2;
  uint32_t :7;
  uint32_t y_half2:1;
  uint32_t idx_y2:1;
} tpageinfo;

typedef struct { /* primitive color information; clut_y would be here,  */
  union {        /* but it is in the texinfo to minimize structure size */
    struct {
      uint8_t r, g, b;
    };
    rgb8 rgb;
    int8_t t[3];
    struct {
      int8_t t1, t2, t3;
    };
  };
BITFIELDS_04(
  uint8_t type:1,
  uint8_t semi_trans:2,
  uint8_t no_cull:1,
  uint8_t clut_x:4
)
} colinfo;

typedef struct { /* texture region information; also clut_y */
 BITFIELDS_07(
  uint32_t uv_idx:10,
  uint32_t color_mode:2,
  uint32_t segment:2,
  uint32_t offs_x:5,
  uint32_t clut_y:7,
  uint32_t :1,
  uint32_t offs_y:5
)
} rgninfo;

typedef struct {
  colinfo;
  uint32_t tpage;
  rgninfo;
} texinfo;

typedef struct {
  colinfo;
  rgninfo;
} texinfo2;

/* vector operations */

/* metaprogramming primitives */
#define EVAL(a) a
#define CONCAT(a,b,...) EVAL(a) EVAL(b)
#define RCONCAT(a,b,...) CONCAT(b,a)

#define REPEAT_9(m,...) m(9,REPEAT_8(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_8(m,...) m(8,REPEAT_7(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_7(m,...) m(7,REPEAT_6(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_6(m,...) m(6,REPEAT_5(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_5(m,...) m(5,REPEAT_4(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_4(m,...) m(4,REPEAT_3(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_3(m,...) m(3,REPEAT_2(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_2(m,...) m(2,REPEAT_1(m,__VA_ARGS__),__VA_ARGS__)
#define REPEAT_1(m,...) m(1,,__VA_ARGS__)
#define REPEAT(n,m,...) REPEAT_ ## n(m,__VA_ARGS__)

/* tuples */
#define TGET(t,i,...) \
T_ ## t ## _ ## i(__VA_ARGS__)

/* binary operation, inferred source operand types */
#define BOp(d,a,b,t,op) \
d = (t)((t)(a) op (t)(b));
/* binary operation, operands are parallel members of tuples */
#define TOp1(i,d,a,b,h,e,f,t,op) \
BOp(TGET(h,i,d),TGET(e,i,a),TGET(f,i,b),t,op)
/* fold macro for TOpN */
#define TOpR(i,r,...) \
EVAL(r) EVAL(TOp1(i,__VA_ARGS__))
/* binary operation, operands are n-tuples */
#define TOpN(n,...) \
REPEAT(n,TOpR,__VA_ARGS__)

/* specializations of TOpN for pair of equal operand types */
#define TOp(n,d,a,b,x,t,op) \
TOpN(n,d,a,b,x,x,x,t,op)
#define ATOp(n,d,a,b,x,y,t,op) \
TOpN(n,d,a,b,x,y,x,t,op)
#define BTOp(n,d,a,b,x,y,t,op) \
TOpN(n,d,a,b,x,x,y,t,op)
#define DTOp(n,d,a,b,x,y,t,op) \
TOpN(n,d,a,b,y,x,x,t,op)
#define DATOp(n,d,a,b,x,y,t,op) \
TOpN(n,d,a,b,y,y,x,t,op)
#define DBTOp(n,d,a,b,x,y,t,op) \
TOpN(n,d,a,b,y,x,y,t,op)
#define ABTOp(n,d,a,b,x,y,t,op) \
TOpN(n,d,a,b,x,y,y,t,op)

#define T_vec_1(n) n.x
#define T_vec_2(n) n.y
#define T_vec_3(n) n.z
#define T_dim_1(n) n.w
#define T_dim_2(n) n.h
#define T_dim_3(n) n.d

#define VecOp(d,a,b,op) \
TOp(3,d,a,b,vec,int32_t,op)
#define Vec2Op(d,a,b,op) \
TOp(2,d,a,b,vec,int32_t,op)

/* Vec2Op specializations */
#define Vec2Add(d,a,b) Vec2Op(d,a,b,+)
#define Vec2Sub(d,a,b) Vec2Op(d,a,b,-)
#define Vec2Mul(d,a,b) Vec2Op(d,a,b,*)
#define Vec2Div(d,a,b) Vec2Op(d,a,b,/)

/* Vec2Op specializations, compound assignment */
#define Vec2AddL(a,b) Vec2Op(a,a,b,+)
#define Vec2SubL(a,b) Vec2Op(a,a,b,-)
#define Vec2MulL(a,b) Vec2Op(a,a,b,*)
#define Vec2DivL(a,b) Vec2Op(a,a,b,/)
#define Vec2AddR(a,b) Vec2Op(b,a,b,+)
#define Vec2SubR(a,b) Vec2Op(b,a,b,-)
#define Vec2MulR(a,b) Vec2Op(b,a,b,*)
#define Vec2DivR(a,b) Vec2Op(b,a,b,/)

/* VecOp specializations */
#define VecAdd(d,a,b) VecOp(d,a,b,+)
#define VecSub(d,a,b) VecOp(d,a,b,-)
#define VecMul(d,a,b) VecOp(d,a,b,*)
#define VecDiv(d,a,b) VecOp(d,a,b,/)

/* VecOp specializations, compound assignment */
#define VecAddL(a,b) VecOp(a,a,b,+)
#define VecSubL(a,b) VecOp(a,a,b,-)
#define VecMulL(a,b) VecOp(a,a,b,*)
#define VecDivL(a,b) VecOp(a,a,b,/)
#define VecAddR(a,b) VecOp(b,a,b,+)
#define VecSubR(a,b) VecOp(b,a,b,-)
#define VecMulR(a,b) VecOp(b,a,b,*)
#define VecDivR(a,b) VecOp(b,a,b,/)

/* specializations for Vec = Vec op Dim and Dim = Vec op Vec */
#define Vec2Dim2Add(d,a,b) BTOp(2,d,a,b,vec,dim,int32_t,+)
#define Vec2Dim2Sub(d,a,b) BTOp(2,d,a,b,vec,dim,int32_t,-)
#define Vec2AddDim(d,a,b) DTOp(2,d,a,b,vec,dim,int32_t,+)
#define Vec2SubDim(d,a,b) DTOp(2,d,a,b,vec,dim,int32_t,-)
#define VecDimAdd(d,a,b) BTOp(3,d,a,b,vec,dim,int32_t,+)
#define VecDimSub(d,a,b) BTOp(3,d,a,b,vec,dim,int32_t,-)
#define VecAddDim(d,a,b) DTOp(3,d,a,b,vec,dim,int32_t,+)
#define VecSubDim(d,a,b) DTOp(3,d,a,b,vec,dim,int32_t,-)

/* bound/rect conversion */
#define Bound2ToRect2(r,b) \
r.loc = b.p1; Vec2SubDim(r.dim,b.p2,b.p1)
#define Rect2ToBound2(b,r) \
b.p1 = r.loc; Vec2Dim2Add(b.p2,r.loc,r.dim)
#define BoundToRect(r,b) \
r.loc = b.p1; VecSubDim(r.dim,b.p2,b.p1)
#define RectToBound(b,r) \
b.p1 = r.loc; VecDimAdd(b.p2,r.loc.r.dim)

#endif /* _GEOM_H_ */
