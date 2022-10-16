#ifndef _F_ZDAT_H_
#define _F_ZDAT_H_

#include "common.h"
#include "geom.h"
#include "ns.h"

#include "formats/gool.h"
#include "formats/wgeo.h"

#define ZONE_FLAG_UP_DOWN               0x1
#define ZONE_FLAG_SOLID_BOTTOM          0x2
#define ZONE_FLAG_HAS_WATER             0x4
#define ZONE_FLAG_LOC_BASED_WAIT        0x8
#define ZONE_FLAG_FOG                   0x10
#define ZONE_FLAG_AUTO_CAM_ZOFFSET      0x80
#define ZONE_FLAG_RIPPLE                0x100
#define ZONE_FLAG_LIGHTNING             0x200
#define ZONE_FLAG_DARK2                 0x400
#define ZONE_FLAG_CAM_BOUNCE            0x1000
#define ZONE_FLAG_NO_SAVE               0x2000
#define ZONE_FLAG_SIDE_SCROLL           0x4000
#define ZONE_FLAG_BACKWARD              0x8000
#define ZONE_FLAG_SOLID_TOP             0x20000
#define ZONE_FLAG_DISCARD_BELOW_PATHS   0x40000
#define ZONE_FLAG_AUTO_CAM_SKIP_DISABLE 0x80000
#define ZONE_FLAG_SOLID_SIDES           0x100000

#define ZONE_FLAG_FOG_LIGHTNING         (ZONE_FLAG_FOG | ZONE_FLAG_LIGHTNING)

typedef struct {
  union {
    eid_t eid;
    entry *entry;
    uint32_t key;
    uint32_t tag;
  };
  vec trans;
  wgeo_header *header;
  wgeo_polygon *polygons;
  wgeo_vertex *vertices;
  wgeo_texinfo *texinfos;
  entry *tpages[8];
} zone_world;

typedef struct {
  uint32_t vram_fill_height;
  uint32_t unknown_a;
  uint32_t visibility_depth;
  uint32_t unknown_b;
  uint32_t unknown_c;
  uint32_t unknown_d;
  uint32_t unknown_e;
  uint32_t flags;
  int32_t water_y;
  eid_t midi;
  uint32_t unknown_g;
  rgb8 unknown_h;
  rgb8 vram_fill;
  uint8_t unused_a;
  rgb8 far_color;
  uint8_t unused_b;
  gool_colors object_colors;
  gool_colors player_colors;
} zone_gfx;

typedef struct {
  uint32_t world_count;
  zone_world worlds[8];
  uint32_t paths_idx;
  uint32_t path_count;
  uint32_t entity_count;
  uint32_t neighbor_count;
  eid_t neighbors[8];
  ns_loadlist loadlist;
  uint32_t display_flags;
  union {
    zone_gfx;
    zone_gfx gfx;
  };
} zone_header;

typedef struct {
  uint16_t root;
  uint16_t max_depth_x;
  uint16_t max_depth_y;
  uint16_t max_depth_z;
  uint16_t nodes[];
} zone_octree;

typedef struct {
  int32_t x;
  int32_t y;
  int32_t z;
  uint32_t w;
  uint32_t h;
  uint32_t d;
  uint32_t unk;
  zone_octree octree;
} zone_rect;

typedef struct {
  uint8_t relation;
  uint8_t neighbor_zone_idx;
  uint8_t path_idx;
  uint8_t goal;
} zone_neighbor_path;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
  int16_t rot_y;
  int16_t rot_x;
  int16_t rot_z;
} zone_path_point;

typedef struct {
  eid_t slst;
  entry *parent_zone;
  uint32_t neighbor_path_count;
  zone_neighbor_path neighbor_paths[4];
  uint8_t entrance_index;
  uint8_t exit_index;
  uint16_t length;
  uint16_t cam_mode;
  int16_t avg_node_dist;
  int16_t cam_zoom;
  uint16_t unknown_a;
  uint16_t unknown_b;
  uint16_t unknown_c;
  int16_t direction_x;
  int16_t direction_y;
  int16_t direction_z;
  zone_path_point points[];
} zone_path;

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
} zone_entity_path_point;

typedef struct {
  entry *parent_zone;
  uint16_t spawn_flags;
  uint16_t group;
  uint16_t id;
  uint16_t path_length;
  union {
    struct {
      uint16_t init_flags_a;
      uint16_t init_flags_b;
      uint16_t init_flags_c;
    };
    ang16 rot;
  };
  uint8_t type;
  uint8_t subtype;
  union {
    zone_entity_path_point path_points[0];
    zone_entity_path_point loc;
  };
} zone_entity;

#endif /* _F_ZDAT_H_ */
