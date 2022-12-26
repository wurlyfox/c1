#include "level.h"
#include "globals.h"
#include "slst.h"
#include "misc.h"
#include "pbak.h"
#include "midi.h"
#include "formats/mdat.h"

/* .data */
const uint32_t size_map[16] = {  0, -256, -128, -64, -48, -40, -32, -26,   /* 80052CA4 */
                               -20,  -14,   -8,   8,  16,  24,  32,  64 };
const uint8_t percent_map[16] = {  2,  16,  30,  44,  58,  72,  86, 100,   /* 80052CE4 */
                                 112, 124, 136, 148, 160, 172, 184, 196 };
/* .sdata */
int bonus_return = 0;       /* 80056490; gp[0x25] */
int ldat_not_inited = 1;    /* 80056494; gp[0x26] */
char title_zone_name[8] = "0b_pZ"; /* 80056498; gp[0x27] */
int first_spawn = 0;        /* 800564A0; gp[0x29] */
/* .bss */
entry *cur_zone, *obj_zone; /* 80057914, 80057918 */
zone_path *cur_path;        /* 8005791C */
int32_t cur_progress;       /* 80057920 */
vec unk_80057924;           /* 80057924 */
int32_t cam_rot_xz;         /* 80057930 */
ang cam_rot_before;         /* 80057934 */
ang cam_rot_after;          /* 80057940 */
vec cam_rot_xz_dir;         /* 8005794C */
uint32_t unk_80057958;      /* 80057958 */
uint32_t unk_8005795C;      /* 8005795C */
int draw_count;             /* 80057960 */
rgb8 prev_vram_fill_color;  /* 80057964 */
rgb8 vram_fill_color;       /* 80057968 */
rgb8 next_vram_fill_color;  /* 8005796C */
uint32_t respawn_stamp;     /* 80057970 */
level_state savestate;      /* 80057974 */

extern ns_struct ns;
extern lid_t cur_lid, next_lid;
extern zone_query cur_zone_query;
extern eid_t crash_eid;
extern gool_object *crash;
extern uint32_t spawns[GOOL_SPAWN_COUNT];
extern uint16_t level_spawns[GOOL_LEVEL_SPAWN_COUNT];
extern gool_handle handles[8];
extern gool_bound object_bounds[28];
extern int object_bound_count;
extern vec cam_trans;
extern ang cam_rot;
extern uint32_t screen_proj;

#ifdef PSX
extern int ticks_elapsed;
#else
#include "solid.h"
#include "pc/time.h"
#include "pc/gfx/soft.h"
extern sw_transform_struct params;
#endif

//----- (800253A0) --------------------------------------------------------
int LdatInit() {
  nsd_ldat *ldat;
  entry *zone_spawn;
  zone_path *path_spawn;
  uint32_t progress_spawn;
  zone_header *header;
  int idx, ref_inc;

  ldat = ns.ldat;
  pbak_state = 0;
  if (*(ns.ldat_eid) == EID_NONE)
    ldat->magic = 0;
  else {
    // gp[0xBA] = 0;
    crash = 0;
    cur_display_flags = GOOL_FLAG_DISPLAY_WORLDS | GOOL_FLAG_DISPANIM_OBJECTS | GOOL_FLAG_CAM_UPDATE;
    next_display_flags = GOOL_FLAG_DISPLAY_WORLDS | GOOL_FLAG_DISPANIM_OBJECTS | GOOL_FLAG_CAM_UPDATE;
    memset((void*)&spawns, 0, sizeof(spawns)); /* clear spawn list */
    GoolInitLevelSpawns(ns.lid); /* reset spawn list */
    progress_spawn = 0;
    ref_inc = 2;
    if (cur_lid == LID_HOGWILD && cur_lid == LID_WHOLEHOG)
      ref_inc = 1;
    GfxInitMatrices();
    if (cur_lid == LID_TITLE && game_state == 0x200 && ldat_not_inited == 0)
      ldat->zone_spawn = NSStringToEID(title_zone_name);
    if (bonus_return) { /* returning from bonus level? */
      ldat->zone_spawn = savestate.zone;
      ldat->path_idx_spawn = savestate.path_idx;
      progress_spawn = savestate.progress;
      bonus_return = 0;
    }
    zone_spawn = NSOpen(&ldat->zone_spawn, 1, ref_inc);
    crash_eid = EID_NONE;
    cur_zone = 0;
    cur_path = 0;
    cur_progress = 0;
    vram_fill_color.r = 0;
    vram_fill_color.g = 0;
    vram_fill_color.b = 0;
    respawn_stamp = 0;
    if (game_state == GAME_STATE_TITLE) /* coming directly from main menu? */
      PbakChoose(&crash_eid); /* load playback demo */
    header = (zone_header*)zone_spawn->items[0];
    idx = header->paths_idx + ldat->path_idx_spawn;
    path_spawn = (zone_path*)zone_spawn->items[idx];
    LevelUpdate(zone_spawn, path_spawn, progress_spawn, 0);
    NSClose(&ldat->zone_spawn, 1);
  }
  ShaderParamsUpdate(1);
  ShaderParamsUpdateRipple(1);
  ldat_not_inited = 0;
  return SUCCESS;
}

//----- (80025628) --------------------------------------------------------
int ZdatOnLoad(entry *zone) {
  zone_header *header;
  zone_path *path;
  zone_entity *entity;
  int i, idx;

  header = (zone_header*)zone->items[0];
  for (i=0;i<header->path_count;i++) {
    idx = header->paths_idx + i;
    path = (zone_path*)zone->items[idx];
    path->parent_zone = zone;
  }
  for (i=0;i<header->entity_count;i++) {
    idx = header->paths_idx + header->path_count + i;
    entity = (zone_entity*)zone->items[idx];
    entity->parent_zone = zone;
  }
  return SUCCESS;
}

//----- (800256DC) --------------------------------------------------------
int MdatOnLoad(entry *mdat) {
  mdat_header *header;
  mdat_entity *entity;
  int i;

  header = (mdat_header*)mdat->items[0];
  for (i=0;i<header->entity_count;i++) {
    entity = (mdat_entity*)mdat->items[2+i];
    entity->parent_zone = mdat;
  }
  return SUCCESS;
}

//----- (80025928) --------------------------------------------------------
void LevelSpawnObjects() {
  entry *neighbor;
  zone_header *header, *n_header;
  zone_entity *entity;
  int i, ii, idx;

  if (!cur_zone) { return; }
  header = (zone_header*)cur_zone->items[0];
  for (i=0;i<header->neighbor_count;i++) {
    neighbor = NSLookup(&header->neighbors[i]);
    n_header = (zone_header*)neighbor->items[0];
    if (!(n_header->display_flags & 2)) { continue; }
    for (ii=0;ii<n_header->entity_count;ii++) {
      idx = n_header->paths_idx + n_header->path_count + ii;
      entity = (zone_entity*)neighbor->items[idx];
      if (entity->group == 3)
        GoolObjectSpawn(neighbor, ii);
    }
  }
}

