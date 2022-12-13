#ifndef _SOLID_H_
#define _SOLID_H_

#include "common.h"
#include "geom.h"
#include "gool.h"
#include "level.h"

extern void TransSmoothStopAtSolid(gool_object *obj, vec *velocity, zone_query *query);
extern void TransPullStopAtSolid(gool_object *obj,
	zone_query *query, vec *trans, vec *velocity);
extern void TransStopAtSolid(gool_object *obj,
	zone_query *query, vec *trans, vec *delta_trans, vec *next_trans);

extern int BinfInit();
extern int BinfKill();
extern void PlotWalls(vec *next_trans, gool_object *obj, zone_query *query);
extern int StopAtWalls(vec *trans, int x, int z, int *adj_x, int *adj_z,
	gool_object *obj, int ret);

#if (!defined (PSX) || defined (PSX_NOASM))
extern int ZoneQueryOctree(zone_rect *zone_rect, bound *bound, zone_query_results *results);
extern void PlotQueryWalls(zone_query *query, bound *nodes_bound,
	int flag, int test_y1_t1, int test_y1, int test_y2, int trans_x, int trans_z);
extern void FindFloorY(gool_object *obj, zone_query *query, bound *nodes_bound,
	bound *collider_bound, int32_t max_y, zone_query_summary *summary,
	int32_t default_y, int (*func)(gool_object*, uint32_t));
extern int FindCeilY(gool_object *obj, zone_query *query, bound *nodes_bound,
	bound *collider_bound, int type_a, int type_b, int32_t default_y);
#endif

#endif /* _SOLID_H_ */
