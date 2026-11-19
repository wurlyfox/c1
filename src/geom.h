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

typedef union {
  struct {
    double x, y;
  };
  double c[2];
} dvec2;

typedef union {
  struct {
    double x, y, z;
  };
  double c[3];
} dvec;

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
typedef dvec dpnt;
typedef dvec2 dpnt2;

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

typedef union {
  struct {
    double y, x, z;
  };
  double c[3];
} dang; /* 3 component, double-valued euler or tait-bryan angle */

typedef union {
  struct {
    double x, y;
  };
  double c[2];
} dang2; /* 2 component, double-valued euler or tait-bryan angle */

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
} fdim3;

typedef union {
  struct {
    double w, h;
  };
  double c[2];
} ddim2;

typedef union {
  struct {
    double w, h, d;
  };
  double c[3];
} ddim;

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

typedef struct {
  dvec p1;
  dvec p2;
} dbound;

typedef struct {
  dvec2 p1;
  dvec2 p2;
} dbound2;

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
    fdim3 dim;
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

typedef union {
  struct {
    double x, y, z;
    double w, h, d;
  };
  struct {
    dvec loc;
    ddim dim;
  };
} drect;

typedef union {
  struct {
    double x, y;
    double w, h;
  };
  struct {
    dvec2 loc;
    ddim2 dim;
  };
} drect2;

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
typedef float fmat_t[3][3];
typedef double dmat_t[3][3];

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

typedef struct {
  union {
    float m[3][3];
    fvec v[3];
    fang a[3];
  };
  fvec t;
} fmat;

typedef struct {
  union {
    double m[3][3];
    dvec v[3];
    dang a[3];
  };
  dvec t;
} dmat;

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

/* line types */
typedef vec line_t[2];
typedef vec16 line16_t[2];
typedef vec8 line8_t[2];
typedef vec2 line2_t[2];
typedef vec216 line216_t[2];
typedef vec28 line28_t[2];
typedef fvec fline_t[2];
typedef fvec2 fline2_t[2];
typedef dvec dline_t[2];
typedef dvec2 dline2_t[2];

/* triangle types */
typedef vec tri_t[3];
typedef vec16 tri16_t[3];
typedef vec8 tri8_t[3];
typedef vec2 tri2_t[3];
typedef vec216 tri216_t[3];
typedef vec28 tri28_t[3];
typedef fvec ftri_t[3];
typedef fvec2 ftri2_t[3];
typedef dvec dtri_t[3];
typedef dvec2 dtri2_t[3];

typedef struct { vec p[3]; } tri;
typedef struct { vec16 p[3]; } tri16;
typedef struct { vec8 p[3]; } tri8;
typedef struct { vec2 p[3]; } tri2;
typedef struct { vec216 p[3]; } tri216;
typedef struct { vec28 p[3]; } tri28;
typedef struct { fvec p[3]; } ftri;
typedef struct { fvec2 p[3]; } ftri2;
typedef struct { dvec p[3]; } dtri;
typedef struct { dvec2 p[3]; } dtri2;

/* quad types */
typedef vec qudad_t[4];
typedef vec16 quad16_t[4];
typedef vec8 quad8_t[4];
typedef vec2 quad2_t[4];
typedef vec216 quad216_t[4];
typedef vec28 quad28_t[4];
typedef fvec fquad_t[4];
typedef fvec2 fquad2_t[4];
typedef dvec dquad_t[4];
typedef dvec2 dquad2_t[4];

typedef struct { vec p[4]; } quad;
typedef struct { vec16 p[4]; } quad16;
typedef struct { vec8 p[4]; } quad8;
typedef struct { vec2 p[4]; } quad2;
typedef struct { vec216 p[4]; } quad216;
typedef struct { vec28 p[4]; } quad28;
typedef struct { fvec p[4]; } fquad;
typedef struct { fvec2 p[4]; } fquad2;
typedef struct { dvec p[4]; } dquad;
typedef struct { dvec2 p[4]; } dquad2;

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
  union {
    colinfo;
#ifdef _WIN32
    colinfo colinfo;
#endif
  };
  uint32_t tpage;
  union {
    rgninfo;
#ifdef _WIN32
    rgninfo rgninfo;
#endif
  };
} texinfo;