// inline code from LevelUpdate
static inline void ZoneTerminateDifference(entry *zone) {
  entry *cur_neighbor, *neighbor;
  zone_header *cur_header, *header, *n_header;
  int i, ii, found;

  if (!cur_zone) { return; }
  cur_header = (zone_header*)cur_zone->items[0];
  header = (zone_header*)zone->items[0];
  for (i=0;i<cur_header->neighbor_count;i++) {
    cur_neighbor = NSLookup(&cur_header->neighbors[i]);
    found = 0;
    for (ii=0;ii<header->neighbor_count;ii++) {
      neighbor = NSLookup(&header->neighbors[ii]);
      if (neighbor == cur_neighbor) {
        found = 1;
        break;
      }
    }
    if (found) /* neighbor found in new zone? */
      continue; /* skip termination */
    n_header = (zone_header*)cur_neighbor->items[0];
    if (n_header->display_flags & 1) {
      GoolZoneObjectsTerminate(cur_neighbor);
      n_header->display_flags &= 0xFFFFFFFC; /* clear bits 1 & 2 */
    }
  }
}

/**
 * update the current zone, path, and progress
 *
 * or (in order):
 * - unload entries and pages in the loadlist for the current zone
 * - set the new current zone, path, and progress
 * - update the current poly id list
 * - load entries and pages in the loadlist for the new current zone
 * - update camera trans and rot according to the new path point
 *
 * zone - zone entry
 * path - path item
 * progress - progress (value)
 * flags - 1 = reset spawn list
 *         2 = ns update
 *         3 = stop midi seq
 */
//----- (80025A60) --------------------------------------------------------
void LevelUpdate(entry *zone, zone_path *path, int32_t progress, uint32_t flags) {
  zone_header *header, *cur_header, *n_header;
  ns_loadlist *loadlist;
  entry *slst, *neighbor;
  slst_item *raw_list, *delta;
  poly_id_list *tmp;
  int path_len, pt_idx, cur_pt_idx;
  int delta_pt_idx, first_pt_idx, mid_pt_idx;
  int i, idx, dir, change, flag;

  if (!zone || !path) {
    ns.level_update_pending = 0;
    return;
  }
  path_len = path->length << 8; /* 8 bit fractional fixed point */
  progress = limit(progress, 0, path_len-1); /* 0.0 to (len-1).9999 */
  header = (zone_header*)zone->items[0];
  pt_idx = progress >> 8;
  cur_pt_idx = cur_progress >> 8;
  change = zone != cur_zone || path != cur_path || pt_idx != cur_pt_idx;
  if (change && header->world_count) { /* change in zone, path, or pt idx? */
    slst = NSOpen(&path->slst, 1, 1);
    delta_pt_idx = abs((progress - cur_progress) >> 8);
    if (path == cur_path &&
      delta_pt_idx <= pt_idx &&
      delta_pt_idx <= path->length-(pt_idx+1)) /* new pt index within path bounds? */
      first_pt_idx = cur_pt_idx; /* apply deltas from cur pt to new pt */
    else { /* changing to new path */
      swap(prev_poly_ids, next_poly_ids);
      mid_pt_idx = path->length / 2;
      if (pt_idx < mid_pt_idx) { /* new pt in first half of path? */
        raw_list = (slst_item*)slst->items[0];
        cur_poly_ids = SlstUpdate(raw_list, cur_poly_ids, next_poly_ids, 1); /* copy raw list for first pt */
        first_pt_idx = 0; /* apply deltas from first pt [to new pt] */
      }
      else { /* new pt in last half */
        raw_list = (slst_item*)slst->items[path->length];
        cur_poly_ids = SlstUpdate(raw_list, cur_poly_ids, next_poly_ids, 0); /* copy raw list for last pt */
        first_pt_idx = path->length-1; /* apply deltas from last pt [to new pt] */
      }
    }
    i = pt_idx - first_pt_idx;
    idx = first_pt_idx;
    while (i != 0) { /* apply deltas from first_pt_idx to new pt */
      swap(prev_poly_ids, next_poly_ids);
      if (i<0) { i++;dir=0;delta=(slst_item*)slst->items[idx--]; }
      else     { i--;dir=1;delta=(slst_item*)slst->items[++idx]; }
      cur_poly_ids = SlstUpdate(delta, cur_poly_ids, next_poly_ids, dir);
    }
    NSClose((entry*)&path->slst, 1);
  }
  if (change || !header->world_count) {
    if (zone != cur_zone) { /* change in zone? */
      flag = 1;
      item_pool1 = 0;
      cur_header = 0;
      if (game_state == GAME_STATE_TITLE)
        flag = (flags & 2);
      if (cur_zone) {
        obj_zone = zone; /* TODO: where else is this used. 'next_zone'? */
        flag = flags & 2;
        cur_header = (zone_header*)cur_zone->items[0];
        ZoneTerminateDifference(zone);
      }
      if (flags & 1) {
        for (i=0;i<sizeof(spawns)/sizeof(uint32_t);i++)
          spawns[i] &= 0xFFFFFFF9;
      }
      cur_zone = zone;
      cur_path = path;
      cur_progress = progress;
      loadlist = 0;
      if (cur_header)
        loadlist = &cur_header->loadlist;
      NSZoneUnload(loadlist);
      loadlist = &header->loadlist;
      for (i=0;i<loadlist->entry_count;i++)
        NSOpen(&loadlist->eids[i], 0, 1);
      for (i=0;i<loadlist->page_count;i++) /* TODO: rename */
        NSPageOpen(loadlist->pgids[i], 0, 1, EID_NONE);
#ifdef PSX
      if (flag)
        NSUpdate2();
#endif
      for (i=0;i<header->neighbor_count;i++) {
        neighbor = NSLookup(&header->neighbors[i]);
        n_header = (zone_header*)neighbor->items[0];
        if (!(n_header->display_flags & 1)) { /* bit 1 clear? */
          prev_box = 0;
          prev_box_entity = 0;
          boxes_y = 0x19000;
          n_header->display_flags |= 3; /* set bits 1 and 2 */
        }
        if (flag)
          n_header->display_flags |= 4; /* set bit 3 */
        else
          n_header->display_flags &= ~4; /* clear bit 3 */
      }
      LevelUpdateMisc(&header->gfx, flags);
    }
    else { /* no change in zone */
      cur_zone = zone;
      cur_path = path;
      cur_progress = progress;
    }
  }
  else if (!change) { /* no change in pt idx (or zone, path)? */
    if (progress == cur_progress) { /* no change in progress? (fractional part) */
      ns.level_update_pending = 0;
      return; /* return */
    }
    cur_progress = progress; /* else set the new progress */
  }
  cam_rot_before = cam_rot;
  ZonePathProgressToLoc(cur_path, -cur_progress, (gool_vectors*)&cam_trans);
  cam_rot_after = cam_rot;
  ns.level_update_pending = 0;
}

//----- (800260AC) --------------------------------------------------------
void LevelUpdateMisc(zone_gfx *gfx, uint32_t flags) {
#ifndef PSX
  int ticks_elapsed;

  ticks_elapsed = GetTicksElapsed();
#endif
  respawn_stamp = ticks_elapsed;
  prev_vram_fill_color = vram_fill_color;
  next_vram_fill_color = gfx->unknown_h;
  cur_zone_flags_ro = gfx->flags;
  if (!(flags & 4))
    MidiSetStateStopped();
}

//----- (80026140) --------------------------------------------------------
void LevelInitGlobals() {
  cur_map_level = 99;
  saved_title_state = -1;
  title_state = 7;
  sfx_vol = 255;
  mus_vol = 255;
  init_life_count = (4 << 8);
  dword_800618E0 = 0;
  dword_8006190C = 0;
  mono = 0;
  level_count = 1;
  debug = 0;
  dword_80061A40 = 0;
  options_changed = 0;
  game_state = 0;
  saved_item_pool1 = 0;
  saved_item_pool2 = 0;
  saved_level_count = 1;
  LevelResetGlobals(1);
}

