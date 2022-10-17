/**
 * Minimal type reflection lib (c1 extension).
 *
 * Usage: define a `refl_type` for each type needing reflection support
 * and call `ReflInit` with an array of pointers to these types.
 * Code following the `ReflInit` will then be able to use library
 * functions for any of the defined types.
 *
 * See refl.md for overview and reference.
 */
#include "refl.h"

static int ReflGetCommon(void *data, refl_type *type, char *name, refl_common *common);
static int ReflGetCommonA(void *data, refl_type *type, char *name, int idx, refl_common *common);

#define TReflPrim(t,s) \
refl_type refl_ ## s = { \
  .name = #t, \
  .size = sizeof(t), \
  .flags = REFL_FLAGS_PRIM \
};

#define TReflGetPrimBody(t,s,fn,...) \
{ \
  refl_value value; \
  t res; \
  value = fn(__VA_ARGS__); \
  res = *(t*)(value.res); \
  return res; \
}

#define TReflGetPrim(t,s) \
t ReflGet ## s(void *data, refl_type *type, char *name) \
TReflGetPrimBody(t,s,ReflGet,data,type,name)

#define TReflGetPrimF(t,s) \
t ReflGet ## s ## F(void *data, refl_field *field) \
TReflGetPrimBody(t,s,ReflGetF,data,field)

#define TReflGetPrimA(t,s) \
t ReflGet ## s ## A(void *data, refl_type *type, char *name, int idx) \
TReflGetPrimBody(t,s,ReflGetA,data,type,name,idx)

#define TReflGetPrimAF(t,s) \
t ReflGet ## s ## AF(void *data, refl_field *field, int idx) \
TReflGetPrimBody(t,s,ReflGetAF,data,field,idx)

#define TReflPrimAll(t,l,u) \
TReflPrim(t,l); \
TReflGetPrim(t,u); \
TReflGetPrimF(t,u); \
TReflGetPrimA(t,u); \
TReflGetPrimAF(t,u);

TReflPrim(int, int);
TReflPrim(size_t, size);
TReflPrimAll(int32_t, int32, Int32);
TReflPrimAll(int16_t, int16, Int16);
TReflPrimAll(int8_t, int8, Int8);
TReflPrimAll(uint32_t, uint32, UInt32);
TReflPrimAll(uint16_t, uint16, UInt16);
TReflPrimAll(uint8_t, uint8, UInt8);
TReflPrimAll(void*, void, Void);

/**
 * allocate a field (object)
 *
 * \return refl_field* - field
 */
refl_field *ReflFieldAlloc() {
  refl_field *field;

  field = (refl_field*)malloc(sizeof(refl_field));
  return field;
}

/**
 * free a field (object)
 *
 * \param field refl_field* - field
 */
void ReflFieldFree(refl_field *field) {
  free(field);
}

/**
 * allocate a dynamic type (object)
 *
 * \return refl_type* - type
 */
refl_type *ReflTypeAlloc() {
  refl_type *type;

  type = (refl_type*)malloc(sizeof(refl_type));
  return type;
}

/**
 * free a dynamic type (object)
 *
 * \param type refl_type* - type
 */
void ReflTypeFree(refl_type *type) {
  if (type->flags & REFL_FLAGS_STATIC)
    return;
  free(type);
}

/**
 * allocate a typelist node
 *
 * \return *refl_typenode - node
 */
refl_typenode *ReflNodeAlloc() {
  refl_typenode *node;

  node = malloc(sizeof(refl_typenode));
  return node;
}

/**
 * free a typelist node
 *
 * \param node refl_typenode* - node
 */
void ReflNodeFree(refl_typenode *node) {
  free(node);
}

refl_typelist typelist = { 0 }; /* global typelist */
refl_value null_value = { 0 };  /* null value */

/**
 * add a type to a typelist
 *
 * \param list refl_typelist* - typelist
 * \param type refl_type* - type
 */
void ReflAddType(refl_typelist *list, refl_type *type) {
  refl_typenode *node;

  node = ReflNodeAlloc();
  node->type = type;
  node->next = 0;
  if (!list->head) {
    list->head = node;
    list->tail = node;
  }
  else {
    list->tail->next = node;
    list->tail = node;
  }
}

/**
 * remove a type from a typelist
 *
 * \param list refl_typelist* - typelist
 * \param type refl_type* - type
 */
