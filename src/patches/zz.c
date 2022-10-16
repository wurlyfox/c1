#include "patches/emu.h"
#include "patches/zz.h"

#define ZZ_JUMPREGISTER_BEGIN(reg) \
do \
{ \
	jr_dest = reg; \
} while (0)

#define ZZ_JUMPREGISTER(addr,label) \
do \
{ \
	if (jr_dest == addr) \
		goto label; \
} while (0)

#define ZZ_JUMPREGISTER_END() \
do \
{ \
	if (jr_dest != 0xDEADBEEF) \
	{ \
	} \
	return jr_dest; \
} while (0)

#define ZZ_CLOCKCYCLES(clocks,address) \
do \
{ \
} while (0)

#define ZZ_CLOCKCYCLES_JR(clocks) \
do \
{ \
} while (0)

#define ZZ_BEGIN_WRAP() \
/*unsigned int */base = 0x70000;

#define ZZ_END_WRAP() \
do \
{ \
} while(0)

#define ZZ_SET_REG(idx,value) \
switch (idx) { \
case 0: A0 = (uint32_t)value; break; \
case 1: A1 = (uint32_t)value; break; \
case 2: A2 = (uint32_t)value; break; \
case 3: A3 = (uint32_t)value; break; \
default: \
    EMU_Write32(SP+(idx*4), (uint32_t)value); \
    break; \
}

//#define ZZ_IN_PTR(idx,value) \
//memcpy((void*)&EMU_ram[base], (void*)value, sizeof(*value)); \
//ZZ_SET_REG(idx,base); \
//base += sizeof(*value)

unsigned int zz_args[16];

#define ZZ_IN_PTR(idx,value) \
ZZ_InPtr(idx,value,sizeof(*value))

#define ZZ_OUT_PTR(idx,value) \
ZZ_OutPtr(idx,value,sizeof(*value))

#define ZZ_IN(idx, value) \
ZZ_SET_REG(idx, value)

#define ZZ_OUT(value) \
value = V0

#include "common.h"
#include "geom.h"
#include "level.h"

int base = 0;

static inline int ZZ_InPtr(int idx, void *value, int size) {
  zz_args[idx] = base;
  memcpy((void*)&EMU_ram[base>>2], (void*)value, size);
  ZZ_SET_REG(idx,base|0x80000000);
  base += (((size+3)/4)*4);
}

static inline void ZZ_OutPtr(int idx, void *value, int size) {
  memcpy((void*)value, (void*)&EMU_ram[zz_args[idx]>>2], size);
}

int ZZ_ZoneQueryOctree(zone_rect *zone_rect, bound *bound, zone_query_results *results) {
  int res;
  uint32_t jr_dest;

  ZZ_BEGIN_WRAP()
  // ZZ_IN_PTR(0,zone_rect);
  ZZ_InPtr(0,zone_rect,sizeof(*zone_rect)+(zone_rect->unk*2));
  ZZ_IN_PTR(1,bound);
  ZZ_IN_PTR(2,results);
  A3 = 0;
  #define ZZ_INCLUDE_CODE
  #include "zz_37d50.h"
  #undef ZZ_INCLUDE_CODE
  ZZ_OUT(res);
  ZZ_OutPtr(2,results,res*sizeof(zone_query_result));
  ZZ_END_WRAP();

  return res;
}

int ZZ_FindFloorY(gool_object *obj,
  zone_query *query,
  bound *nodes_bound,
  bound *collider_bound,
  int32_t max_y,
  zone_query_summary *summary,
  int32_t default_y,
  int (*func)(gool_object*, uint32_t)) {
  int res;
  uint32_t jr_dest;

  ZZ_BEGIN_WRAP();
  ZZ_InPtr(0,obj,sizeof(gool_object)+0x100);
  ZZ_IN_PTR(1,query);
  ZZ_IN_PTR(2,nodes_bound);
  ZZ_IN_PTR(3,collider_bound);
  ZZ_IN(4,max_y);
  ZZ_IN_PTR(5,summary);
  ZZ_IN(6,default_y);
  ZZ_IN(7,0x8002C3B8); // for now
  #define ZZ_INCLUDE_CODE
  goto ZZ_38AC4;
  #include "zz_2c3b8.h"
  #include "zz_38ac4.h"
  #undef ZZ_INCLUDE_CODE
  ZZ_OutPtr(0,obj,sizeof(gool_object)+0x100);
  ZZ_OUT_PTR(5,summary);
  ZZ_OUT(res);
  ZZ_END_WRAP();

  return res;
}

int ZZ_SlstDecode(slst_item *item, poly_id_list *src, poly_id_list *dst, int dir) {
  uint32_t jr_dest;
  uint16_t len;

  ZZ_BEGIN_WRAP();
  ZZ_InPtr(0,item,4+(item->length*4));
  ZZ_InPtr(1,src,4+(src->len*4));
  ZZ_InPtr(2,dst,0x2000);
  #define ZZ_INCLUDE_CODE
  if (dir == 0) { goto ZZ_33878; }
  else if (dir == 1) { goto ZZ_334A0; }
  #include "zz_33878_41c.h"
  #include "zz_334a0.h"
  len = EMU_ReadU16(zz_args[2]);
  ZZ_OutPtr(2,dst,4+(len*2));
  return 1;
  #include "zz_33878.h"
  len = EMU_ReadU16(zz_args[2]);
  ZZ_OutPtr(2,dst,4+(len*2));
  return 1;
  ZZ_END_WRAP();
  #undef ZZ_INCLUDE_CODE
}

int ZZ_SlstDecodeForward(slst_item *item, poly_id_list *src, poly_id_list *dst) {
  ZZ_SlstDecode(item, src, dst, 1);
}

int ZZ_SlstDecodeBackward(slst_item *item, poly_id_list *src, poly_id_list *dst) {
  ZZ_SlstDecode(item, src, dst, 0);
}