//----- (80026200) --------------------------------------------------------
void LevelResetGlobals(int flag) {
  if (!flag) { return; }
  checkpoint_id = -1;
  death_count = 0;
  respawn_count = 0;
  health = 0;
  fruit_count = 0;
  cortex_count = 0;
  brio_count = 0;
  tawna_count = 0;
  levels_unlocked = 1;
  item_pool1 = 0;
  item_pool2 = 0;
  is_first_zone = 1;
  cur_map_level = 99;
  level_count = 1;
  saved_item_pool1 = 0;
  saved_item_pool2 = 0;
  saved_level_count = 1;
  life_count = init_life_count;
  memset(level_spawns, 0, sizeof(level_spawns));
#ifdef PSX
  sub_8003D1B4();
#endif
}

//----- (800262DC) --------------------------------------------------------
void LevelInitMisc(int flag) {
  switch (ns.ldat->lid) {
  case 5:
    if (flag)
      GoolObjectCreate(&handles[4], 9, 4, 0, 0, 1);
    break;
  case 0xE:
    break;
  case 0x14:
  case 0x16:
    if (flag)
      GoolObjectCreate(&handles[4], 23, 6, 0, 0, 1);
    break;
  case 0x17:
    if (flag)
      ambiance_obj = GoolObjectCreate(&handles[4], 39, 4, 0, 0, 1);
    break;
  case 0x22:
  case 0x2E:
    if (flag)
      GoolObjectCreate(&handles[4], 53, 13, 0, 0, 1);
    break;
  case 0x28:
  case 0x2A:
    ShaderParamsUpdate(1);
    break;
  default:
    ambiance_obj = 0;
    break;
  }
  is_first_zone = 1;
  box_count = 0;
  gem_stamp = 0;
  island_cam_state = 0;
  if (pbak_state != 2)
    caption_obj = 0;
  title_pause_state = 0;
  TransSmoothStopAtSolid(0, 0, 0);
  fade_step = 32;
  cur_zone_query.once = 0;
  if (game_state != GAME_STATE_TITLE)
    fade_counter = 288;
}

//----- (80026460) --------------------------------------------------------
void LevelSaveState(gool_object *obj, level_state *state, int flag) {
  zone_header *header;
  zone_path *path;
  int i, idx;

  header = (zone_header*)cur_zone->items[0];
  if (header->flags & 0x2000) { return; } /* do not save state if restricted for zone */
  state->flag = flag;
  state->player_trans = crash->trans;
  state->player_rot = crash->rot;
  state->player_scale = crash->scale;
  if (obj->status_b & 0x200) /* obj status_b bit 10 set? */
    state->player_trans = obj->trans; /* override saved trans vector with obj trans vector */
  if (checkpoint_id != -1 && checkpoint_id) /* checkpoint? */
    state->player_trans = spawn_trans; /* override saved trans vector */
  state->player_rot.x = 0;
  state->player_rot.y = 0;
  state->player_rot.z = 0;
  state->zone = cur_zone->eid;
  state->progress = cur_progress;
  state->lid = ns.ldat->lid;
  for (i=0;i<header->path_count;i++) { /* find cur path index */
    idx = header->paths_idx + i;
    path = (zone_path*)cur_zone->items[idx];
    if (path == cur_path) {
      state->path_idx = i;
      break;
    }
  }
  state->_box_count = box_count;
  for (i=0;i<sizeof(spawns)/sizeof(uint32_t);i++)
    state->spawns[i] = spawns[i];
}

//----- (80026650) --------------------------------------------------------
void LevelRestart(level_state *state) {
  zone_header *header, *n_header;
  zone_path *path;
  entry *zone, *neighbor;
  eid_t zone_eid;
  int i, idx;
  gool_object *collider;

  bonus_round = 0;
  if (state->lid != ns.ldat->lid) { /* saved state for level different from cur? */
    next_lid = -2;
    first_spawn = 1;
    return;
  }
  GoolSendToColliders(0, GOOL_EVENT_RESPAWN, 0, 0, 0);
  if (cur_zone) {
    header = (zone_header*)cur_zone->items[0];
    obj_zone = (entry*)-1;
    for (i=0;i<header->neighbor_count;i++) {
      neighbor = NSLookup(&header->neighbors[i]);
      n_header = (zone_header*)neighbor->items[0];
      if (n_header->display_flags & 1) {
        GoolZoneObjectsTerminate(neighbor);
        n_header->display_flags &= 0xFFFFFFFC;
      }
    }
    NSZoneUnload(&header->loadlist);
  }
  cur_zone = 0;
#ifdef PSX
  SetGeomScreen(screen_proj);
#else
  params.screen_proj = screen_proj;
#endif
  if (first_spawn) {
    for (i=0;i<sizeof(spawns)/sizeof(uint32_t);i++)
      spawns[i] = state->spawns[i];
    if (checkpoint_id != -1) {
      spawns[checkpoint_id >> 8] &= ~2;
      spawns[checkpoint_id >> 8] |= 8;
    }
    for (i=0;i<sizeof(spawns)/sizeof(uint32_t);i++)
      spawns[i] &= ~1;
  }
  zone_eid = state->zone;
  zone = (entry*)NSOpen(&zone_eid, 1, 1);
  header = (zone_header*)zone->items[0];
  idx = header->paths_idx + state->path_idx;
  path = (zone_path*)zone->items[idx];
  LevelUpdate(zone, path, state->progress, first_spawn==0);
  NSClose(&zone_eid, 1);
  GoolObjectCreate(&handles[6], 0, 0, 0, 0, 1);
  crash->trans = state->player_trans;
  crash->rot = state->player_rot;
  crash->scale = state->player_scale;
  crash->zone = cur_zone;
  crash->floor_impact_stamp = 0;
  crash->velocity.x = 0;
  crash->velocity.y = 0;
  crash->velocity.z = 0;
  crash->speed = 0;
  crash->target_rot.x = crash->rot.x;
  if (collider=crash->collider) {
    collider->collider = 0;
    crash->collider = 0;
  }
  draw_count = 0;
  screen_shake = 0;
  // dword_800566E8 = 0;
  next_display_flags = GOOL_FLAG_DISPLAY_WORLDS | GOOL_FLAG_DISPANIM_OBJECTS | GOOL_FLAG_CAM_UPDATE;
  if (!first_spawn) {
    respawn_count += 0x100;
    if (state->flag)
      death_count = 0;
    else
      death_count += 0x100;
  }
  LevelInitMisc(0);
  if (first_spawn) {
    box_count = state->_box_count;
    first_spawn = 0;
    if (checkpoint_id != -1)
      box_count -= 0x100;
  }
}

//----- (80026A40) --------------------------------------------------------
// inline usage in ZoneFindNeighbor
static inline int TestPointInRect(rect *r, vec *v) {
  return (v->x >= r->loc.x << 8
       && v->y >= r->loc.y << 8
       && v->z >= r->loc.z << 8
       && v->x <= (r->loc.x + (int32_t)r->dim.w) << 8
       && v->y <= (r->loc.y + (int32_t)r->dim.h) << 8
       && v->z <= (r->loc.z + (int32_t)r->dim.d) << 8);
}

/*
int sub_80026ADC(unk_struct *s, vec *v) {
  rect *r;
  r = *(rect**)(((uint8_t*)s) + 0x14);
  return TestPointInRect(r, v);
}
*/