void ReflRemoveType(refl_typelist *list, refl_type *type) {
  refl_typenode *it, *node;

  if (!list->head) { return; }
  if (list->head->type == type) {
    it = list->head;
    list->head = it->next;
    if (list->tail->type == type)
      list->tail = 0;
    ReflNodeFree(it);
    return;
  }
  for (it=list->head;it;it=it->next) {
    if (it->next->type == type) { break; }
  }
  if (!it) { return; }
  node = it->next;
  it->next = node->next;
  if (list->tail->type == type)
    list->tail = it;
  ReflNodeFree(node);
}

/**
 * look up a type by name
 *
 * \param list refl_typelist* - typelist to search; 0 for global typelist
 * \param name char* - typename
 * \returns refl_type* - type, or 0 if not found
 */
refl_type *ReflGetType(refl_typelist *list, char *name) {
  refl_typenode *it;

  if (list==0) { list=&typelist; }
  for (it=list->head;it;it=it->next) {
    if (strcmp(it->type->name, name) == 0) { break; }
  }
  if (!it) { return 0; }
  return it->type;
}

/**
 * perform remaining initialization for a statically defined refl_type
 *
 * this includes:
 * - establishing the linked list between fields defined in the
 *   fields[] flexible array
 * - resolving typename to a refl_type for each field
 * - resolving base typename to a refl_type, if specified
 *
 * \param type refl_type* - type object
 */
void ReflTypeInit(refl_type *type) {
  refl_field *it, *next;
  int i, len;

  if (type->flags & REFL_FLAGS_INITED
   || !(type->flags & REFL_FLAGS_STATIC))
    return;
  if (type->flags & REFL_FLAGS_PRIM)
    return; 
  /* inheritance extension
     it a base type name exists then look it up
     then insert the type into the base type's subtypes list */
  if (type->basename[0]) {
    type->base = ReflGetType(&typelist, type->basename);
    if (type->base) {
      if (!type->base->subtypes) {
        type->base->subtypes = (refl_typelist*)malloc(sizeof(refl_typelist));
        type->base->subtypes->head = 0;
        type->base->subtypes->tail = 0;
      }
      ReflAddType(type->base->subtypes, type);
    }
  }
  /* ------------------- */
  type->field = &type->fields[0];
  for (i=0;;i++) {
    it = &type->fields[i];
    next = &type->fields[i+1];
    it->parent = type;
    if (!it->type && it->typename[0]) {
      len = strlen(it->typename);
      if (it->typename[len-1] == '*') {
        it->typename[len-1] = 0;
        it->flags |= REFL_FLAGS_POINTER;
      }
      it->type = ReflGetType(&typelist, it->typename);
    }
    if (next->name[0])
      it->next = next;
    else {
      it->next = 0;
      break;
    }
  }
  type->flags |= REFL_FLAGS_INITED;
}

refl_type *prims[9] = {
  &refl_int32, &refl_int16, &refl_int8,
  &refl_uint32, &refl_uint16, &refl_uint8,
  &refl_int, &refl_size, 0
};

/**
 * add an array of statically defined types (pointers)
 * to the global typelist, and perform remaining initialization
 * for each type
 *
 * \param types refl_type** - zero terminated array
 *                            of pointers to types
 */
void ReflInit(refl_type **types) {
  refl_type *type;
  refl_typenode *it;
  int i;

  /* add primitive types to the global typelist */
  for (i=0;;i++) {
    type = prims[i];
    ReflAddType(&typelist, type);
    if (!prims[i+1]) { break; }
  }
  /* add all input types to the global typelist before init
     so we can resolve typenames */
  for (i=0;;i++) {
    type = types[i];
    ReflAddType(&typelist, type);
    if (!types[i+1]) { break; }
  }
  /* init all types in the typelist */
  for (it=typelist.head;it;it=it->next)
    ReflTypeInit(it->type);
}

/**
 * get the value of an integer-valued field
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - field name
 * \return int - integer value;
 *               or 0 if the field is not one of the defined
 *               primitive integer types or any size other
 *               than 4, 2, or 1 byte(s)
 */
int ReflGetInt(void *data, refl_type *type, char *name) {
  refl_field *field;

  field = ReflGetField(type, name, data);
  return ReflGetIntF(data, field);
}

