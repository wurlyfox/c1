#ifndef _LEVEL_H_
#define _LEVEL_H_

#include "common.h"
#include "ns.h"
#include "gfx.h"
#include "gool.h"

typedef struct {
  vec player_trans;
  ang player_rot;
  vec player_scale;
  eid_t zone;
  int path_idx;
  uint32_t progress;
  lid_t lid;
  int flag;
  uint16_t level_spawns[GOOL_LEVEL_SPAWN_COUNT];
  uint32_t spawns[GOOL_SPAWN_COUNT];
  int _box_count;
} level_state;

/* zone query structure definitions */
typedef struct {
  union {
    struct {
      uint16_t level:3;
      uint16_t node:13;
    };
    int16_t is_node;
    int16_t value;
  };
  int16_t x;
  int16_t y;
  int16_t z;
} zone_query_result;

typedef struct {
  int16_t zero;
  int16_t w;
  int16_t h;
  int16_t d;
  int16_t unused;
  int16_t max_depth_x;
  int16_t max_depth_y;
  int16_t max_depth_z;
} zone_query_result_rect;

typedef struct {
  zone_query_result_rect rect;
  zone_query_result r[];
} zone_query_results;

typedef struct {
  int32_t ceil;
  int32_t solid_nodes_y;
  int32_t floor_nodes_y;
  int32_t solid_objs_y;
} zone_query_summary;

typedef struct _zone_query {
  union {
    zone_query_result results[0x200];
    zone_query_result_rect result_rect;
  };
  int once;
  int result_count;
  bound nodes_bound; /* bound with which to query */
  bound collider_bound; /* bound with which to test query results against, usually a subbound of nodes_bound */
  int32_t floor;
  union {
    zone_query_summary;
    zone_query_summary summary_results;
  };
  int i;
} zone_query;

extern int LdatInit();
extern int ZdatOnLoad(entry*);
extern int MdatOnLoad(entry*);

extern void LevelSpawnObjects();
extern void LevelUpdate(entry*,zone_path*,int32_t,uint32_t);
extern void LevelUpdateMisc(zone_gfx*,uint32_t);
extern void LevelInitGlobals();
extern void LevelResetGlobals(int);
extern void LevelInitMisc(int);
extern void LevelSaveState(gool_object*,level_state*,int);
extern void LevelRestart(level_state*);

extern int TestPointInBound(vec*,bound*);
extern int TestRectIntersectsBound(bound*,rect*);
extern int TestBoundIntersection(bound*,bound*);
extern int TestBoundInBound(bound*,bound*);

extern entry *ZoneFindNeighbor(entry*,vec*);
extern uint16_t ZoneFindNode(zone_rect*,uint16_t,rect,vec*,vec*,int);
extern uint16_t ZoneFindNearestNode(zone_rect*,uint16_t,rect*,vec*,int,int);
extern gool_objnode ZoneFindNearestObjectNode(gool_object*,vec*);
extern gool_objnode ZoneFindNearestObjectNode2(gool_object*,vec*);
extern gool_objnode ZoneFindNearestObjectNode3(gool_object*,vec*,int,int);
extern uint16_t ZoneReboundVector(vec*,vec*);
extern void ZoneColorsScaleByNode(int,gool_colors*,gool_colors*);
extern void ZoneColorsScaleSeek(gool_colors*,gool_object*,int,int);
extern void ZoneColorsScaleSeekByEntityNode(gool_object*);
extern int ZoneQueryOctrees(vec*,gool_object*,zone_query*);
extern int ZoneQueryOctrees2(gool_object*,zone_query*);
extern int ZonePathProgressToLoc(zone_path*,int,gool_vectors*);
extern zone_path *ZoneGetNeighborPath(entry*,zone_path*,int);

extern int CamUpdate(); /* cam.c */

#endif /* _LEVEL_H_ */