//----- (80026B80) --------------------------------------------------------
int TestPointInBound(vec *v, bound *b) {
  return (v->x >= b->p1.x && v->y >= b->p1.y && v->z >= b->p1.z
       && v->x <  b->p2.x && v->y <  b->p2.y && v->z <  b->p2.z);
}

//----- (80026C04) --------------------------------------------------------
int TestRectIntersectsBound(bound *b, rect *r) {
  return ((r->loc.x << 8) < b->p2.x
       && (r->loc.y << 8) < b->p2.y
       && (r->loc.z << 8) < b->p2.z
       && ((r->loc.x + (int32_t)r->dim.w) << 8) >= b->p1.x
       && ((r->loc.y + (int32_t)r->dim.h) << 8) >= b->p1.y
       && ((r->loc.z + (int32_t)r->dim.d) << 8) >= b->p1.z);
}

//----- (80026CA8) --------------------------------------------------------
int TestBoundIntersection(bound *ba, bound *bb) {
  return (bb->p2.y >= ba->p1.y && bb->p1.y <  ba->p2.y
       && bb->p1.x <  ba->p2.x && bb->p1.z <  ba->p2.z
       && bb->p2.x >= ba->p1.x && bb->p2.z >= ba->p1.z);
}

//----- (80026D40) --------------------------------------------------------
int TestBoundInBound(bound *ba, bound *bb) {
  return (ba->p1.x >= bb->p1.x
       && ba->p1.y >= bb->p1.y
       && ba->p1.z >= bb->p1.z
       && ba->p2.x <  bb->p2.x
       && ba->p2.y <  bb->p2.y
       && ba->p2.z <  bb->p2.z);
}

//----- (80026DD4) --------------------------------------------------------
entry *ZoneFindNeighbor(entry *zone, vec *v) {
  zone_header *header; // $s3 MAPDST
  zone_rect *z_rect; // $v1
  entry *neighbor; // $t2
  int i, last_idx; // $s2, $s0 MAPDST

  header = (zone_header*)zone->items[0];
  last_idx = header->neighbor_count - 1;
  for (i=last_idx;i>=0;i--) { /* find the neighbor zone which contains point v */
    neighbor = NSLookup(&header->neighbors[i]);
    z_rect = (zone_rect*)neighbor->items[1];
    if (TestPointInRect((rect*)z_rect, v))
      return neighbor;
  }
  if (zone == cur_zone)
    return (entry*)ERROR_NO_NEIGHBORS_FOUND;
  return ZoneFindNeighbor(cur_zone, v); /* inlined in the original impl. */
}

/**
 * finds the point of intersection between a zone octree and a ray
 * returns the node* which contains the point of intersection
 * (*or more appropriately, the corresponding rect)
 *
 * the approximate point of intersection is returned in vector argument va
 * a 'rebound' vector which is in some opposing direction to vb, is also returned in arg vb
 *
 * dim - zone dimensions item with octree
 * root - root node of [sub]octree
 * rect - rect corresponding to root node
 * va - initial point of the ray
 * vb - direction of the ray
 * level - current level of recursion (user should pass 0)
 */
//----- (80026FF0) --------------------------------------------------------
uint16_t ZoneFindNode(zone_rect *zone_rect, uint16_t root, rect _rect, vec *va, vec *vb, int level) {
  zone_octree *tree;
  rect child_rect;
  uint16_t node, child_node;
  uint16_t *child_nodes;
  vec corner;
  int32_t t,tx,ty,tz;
  int i,j,k,idx,res;

  node = root;
  if (node & 1) {/* node is a leaf node? */
    /* where N is the node rect, A is vector va and B is dir vector vb */
    /* A is now located inside the leaf node */
    /* point of intersection is now in -opposite- direction of B from A */
    /* get pt of intersection of N and the ray R2=(A-B)+Bt */
    /* find corner of N nearest to the pt */
    corner.x = _rect.loc.x + (int32_t)(vb->x<0?_rect.dim.w:0);
    corner.y = _rect.loc.y + (int32_t)(vb->y<0?_rect.dim.h:0);
    corner.z = _rect.loc.z + (int32_t)(vb->z<0?_rect.dim.d:0);
    /* plug the ray eq into the plane eqs for nearest 3 faces of N */
    /* and solve for the *furthest* t (nearest in opposite direction) */
    /* i.e. C=(A-B)+Bt => tn=(Cn-(An-Bn))/Bn => t=max(ti) */
    t = -0x80000000; /* INT_MIN */
    if (vb->x) {
      tx = ((corner.x-(va->x-vb->x))<<8)/vb->x;
      if (tx>t) { t = tx; i = 0; }
    }
    if (vb->z) {
      tz = ((corner.z-(va->z-vb->z))<<8)/vb->z;
      if (tz>t || abs(vb->x) < abs(vb->z)) { t = tz; i = 2; }
    }
    if (vb->y) {
      ty = ((corner.y-(va->y-vb->y))<<8)/vb->y;
      if (ty>t
       || (i==0 && abs(vb->x) < abs(vb->y))
       || (i==2 && abs(vb->z) < abs(vb->y))) { t = ty; i = 1; }
    }
    /* project A to the nearest face
      x
       x  _________
        x |        |
         xX<-A     |
          | x      |
          |x       |
         x|        |
        x |________|
       x
       (this is why the output A is considered 'approximate')
    */
    if (i == 0) { vb->x = -vb->x; va->x = corner.x; }
    if (i == 1) { vb->y = -vb->y; va->y = corner.y; }
    if (i == 2) { vb->z = -vb->z; va->z = corner.z; }
    return node;
  }
  else if (node) { /* node is a solid [non-leaf] node? */
    tree = &zone_rect->octree;
    child_nodes = (uint16_t*)((uint8_t*)zone_rect + node);
    child_rect = _rect;
    /* subdivide each dimension for which level is below max depth */
    if (level < tree->max_depth_x)
      child_rect.dim.w /= 2;
    if (level < tree->max_depth_y)
      child_rect.dim.h /= 2;
    if (level < tree->max_depth_z)
      child_rect.dim.d /= 2;
    idx=0;
    for (i=0;i<((level<tree->max_depth_x)?2:1);i++) {
      for (j=0;j<((level<tree->max_depth_y)?2:1);j++) {
        for (k=0;k<((level<tree->max_depth_z)?2:1);k++) {
          child_rect.loc.x = _rect.loc.x + (int32_t)(i?child_rect.dim.w:0);
          child_rect.loc.y = _rect.loc.y + (int32_t)(j?child_rect.dim.h:0);
          child_rect.loc.z = _rect.loc.z + (int32_t)(k?child_rect.dim.d:0);
          child_node = child_nodes[idx++];
          if (TestPointInRect(&child_rect, va)) {
            res = ZoneFindNode(zone_rect, child_node, child_rect, va, vb, level+1);
            if (res) { return res; }
          }
        }
      }
    }
    return 0; /* continue at previous level */
  }
  else { /* node is an empty non-leaf node */
    /* where N is the node rect, A is vector va and B is dir vector vb */
    /* get pt of intersection of N and the ray R=A+Bt */
    /* find the corner of N nearest to the pt */
    corner.x = _rect.loc.x + (int32_t)(vb->x>0?_rect.dim.w:0);
    corner.y = _rect.loc.y + (int32_t)(vb->y>0?_rect.dim.h:0);
    corner.z = _rect.loc.z + (int32_t)(vb->z>0?_rect.dim.d:0);
    /* plug the ray eq into the plane eqs for nearest 3 faces of N */
    /* and solve for the nearest t */
    /* i.e. C=A+Bt => tn=(Cn-An)/Bn => t=min(ti) */
    t = 0x7FFFFFFF; /* INT_MAX */
    if (vb->x) {
      tx = ((corner.x-va->x)<<8)/vb->x;
      if (tx<t) { t = tx; }
    }
    if (vb->y) {
      ty = ((corner.y-va->y)<<8)/vb->y;
      if (ty<t) { t = ty; }
    }
    if (vb->z) {
      tz = ((corner.z-va->z)<<8)/vb->z;
      if (tz<t) { t = tz; }
    }
    t += (1<<8); /* make slight adjustment to t; this will move va into a neighboring node */
    va->x += (t*vb->x)>>8;
    va->y += (t*vb->y)>>8;
    va->z += (t*vb->z)>>8;
    return 0; /* continue at previous level */
  }
}