/**
 * get the value of an integer-valued field (by field object)
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \return int - integer value;
 *               or 0 if the field is not one of the defined
 *               primitive integer types or any size other
 *               than 4, 2, or 1 byte(s)
 */
int ReflGetIntF(void *data, refl_field *field) {
  refl_type *type;
  char *name;

  type = field->parent;
  name = field->name;

  if (field->type == &refl_int32
   || field->type == &refl_int)
    return ReflGetInt32(data, type, name);
  else if (field->type == &refl_int16)
    return ReflGetInt16(data, type, name);
  else if (field->type == &refl_int8)
    return ReflGetInt8(data, type, name);
  else if (field->type == &refl_uint32
        || field->type->size == 4)
    return ReflGetUInt32(data, type, name);
  else if (field->type == &refl_uint16
        || field->type->size == 2)
    return ReflGetUInt16(data, type, name);
  else if (field->type == &refl_uint8
        || field->type->size == 1)
    return ReflGetUInt8(data, type, name);
  return 0;
}

/**
 * get the value of an element of an integer array
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - field name
 * \param idx int - element index
 * \return int - integer value;
 *               or 0 if the field is not one of the defined
 *               primitive integer types or any size other
 *               than 4, 2, or 1 byte(s)
 */
int ReflGetIntA(void *data, refl_type *type, char *name, int idx) {
  refl_field *field;

  field = ReflGetField(type, name, data);
  return ReflGetIntAF(data, field, idx);
}

/**
 * get the value of an element of an integer array (by field object)
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param idx int - element index
 * \return int - integer value;
 *               or 0 if the field is not one of the defined
 *               primitive integer types or any size other
 *               than 4, 2, or 1 byte(s)
 */
int ReflGetIntAF(void *data, refl_field *field, int idx) {
  refl_type *type;
  char *name;

  type = field->parent;
  name = field->name;
  if (field->type == &refl_int32
   || field->type == &refl_int)
    return ReflGetInt32A(data, type, name, idx);
  else if (field->type == &refl_int16)
    return ReflGetInt16A(data, type, name, idx);
  else if (field->type == &refl_int8)
    return ReflGetInt8A(data, type, name, idx);
  else if (field->type == &refl_uint32
        || field->type->size == 4)
    return ReflGetUInt32A(data, type, name, idx);
  else if (field->type == &refl_uint16
        || field->type->size == 2)
    return ReflGetUInt16A(data, type, name, idx);
  else if (field->type == &refl_uint8
        || field->type->size == 1)
    return ReflGetUInt8A(data, type, name, idx);
  return 0;
}

/**
 * look up a field of a structure type by name
 *
 * (if they exist, the chains of base types or dynamic subtypes
 * are also searched from bottom to top for the field)
 *
 * \param type refl_type* - type object
 * \param name char* - field name
 * \param data void* - data object (only needed if searching dynamic subtypes)
 */
refl_field *ReflGetField(refl_type *type, char *name, void *data) {
  static refl_field dummy;
  static int st=0;
  refl_field *field;
  refl_type *subtype;
  if (name == 0) {
    dummy.type = type;
    dummy.parent = type;
    dummy.offset = 0;
    return &dummy;
  }
  /* inheritance extension
     if there is a dynamic subtype then compute it and search for the field
     in the subtype */
  if (data && type->subtype_fn) {
    subtype = (*type->subtype_fn)(data, type);
    if (subtype && st >= 0) {
      ++st;
      field = ReflGetField(subtype, name, data);
      --st;
      if (field) { return field; }
    }
  }
  /* ---------------------- */
  field = type->field;
  while (field && strcmp(field->name, name) != 0) {
    field = field->next;
  }
  /* inheritance extension
     if the field is not found and there is a base type
     then search for the field in the base type */
  if (!field && type->base && st <= 0) {
    --st;
    field = ReflGetField(type->base, name, data);
    ++st;
  }
  /* ----------------------- */
  return field;
}

/**
 * get the length/element count of an array field
 *
 * - if the field has a count_fn callback, it is called
 *   with the data and field (or fcount field if it exists)
 *   to get the length
 * - else if the field specifies an fcount fieldname, the
 *   value of that field determines the length
 * - else if the field specifies a constant length it
 *   is used as the length
 * - else the field is a non-array field and the function
 *   returns -1
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \returns int - length/element count, or -1 if error
 */