typedef struct {
  union {
    colinfo;
#ifdef _WIN32
    colinfo colinfo;
#endif
  };
  union {
    rgninfo;
#ifdef _WIN32
    rgninfo rgninfo;
#endif
    rgninfo rgninfos[0];
  };
} texinfo2;

/* vector operations */

/* metaprogramming primitives */
#define ARGC(...) \
ARGC_AUX(,##__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define ARGC_AUX(_0, _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8, \
                 _9, _10, _11, _12, _13, _14, _15, _16, x, ...) x

#define TG0(_0, ...) _0
#define TG1(_0, _1, ...) _1

#define _CONCAT(a,b) a ## b
#define CONCAT(a,b) _CONCAT(a,b)
#define CONCATX(a,b,...) CONCAT(a,b)(__VA_ARGS__)
#define CONCATC(m,...) CONCATX(m, ARGC(__VA_ARGS__), __VA_ARGS__)

#define STMT_EXPR_N1(a,ra,x)  \
({ __auto_type ra = (a); x; })
#define STMT_EXPR_N2(a,b,ra,rb,x) \
({ __auto_type _ra = (a); __auto_type _rb = (b); \
   __auto_type ra = _ra; __auto_type rb = _rb; x; })
#define STMT_EXPR_N3(a,b,c,ra,rb,rc,x) \
({ __auto_type _ra = (a); __auto_type _rb = (b); __auto_type _rc = (c); \
   __auto_type ra = _ra; __auto_type rb = _rb; __auto_type rc = _rc; x; })
#define STMT_EXPR_1(a,x) STMT_EXPR_N1(a,ra,x)
#define STMT_EXPR_2(a,b,x) STMT_EXPR_N2(a,b,ra,rb,x)
#define STMT_EXPR_3(a,b,c,x) STMT_EXPR_N3(a,b,c,ra,rb,rc,x)

#define MAP_EXPR_2(t,a,c,f) STMT_EXPR_N1(a, ma, \
((t){ .c = { f(ma.c[0]), f(ma.c[1]) } }))
#define MAP_EXPR_3(t,a,c,f) STMT_EXPR_N1(a, ma, \
((t){ .c = { f(ma.c[0]), f(ma.c[1]), f(ma.c[2]) } }))
#define MAPX_EXPR_2(t,a,c,d,x) STMT_EXPR_N1(a, ma, \
({ typeof(ma.c[0]) d; (t){ .c = { ((d=ma.c[0]),x), ((d=ma.c[1]),x) } }; }))
#define MAPX_EXPR_3(t,a,c,d,x) STMT_EXPR_N1(a, ma, \
({ typeof(ma.c[0]) d; (t){ .c = { ((d=ma.c[0]),x), ((d=ma.c[1]),x), ((d=ma.c[2]),x) } }; }))

/* templates for 2d vector ops */
#define VEC2_OP_OP(t,a,b,op) \
((t){ .c = { a.c[0] op b.c[0], a.c[1] op b.c[1] } })
#define VEC2_OP_UNARY(t,a,b,op) \
((t){ .c = { op a.c[0], op a.c[1] }})
#define VEC2_OP_SCALAR(t,a,b,op) \
((t){ .c = { a.c[0] op b, a.c[1] op b }})
#define VEC2_OP_MAP(t,a,b,f) \
((t){ .c = { f(a.c[0]), f(a.c[1]) } })
#define VEC2_OP_ZIP(t,a,b,f) \
((t){ .c = { f(a.c[0],b.c[0]), f(a.c[1],b.c[1]) } })
#define VEC2_OP_MAPX(t,a,b,x) \
({ typeof(a.c[0]) c; (t){ .c = { ((c=a.c[0]),x), ((c=a.c[1]),x) } }; })
#define VEC2_OP_ZIPX(t,a,b,x) \
({ typeof(a.c[0]) l,r; (t){ .c = { ((l=a.c[0]),(r=b.c[0]),x), ((l=a.c[1]),(r=b.c[1]),x) } }; })
#define VEC2_OP_OPFOLD(t,a,b,op) \
((a.c[0] TG0 op b.c[0]) TG1 op (a.c[1] TG0 op b.c[1]))