//----- (800277EC) --------------------------------------------------------
uint16_t ZoneReboundVector(vec *va, vec *vb) {
  zone_header *header;
  zone_rect *z_rect;
  entry *neighbor;
  rect _rect;
  uint16_t root, node;
  int i;

  if (!vb->x && !vb->y && !vb->z) { return 0; }
  header = (zone_header*)cur_zone->items[0];
  while (1) { /* this will loop endlessly while ZoneFindNode returns 0 */
    for (i=0;i<header->neighbor_count;i++) {
      neighbor = NSLookup(&header->neighbors[i]);
      z_rect = (zone_rect*)neighbor->items[1];
      if (TestPointInRect((rect*)z_rect, va))
        break;
    }
    if (i==header->neighbor_count) /* did not find a zone that contains va? */
      return 1; /* return */
    _rect = *((rect*)z_rect);
    _rect.x<<=8;_rect.y<<=8;_rect.z<<=8;
    _rect.w<<=8;_rect.h<<=8;_rect.d<<=8;
    root = z_rect->octree.root;
    node = ZoneFindNode(z_rect, root, _rect, va, vb, 0);
    if (node)
      return node;
  }
}

//----- (80027A08) --------------------------------------------------------
static inline void sub_80027A08(gool_object *obj, int idx, uint32_t val) {
  uint32_t inc, res;

  inc = obj->state_flags & 0x18 ? 0: 0x18;
  if (idx < 0)
    res = inc + val;
  else
    res = inc + size_map[idx];
  obj->size = res;
}

//----- (80027A4C) --------------------------------------------------------
static void ZoneColorsScale(gool_colors *src, gool_colors *dst, int rp, int gp, int bp) {
  uint32_t rf, gf, bf;
  int i;

  rf = (rp << 12) / 100; /* percentage to fraction */
  gf = (gp << 12) / 100;
  bf = (bp << 12) / 100;
  dst->color.r = (src->color.r * rf) >> 12;
  dst->color.g = (src->color.g * gf) >> 12;
  dst->color.b = (src->color.b * bf) >> 12;
  dst->intensity = src->intensity;
  for (i=0;i<3;i++) {
    dst->light_mat.v[i].x = (src->light_mat.v[i].x * rf) >> 12;
    dst->light_mat.v[i].y = (src->light_mat.v[i].y * gf) >> 12;
    dst->light_mat.v[i].z = (src->light_mat.v[i].z * bf) >> 12;
    dst->color_mat.c[i] = src->color_mat.c[i];
  }
}

//----- (80027BC4) --------------------------------------------------------
void ZoneColorsScaleByNode(int subtype, gool_colors *src, gool_colors *dst) {
  lid_t lid; // $v1
  uint8_t pct;

  if (subtype < 39) {
    ZoneColorsScale(src, dst, 100, 100, 100);
    return;
  }
  ZoneColorsScale(src, dst, 0, 0, 0);
  lid = ns.ldat->lid;
  if (lid == LID_CORTEXPOWER && subtype == 40) {
    dst->light_mat.v[0].x = 0;
    dst->light_mat.v[0].y = -8601;
    dst->light_mat.v[0].z = 0;
    dst->light_mat.v[1].x = -3809;
    dst->light_mat.v[1].y = -1679;
    dst->light_mat.v[1].z = 2621;
    dst->light_mat.v[2].x = 3563;
    dst->light_mat.v[2].y = 4915;
    dst->light_mat.v[2].z = -286;
    dst->color.r = 0;
    dst->color.g = 255;
    dst->color.b = 255;
    dst->color_mat.c[0].r = 0;
    dst->color_mat.c[0].g = 255;
    dst->color_mat.c[0].b = 0;
    dst->color_mat.c[1].r = 88;
    dst->color_mat.c[1].g = 637;
    dst->color_mat.c[1].b = 90;
    dst->color_mat.c[2].r = 284;
    dst->color_mat.c[2].g = 128;
    dst->color_mat.c[2].b = 128;
    dst->intensity.r = 255;
    dst->intensity.g = 255;
    dst->intensity.b = 255;
  }
  else if (lid == LID_TOXICWASTE && subtype == 40) {
    dst->light_mat.v[0].x = 0;
    dst->light_mat.v[0].y = -8601;
    dst->light_mat.v[0].z = 0;
    dst->light_mat.v[1].x = -3809;
    dst->light_mat.v[1].y = -1679;
    dst->light_mat.v[1].z = 2621;
    dst->light_mat.v[2].x = 3563;
    dst->light_mat.v[2].y = 4915;
    dst->light_mat.v[2].z = -286;
    dst->color.r = 0;
    dst->color.g = 255;
    dst->color.b = 255;
    dst->color_mat.c[0].g = 255;
    dst->color_mat.c[0].r = 192;
    dst->color_mat.c[0].b = 192;
    dst->color_mat.c[1].r = 224;
    dst->color_mat.c[1].g = 400;
    dst->color_mat.c[1].b = 224;
    dst->color_mat.c[2].r = 260;
    dst->color_mat.c[2].g = 240;
    dst->color_mat.c[2].b = 240;
    dst->intensity.r = 255;
    dst->intensity.g = 255;
    dst->intensity.b = 255;
  }
  else if (lid == LID_BOULDERDASH && subtype == 40) {
    ZoneColorsScale(src, dst, 100, 100, 100);
    dst->color_mat.c[0].r = 0;
    dst->color_mat.c[0].g = 944;
    dst->color_mat.c[0].b = 944;
    dst->color_mat.c[1].r = 0;
    dst->color_mat.c[1].g = 249;
    dst->color_mat.c[1].b = 255;
    dst->color_mat.c[2].r = 0;
    dst->color_mat.c[2].g = 100;
    dst->color_mat.c[2].b = 255;
    dst->intensity.r = 0;
    dst->intensity.g = 255;
    dst->intensity.b = 255;
  }
  else if (lid == LID_TEMPLERUINS || lid == LID_JAWSOFDARKNESS) {
    switch (subtype) {
    case '(':
      ZoneColorsScale(src, dst, 50, 100, 100);
      break;
    case ')':
      ZoneColorsScale(src, dst, 75, 100, 100);
      break;
    case '*':
      ZoneColorsScale(src, dst, 100, 100, 100);
      break;
    case '+':
      ZoneColorsScale(src, dst, 125, 100, 100);
      break;
    case ',':
      ZoneColorsScale(src, dst, 150, 100, 100);
      break;
    default:
      break;
    }
  }
  if (subtype < 40)
    ZoneColorsScale(src, dst, 100, 100, 100);
  else if (subtype >= 48) {
    pct = percent_map[subtype-48]; /* percentage */
    ZoneColorsScale(src, dst, pct, pct, pct);
  }
}