int ReflGetCount(void *data, refl_field *field) {
  refl_type *type, *fctype;
  refl_field *fcount;
  int count;

  count = -1;
  if (field->count_fn) {
    type = field->parent;
    if (field->fcount[0])
      fcount = ReflGetField(type, field->fcount, data);
    count = (*field->count_fn)(data, fcount);
  }
  else if (field->fcount[0]) {
    type = field->parent;
    fcount = ReflGetField(type, field->fcount, data);
    fctype = fcount->type;
    if (fctype->size == 4 && (fctype->flags & REFL_FLAGS_PRIM))
      count = ReflGetInt32(data, type, field->fcount);
    else if (fctype->size == 2 && (fctype->flags & REFL_FLAGS_PRIM))
      count = ReflGetInt16(data, type, field->fcount);
    else if (fctype->size == 1 && (fctype->flags & REFL_FLAGS_PRIM))
      count = ReflGetInt8(data, type, field->fcount);
  }
  else if (field->count)
    count = field->count;
  return count;
}

/**
 * get the offset of a structure field
 *
 * (offset may be directly specified for the field
 *  or calculated as offset + size of a specified preceding field)
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \returns int - field offset
 */
int ReflGetOffset(void *data, refl_field *field) {
  refl_common common;
  refl_type *type;

  if (field->after[0]) {
    type = field->parent;
    ReflGetCommon(data, type, field->after, &common);
    return common.offset + common.size;
  }
  else
    return field->offset;
}

/**
 * get the offset of an element of an array field, with respect to
 * the containing structure
 *
 * (offset is calculated as offset of array field + sum of sizes of
 *  preceding elements)
 *
 * idx can range from 0 to the array length; if idx is equal to the
 * array length, the offset returned will be the offset plus size
 * of the last element
 *
 * \param data void* - data object
 * \param field refl_field* - [array] field object
 * \param idx int - element index
 * \returns int - field offset
 */
int ReflGetOffsetA(void *data, refl_field *field, int idx) {
  refl_field *f;
  uint8_t *data_base;
  size_t size;
  int count, offset, offs_base, offs_max;
  int i;

  offset = ReflGetOffset(data, field);
  if (field->flags & REFL_FLAGS_POINTER)
    offset += sizeof(void*) * idx;
  else if (field->flags & REFL_FLAGS_2DARRAY) {
    for (i=0;i<idx;i++) {
      data_base = (uint8_t*)data + offset;
      offset += ReflGetSizeA(data, field, i);
    }
  }
  else if (field->type->flags & REFL_FLAGS_FIXED
        || field->type->flags & REFL_FLAGS_PRIM)
    offset += field->type->size * idx;
  else {
    offs_max = offset;
    /* logic from ReflGetSizeA; used to get successive offsets
       of (structure-valued) elements up to the current */
    for (i=0;i<idx;i++) {
      for (f=field->type->field;f;f=f->next) {
        data_base = (uint8_t*)data + offset;
        offs_base = offset + ReflGetOffset(data_base, f);
        size = ReflGetSize(data_base, f);
        offs_max = max(offs_base + size, offs_max);
      }
      offset = offs_max;
    }
  }
  return offset;
}

/**
 * get the size of a structure field
 *
 * (where element count is 1 for non-array fields)
 * - defined as element count * element size for elements of
 *   equal size (primitives, fixed size structs)
 * - defined as offset plus size of last element of the
 *   array, with respect to structure, minus offset of
 *   the first element
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \returns size_t - size
 */
size_t ReflGetSize(void *data, refl_field *field) {
  refl_field *f;
  size_t size;
  int count, offset;

  count = ReflGetCount(data, field);
  if (count == -1) { count = 1; }
  if (field->flags & REFL_FLAGS_POINTER)
    size = sizeof(void*) * count;
  else if (!(field->flags & REFL_FLAGS_2DARRAY) &&
           (field->type->flags & REFL_FLAGS_FIXED
         || field->type->flags & REFL_FLAGS_PRIM))
    size = field->type->size * count;
  else {
    offset = ReflGetOffsetA(data, field, count);
    size = offset - ReflGetOffset(data, field);
  }
  return size;
}