/* templates for 3d vector ops */
#define VEC3_OP_OP(t,a,b,op) \
((t){ .c = { a.c[0] op b.c[0], a.c[1] op b.c[1], a.c[2] op b.c[2] } })
#define VEC3_OP_UNARY(t,a,b,op) \
((t){ .c = { op a.c[0], op a.c[1], op a.c[2] }})
#define VEC3_OP_SCALAR(t,a,b,op) \
((t){ .c = { a.c[0] op b, a.c[1] op b, a.c[2] op b } })
#define VEC3_OP_MAP(t,a,b,f) \
((t){ .c = { f(a.c[0]), f(a.c[1]), f(a.c[2]) } })
#define VEC3_OP_ZIP(t,a,b,f) \
((t){ .c = { f(a.c[0],b.c[0]), f(a.c[1],b.c[1]), f(a.c[2],b.c[2]) } })
#define VEC3_OP_MAPX(t,a,b,x) \
({ typeof(a.c[0]) c; (t){ .c = { ((c=a.c[0]),x), ((c=a.c[1]),x), ((c=a.c[2]),x) } }; })
#define VEC3_OP_ZIPX(t,a,b,x) \
({ typeof(a.c[0]) l,r; (t){ .c = { \
  ((l=a.c[0]),(r=b.c[0]),x), \
  ((l=a.c[1]),(r=b.c[1]),x), \
  ((l=a.c[2]),(r=b.c[2]),x) \
} }; })
#define VEC3_OP_OPFOLD(t,a,b,op) \
((a.c[0] TG0 op b.c[0]) TG1 op (a.c[1] TG0 op b.c[1]) TG1 op (a.c[2] TG0 op b.c[2]))