//----- (80027F00) --------------------------------------------------------
static inline void ZoneColorSeek(int16_t *src, int16_t *dst, int16_t step) {
  int16_t delta;

  if (step) {
    delta = *dst - *src;
    if (delta > step)
      *dst = *src + step;
    else if (delta < -step)
      *dst = *src - step;
  }
  *src = *dst;
}

/**
 * scales input [gool] colors per the given node subtype
 * if flag is set, the input obj colors will seek towards the scaled input colors
 */
//----- (80027F50) --------------------------------------------------------
void ZoneColorsScaleSeek(gool_colors *colors, gool_object *obj, int subtype, int flag) {
  gool_colors *src, dst;
  int i, step;

  if (obj == crash && obj->state_flags & 0x20)
    subtype = 0x37;
  ZoneColorsScaleByNode(subtype, colors, &dst);
  src = &obj->colors;
  step = flag ? 0x15E : 0;
  ZoneColorSeek(&src->color.r, &dst.color.r, step);
  ZoneColorSeek(&src->color.g, &dst.color.g, step);
  ZoneColorSeek(&src->color.b, &dst.color.b, step);
  ZoneColorSeek(&src->intensity.r, &dst.intensity.r, step);
  ZoneColorSeek(&src->intensity.g, &dst.intensity.g, step);
  ZoneColorSeek(&src->intensity.b, &dst.intensity.b, step);
  for (i=0;i<3;i++) {
    ZoneColorSeek(&src->light_mat.v[i].x, &dst.light_mat.v[i].x, step);
    ZoneColorSeek(&src->light_mat.v[i].y, &dst.light_mat.v[i].y, step);
    ZoneColorSeek(&src->light_mat.v[i].z, &dst.light_mat.v[i].z, step);
    ZoneColorSeek(&src->color_mat.c[i].r, &dst.color_mat.c[i].r, step);
    ZoneColorSeek(&src->color_mat.c[i].g, &dst.color_mat.c[i].g, step);
    ZoneColorSeek(&src->color_mat.c[i].b, &dst.color_mat.c[i].b, step);
  }
}

//----- (8002832C) --------------------------------------------------------
uint16_t ZoneFindNearestNode(zone_rect *zone_rect, uint16_t root, rect *nrect, vec *v, int level, int flags) {
  zone_octree *tree;
  rect child_rect;
  uint16_t node, *child_nodes;
  int i,j,k,ii,ij,ik,fi,fj,fk;
  int type, subtype, idx;

  node = root;
  if ((node & 1) || node == 0) { /* is the node a leaf node or empty? */
    type = (node & 0xE) >> 1;
    subtype = (node & 0x3F0) >> 4;
    if ((node & 1) && (!(flags & 8) || type != 3)
      && type != 4 && subtype != 11) { /* node is a leaf of type != 3,4, subtype != 11, and flag 4 is clear? */
      if (flags & 1)
        v->y = nrect->loc.y + (int32_t)nrect->dim.h;
      else if (flags & 2)
        v->z = nrect->loc.z + (int32_t)nrect->dim.d;
      return node;
    }
    if (flags & 1)
      v->y = nrect->loc.y - 1;
    else if (flags & 2)
      v->z = nrect->loc.z - 1;
    return 0;
  }
  else { /* the node is a non-empty non-leaf */
    tree = &zone_rect->octree;
    child_nodes = (uint16_t*)((uint8_t*)zone_rect + node);
    child_rect = *nrect;
    ii=0;ij=0;ik=0; /* child [octant] index */
    fi=0;fj=0;fk=0; /* flags set when there is a subdivision */
    /* subdivide each dimension for which level is below max depth */
    if (level < tree->max_depth_x) {
      child_rect.dim.w /= 2;
      fi = 1;
      if (v->x >= nrect->loc.x + (int32_t)child_rect.dim.w) { /* v is in the right half? */
        child_rect.loc.x = nrect->loc.x + (int32_t)child_rect.dim.w; /* adjust x loc */
        ii = 1; /* adjust x idx */
      }
    }
    if (level < tree->max_depth_y) {
      child_rect.dim.h /= 2;
      fj = 1;
      if (v->y >= nrect->loc.y + (int32_t)child_rect.dim.h) { /* v is in the lower half? */
        child_rect.loc.y = nrect->loc.y + (int32_t)child_rect.dim.h; /* adjust y loc */
        ij = 1; /* adjust y idx */
      }
    }
    if (level < tree->max_depth_z) {
      child_rect.dim.d /= 2;
      fk = 1;
      if (v->z >= nrect->loc.z + (int32_t)child_rect.dim.d) { /* v is in the back half? */
        child_rect.loc.z = nrect->loc.z + (int32_t)child_rect.dim.d; /* adjust z loc */
        ik = 1; /* adjust z idx */
      }
    }
    /* calculate octant index of the child node */
    idx=0;
    for (i=0;i<(fi?2:1);i++) {
     for (j=0;j<(fj?2:1);j++) {
      for (k=0;k<(fk?2:1);k++) {
       if (i == ii && j == ij && k == ik)
        return ZoneFindNearestNode(zone_rect, child_nodes[idx],
         &child_rect, v, level+1, flags);
       idx++;
      }
     }
    }
    return 0;
  }
}

/* first argument is unused! */
//----- (80028644) --------------------------------------------------------
gool_objnode ZoneFindNearestObjectNode(gool_object *obj, vec *v) {
  gool_objnode res;
  gool_bound *bound;
  gool_object *found_max;
  zone_header *header;
  zone_rect *z_rect;
  entry *neighbor;
  rect _rect;
  vec va;
  uint16_t root;
  int i, y_max;

  y_max = -999999999;
  va = *v;
  header = (zone_header*)cur_zone->items[0];
  /* first find the nearest octree node to input vec
   amongst cur zone and neighbors, if any such node */
  while (1) {
    for (i=0;i<header->neighbor_count;i++) {
      neighbor = NSLookup(&header->neighbors[i]);
      z_rect = (zone_rect*)neighbor->items[1];
      if (TestPointInRect((rect*)z_rect, v))
        break;
    }
    if (i==header->neighbor_count) {
      res.node = 0;
      break;
    }
    _rect = *((rect*)z_rect);
    _rect.x<<=8;_rect.y<<=8;_rect.z<<=8;
    _rect.w<<=8;_rect.h<<=8;_rect.d<<=8;
    root = z_rect->octree.root;
    res.node = ZoneFindNearestNode(z_rect, root, &_rect, v, 0, 9);
    if (res.node)
      break;
  }
  /* find an object which collides with the input vec
     and which is stopped by some node */
  found_max = 0;
  for (i=0;i<object_bound_count;i++) {
    bound = &object_bounds[i];
    if (!(bound->obj->status_b & 0x20000)) { continue; }
    if (bound->p1.x - 20000 <= va.x && va.x <= bound->p2.x + 20000 /* bound box padding! */
      && bound->p1.z - 20000 <= va.z && va.z <= bound->p2.z + 20000) {
      if (bound->p1.y <= va.y && va.y <= bound->p2.y) { /* collides with va? */
        v->y = bound->p2.y; /* replace with top (bottom?) bound y location */
        res.obj = bound->obj;
        return res; /* return the obj */
      }
      else if (bound->p2.y > y_max && va.y >= bound->p2.y) {
        /* keep track of highest object below va */
        found_max = bound->obj;
        y_max = bound->p2.y;
      }
    }
  }
  if (found_max && (!res.node || y_max >= v->y)) {
    /* found a highest below obj */
    /* and no node or max object y is nearer to va? */
    v->y = y_max; /* replace with max object y */
    res.obj = found_max; /* put max object in the result */
  }
  return res; /* return; either obj or node */
}