/**
 * get the size of an element of an array field
 *
 * - defined as the primitive/fixed-size type size if array of
 *   primitives or fixed-size type
 * - defined as the result of size_fn callback for 2d primitive arrays
 * - defined as the max (offset+size) value of the fields of the
 *   structure of the idx'th element minus the offset of the idx'th
 *   element, both offsets with respect to 'data'
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param idx int - element index
 * \returns size_t - size
 */
size_t ReflGetSizeA(void *data, refl_field *field, int idx) {
  refl_field *f;
  uint8_t *data_base;
  size_t size;
  int offset, offs_base, offs_max;

  if (field->flags & REFL_FLAGS_POINTER)
    size = sizeof(void*);
  else if (field->flags & REFL_FLAGS_2DARRAY)
    size = field->size_fn(data, field, idx);
  else if (field->type->flags & REFL_FLAGS_FIXED
        || field->type->flags & REFL_FLAGS_PRIM)
    size = field->type->size;
  else {
    offset = ReflGetOffsetA(data, field, idx);
    offs_max = offset;
    for (f=field->type->field;f;f=f->next) {
      data_base = (uint8_t*)data + offset;
      offs_base = offset + ReflGetOffset(data_base, f);
      size = ReflGetSize(data_base, f);
      offs_max = max(offs_base + size, offs_max);
    }
    size = offs_max - offset;
  }
  return size;
}

/**
 * get common attributes for a structure field or primitive type
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - field name (or 0 if type is prim)
 * \param common refl_common* - (out) common attributes
 * \return int - success code
 */
static int ReflGetCommon(void *data, refl_type *type, char *name, refl_common *common) {
  refl_field *field;

  if (type->flags & REFL_FLAGS_PRIM) {
    common->field = 0;
    common->size = type->size;
    common->offset = 0;
    common->idx = -1;
  }
  else {
    common->field = field = ReflGetField(type, name, data);
    if (!field) { return 0; }
    common->size = ReflGetSize(data, field);
    common->offset = ReflGetOffset(data, field);
    common->idx = -1;
  }
  return 1;
}

/**
 * get common attributes for a structure field
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param common refl_common* - (out) common attributes
 * \return int - success code
 */
static int ReflGetCommonF(void *data, refl_field *field, refl_common *common)  {
  common->field = field;
  if (!field) { return 0; }
  common->size = ReflGetSize(data, field);
  common->offset = ReflGetOffset(data, field);
  common->idx = -1;
  return 1;
}

/**
 * get common attributes for an element of an array
 *
 * the array can be a structure field or an array of primitives
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - field name
 * \param idx int - element index
 * \param common refl_common* - (out) common attributes
 * \return int - success code
 */
static int ReflGetCommonA(void *data, refl_type *type, char *name, int idx, refl_common *common) {
  refl_field *field;

  if (type->flags & REFL_FLAGS_PRIM) {
    common->field = 0;
    common->size = type->size;
    common->offset = type->size*idx;
    common->idx = -1;
  }
  else {
    common->field = field = ReflGetField(type, name, data);
    if (!field) { return 0; }
    common->size = ReflGetSizeA(data, field, idx);
    common->offset = ReflGetOffsetA(data, field, idx);
    common->idx = idx;
  }
  return 1;
}

/**
 * get common attributes for an element of an array in a structure
 *
 * \param data void* - data object
 * \param type refl_field* - field object
 * \param idx int - element index
 * \param common refl_common* - (out) common attributes
 * \return int - success code
 */
static int ReflGetCommonAF(void *data, refl_field *field, int idx, refl_common *common) {
  common->field = field;
  if (!field) { return 0; }
  common->size = ReflGetSizeA(data, field, idx);
  common->offset = ReflGetOffsetA(data, field, idx);
  common->idx = idx;
  return 1;
}

/**
 * get the value of a field, using common attributes
 *
 * \param data void* - data object
 * \param common refl_common* - common attributes
 * \return refl_value* - value object
 */
static refl_value ReflGetValue(void *data, refl_common *common) {
  refl_value value;

  value.field = common->field;
  value.offset = common->offset;
  value.size = common->size;
  value.idx = -1;
  /*
  value->res = ReflResultAlloc(common.size);
  memcpy(value->res, (uint8_t*)data + value->offset, value->size);
  */
  value.res = (uint8_t*)data + value.offset;
  value.valid = 1;
  return value;
}

/**
 * get the value of a field
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - field name
 * \return refl_value* - value object
 */
