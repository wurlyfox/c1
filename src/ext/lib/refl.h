#ifndef _REFL_H_
#define _REFL_H_

#include <stdlib.h>
#include "common.h"

#define REFL_FLAGS_INITED  1
#define REFL_FLAGS_STATIC  2
#define REFL_FLAGS_PRIM    4
#define REFL_FLAGS_FIXED   8

#define REFL_FLAGS_POINTER 1
#define REFL_FLAGS_2DARRAY 2

typedef struct _refl_type refl_type;
typedef struct _refl_field refl_field;
typedef struct _refl_typelist refl_typelist;

typedef int (*refl_countfn_t)(void* data, refl_field *field);
typedef size_t (*refl_sizefn_t)(void* data, refl_field *field, int idx);

typedef struct _refl_field {
  char name[64];             /* field name */
  char typename[64];         /* field type name (used for static initialization) */
  refl_type *type;           /* field type */
  int offset;                /* offset w.r.t. parent type */
  char after[64];            /* placement (used in case of non-fixed offset,
                                           i.e. when predecessors are variable len) */
  int flags;                 /* flags (ispointer, etc.) */
  int count;                 /* 0 if field is non fixed-length array; else constant array length */
  char fcount[64];           /* 0 if field is non var-length array; else array length field name */
  refl_countfn_t count_fn;   /* 0 if field is non var-length array; else function for array length */
  refl_sizefn_t size_fn;     /* 0 if field is non var-elem-size 2d prim array; else function for elem size */
  refl_type *parent;         /* parent type */
  refl_field *next;          /* next field of parent type */
} refl_field;

typedef refl_type *(*refl_typefn_t)(void* data, refl_type *type);

typedef struct _refl_type {
  char name[64];             /* type name */
  int flags;                 /* flags (inited, static, isprim, fixed, etc.) */
  size_t size;               /* fixed size for primitive types */
  refl_field *field;         /* fields list, first child */
  /* inheritance features */
  char basename[64];         /* base typename if type inherits */
  refl_type *base;           /* base type if type inherits */
  refl_typelist *subtypes;   /* subtypes if type is inherited from */
  refl_typefn_t subtype_fn;  /* subtype func if type is inherited from */
  /* -------------------- */
  refl_field fields[];       /* for static initialization; cvt to linked list at init */
} refl_type;

#define REFL_FIELD(t,ft,n) \
{ .name=#n, .offset = offsetof(t,n), .typename=#ft }
#define REFL_ARRAY_FIELD(t,ft,n,c) \
{ .name=#n, .offset = offsetof(t,n), .typename=#ft, .count=c }
#define REFL_DYNARRAY_FIELD(t,ft,n,fc) \
{ .name=#n, .offset = offsetof(t,n), .typename=#ft, .fcount=#fc }
#define REFL_DYNARRAY_FIELD_F(t,ft,n,fc,cfn) \
{ .name=#n, .offset = offsetof(t,n), .typename=#ft, .fcount=#fc, .count_fn=cfn }
#define REFL_DYN2DARRAY_FIELD(t,ft,n,fc,sfn) \
{ .name=#n, .flags=REFL_FLAGS_2DARRAY, .offset = offsetof(t,n), .typename=#ft, .fcount=#fc, .size_fn=sfn }
#define REFL_DYN2DARRAY_FIELD_F(t,ft,n,fc,cfn,sfn) \
{ .name=#n, .flags=REFL_FLAGS_2DARRAY, .offset = offsetof(t,n), .typename=#ft, .fcount=#fc, .count_fn=cfn, .size_fn=sfn }
#define REFL_FIELD_A(t,ft,n,a) \
{ .name=#n, .after=#a, .typename=#ft }
#define REFL_ARRAY_FIELD_A(t,ft,n,a,c) \
{ .name=#n, .after=#a, .typename=#ft, .count=c }
#define REFL_DYNARRAY_FIELD_A(t,ft,n,a,fc) \
{ .name=#n, .after=#a, .typename=#ft, .fcount=#fc }
#define REFL_DYNARRAY_FIELD_AF(t,ft,n,a,fc,cfn) \
{ .name=#n, .after=#a, .typename=#ft, .fcount=#fc, .count_fn=cfn }
#define REFL_DYN2DARRAY_FIELD_A(t,ft,n,a,fc,sfn) \
{ .name=#n, .flags=REFL_FLAGS_2DARRAY, .after=#a, .typename=#ft, .fcount=#fc, .size_fn=sfn }
#define REFL_DYN2DARRAY_FIELD_AF(t,ft,n,a,fc,cfn,sfn) \
{ .name=#n, .flags=REFL_FLAGS_2DARRAY, .after=#a, .typename=#ft, .fcount=#fc, .count_fn=cfn, .size_fn=sfn }
#define REFL_TERM \
{ .name="\0" }

typedef struct {
  refl_field *field;
  int offset;
  size_t size;
  int idx;
} refl_common;

typedef void* refl_result_t;
typedef struct {
  int valid;
  refl_result_t res;
/* refl_common */
  refl_field *field;
  int offset;
  size_t size;
  int idx;
} refl_value;

typedef struct {
  int len;
  int id[32];
  refl_value values[32];
  char name[1024];
} refl_path;

typedef struct _refl_typenode {
  refl_type *type;
  struct _refl_typenode *next;
} refl_typenode;

typedef struct _refl_typelist {
  refl_typenode *head;
  refl_typenode *tail;
} refl_typelist;

typedef int (*refl_visit_t)(refl_value*, refl_path *path, int leaf);
typedef void* (*refl_map_t)(refl_value*, refl_path *path, void** mapped);

extern void ReflInit(refl_type **types); /* remaining init for statically initialized types and fields */
/* type and field metadata */
extern refl_type *ReflGetType(refl_typelist *list, char *name);
extern refl_field *ReflGetField(refl_type *type, char *name, void *data);
extern int ReflGetCount(void *data, refl_field *field);
extern int ReflGetOffset(void *data, refl_field *field);
extern int ReflGetOffsetA(void *data, refl_field *field, int idx);
extern size_t ReflGetSize(void *data, refl_field *field);
extern size_t ReflGetSizeA(void *data, refl_field *field, int idx);
/* get/set */
extern refl_value ReflGet(void *data, refl_type *type, char *name);
extern refl_value ReflGetA(void *data, refl_type *type, char *name, int idx);
extern refl_value ReflGetF(void *data, refl_field *field);
extern refl_value ReflGetAF(void *data, refl_field *field, int idx);
extern int ReflSet(void *data, refl_type *type, char *name, void *src);
extern int ReflSetA(void *data, refl_type *type, char *name, int idx, void *src);
extern int ReflSetF(void *data, refl_field *field, void *src);
extern int ReflSetAF(void *data, refl_field *field, int idx, void *src);
extern int ReflGetInt(void *data, refl_type *type, char *name);
extern int ReflGetIntF(void *data, refl_field *field);
extern int ReflGetIntA(void *data, refl_type *type, char *name, int idx);
extern int ReflGetIntAF(void *data, refl_field *field, int idx);
/* traverse */
extern void ReflTraverse(void *data, refl_type *type, refl_visit_t visit);
extern void **ReflMap(void *data, refl_type *type, refl_map_t map);
/* misc */
extern refl_value ReflGetParent(void *data, refl_field *field, refl_type *ptype, char *pname);
extern int ReflStrlen(void* data, refl_field *field, int idx);

extern refl_type page_type;
extern refl_type *types[];

#endif /* _REFL_H_ */