//----- (800289B4) --------------------------------------------------------
gool_objnode ZoneFindNearestObjectNode2(gool_object *obj, vec *v) {
  gool_objnode res;
  gool_bound *bound;
  gool_object *found_max;
  gool_colors *colors;
  zone_header *header;
  zone_rect *z_rect;
  entry *neighbor;
  rect _rect;
  vec va, vb;
  uint16_t root, node_val;
  uint32_t size;
  int i, y_max, found;

  res.value = 0;
  if (!(obj->status_b & 0x4000000))
    return res;
  y_max = -999999999;
  header = (zone_header *)cur_zone->items[0];
  /* first find the nearest octree node to input vec
     amongst cur zone and neighbors, if any such node */
  while (1) {
    for (i=0;i<header->neighbor_count;i++) {
      neighbor = NSLookup(&header->neighbors[i]);
      z_rect = (zone_rect*)neighbor->items[1];
      if (TestPointInRect((rect*)z_rect, v))
        break;
    }
    if (i==header->neighbor_count) {
      res.node = 0;
      break;
    }
    _rect = *((rect*)z_rect);
    _rect.x<<=8;_rect.y<<=8;_rect.z<<=8;
    _rect.w<<=8;_rect.h<<=8;_rect.d<<=8;
    root = z_rect->octree.root;
    res.node = ZoneFindNearestNode(z_rect, root, &_rect, v, 0, 1);
    if (res.node)
      break;
  }
  /* find an object (other than obj) which collides with the input vec
     and which is stopped by some node */
  found = 0; found_max = 0;
  for (i=0;i<object_bound_count;i++) {
    bound = &object_bounds[i];
    if (!((bound->obj->status_b & 0x40020000) == 0x20000)) { continue; }
    if (va.x >= bound->p1.x - 35000 && va.x <= bound->p2.x + 35000
      && va.z >= bound->p1.z - 35000 && va.z <= bound->p2.z + 35000) {
      if (va.y >= bound->p1.y && va.y <= bound->p2.y) {
        res.obj = bound->obj;
        va.y = bound->p1.y;
        found = 1;
        break;
      }
      else if (bound->p2.y > y_max && vb.y >= bound->p2.y) {
        /* keep track of highest object below va */
        found_max = bound->obj;
        y_max = bound->p2.y;
      }
    }
  }
  if (found_max && (!res.obj || y_max >= va.y)) {
    /* found a highest below obj */
    /* and no node or max object y is nearer to va? */
    v->y = y_max; /* replace with max object y */
    res.obj = found_max; /* put max object in the result */
  }
  if (!res.value) /* has nothing been found? */
    sub_80027A08(obj->parent, -1, 0);
  else if (found) { /* has an object been found? */
    size = res.obj->size;
    if (obj->trans.y > cam_trans.y)
      size -= (obj->trans.y - cam_trans.y) >> 12;
    sub_80027A08(obj->parent, -1, size);
  }
  else /* a node was found */
    sub_80027A08(obj->parent, (res.node&0x3C00)>>10, 0);
  return res; /* gool object or node */
}

//----- (80028E3C) --------------------------------------------------------
gool_objnode ZoneFindNearestObjectNode3(gool_object *obj, vec *v, int flags, int flag) {
  gool_objnode res;
  gool_bound *bound;
  gool_object *found_max;
  gool_colors *colors;
  zone_header *header;
  zone_rect *z_rect;
  entry *neighbor;
  rect _rect;
  vec va, vb;
  uint16_t root, node_val;
  int i, yz_max, found, subtype;

  res.value = 0;
  if (!(obj->status_b & 0x4000000) || ((flags & 4) && !(obj->parent->status_b & 0x4000000)))
    return res;
  yz_max = -999999999;
  header = (zone_header *)cur_zone->items[0];
  va.x = v->x;
  va.y = v->y + 25000;
  va.z = v->z;
  vb = va;
  /* first find the nearest octree node to input vec
     amongst cur zone and neighbors, if any such node */
  while (1) {
    for (i=0;i<header->neighbor_count;i++) {
      neighbor = NSLookup(&header->neighbors[i]);
      z_rect = (zone_rect*)neighbor->items[1];
      if (TestPointInRect((rect*)z_rect, &va))
        break;
    }
    if (i==header->neighbor_count) {
      res.node = 0;
      break;
    }
    _rect = *((rect*)z_rect);
    _rect.x<<=8;_rect.y<<=8;_rect.z<<=8;
    _rect.w<<=8;_rect.h<<=8;_rect.d<<=8;
    root = z_rect->octree.root;
    res.node = ZoneFindNearestNode(z_rect, root, &_rect, &va, 0, flags & 3);
    if (res.node)
      break;
  }
  /* find an object (other than obj) which collides with the input vec
     and which is stopped by some node */
  found = 0; found_max = 0;
  for (i=0;i<object_bound_count;i++) {
    bound = &object_bounds[i];
    if (bound->obj == obj || bound->obj->node == 0xFFFF)
      continue;
    if (flags & 1) {
      if (va.x >= bound->p1.x && va.x <= bound->p2.x
        && va.z >= bound->p1.z && va.z <= bound->p2.z) {
        if (va.y >= bound->p1.y && va.y <= bound->p2.y) {
          res.obj = bound->obj;
          va.y = bound->p1.y;
          found = 1;
          break;
        }
        else if (bound->p2.y > yz_max && vb.y >= bound->p2.y) {
          /* keep track of highest object below va */
          found_max = bound->obj;
          yz_max = bound->p2.y;
        }
      }
    }
    else if (flags & 2) {
      if (va.x >= bound->p1.x && va.x <= bound->p2.x
        && va.y >= bound->p1.y && va.y <= bound->p2.y) {
        if (va.z >= bound->p1.z && va.z < bound->p2.z) {
          res.obj = bound->obj;
          va.z = bound->p1.z;
          found = 1;
          break;
        }
        if (bound->p2.z > yz_max && va.z >= bound->p2.z) {
          /* keep track of nearest z-wise object behind va */
          found_max = bound->obj;
          yz_max = bound->p2.z;
        }
      }
    }
  }
  if (!found && found_max) { /* a highest below or nearest z-wise object found? */
    if (flags & 1) {
      if (!res.obj || yz_max >= va.y) { /* no collider found or max object y is nearer to va? */
        res.obj = found_max;
        va.y = yz_max; /* use the max object y as it is nearest to va */
        found = 1;
      }
    }
    else if (flags & 2) {
      if (!res.obj || yz_max >= va.z) { /* no collider found or max object z is nearer to va? */
        res.obj = found_max;
        va.z = yz_max; /* use the max object z as it is nearest to va */
        found = 1;
      }
    }
  }
  if (flags & 4)
    header = (zone_header*)obj->parent->zone->items[0];
  else
    header = (zone_header*)obj->zone->items[0];
  if (obj == crash || obj->parent == crash)
    colors = &header->player_colors;
  else
    colors = &header->object_colors;
  if (!res.value) /* has nothing been found? */
    subtype = -1;
  else if (found) { /* has an object been found? */
    subtype = res.obj->node; /* get node subtype from the object */
    if (subtype < 0) { /* subtype is negative? ('no seek' bit set) */
      flag = 0; /* clear the seek flag */
      subtype = -subtype; /* clear the 'no seek' bit */
    }
  }
  else { /* a node was found */
    subtype = (res.node & 0x3F0) >> 4; /* get subtype from the node */
    if (subtype < 39)
      subtype = -1; /* only 39 to 63 have color seek behavior */
  }
  if (flags & 4) {
    /* seek parent object colors towards zone colors
       scaled per the node value */
    ZoneColorsScaleSeek(colors, obj->parent, subtype, flag);
  }
  return res; /* gool object or node */
}