refl_value ReflGet(void *data, refl_type *type, char *name) {
  refl_common common;

  if (!ReflGetCommon(data, type, name, &common))
    return null_value;
  return ReflGetValue(data, &common);
}

/**
 * get the value of a field (by field object)
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \return refl_value* - value object
 */
refl_value ReflGetF(void *data, refl_field *field) {
  refl_common common;

  if (!ReflGetCommonF(data, field, &common))
    return null_value;
  return ReflGetValue(data, &common);
}

/**
 * get the value of an array element
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - array field name
 * \param idx int - element index
 * \return refl_value* - value object
 */
refl_value ReflGetA(void *data, refl_type *type, char *name, int idx) {
  refl_common common;

  if (!ReflGetCommonA(data, type, name, idx, &common))
    return null_value;
  return ReflGetValue(data, &common);
}

/**
 * get the value of an array element (by field object)
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param idx int - element index
 * \return refl_value* - value object
 */
refl_value ReflGetAF(void *data, refl_field *field, int idx) {
  refl_common common;

  if (!ReflGetCommonAF(data, field, idx, &common))
    return null_value;
  return ReflGetValue(data, &common);
}

/**
 * set the value of a field
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - field name
 * \param src void* - pointer to source value/data
 * \return refl_value* - value object
 */
int ReflSet(void *data, refl_type *type, char *name, void *src) {
  refl_common common;

  if (!ReflGetCommon(data, type, name, &common))
    return 0;
  memcpy((uint8_t*)data + common.offset, src, common.size);
  return 1;
}

/**
 * set the value of a field (by field object)
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param src void* - pointer to source value/data
 * \return int - success code
 */
int ReflSetF(void *data, refl_field *field, void *src) {
  refl_common common;

  if (!ReflGetCommonF(data, field, &common))
    return 0;
  memcpy((uint8_t*)data + common.offset, src, common.size);
  return 1;
}

/**
 * set the value of an array element
 *
 * \param data void* - data object
 * \param type refl_type* - parent type
 * \param name char* - array field name
 * \param idx int - element index
 * \param src void* - pointer to source value/data
 * \return int - success code
 */
int ReflSetA(void *data, refl_type *type, char *name, int idx, void *src) {
  refl_common common;

  if (!ReflGetCommonA(data, type, name, idx, &common))
    return 0;
  memcpy((uint8_t*)data + common.offset, src, common.size);
  return 1;
}

/**
 * set the value of an array element (by field object)
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param idx int - element index
 * \param src void* - pointer to source value/data
 * \return int - success code
 */
int ReflSetAF(void *data, refl_field *field, int idx, void *src) {
  refl_common common;

  if (!ReflGetCommonAF(data, field, idx, &common))
    return 0;
  memcpy((uint8_t*)data + common.offset, src, common.size);
  return 1;
}

// misc helper functions

/**
 * get the value of a parent
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param ptype refl_type* - grandparent type
 * \param pname char* - parent field name
 * \return refl_value - value object
 */
refl_value ReflGetParent(void *data, refl_field *field, refl_type *ptype, char *pname) {
   refl_field *pfield;
   refl_common common;

   pfield = ReflGetField(ptype, pname, 0);
   data = (uint8_t*)data - field->offset - pfield->offset;
   ReflGetCommon(data, ptype, pname, &common);
   return ReflGetValue(data, &common);
}

/**
 * get the length of a string (char array) type field
 * or the length of an element of a field of an array of
 * variable length/zero-terminated string type
 *
 * intended for use as size callback for fields of such type
 *
 * \param data void* - data object
 * \param field refl_field* - field object
 * \param idx int - 0 for non-array, or index of array element
 * \return int - string length
 */
int ReflStrlen(void* data, refl_field *field, int idx) {
  char *str;
  int i, offset, len;

  offset=ReflGetOffset(data, field);
  for (i=0;i<idx;i++)
    data = (uint8_t*)data + strlen(data) + 1;
  len = strlen(data);
  return len;
}

void ReflPrintPrim(refl_value *value, refl_path *path) {
  int ivalue;

  if (value->field->flags & REFL_FLAGS_2DARRAY)
    printf("%s = (%i byte array)\n", path->name, value->size);
  else if (value->field->flags & REFL_FLAGS_POINTER) {
    ivalue = ReflGetInt32(value->res, &refl_uint32, 0);
    printf("%s = 0x%08X (pointer)\n", path->name, ivalue);
  }
  else {
    ivalue = ReflGetInt(value->res, value->field->type, 0);
    printf("%s = %i\n", path->name, ivalue);
  }
}

