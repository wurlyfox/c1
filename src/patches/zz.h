#ifndef _ZZ_H_
#define _ZZ_H_

#include "common.h"
#include "geom.h"
#include "level.h"
#include "gool.h"

#include "formats/slst.h"

extern int ZZ_ZoneQueryOctree(zone_rect *zone_rect, bound *bound, zone_query_results *results);
extern int ZZ_FindFloorY(gool_object *obj,
  zone_query *query,
  bound *nodes_bound,
  bound *collider_bound,
  int32_t max_y,
  zone_query_summary *summary,
  int32_t default_y,
  int (*func)(gool_object*, uint32_t));

extern int ZZ_SlstDecodeForward(slst_item *item, poly_id_list *src, poly_id_list *dst);
extern int ZZ_SlstDecodeBackward(slst_item *item, poly_id_list *src, poly_id_list *dst);


#endif /* _ZZ_H_ */