//----- (8002940C) --------------------------------------------------------
void ZoneColorsScaleSeekByEntityNode(gool_object *obj) {
  zone_header *header;
  gool_colors *colors;
  int16_t node;
  int subtype;

  if (!obj->entity) { return; }
  header = (zone_header*)obj->zone->items[0];
  if (obj == crash || obj->parent == crash)
    colors = &header->player_colors;
  else
    colors = &header->object_colors;
  node = (int16_t)((obj->entity->spawn_flags) >> 3);
  if (node != -1) {
    subtype = (node & 0x3F0) >> 4;
    if (subtype < 39)
      subtype = -1;
    ZoneColorsScaleSeek(colors, obj, subtype, 0);
  }
}

/* second argument is unused!!! */
//----- (800294B0) --------------------------------------------------------
int ZoneQueryOctrees(vec *v, gool_object *obj, zone_query *query) {
  zone_header *header; // $s2
  zone_rect *z_rect;
  entry *neighbor;
  zone_query_results *results_tail;
  int i, count;

  query->once = 1;
  query->result_count = 0;
  query->nodes_bound.p1.x = v->x - 76800;
  query->nodes_bound.p1.y = v->y - 68480;
  query->nodes_bound.p1.z = v->z - 76800;
  query->nodes_bound.p2.x = v->x + 76800;
  query->nodes_bound.p2.y = v->y + 238720;
  query->nodes_bound.p2.z = v->z + 76800;
  header = (zone_header*)cur_zone->items[0];
  for (i=0;i<header->neighbor_count;i++) {
    neighbor = NSLookup(&header->neighbors[i]);
    z_rect = (zone_rect*)neighbor->items[1];
    if (TestRectIntersectsBound(&query->nodes_bound, (rect*)z_rect)) {
      count = query->result_count;
      results_tail = (zone_query_results*)&query->results[count];
#ifdef PSX
      query->result_count = RZoneQueryOctree(z_rect, &query->nodes_bound, results_tail);
#else
      query->result_count += ZoneQueryOctree(z_rect, &query->nodes_bound, results_tail);
#endif
    }
  }
  count = query->result_count;
  *((int32_t*)(&query->results[count])) = -1;
  return count;
}

//----- (8002967C) --------------------------------------------------------
int ZoneQueryOctrees2(gool_object *obj, zone_query *query) {
  return ZoneQueryOctrees(&obj->trans, obj, query);
}

//----- (800296A8) --------------------------------------------------------
int ZonePathProgressToLoc(zone_path *path, int progress, gool_vectors *cam) {
  entry *zone, *neighbor;
  zone_header *header, *n_header;
  zone_rect *z_rect, *nz_rect;
  zone_path *n_path;
  zone_path_point *point, *point_next;
  zone_neighbor_path *neighbor_path;
  gool_vectors cam_next;
  int32_t fractional;
  int pt_idx, idx_next, flag;
  int i, neighbor_idx, n_path_idx;

  zone = path->parent_zone;
  header = (zone_header*)zone->items[0];
  z_rect = (zone_rect*)zone->items[1];
  pt_idx = abs(progress) >> 8;
  point = &path->points[pt_idx];
  cam->trans.x = (z_rect->x + point->x) << 8;
  cam->trans.y = (z_rect->y + point->y) << 8;
  cam->trans.z = (z_rect->z + point->z) << 8;
  cam->rot.y = point->rot_y;
  cam->rot.x = point->rot_x;
  cam->rot.z = point->rot_z;
  fractional = abs(progress) & 0xFF;
  n_path = 0;
  flag = 1; /* flag for 'next point internal to this path' */
  /* progress has a nonzero fractional part and this is the last pt? */
  if (pt_idx == path->length-1 && fractional) {
    for (i=0;i<path->neighbor_path_count;i++) { /* find the following path */
      neighbor_path = &path->neighbor_paths[i];
      if (!(neighbor_path->relation & 2)) { continue; } /* skip preceding paths */
      neighbor_idx = neighbor_path->neighbor_zone_idx;
      neighbor = NSLookup(&header->neighbors[neighbor_idx]);
      n_header = (zone_header*)neighbor->items[0];
      n_path_idx = n_header->paths_idx + neighbor_path->path_idx;
      n_path = (zone_path*)neighbor->items[n_path_idx];
      break;
    }
    if (n_path && n_path->cam_mode != 1) { /* was a following path found? */
      idx_next = (neighbor_path->goal&2) ? n_path->length-1: 0;
      point_next = &n_path->points[idx_next];
      nz_rect = (zone_rect*)neighbor->items[1];
      cam_next.trans.x = (nz_rect->x + point_next->x) << 8;
      cam_next.trans.y = (nz_rect->y + point_next->y) << 8;
      cam_next.trans.z = (nz_rect->z + point_next->z) << 8;
      cam_next.rot.y = point_next->rot_y;
      cam_next.rot.x = point_next->rot_x;
      cam_next.rot.z = point_next->rot_z;
      flag = 0; /* next point is external to the path */
    }
  }
  if (flag) { /* next point is internal to the path? */
    idx_next = min(pt_idx+1, path->length-1);
    point_next = &path->points[idx_next];
    cam_next.trans.x = (z_rect->x + point_next->x) << 8;
    cam_next.trans.y = (z_rect->y + point_next->y) << 8;
    cam_next.trans.z = (z_rect->z + point_next->z) << 8;
    cam_next.rot.y = point_next->rot_y;
    cam_next.rot.x = point_next->rot_x;
    cam_next.rot.z = point_next->rot_z;
  }
  /* adjust final values by interpolating between cam and cam_next
     using the fractional part of progress */
  cam->trans.x += ((cam_next.trans.x - cam->trans.x)*fractional) >> 8;
  cam->trans.y += ((cam_next.trans.y - cam->trans.y)*fractional) >> 8;
  cam->trans.z += ((cam_next.trans.z - cam->trans.z)*fractional) >> 8;
  cam->rot.y += (GoolAngDiff(cam->rot.y, cam_next.rot.y)*fractional) >> 8;
  cam->rot.x += ((cam_next.rot.x - cam->rot.x)*fractional) >> 8;
  cam->rot.z += ((cam_next.rot.z - cam->rot.z)*fractional) >> 8;
  return path->length;
}

// inline code in various funcs
inline zone_path *ZoneGetNeighborPath(entry *zone, zone_path *path, int idx) {
  zone_header *header, *n_header;
  zone_path *n_path;
  zone_neighbor_path neighbor_path;
  entry *neighbor;
  int neighbor_idx, n_path_idx;

  neighbor_path = path->neighbor_paths[idx];
  neighbor_idx = neighbor_path.neighbor_zone_idx;
  header = (zone_header*)zone->items[0];
  neighbor = NSLookup(&header->neighbors[neighbor_idx]);
  n_header = (zone_header*)neighbor->items[0];
  n_path_idx = n_header->paths_idx + neighbor_path.path_idx;
  n_path = (zone_path*)neighbor->items[n_path_idx];
  return n_path;
}