/**
 * push to a value path
 *
 * \param path refl_path* - path
 * \param value refl_value - value to append
 * \param id int - id (field index) to append
 * \param name char* - field name to append
 * \param type int - format type (non-array or array)
 */
static void ReflPathPush(refl_path *path, refl_value value, int id, char *name, int type) {
  int nlen, prefix;

  nlen = strlen(path->name);
  prefix = (nlen != 0);
  path->id[path->len] = id;
  path->values[path->len] = value;
  ++path->len;
  if (!prefix) {
    strcpy(path->name+nlen, name);
    return;
  }
  switch (type) {
  case 1: // non-array
    sprintf(path->name+nlen, ".%s", name);
    break;
  case 2: // array
    sprintf(path->name+nlen, ".%s[%d]", name, id);
    break;
  case 3: // primitive
    sprintf(path->name+nlen, "::%s", name);
    break;
  }
}

/**
 * pop from a value path
 *
 * \param path refl_path* - path
 */
static void ReflPathPop(refl_path *path) {
  int nlen, i;

  nlen = strlen(path->name);
  for (i=nlen-1;i>=0;i--) {
    if (path->name[i]=='.') { break; }
    if (path->name[i]==':') { --i; break; }
    //if (path->name[i]=='[') { break; }
  }
  path->name[i] = 0;
  --path->len;
}

typedef int (*refl_visit_t)(refl_value*, refl_path *path, int leaf);
typedef void* (*refl_map_t)(refl_value*, refl_path *path, void **mapped);

#define REFL_CONTEXT \
void *data; \
refl_value value; \
refl_type *type; \
refl_path *path; \
int leaf; \
refl_visit_t visit; \
refl_map_t map; \
void **mapped;

#define REFL_CONTEXT_GET(context) \
data = context->data; \
value = context->value; \
type = context->type; \
path = context->path; \
leaf = context->leaf; \
visit = context->visit; \
map = context->map; \
mapped = context->mapped;

typedef struct {
  REFL_CONTEXT
} refl_context;

/**
 * helper function for ReflTraverseR
 *
 * runs the visit callback,
 * passing the value, path, and isleaf properties for the field
 * if it returns zero, then:
 * - -1 is returned
 * - the field is not mapped
 * - no further recursion is done into the field
 * otherwise if an array of already mapped children has been passed
 * - the already mapped children are passed to the map callback
 * otherwise if the field is a non-leaf
 * - ReflTraverseR is called recursively on the field value/type/path
 * - the result (array of pointers to mapped children) is passed to
 *   the map callback
 * result of the map callback in either case is returned
 *
 * \param context refl_context* - context
 * \return void** - zero terminated array of pointers to mapped fields
 */
static void **ReflTraverseR(refl_context *context);
static void *ReflTraverseC(refl_context *context) {
  REFL_CONTEXT;
  int res, free_mapped;
  void *mapres;

  REFL_CONTEXT_GET(context);
  mapres = 0;
  free_mapped = (mapped == 0);
  res = 1;
  if (visit) /* visit callback? */
    res = (*visit)(&value, path, leaf);
  if (!res) { return (void*)-1; } /* return on visit callback filter */
  context->data = value.res; /* set new data */
  context->type = value.field->type;
  if (!leaf && !mapped) /* non-leaf and non-array? */
    mapped = ReflTraverseR(context); /* traverse child struct */
  context->type = type; /* restore old type */
  context->data = data; /* restore old data */
  context->value = value;
  context->leaf = leaf; /* restore old leaf flag */
  if (map) /* map callback? */
    mapres = (*map)(&value, path, mapped); /* pass value,path,mapped children */
  if (!leaf && free_mapped) { /* non-leaf and non-array? */
    free(mapped); /* free the array of mapped children (but not the children themselves) */
    context->mapped = 0;
  }
  return mapres; /* return the mapped field */
}

/**
 * traverse the next level of data structure described in the context
 *
 * \param context refl_context* - context
 * \return void** - zero terminated array of pointers to mapped fields
 */