/* generic vector op templates */
#define VEC2_OP(type, a, b, arg) \
STMT_EXPR_2(a, b, VEC2_OP_ ## type(typeof((a)),ra,rb,arg))
#define VEC3_OP(type, a, b, arg) \
STMT_EXPR_2(a, b, VEC3_OP_ ## type(typeof((a)),ra,rb,arg))

/* 2d vector [generic] constructors */
#define Vec2(type, x, y) ((type){ (x),(y) })
#define Vec2Const(type, c) \
STMT_EXPR_N1(c, rc, ((type){ rc,rc }))
#define Vec2Cast(type, a) STMT_EXPR_1(a,((type){ ra.x, ra.y }))

/* 2d vector [generic] ops */
#define Vec2Add(a,b) VEC2_OP(OP,a,b,+)
#define Vec2Sub(a,b) VEC2_OP(OP,a,b,-)
#define Vec2Mul(a,b) VEC2_OP(OP,a,b,*)
#define Vec2Div(a,b) VEC2_OP(OP,a,b,/)
#define Vec2Pow(a,b) VEC2_OP(ZIP,a,b,pow)
#define Vec2AddS(a,b) VEC2_OP(SCALAR,a,b,+)
#define Vec2SubS(a,b) VEC2_OP(SCALAR,a,b,-)
#define Vec2MulS(a,b) VEC2_OP(SCALAR,a,b,*)
#define Vec2DivS(a,b) VEC2_OP(SCALAR,a,b,/)
#define Vec2PowS(a,b) VEC2_OP(MAPX,a,b,pow(c,b))
#define Vec2Neg(a) VEC2_OP(UNARY,a,b,-)
#define Vec2Floor(a) VEC2_OP(MAP,a,b,floor)
#define Vec2Ceil(a) VEC2_OP(MAP,a,b,ceil)
#define Vec2Round(a) VEC2_OP(MAP,a,b,round)
#define Vec2MinB(a) VEC2_OP(ZIP,a,b,min)
#define Vec2MaxB(a) VEC2_OP(ZIP,a,b,max)
#define Vec2Eq(a,b) VEC2_OP(OPFOLD,a,b,(==,&&))
#define Vec2Dot(a,b) VEC2_OP(OPFOLD,a,b,(*,+))

#define Vec2Mag(a) STMT_EXPR_N1(a,rv,\
  ((typeof((rv).c[0]))(sqrt((double)Vec2Dot(rv,rv)))))
#define Vec2Dist(a,b) (Vec2Mag(Vec2Sub(a,b)))
#define Vec2SqrDist(a,b) (Vec2Dot(Vec2Sub(a,b),Vec2Sub(a,b)))
#define Vec2Norm(a) (Vec2DivS(a,Vec2Mag(a)))
#define Vec2PNorm(a) (Vec2SubS(Vec2MulS(Vec2Div(a,b),2),1))
#define Vec2PDenorm(a) (Vec2DivS(Vec2Mul(Vec2Add(a,1),b),2))
#define Vec2Dezero(a) VEC2_OP(MAPX,a,0,(c==0?0.0001:c))
#define Vec2Interp(a,b,t) (Vec2Add(a,Vec2MulS(Vec2Sub(b,a),t)))

#define Vec2Cross(a,b) \
STMT_EXPR_2(a,b,ra.c[0]*rb.c[1]-ra.c[1]*rb.c[0])

/* 3d vector [generic] constructors */
#define Vec(type, x, y, z) ((type){ (x),(y),(z) })
#define VecConst(type, c) \
STMT_EXPR_N1((c),rc,((type){ rc,rc,rc }))
#define VecCast(type, a) STMT_EXPR_1(a,((type){ ra.x, ra.y, ra.z }))

/* 3d vector [generic] properties */
#define VecXY(a) STMT_EXPR_1(a,((typeof(ra.xy)){ ra.x, ra.y }))
#define VecXZ(a) STMT_EXPR_1(a,((typeof(ra.xy)){ ra.x, ra.z }))
#define VecYZ(a) STMT_EXPR_1(a,((typeof(ra.xy)){ ra.y, ra.z }))

/* 3d vector [generic] ops */
#define VecAdd(a,b) VEC3_OP(OP,a,b,+)
#define VecSub(a,b) VEC3_OP(OP,a,b,-)
#define VecMul(a,b) VEC3_OP(OP,a,b,*)
#define VecDiv(a,b) VEC3_OP(OP,a,b,/)
#define VecPow(a,b) VEC3_OP(ZIP,a,b,pow)
#define VecAddS(a,b) VEC3_OP(SCALAR,a,b,+)
#define VecSubS(a,b) VEC3_OP(SCALAR,a,b,-)
#define VecMulS(a,b) VEC3_OP(SCALAR,a,b,*)
#define VecDivS(a,b) VEC3_OP(SCALAR,a,b,/)
#define VecPowS(a,b) VEC3_OP(MAPX,a,b,pow(c,b))
#define VecNeg(a) VEC3_OP(UNARY,a,0,-)
#define VecFloor(a) VEC3_OP(MAP,a,0,floor)
#define VecCeil(a) VEC3_OP(MAP,a,0,ceil)
#define VecRound(a) VEC3_OP(MAP,a,0,round)
#define VecMinB(a) VEC3_OP(ZIP,a,0,min)
#define VecMaxB(a) VEC3_OP(ZIP,a,0,max)
#define VecEq(a,b) VEC3_OP(OPFOLD,a,b,(==,&&))
#define VecDot(a,b) VEC3_OP(OPFOLD,a,b,(*,+))
#define VecDezero(a) VEC3_OP(MAPX,a,0,(c==0?0.0001:c))

#define VecMag(a) STMT_EXPR_N1(a,rv,\
  ((typeof((rv).c[0]))(sqrt((double)VecDot(rv,rv)))))
#define VecDist(a,b) (VecMag(VecSub(a,b)))
#define VecSqrDist(a,b) (VecDot(VecSub(a,b),VecSub(a,b)))
#define VecNorm(a) (VecDivS(a,VecMag(a)))
#define VecPNorm(a,b) (VecSubS(VecMulS(VecDiv(a,b),2),1))
#define VecPDenorm(a,b) (VecDivS(VecMul(VecAddS(a,1),b),2))
#define VecInterp(a,b,t) (VecAdd(a,VecMulS(VecSub(b,a),t)))

#define VecCross(a,b) \
STMT_EXPR_2(a,b,((typeof(ra)){ \
  ra.c[1]*rb.c[2]-ra.c[2]*rb.c[1], \
  ra.c[2]*rb.c[0]-ra.c[0]*rb.c[2], \
  ra.c[0]*rb.c[1]-ra.c[1]*rb.c[0]}))

#endif /* _GEOM_H_ */