static void **ReflTraverseR(refl_context *context) {
  refl_type *type, *subtype;
  refl_field *f;
  void *data, **mapped, **a_mapped, **children;
  int i, ii, fcount, count;

  data = context->data;
  type = context->type;
  if (type->flags & REFL_FLAGS_PRIM) { /* shouldn't traverse a primitive type, but the code is here just in case */
    context->leaf = 1;
    context->value = ReflGet(data, type, 0);
    context->mapped = 0;
    ReflPathPush(context->path, context->value, 0, type->name, 3);
    mapped = (void**)malloc(sizeof(void*)*2);
    mapped[1] = 0;
    mapped[0] = ReflTraverseC(context);
    ReflPathPop(context->path);
    return mapped;
  }
  for (f=type->field,count=0;f;f=f->next,count++);
  mapped = malloc(sizeof(void*)*(count+1));
  mapped[count] = 0;
  for (f=type->field,i=0;f;f=f->next,i++) {
    a_mapped = 0;
    context->value = ReflGet(data, type, f->name);
    context->leaf = (f->type->flags & REFL_FLAGS_PRIM)
                 || (f->flags & REFL_FLAGS_POINTER);
    count = ReflGetCount(data, f);
    if (count != -1) { /* array field? */
      a_mapped = malloc(sizeof(void*)*(count+1));
      a_mapped[count] = 0;
      context->mapped = 0;
      for (ii=0;ii<count;ii++) {
        context->value = ReflGetA(data, type, f->name, ii);
        ReflPathPush(context->path, context->value, ii, f->name, 2);
        a_mapped[ii] = ReflTraverseC(context);
        ReflPathPop(context->path);
      }
    }
    context->mapped = a_mapped;
    ReflPathPush(context->path, context->value, i, f->name, 1);
    mapped[i] = ReflTraverseC(context);
    ReflPathPop(context->path);
    if (a_mapped) {
      /* free the array (of pointers), but not the pointed mapped datas */
      free(a_mapped);
      a_mapped = 0;
    }
  }
  /*
   the behavior for traversal/mapping of an object of base type
   and particular subtype is to traverse all fields of the base (current)
   type, all fields of the subtype, all fields of its subtype, and so on,
   at the same level.
   a tail call will provide this effect, as long as we merge the mapped results
   with those for the subtype(s).
   note that if we want to ignore fields of a base type with subtypes in our
   callback and only process subtype fields, we can just check if the value field
   parent type has a subtype_fn; if it has one then we know it is a field of a base
   type and we can return null (i.e. -1)-provided that the logic is in place to ignore
   mapped children of this value when mapping parents
   */
  if (type->subtype_fn) {
    subtype = (*type->subtype_fn)(data, type);
    if (subtype) {
      context->type = subtype; /* only change is to type; all else in context stays the same */
      a_mapped = ReflTraverseR(context);
      context->type = type; /* restore the base type */
      /* merge results */
      for (i=0;a_mapped[i];i++);
      for (f=type->field,count=0;f;f=f->next,count++);
      mapped = realloc(mapped, sizeof(void*)*(count+i+1));
      for (i=0;a_mapped[i];i++)
        mapped[count+i]=a_mapped[i];
      mapped[count+i+1]=0;
    }
  }
  return mapped;
}

/**
 * traverse a data object of the given type
 *
 * \param data void* - data object
 * \param type refl_type* - type object
 * \param visit refl_visit_t - callback to run at each field node;
 *                             returning 0 at a given node prevents further recursion there
 */
void ReflTraverse(void *data, refl_type *type, refl_visit_t visit) {
  refl_context context = { 0 };
  refl_path path = { 0 };
  void **res;
  context.data = data;
  context.type = type;
  context.path = &path;
  context.visit = visit;
  res=ReflTraverseR(&context);
  free(res);
}

/**
 * map a data object of the given type
 *
 * \param data void* - data object
 * \param type refl_type* - type object
 * \param map refl_map_t - callback to map each field node
 * \return array of pointers to the [recursively] mapped fields of type
 *         it is the user's responsibility to free this array
 */
void **ReflMap(void *data, refl_type *type, refl_map_t map) {
  refl_context context = { 0 };
  refl_path path = { 0 };
  context.data = data;
  context.type = type;
  context.path = &path;
  context.map = map;
  return ReflTraverseR(&context);
}
