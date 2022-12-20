#include "gool.h"
#include "globals.h"
#include "math.h"
#include "pad.h"
#include "solid.h"
#include "level.h"
#include "audio.h"
#include "midi.h"
#ifdef GOOL_DEBUG
#include "ext/disgool.h"
#endif
#ifdef PSX
#include <LIBGTE.h>
#include "psx/card.h"
#else
#include "pc/gfx/soft.h"
int CardControl(int op, int part_idx) { return 1; }
#endif

/* forward declarations for unexported funcs */
int GoolObjectChangeState(gool_object*,uint32_t,int,uint32_t*);
int GoolObjectInit(gool_object*,int,int,int,int*);
int GoolObjectKill(gool_object*,int);
int GoolObjectPushFrame(gool_object*,int,uint32_t);
int GoolObjectUpdate(gool_object*,int);
void GoolObjectTransform(gool_object*);
int GoolObjectColors(gool_object*);
int GoolObjectBound(gool_object*);
int GoolObjectPhysics(gool_object*);
int GoolObjectInterpret(gool_object*,uint32_t,gool_state_ref*);
void GoolTextObjectTransform(gool_object*,gool_text*,int,void*,int);
int GoolProject(vec*,vec*);
int GoolSendToColliders2(gool_object*,gool_object*,uint32_t,int,int,uint32_t*);
int GoolObjectInterrupt(gool_object*,uint32_t,int,uint32_t*);
#ifdef GOOL_DEBUG
void GoolObjectPrintDebug(gool_object*,FILE*);
#endif

#define init_obj          player
#define main_obj          crash

#define GETRFP(obj)       (!obj->fp)?0:((uint32_t)obj->fp-(uint32_t)&obj->process)
#define GETRSP(obj)       (!obj->sp)?0:((uint32_t)obj->sp-(uint32_t)&obj->process)

static inline void GoolObjectPush(gool_object *obj, uint32_t value) {
  *(obj->sp++) = value;
}
static inline uint32_t GoolObjectPop(gool_object *obj) {
  return *(--obj->sp);
}
static inline uint32_t GoolObjectPeek(gool_object *obj) {
  return *obj->sp;
}
static inline gool_object *GoolObjectPrevChild(gool_object *obj, gool_object *child) {
  gool_object *cur;

  cur = GoolObjectGetChildren(obj);
  while (cur && cur->sibling != child)
    cur = cur->sibling;
  return cur;
}
static inline void GoolObjectRemoveChild(gool_object *obj, gool_object *child) {
  gool_object *first, *prev;

  first = GoolObjectGetChildren(obj);
  if (!first) { return; }
  if (child == first) {
    GoolObjectSetChildren(obj, child->sibling);
  }
  else if (prev = GoolObjectPrevChild(obj, child))
    prev->sibling = child->sibling;
}
static inline void GoolObjectAddChild(gool_object *obj, gool_object *child) {
  if (child->parent)
    GoolObjectRemoveChild(child->parent, child);
  child->parent = obj;
  child->sibling = GoolObjectGetChildren(obj);
  GoolObjectSetChildren(obj, child);
}

/* .data */
const gool_move_state move_states[16] = { /* 80052B64 */
  { .dir = 8, .angle =     0, .speed_scale = 0x100 },
  { .dir = 0, .angle = 0x800, .speed_scale = 0x100 },
  { .dir = 2, .angle = 0x400, .speed_scale = 0x100 },
  { .dir = 1, .angle = 0x600, .speed_scale = 0x147 },
  { .dir = 4, .angle =     0, .speed_scale = 0x100 },
  { 8, 0, 0 },
  { .dir = 3, .angle = 0x200, .speed_scale = 0x147 },
  { 8, 0, 0 },
  { .dir = 6, .angle = 0xC00, .speed_scale = 0x100 },
  { .dir = 7, .angle = 0xA00, .speed_scale = 0x147 },
  { 8, 0, 0 },
  { 8, 0, 0 },
  { .dir = 5, .angle = 0xE00, .speed_scale = 0x147 },
  { 8, 0, 0 },
  { 8, 0, 0 },
  { 8, 0, 0 }
};
const gool_accel_state accel_states[7] = { /* 80052C24 */
  { .accel =        0, .max_speed = 0x7D000, .unk =    0, .decel =        0 },
  { .accel = 0x271000, .max_speed = 0x96000, .unk = 0x1E, .decel = 0x271000 },
  { .accel = 0x138800, .max_speed = 0x96000, .unk = 0x1E, .decel =  0x9C400 },
  { .accel = 0x190000, .max_speed = 0xAAE60, .unk =  0xF, .decel = 0x190000 },
  { .accel = 0x271000, .max_speed = 0xC8000, .unk = 0x1E, .decel = 0x271000 },
  { .accel = 0x1A0AAA, .max_speed = 0x64000, .unk = 0x1E, .decel = 0x271000 },
  { .accel =  0xD0555, .max_speed = 0x64000, .unk = 0x1E, .decel =  0x9C400 }
};

/* .sdata */
#ifdef PSX
#include "psx/r3000a.h"
extern scratch scratch;
gool_const_buf in_consts  =
  { .buf = scratch.consts, .idx = 0 }; /* 80056480; gp[0x21] */
gool_const_buf out_consts =
  { .buf = scratch.consts, .idx = 0 }; /* 80056488; gp[0x23] */
#else
uint32_t consts[2];
gool_const_buf in_consts  =
  { .buf = consts, .idx = 0 };     /* 80056480; gp[0x21] */
gool_const_buf out_consts =
  { .buf = consts, .idx = 0 };     /* 80056488; gp[0x23] */
#endif
/* .sbss */
eid_t crash_eid    = 0;            /* 800566B0; gp[0xAD] */
gool_object *crash = 0;            /* 800566B4; gp[0xAE] */
/* .bss */
uint16_t level_spawns[GOOL_LEVEL_SPAWN_COUNT]; /* 8005E348 */
uint32_t spawns[GOOL_SPAWN_COUNT]; /* 8005FF58 */
gool_object *objects;              /* 80060DB0 */
gool_object *player;               /* 80060DB4 */
gool_handle handles[8];            /* 80060DB8 */
gool_handle free_objects;          /* 80060DF8 */
gool_object *cur_obj;              /* 80060E00 */
uint32_t frames_elapsed;           /* 80060E04 */
gool_bound object_bounds[28];      /* 80060E08 */
int object_bound_count;            /* 80061888 */
gool_globals globals;              /* 8006188C */

#ifdef PSX
#include "psx/gpu.h"
extern gfx_context_db context;
#else
#include "pc/gfx/gl.h"
#include "pc/gfx/soft.h"
extern gl_context context;
extern sw_transform_struct params;
#endif
extern int draw_count;

extern ns_struct ns;
extern pad pads[2];
extern lid_t next_lid;
extern entry *cur_zone;
extern entry *obj_zone;
extern zone_query cur_zone_query;
extern level_state savestate;
extern vec v_zero;
extern vec2 screen;
extern uint32_t screen_proj;
extern mat16 mn_trans;
extern vec cam_trans, cam_trans_prev;
extern mat16 ms_rot, ms_cam_rot;
extern int32_t cam_rot_xz;
extern quad28_t uv_map[600];

//----- (8001AAD8) --------------------------------------------------------
int GoolInitAllocTable() {
  gool_handle *handle; // $v1
  gool_object *obj;
  int i; // $a0 MAPDST

  crash   = 0;
  player  = (gool_object*)malloc(sizeof(gool_object)+0x100);
  objects = (gool_object*)malloc(sizeof(gool_object)*GOOL_OBJECT_COUNT);
  if (!objects || !player) { return ERROR_MALLOC_FAILED; }
  cur_obj = 0;
  frames_elapsed = 0;
  free_objects.type = 2;
  free_objects.children = objects;
  for (i=0;i<GOOL_OBJECT_COUNT;i++) {
    obj = &objects[i];
    obj->handle.type = 0;
    obj->parent = (gool_object*)&free_objects;
    obj->children = 0;
    if (i < (GOOL_OBJECT_COUNT-1))
      obj->sibling = &objects[i+1];
    else
      obj->sibling = 0;
  }
  handle=handles;
  for (i=0;i<8;i++,handle++) {
    handle->type = 2;
    handle->children = 0;
  }
  player->handle.type = 0;
  player->parent = 0;
  player->sibling = 0;
  player->children = 0;
  return SUCCESS;
}

//----- (8001AC28) --------------------------------------------------------
int GoolKillAllocTable() {
  free(objects);
  free(player);
  return SUCCESS;
}

//----- (8001AC60) --------------------------------------------------------
int GoolInitLid() {
  cur_lid_ro = ns.ldat->lid << 8;  /* TODO: verify this global, @ 0x8006188C*/
  if (ns.ldat->magic)
    return SUCCESS;
  else
    return ERROR;
}

//----- (8001AC9C) --------------------------------------------------------
int GoolObjectOrientOnPath(gool_object *obj, int progress, vec *loc) {
  zone_rect *rect; // $t0
  zone_entity *entity; // $s5
  int type, idx, path_length; // $a0, $s0, $v1
  int scale, fractional, idx_next;
  uint32_t status_a, status_b;
  vec *trans, *zone_loc, loc_next, dist_obj, dot, dir;
  zone_entity_path_point *path_pt, *next_pt; // $a0
  uint32_t dist_xz;
  int32_t x, z, proj, proj_part;

  entity = obj->entity;
  if (!obj || !entity) { return ERROR; }
  type = entity->parent_zone->type;
  if (type == 7) {
    rect = (zone_rect*)entity->parent_zone->items[1];
    zone_loc = (vec*)&rect->x;
    scale = 2;
  }
  else if (type == 17) { /* mdat entity */
    zone_loc = &v_zero;
    scale = 0;
  }
  path_length = entity->path_length;
  idx = abs(progress) >> 8;
  fractional = progress & 0xFF;
  path_pt = &entity->path_points[idx];
  if ((idx == (path_length - 1)) && idx) { /* at last point and more than 1 point? */
    fractional = (progress & 0xFF) + 0x100;
    path_pt = &entity->path_points[path_length-2];
  }
  loc->x = ((path_pt->x << scale) + zone_loc->x) << 8;
  loc->y = ((path_pt->y << scale) + zone_loc->y) << 8;
  loc->z = ((path_pt->z << scale) + zone_loc->z) << 8;
  next_pt = path_pt + 1;
  if (path_length == 1)
    return 1;
  loc_next.x = ((next_pt->x << scale) + zone_loc->x) << 8;
  loc_next.y = ((next_pt->y << scale) + zone_loc->y) << 8;
  loc_next.z = ((next_pt->z << scale) + zone_loc->z) << 8;
  dir.x = loc_next.x - loc->x;
  dir.y = loc_next.y - loc->y;
  dir.z = loc_next.z - loc->z;
  trans = &obj->trans;
  if (obj->status_b & GOOL_FLAG_ORIENT_ON_PATH) {
    /* let vector A be a vector
       starting at the object's current location
       and ending at/pointing towards the path point corresponding to progress
       let vector B be the direction of the path
       at that path point
       find the projection of A onto B */
    /* get ||B|| */
    dist_xz = sqrt((dir.x>>8)*(dir.x>>8)+((dir.z>>8)*(dir.z>>8)));
    if (dist_xz == 0) { return ERROR; }
    /* compute A */
    dist_obj.x = trans->x-loc->x;
    dist_obj.z = trans->z-loc->z;
    /* compute the components which contribute to the sum of A . B */
    dot.x = (dist_obj.x>>4)*(dir.x>>4);
    dot.z = (dist_obj.z>>4)*(dir.z>>4);
    /* compute the scalar projection: (A . B) / ||B||^2 */
    proj = (dot.x+dot.z)/((int32_t)dist_xz*(int32_t)dist_xz);
    if (proj >= 0x100) { /* too far of a corresponding change in progress? */
      idx_next = idx+1;
      if (idx < path_length-1)
        return GoolObjectOrientOnPath(obj, idx_next<<8, loc); /* see if next pt is closer */
    }
    /* recompute (A . B) / ||B||;
       [compute the vector projection in the order:
       ((A . B) / ||B||) x (B / ||B||)
       to avoid precision errors] */
    proj_part = ((dot.x + dot.z)/(int32_t)dist_xz);
    x = abs((((proj_part>>4)*(dir.x>>4))/(int32_t)dist_xz) - dist_obj.x);
    z = abs((((proj_part>>4)*(dir.z>>4))/(int32_t)dist_xz) - dist_obj.z);
    /* do a sort of manhattan distance calc
       is this moment of inertia? */
    if (z<x)
      obj->misc_c.y = x + z/2;
    else
      obj->misc_c.y = z + x/2;
    if (obj->misc_c.y > obj->_154 || proj >= 0x100 || idx == 0) {
      obj->status_a |= 0x200;
    }
    if (dir.z*(trans->x>>8)-dir.x*(trans->z>>8)
      -(loc->x>>8)*loc_next.z+(loc->z>>8)*loc_next.x < 0) /* cross product */
      obj->misc_c.y = -obj->misc_c.y;
  }
  status_a = obj->status_a;
  if (obj->path_progress >= 0 && idx < entity->path_length-1)
    obj->status_a &= ~GOOL_FLAG_TOWARD_GOAL;
  else
    obj->status_a |= GOOL_FLAG_TOWARD_GOAL;
  if ((status_a & GOOL_FLAG_TOWARD_GOAL) == (obj->status_a & GOOL_FLAG_TOWARD_GOAL)
   || (obj->status_a & GOOL_FLAG_CHANGE_PATH_DIR))
    obj->status_a &= ~GOOL_FLAG_CHANGE_PATH_DIR;
  else
    obj->status_a |= GOOL_FLAG_CHANGE_PATH_DIR;
  status_a = obj->status_a;
  status_b = obj->status_b;
  if (status_b & GOOL_FLAG_TRACK_PATH_SIGN) { /* moving downward */
    if ((status_b & GOOL_FLAG_TRACK_PATH_ROT) && (status_a & GOOL_FLAG_TOWARD_GOAL))
      obj->target_rot.x = atan2(-dir.x, -dir.z);
    else
      obj->target_rot.x = atan2(dir.x, dir.z);
  }
  if (status_b & 0x800) {
    if (abs(dir.z)<abs(dir.x))
      z = abs(dir.x)+(abs(dir.z))/2;
    else
      z = abs(dir.z)+(abs(dir.x))/2;
    if (status_b & GOOL_FLAG_TRACK_PATH_SIGN) {
      obj->rot.z = atan2(dir.x, dir.z);
      obj->target_rot.y = -atan2(dir.y, z);
    }
    else {
      obj->rot.z = atan2(-dir.x, -dir.z);
      obj->target_rot.y = atan2(dir.y, z);
    }
  }
  /* use fractional part of progress to interpolate between current and next point */
  loc->x += (dir.x*fractional)>>8;
  loc->y += (dir.y*fractional)>>8;
  loc->z += (dir.z*fractional)>>8;
  return path_length;
}

//----- (8001B3F0) --------------------------------------------------------
void GoolInitLevelSpawns(lid_t lid) {
  int idx;
  uint16_t *level_spawn;
  level_spawn = level_spawns;
  for (;*level_spawn;level_spawn++) {
    if (lid == (*level_spawn >> 9)) {
      idx = *level_spawn & 0x1FF;
      spawns[idx] |= 8;
    }
  }
}

//----- (8001B648) --------------------------------------------------------
int GoolObjectTraverseTreePreorder(
  gool_object *obj,
  int (*func)(gool_object*,int),
  int arg) {
  gool_object *child, *sibling; // $a0, @s0
  int res; // $v0 MAPDST

  res = func(obj, arg);
  if (res == ERROR_INVALID_RETURN) {
    GoolObjectKill(obj, 0);
    return ERROR_INVALID_RETURN;
  }
  child = obj->children;
  while (child) {
    if (ISFREEOBJECT(child)) { break; }
    sibling = child->sibling;
    GoolObjectTraverseTreePreorder(child, func, arg);
    child = sibling;
  }
  return res;
}

//----- (8001B6F0) --------------------------------------------------------
int GoolObjectTraverseTreePostorder(
  gool_object *obj,
  int (*func)(gool_object*,int),
  int arg) {
  gool_object *child; // $a0
  gool_object *sibling; // $s0
  int res; // $v0

  child = obj->children;
  while (child) {
    sibling = child->sibling;
    res = GoolObjectTraverseTreePostorder(child, func, arg);
    child = sibling;
    if (res == ERROR_INVALID_RETURN)
      GoolObjectKill(obj, 0);
  }
  return func(obj, arg);
}

//----- (8001B788) --------------------------------------------------------
int GoolObjectSearchTree(
  gool_object *obj,
  int (*func)(gool_object*,int),
  int arg) {
  // int prev_result; // $v1
  gool_object *child, *sibling; // $a0, $s0
  int res; // $v0

  if (!obj) { return 0; }
  if (!ISHANDLE(obj)) {
    res = func(obj, arg);
    if (res) { return res; }
  }
  child = GoolObjectGetChildren(obj);
  if (!child) { return 0; }
  while (!res && child) {
    sibling = child->sibling;
    res = GoolObjectSearchTree(child, func, arg);
    child = sibling;
  }
  return res;
}

//----- (8001B84C) --------------------------------------------------------
int GoolObjectHandleTraverseTreePreorder(
  gool_object *obj,
  int (*func)(gool_object*,int),
  int arg) {
  gool_object *child, *sibling; // $s0, $s1
  gool_object *grandchild, *next_grandchild; // %a0, $s0
  int res; // $v0

  child = GoolObjectGetChildren(obj);
  while (child) {
    sibling = child->sibling;
    res = func(child, arg);
    if (res == ERROR_INVALID_RETURN)
      GoolObjectKill(child, 0);
    else {
      grandchild = child->children;
      while (grandchild) {
        if (ISFREEOBJECT(grandchild))
          break;
        next_grandchild = grandchild->sibling;
        GoolObjectTraverseTreePreorder(grandchild, func, arg);
        grandchild = next_grandchild;
      }
    }
    child = sibling;
  }
  return SUCCESS;
}

//----- (8001B92C) --------------------------------------------------------
int GoolObjectHandleTraverseTreePostorder(
  gool_object *obj,
  int (*func)(gool_object*,int),
  int arg) {
  gool_object *child, *sibling; // $s0, $s5
  gool_object *grandchild, *next_grandchild; // %a0, $s1
  int res; // $v0 MAPDST

  child = GoolObjectGetChildren(obj);
  while (child) {
    grandchild = child->children;
    sibling = child->sibling;
    /* TODO: verify-should there be an else block somewhere here */
    while (grandchild) {
      next_grandchild = grandchild->sibling;
      res = GoolObjectTraverseTreePostorder(grandchild, func, arg);
      if (res == ERROR_INVALID_RETURN)
        GoolObjectKill(child, 0);
      grandchild = next_grandchild;
    }
    res = func(child, arg);
    if (res == ERROR_INVALID_RETURN)
      GoolObjectKill(obj, 0);
    child = sibling;
  }
  return SUCCESS;
}

//----- (8001BA18) --------------------------------------------------------
void GoolForEachObjectHandle(
  int (*func)(gool_object*,int,int),
  int arg1,
  int arg2) {
  int i;
  gool_handle *handle;
  for (i=0;i<8;i++) {
    handle = &handles[i];
    func((gool_object*)handle,arg1,arg2);
  }
}

//----- (8001BA90) --------------------------------------------------------
gool_object *GoolObjectHasPidFlags(gool_object *obj, uint32_t pid_flags) {
  if (obj->pid_flags == pid_flags)
    return obj;
  return 0;
}

//----- (8001BAB0) --------------------------------------------------------
int GoolFindNearestObject(gool_object *obj, gool_nearest_query *query) {
  gool_header *header;
  gool_state *states, *state;
  gool_state_maps *maps;
  int category, state_idx, flag;
  uint32_t dist, arg;

  if (obj == query->obj) { return 0; }
  header = (gool_header*)obj->global->items[0];
  category = header->category >> 8;
  if (!((1 << category) & query->categories)) { return 0; }
  dist = ApxDist(&obj->trans, &query->obj->trans);
  if (dist >= query->dist) { return 0; } /* return if farther than current min dist */
  if (query->event == 0xFF) { /* null event? */
    query->nearest_obj = obj; /* set as new nearest obj */
    query->dist = dist; /* set as new min dist */
    return 0; /* return */
  }
  maps = (gool_state_maps*)obj->global->items[3];
  state_idx = maps->event_map[query->event >> 8];
  if (state_idx == 0xFF) { /* null state index? */
    if (query->event != GOOL_EVENT_HIT /* not a hit a event? */
      || obj->invincibility_state == 2
      || obj->invincibility_state == 3
      || obj->invincibility_state == 4
      || obj->status_c & 2) {
      if ((query->event == GOOL_EVENT_HIT_INVINCIBLE && !(obj->state_flags & 0x800))
        || query->event == GOOL_EVENT_WIN_BOSS) { /* hit when invincible or win boss? */
        query->nearest_obj = obj; /* set as new nearest obj */
        query->dist = dist; /* set as new min dist */
      }
    }
    return 0;
  }
  if (state_idx & 0x8000) {
    if (query->event == GOOL_EVENT_STATUS) { /* querying for status? */
      arg = 0x100;
      obj->interrupter = query->obj;
      GoolObjectInterrupt(obj, state_idx, 1, &arg); /* interrupt the object */
      if (obj->ack) { /* request acknowledged? */
        query->nearest_obj = obj; /* set as new nearest obj */
        query->dist = dist; /* set as new min dist */
      }
    }
    else { /* not querying for status */
      query->nearest_obj = obj; /* set as new nearest obj */
      query->dist = dist; /* set as new min dist */
    }
    return 0;
  }
  states = (gool_state*)obj->global->items[4];
  state = &states[state_idx];
  if (obj->invincibility_state >= 2 && obj->invincibility_state <= 4)
    flag = (obj->status_c | 0x1002) & state->flags;
  else
    flag = obj->status_c & state->flags;
  if (!flag) { /* status_c has state flag set? */
    query->nearest_obj = obj; /* set as new nearest obj */
    query->dist = dist; /* set as new min dist */
  }
  return 0;
}

//----- (8001BCA4) --------------------------------------------------------
gool_object *GoolObjectTestStateFlags(gool_object *obj, int flags) {
  if (obj->state_flags & flags)
    return obj;
  return 0;
}

// does not exist on its own in assembly
static inline gool_object *GoolObjectAlloc(int flag) {
  /* TODO: should retval be an out param? */
  gool_object *free_obj, *target, *found, *cur;
  gool_handle *handle;

  while (1) {
    free_obj = GoolObjectGetChildren((gool_object*)&free_objects);
    if (free_obj) /* free object available? */
      return free_obj; /* return it */
    else if (flag) { /* term flag set? (and no free objects available) */
      handle = &handles[3];
      if (ISHANDLE(handle))
        target = handle->children;
      else if (GoolObjectTestStateFlags((gool_object*)handle, 0x80000))
        target = GoolObjectGetChildren(handle);
      for (cur=target;cur&&!found;cur=cur->sibling)
        found = (gool_object*)GoolObjectSearchTree(
          cur,
          (gool_ifnptr_t)GoolObjectTestStateFlags,
          0x80000);
      if (!found) /* no objects available to steal from? */
        return (gool_object*)ERROR_OBJECT_POOL_FULL; /* no free objects available */
      GoolObjectTerminate(found, 0); /* terminate/free up the existing object */
    }
    else
      return (gool_object*)ERROR_OBJECT_POOL_FULL; /* no free objects available */
  }
}

//----- (8001BCC8) --------------------------------------------------------
gool_object *GoolObjectSpawn(entry *zone, int entity_idx) {
  gool_object *obj, *near_box; /* near_box = $s3 */
  gool_handle *handle;
  gool_colors *colors;
  zone_header *header;
  zone_entity *entity;
  gool_header *g_header;
  entry *exec;
  uint32_t id, spawn;
  int i, is_main, count, idx, res;

  if (!zone) { return (gool_object*)ERROR; };
  if (zone->type == 7) { /* zdat? */
    header = (zone_header*)zone->items[0];
    idx = header->paths_idx + header->path_count + entity_idx;
    entity = (zone_entity*)zone->items[idx];
  }
  else if (zone->type == 17) /* mdat? */
    entity = (zone_entity*)zone->items[2+entity_idx];
  else
    entity = (zone_entity*)zone->items[entity_idx];
  is_main = entity->group == 3 && (entity->type == 0
             || (entity->id > 0 && entity->id < 5)
             || (entity->type == 0x2C && entity->subtype == 0)
             || entity->type == 0x30);
  // if (!is_main) { return ERROR; }
  if (is_main && main_obj) /* trying to spawn the main obj (crash, etc.) but already spawned? */
    return (gool_object*)ERROR; /* return error */
  if (entity->type == 0x22) { /* is box type? */
    if (prev_box_entity
      && absdiff(entity->loc.x, prev_box_entity->loc.x) < 10
      && absdiff(entity->loc.z, prev_box_entity->loc.z) < 10
      && absdiff(entity->loc.y, prev_box_entity->loc.y + 100) < 10) /* last box entity within range? */
      near_box = prev_box; /* last box is nearest */
    else {
      boxes_y = 0x19000;
      prev_box = 0;
      near_box = 0;
    }
    prev_box_entity = entity; /* set for next call */
  }
  id = entity->id;
  spawn = spawns[id];
  if ((spawn & 1) || (spawn & 2)) { /* already spawned? */
    if (entity->type == 0x22)
      boxes_y += 0x19000; /* still increment if box type */
    return (gool_object*)ERROR; /* return error */
  }
  if (is_main) {
    main_obj = init_obj;
    obj = main_obj;
    handle = &handles[6];
  }
  else {
    obj = GoolObjectAlloc(1);
    if (ISERRORCODE(obj))
      return obj;
    handle = &handles[3];
  }
  GoolObjectAddChild((gool_object*)handle, obj);
  obj->handle.subtype = entity->group;
  res = GoolObjectInit(obj, entity->type, entity->subtype, 0, 0);
  if (ISERRORCODE(res))
    GoolObjectAddChild((gool_object*)&free_objects, obj);
  spawns[id] |= 1; /* set 'spawned' bit */
  obj->pid_flags = id << 8; /* TODO: make bitfield in struct */
  if (zone && zone->type == 17) { zone = cur_zone; }
  obj->zone = zone;
  header = (zone_header*)zone->items[0];
  colors = is_main ? &header->player_colors:
                     &header->object_colors;
  obj->colors = *colors;
  obj->entity = entity;
  obj->path_length = entity->path_length << 8;
  if (!(entity->spawn_flags & 1)) { /* has rot initializer? */
    obj->rot.y = entity->rot.y;
    obj->rot.x = entity->rot.x;
    obj->rot.z = entity->rot.z;
  }
  obj->mode_flags_a = ((int32_t)entity->init_flags_a) << 8;
  obj->mode_flags_b = ((int32_t)entity->init_flags_b) << 8;
  obj->mode_flags_c = ((int32_t)entity->init_flags_c) << 8;
  GoolObjectOrientOnPath(obj, 0, &obj->trans);
  if (obj == crash && next_lid == -1)
    LevelSaveState(obj, &savestate, 1); /* save state when spawning crash */
  exec = obj->global;
  g_header = (gool_header*)exec->items[0];
  if (g_header->category == 0x300) /* is an enemy? */
    GoolObjectAddChild((gool_object*)&handles[4], obj); /* add to enemy list */
  if (g_header->type == 0x22) { /* box type entity? */
    if (near_box) { /* is there a nearby box? */
      /* append cur to doubly linked box list with prev as its tail */
      obj->box_link.prev = near_box;
      obj->box_link.next = 0;
      near_box->box_link.next = obj;
    }
    else {
      obj->box_link.prev = 0;
      obj->box_link.next = 0;
    }
    obj->trans.y += (0x19000 - boxes_y);
    prev_box = obj;
    if (!(header->flags & 4)) { /* location dependent counter? */
      count = ((obj->trans.z >> 4) ^ obj->trans.x) & 7;
      obj->anim_counter = count;
      if (count) /* nonzero count? */
        obj->status_b |= GOOL_FLAG_STALL;
    }
  }
  return obj;
}

//----- (8001C6C8) --------------------------------------------------------
gool_object *GoolObjectCreate(void *parent, int exec, int subtype, int argc, uint32_t *argv, int flag) {
  gool_object *_parent, *child;
  entry *zone;
  zone_header *header;

  _parent = (gool_object*)parent;
  if (!exec && !subtype) {
    crash = player;
    child = crash;
  }
  else {
    child = GoolObjectAlloc(flag);
    if (ISERRORCODE(child))
      return child;
  }
  GoolObjectAddChild(_parent, child);
  child->handle.subtype = 3;
  GoolObjectInit(child, exec, subtype, argc, argv);
  zone = child->zone ? child->zone : cur_zone;
  header = (zone_header*)zone->items[0];
  child->colors = child == crash ? header->player_colors :
                                   header->object_colors;
  return child;
}

//----- (8001CB80) --------------------------------------------------------
int GoolObjectInit(gool_object *obj, int exec, int subtype, int argc, int *argv) {
  gool_object *parent; // $a0 MAPDST
  gool_vectors *vectors; // $a3
  gool_header *header;
  gool_state_maps *maps;
  entry *global;
  eid_t *p_eid;
  int idx, idx_states, state, res; // ?, ?, $a1

  parent = obj->parent;
  obj->node = 0xFFFF;
  obj->pid_flags = 0;
  obj->entity = 0;
  obj->path_progress = 0;
  obj->path_length = 0;
  obj->handle.type = 1;
  obj->misc_a.x = 0;
  obj->misc_a.y = 0;
  obj->misc_a.z = 0;
  obj->misc_b.y = 0;
  obj->misc_b.x = 0;
  obj->misc_b.z = 0;
  obj->misc_c.x = 0;
  obj->misc_c.y = 0;
  obj->misc_c.z = 0;
  obj->speed = 0;
  obj->anim_frame = 0;
  obj->ack = 0;
  obj->misc_b.x = 0;
  obj->status_a = 0;
  obj->status_c = 0;
  obj->status_b = 0;
  obj->invincibility_state = 0;
  obj->size = 0;
  obj->floor_impact_stamp = 0;
  obj->hotspot_size = 0;
  obj->voice_id = -2;
  if (parent && parent->handle.type == 1) {
    obj->zone = parent->zone;
    obj->trans = parent->trans;
    obj->rot = parent->rot;
    obj->scale = parent->scale;
  }
  else {
    vectors = &obj->vectors;
    obj->zone = 0;
    obj->rot.y = 0;
    obj->rot.x = 0;
    obj->rot.z = 0;
    obj->scale.x = 0x1000;
    obj->scale.y = 0x1000;
    obj->scale.z = 0x1000;
  }
  p_eid = &ns.ldat->exec_map[exec];
  if (exec == 4 || exec == 5 || exec == 29) { obj->zone = 0; }
  if (exec == 0)                            { obj->cam_zoom = 0; }
  if (obj->handle.subtype != 3 || *p_eid == EID_NONE || exec >= 64) { return ERROR; }
  global = NSOpen(p_eid, 1, 0);
  obj->global = global;
  if (ISERRORCODE(global)) { return ERROR; }
  obj->self = obj;
  obj->collider = 0;
  obj->creator = 0;
  obj->interrupter = 0;
  global = obj->global;
  obj->subtype = subtype;
  obj->anim_seq = 0;
  obj->once_p = 0;
  obj->player = player;
  header = (gool_header*)obj->global->items[0];
  maps = (gool_state_maps*)obj->global->items[3];
  idx_states = header->subtype_map_idx;
  state = maps->subtype_map[idx_states+subtype];
  if (state == 0xFF)
    return ERROR_INVALID_STATE;
  res = GoolObjectChangeState(obj, state, argc, argv);
  if (ISERRORCODE(res))
    return ERROR;
  return SUCCESS;
}

//----- (8001CDD0) --------------------------------------------------------
int GoolObjectKill(gool_object *obj, int sig) {
  gool_object *child, *next, *children;
  int idx;

  /* when the object is crash, we do not send the term event
   * except for at the title, when the main obj (i.e. crash) is used for
   * GamOC (title card state); the previous instance needs to be
   * terminated (and recreated for the next state) for each change in
   * title state */
  if (!obj || (obj == crash && ns.ldat->lid != LID_TITLE))
    return SUCCESS;
  if (sig)
    GoolSendEvent(0, obj, GOOL_EVENT_TERMINATE, 0, 0);
  child = GoolObjectGetChildren(obj);
  while (child) { /* terminate descendants */
    next = child->sibling;
    GoolObjectKill(child, sig);
    child = next;
  }
  AudioVoiceFree(obj);
  idx = obj->pid_flags >> 8;
  spawns[idx] &= 0xFFFFFFFE; /* clear bit 1 */
  obj->handle.type = 0;
  if (obj == crash) {
    children = GoolObjectGetChildren(&free_objects);
    GoolObjectAddChild((gool_object*)&free_objects, obj);
    GoolObjectSetChildren(&free_objects, children);
    player->parent = 0;
    player->children = 0;
    player->sibling = 0;
    crash = 0;
  }
  else
    GoolObjectAddChild((gool_object*)&free_objects, obj); /* return to free list */
  return SUCCESS;
}

//----- (8001D11C) --------------------------------------------------------
int GoolZoneObjectTerminate(gool_object *obj, entry *zone) {
  if (obj->handle.type == 0 || obj->zone != zone
   || ((obj->status_b & 0x1000000) && (obj->state_flags & 0x40000)))
    return SUCCESS;
  GoolSendEvent(0, obj, GOOL_EVENT_TERMINATE, 0, 0);
  if (obj->zone == zone || (int)obj_zone == -1)
    return GoolObjectKill(obj, 0);
  return SUCCESS;
}

//----- (8001D1E0) --------------------------------------------------------
int GoolObjectTerminate(gool_object *obj, int ignored) {
  return GoolObjectKill(obj, 1);
}

//----- (8001D200) --------------------------------------------------------
void GoolZoneObjectsTerminate(entry *zone) {
  int i;
  gool_handle *handle;
  for (i=0;i<8;i++) {
    handle = &handles[i];
    GoolObjectHandleTraverseTreePostorder(
      (gool_object*)handle,
      (gool_ifnptr_t)GoolZoneObjectTerminate,
      (int)zone);
  }
}

//----- (8001D624) --------------------------------------------------------
// unused code
void GoolZoneNeighborObjectsTerminate() {
  zone_header *header;
  gool_handle *handle;
  entry *neighbor;
  int i;

  if (!cur_zone) { return; }
  header = (zone_header*)cur_zone->items[0];
  for (i=0;i<header->neighbor_count;i++) {
    neighbor = NSLookup(&header->neighbors[i]);
    GoolZoneObjectsTerminate(neighbor);
  }
}

//----- (8001D33C) --------------------------------------------------------
void GoolObjectLocalBound(gool_object *obj, vec *scale) {
  entry *svtx;
  gool_anim *anim;
  svtx_frame *frame;
  vec scale_abs;
  bound bnd;
  int res, frame_idx;

  if (!obj->anim_seq) { return; }
  anim = obj->anim_seq;
  /* below section unnecessary on pc and causes collision issues */
#ifdef PSX
  if (anim->type != 4) {
    if ((anim->eid & 0xFF800003) != 0x80000000) { return; } /* has the eid not been resolved? i.e. is it a non-pointer? */
    res = (int)NSLookup(&anim->eid);
    if (ISERRORCODE(res)) { return; } /* return if unable to resolve */
  }
#endif
  scale_abs = *scale;
  scale_abs.x = abs(scale_abs.x); /* don't flip in x dir for negative values */
  if (anim->type == 1) { /* vertex anim? */
    svtx = NSLookup(&anim->v.eid);
    frame_idx = obj->anim_frame >> 8;
    frame = (svtx_frame*)svtx->items[frame_idx];
    bnd = *(bound*)&frame->bound_x1;
    if (obj == crash) { /* is crash object? */
      bnd.p1.x += frame->col_x; /* readjust bound box center for collision purposes */
      bnd.p1.y += frame->col_y;
      bnd.p1.z += frame->col_z;
      bnd.p2.x += frame->col_x;
      bnd.p2.y += frame->col_y;
      bnd.p2.z += frame->col_z;
    }
    obj->bound.p1.x = ((bnd.p1.x >> 8) * scale_abs.x) >> 4;
    obj->bound.p1.y = ((bnd.p1.y >> 8) * scale_abs.y) >> 4;
    obj->bound.p1.z = ((bnd.p1.z >> 8) * scale_abs.z) >> 4;
    obj->bound.p2.x = ((bnd.p2.x >> 8) * scale_abs.x) >> 4;
    obj->bound.p2.y = ((bnd.p2.y >> 8) * scale_abs.y) >> 4;
    obj->bound.p2.z = ((bnd.p2.z >> 8) * scale_abs.z) >> 4;
  }
  else { /* bounding box is a 25x25x25 box */
    obj->bound.p1.x = -((scale_abs.x * 200) >> 4);
    obj->bound.p1.y = -((scale_abs.y * 200) >> 4);
    obj->bound.p1.z = -((scale_abs.z * 200) >> 4);
    obj->bound.p2.x = (scale_abs.x * 200) >> 4;
    obj->bound.p2.y = (scale_abs.y * 200) >> 4;
    obj->bound.p2.z = (scale_abs.z * 200) >> 4;
  }
  obj->scale = *scale;
}

//----- (8001D5EC) --------------------------------------------------------
void GoolUpdateObjects(int flag) {
  gool_handle *handle;
  int i;

  object_bound_count = 0;
  draw_count_ro = draw_count;
#ifdef PSX
  frames_elapsed = context.c2_p->draw_stamp / 34;
#else
  frames_elapsed = context.draw_stamp / 34;
#endif
  if (!crash) { return; }
  for (i=0;i<8;i++) {
    handle = &handles[i];
    GoolObjectHandleTraverseTreePreorder(
      OBJECT(handle),
      GoolObjectUpdate,
      flag);
  }
}

//----- (8001D698) --------------------------------------------------------
int GoolObjectChangeState(gool_object *obj, uint32_t state, int argc, uint32_t *argv) {
  entry *exec, *external; // $v0
  gool_header *header;
  gool_state *state_descs, *state_desc; // ?, $s1
  gool_state_ref transition;
  uint32_t *code, *data;
  uint32_t flags;
  uint32_t *once_p; // $s3
  int i, res; // ?, $v0

  once_p = obj->once_p;
  if (state == 0xFF || (obj->status_b & GOOL_FLAG_STALL))
    return ERROR_INVALID_STATE;
  obj->state = state;
  exec = obj->global;
  state_descs = (gool_state*)exec->items[4];
  state_desc = &state_descs[state];
  obj->status_c = state_desc->status_c;
  data = (uint32_t*)exec->items[2];
  external = NSLookup(&data[state_desc->extern_idx]);
  obj->external = external;
  code = (uint32_t*)external->items[1];
  if (state_desc->pc_code != 0x3FFF)
    obj->pc = &code[state_desc->pc_code];
  else
    obj->pc = 0;
  if (state_desc->pc_event != 0x3FFF)
    obj->ep = &code[state_desc->pc_event];
  else
    obj->ep = 0;
  if (state_desc->pc_trans != 0x3FFF)
    obj->tp = &code[state_desc->pc_trans];
  else
    obj->tp = 0;
  header = (gool_header*)exec->items[0];
  obj->fp = (uint32_t*)&obj->process;
  obj->sp = (uint32_t*)&obj->process + header->init_sp;
  obj->status_a |= GOOL_FLAG_FIRST_FRAME | GOOL_FLAG_KEEP_EVENT_STACK;
  obj->state_flags = state_desc->flags;
  for (i=0;i<argc;i++)
    GoolObjectPush(obj, argv[i]);
  GoolObjectPushFrame(obj, argc, 0xFFFF);
  if (once_p) { /* is there a once block? */
    GoolObjectPushFrame(obj, 0, 0xFFFF);
    obj->pc = once_p;
    flags = GOOL_FLAG_SUSPEND_ON_RET | GOOL_FLAG_SUSPEND_ON_RETLNK
          | GOOL_FLAG_STATUS_PRESERVE;
    res = GoolObjectInterpret(obj, flags, &transition);
    if (ISERRORCODE(res))
      return res;
  }
  else
    GoolObjectPush(obj, 0);
  obj->state_stamp = frames_elapsed;
  if (obj->tp && obj == cur_obj) {
    GoolObjectPushFrame(obj, 0, 0xFFFF);
    obj->pc = obj->tp;
    flags = GOOL_FLAG_SUSPEND_ON_RET | GOOL_FLAG_SUSPEND_ON_RETLNK;
    res = GoolObjectInterpret(obj, flags, &transition);
    if (ISERRORCODE(res))
      return res;
  }
  return SUCCESS;
}

//----- (8001D914) --------------------------------------------------------
int GoolObjectPushFrame(gool_object *obj, int argc, uint32_t flags) {
  uint16_t rsp, rfp;
  rsp = GETRSP(obj)-(argc*4);
  rfp = GETRFP(obj);
  obj->fp = obj->sp;
  GoolObjectPush(obj, flags);
  GoolObjectPush(obj, (uint32_t)obj->pc);
  GoolObjectPush(obj, (rfp << 16) | rsp);
  return SUCCESS;
}

//----- (8001D97C) --------------------------------------------------------
// unreferenced; exists as inline code elsewhere
static int GoolObjectPopFrame(gool_object *obj, uint32_t *flags) {
  uint32_t range;
  uint16_t rsp, rfp;
  uint16_t flags_prev;

  obj->sp = obj->fp + 2;
  range = GoolObjectPeek(obj);
  rsp=range&0xFFFF;rfp=range>>16;
  if (!rfp) /* initial stack frame? */
    return ERROR_INVALID_RETURN; /* return error */
  // GoolObjectPop(obj); /* pop range */
  // obj->sp = obj->fp + ;
  obj->pc = (uint32_t*)GoolObjectPop(obj); /* pop and restore pc to its previous location */
  if (flags) { /* out var passed for flags? */
    flags_prev = GoolObjectPop(obj);
    *flags = (*flags & 0xFFFF0000) | flags_prev; /* restore previous value */
  }
  obj->sp = (uint32_t*)((uint8_t*)&obj->process + rsp);
  obj->fp = (uint32_t*)((uint8_t*)&obj->process + rfp);
  return SUCCESS;
}

//----- (8001DA0C) --------------------------------------------------------
int GoolObjectUpdate(gool_object *obj, int flag) {
  gool_header *header;
  gool_state_ref response;
  uint32_t status_b, category, flags;
  uint32_t top, timestamp, wait;
  int animate, display;
  int elapsed_since;
  int res;

  if (obj->handle.subtype != 3) { return SUCCESS; }
  if (obj == crash)
    PadUpdate();
  cur_obj = obj;
  status_b = obj->status_b;
  if ((cur_display_flags & GOOL_FLAG_ANIMATE) == 0)
    animate = 0;
  else if (((status_b & GOOL_FLAG_FORCE_UPDATE) ||
    (obj->state_flags & GOOL_FLAG_MENUTEXT_STATE))
     && (cur_display_flags & GOOL_FLAG_FORCE_ANIM_MENUS))
    animate = 1;
  else {
    header = (gool_header*)obj->global->items[0];
    category = header->category;
    switch (category) {
    case 0x100: animate = cur_display_flags & GOOL_FLAG_ANIMATE_C1;   break;
    case 0x300: animate = cur_display_flags & GOOL_FLAG_ANIMATE_C356; break;
    case 0x400: animate = cur_display_flags & GOOL_FLAG_ANIMATE_C4;   break;
    case 0x500: animate = cur_display_flags & GOOL_FLAG_ANIMATE_C356; break;
    case 0x600: animate = cur_display_flags & GOOL_FLAG_ANIMATE_C356; break;
    case 0x200:
      animate = cur_display_flags & GOOL_FLAG_ANIMATE_C2;
      if (header->type == 4 && (obj->subtype == 4 || obj->subtype == 7))
        flag = 1; /* override */
      break;
    default:
      animate = 0;
      break;
    }
  }
  if (animate && flag) {
#ifdef CFLAGS_GOOL_DEBUG
    gool_debug *dbg;
    dbg = GoolObjectDebug(obj);
#endif
    if ((status_b & GOOL_FLAG_STALL) && (obj->anim_counter != 0)) {
      obj->anim_counter--; /* decrement while stall flag is set */
      if (obj->anim_counter == 0)
        obj->status_b &= ~GOOL_FLAG_STALL; /* clear stall flag when 0 is reached */
    }
#ifdef CFLAGS_GOOL_DEBUG
    else if ((dbg->flags & GOOL_FLAGS_PAUSED) == GOOL_FLAGS_PAUSED) {}
#endif
    else { /* countdown finished in previous frame */
      obj->anim_stamp = frames_elapsed; /* record 'timestamp' */
      if ((status_b & GOOL_FLAG_COLLIDABLE) && /* collidable object? */
        obj->anim_stamp == crash->anim_stamp) /* same frame as crash? */
        GoolObjectBound(obj); /* calculate bound */
#ifdef CFLAGS_GOOL_DEBUG
      if (dbg->flags & GOOL_FLAG_PAUSED_TRANS) {}
      else if (obj->tp) {
        if (!(dbg->flags & GOOL_FLAG_RESTORED_TRANS)) { /* not resuming from trans block? */
          GoolObjectPushFrame(obj, 0, 0xFFFF);
          obj->pc = obj->tp;
        }
        else
          dbg->flags &= ~GOOL_FLAG_RESTORED_TRANS;
#else
      if (obj->tp) { /* is there a trans block to run? */
        GoolObjectPushFrame(obj, 0, 0xFFFF);
        obj->pc = obj->tp;
#endif
        flags = GOOL_FLAG_SUSPEND_ON_RET | GOOL_FLAG_SUSPEND_ON_RETLNK;
        res = GoolObjectInterpret(obj, flags, &response); /* run it */
        if (ISERRORCODE(res)) { return res; } /* return on fail */
      }
      top = *(obj->sp-1); /* peek back one from top */
      timestamp = top & 0x00FFFFFF;
      wait = (top & 0xFF000000) >> 24;
      elapsed_since = frames_elapsed - timestamp;
#ifdef CFLAGS_GOOL_DEBUG
      if (dbg->flags & GOOL_FLAG_PAUSED_CODE) {}
      else
#endif
      if (elapsed_since >= wait) { /* if the required wait time (or more) has elapsed */
        obj->sp--;                 /* get rid of the now useless tag */
        res = GoolObjectInterpret(obj, GOOL_FLAG_SUSPEND_ON_ANIM, &response); /* ...and continue execution until another animation instruction is reached */
        if (ISERRORCODE(res)) { return res; } /* return on fail */
      }
      GoolObjectColors(obj);
      GoolObjectPhysics(obj);
      obj->status_a &= ~GOOL_FLAG_FIRST_FRAME; /* clear first frame flag */
    }
  }
  /* if object has no anim sequence, is not displayable (visible?), or global display flag is clear */
  if (!obj->anim_seq ||
      (status_b & GOOL_FLAG_INVISIBLE) ||
      !(cur_display_flags & GOOL_FLAG_DISPLAY)) {
    cur_obj = 0; /* indicate that there is no 'current object' to display */
    return SUCCESS; /* ...and don't render or display the object */
  }
  if (((status_b & GOOL_FLAG_FORCE_UPDATE) ||
      (obj->state_flags & GOOL_FLAG_MENUTEXT_STATE))
       && (cur_display_flags & GOOL_FLAG_FORCE_DISP_MENUS))
    display = 1;
  else {
    header = (gool_header*)obj->global->items[0];
    category = header->category;
    switch (category) {
    case 0x100: display = cur_display_flags & GOOL_FLAG_DISPLAY_C1;   break;
    case 0x300: display = cur_display_flags & GOOL_FLAG_DISPLAY_C356; break;
    case 0x400: display = cur_display_flags & GOOL_FLAG_DISPLAY_C4;   break;
    case 0x500: display = cur_display_flags & GOOL_FLAG_DISPLAY_C356; break;
    case 0x600: display = cur_display_flags & GOOL_FLAG_DISPLAY_C356; break;
    case 0x200: display = cur_display_flags & GOOL_FLAG_DISPLAY_C2;   break;
    default:
      display = 0;
      break;
    }
  }
  if (display)
    GoolObjectTransform(obj);
  cur_obj = 0;
  return SUCCESS;
}

//----- (8001DE78) --------------------------------------------------------
void GoolObjectTransform(gool_object *obj) {
  gool_anim *anim; // $s0
  entry *en, *svtx, *cvtx, *zone;
  zone_header *header;
  texinfo *info;
  texinfo2 *info2;
  tpageinfo pginfo;
  eid_t tpag;
  gool_frag *frags;
  bound2 bound;
  mat16 *m_rot;
  gool_vectors *obj_vectors, *cam_vectors;
  int i,frame_idx,status_b,flag,flag2;
  int res,far,shrink,size,x,offs;
  int *src,*dst;
  void *ot, **prims_tail;
  char *eid_str;

  status_b = obj->status_b;
  anim = (gool_anim*)obj->anim_seq;
  frame_idx = obj->anim_frame >> 8;
#ifdef PSX
  ot = context.cur->ot;
#else
  ot = context.ot;
#endif
  switch (anim->type) {
  case GOOL_ANIM_TYPE_VTX:
    en = NSLookup(&anim->v.eid);
    if (en->type != 20) {
      svtx = NSLookup(&anim->v.eid);
      GfxTransformSvtx((svtx_frame*)svtx->items[frame_idx], ot, obj);
    }
    else {
      cvtx = NSLookup(&anim->v.eid);
      GfxTransformCvtx((cvtx_frame*)cvtx->items[frame_idx], ot, obj);
    }
    if (status_b & 0x100000) {
      /* inherit zone colors */
      zone = obj->zone ? obj->zone : cur_zone;
      header = (zone_header*)zone->items[0];
      obj->colors = obj == crash ? header->player_colors:
                                   header->object_colors;
    }
    break;
  case GOOL_ANIM_TYPE_SPRITE:
    /* 200 * 2^(x/27279) */
    x = obj->scale.x;
    shrink = abs(x) / 27279;
    obj_vectors = &obj->vectors;
    cam_vectors = (gool_vectors*)(status_b & 0x200000 ? &cam_trans: &cam_trans_prev);
    flag = status_b & GOOL_FLAG_2D;
    m_rot = status_b & 0x200000 ? &ms_cam_rot: &ms_rot;
    header = (zone_header*)cur_zone->items[0];
    far = header->visibility_depth >> 8;
    flag2 = (status_b & 0x40000)?0:1;
#ifdef PSX
    res = RGteCalcSpriteRotMatrix(
      obj_vectors,
      cam_vectors,
      flag,
      shrink,
      m_rot,
      screen_proj,
      far);/*,
      flag2);*/
#else
    res = SwCalcSpriteRotMatrix(
      obj_vectors,
      cam_vectors,
      flag,
      shrink,
      m_rot,
      screen_proj,
      far,
      &params);
#endif
    if (!res) { return; }
    info2 = &anim->s.texinfos[frame_idx];
#ifdef PSX
    res = (int)NSLookup(&anim->s.tpage);
    pginfo = *((tpageinfo*)&res);
    prims_tail = GpuGetPrimsTail();
    RGteTransformSprite(
      context.cur->ot,
      info2,
      pginfo,
      200 << shrink,
      obj->size + 0x800-(screen_proj/2),
      prims_tail,
      (rect28*)uv_map);
#else
    tpag = anim->s.tpage;
    prims_tail = GLGetPrimsTail();
    SwTransformSprite(
      ot,
      info2,
      tpag,
      200 << shrink,
      obj->size + 0x800-(screen_proj/2),
      prims_tail,
      (rect28*)uv_map,
      &params);
#endif
    break;
  case GOOL_ANIM_TYPE_FONT:
    break;
  case GOOL_ANIM_TYPE_TEXT:
    flag = status_b & 0x400;
    GoolTextObjectTransform(obj,&anim->t,frame_idx,ot,flag);
    break;
  case GOOL_ANIM_TYPE_FRAG:
    x = obj->scale.x;
    shrink = abs(x) / 27279;
    obj_vectors = &obj->vectors;
    cam_vectors = (gool_vectors*)&cam_trans_prev;
    flag = status_b & GOOL_FLAG_2D;
    m_rot = &ms_rot;
    header = (zone_header*)cur_zone->items[0];
    far = header->visibility_depth >> 8;
    flag2 = (status_b & 0x40000)?0:1;
#ifdef PSX
    res = RGteCalcSpriteRotMatrix(
      obj_vectors,
      cam_vectors,
      flag,
      shrink,
      m_rot,
      screen_proj,
      far);/*,
      flag2);*/
#else
    res = SwCalcSpriteRotMatrix(
      obj_vectors,
      cam_vectors,
      flag,
      shrink,
      m_rot,
      screen_proj,
      far,
      &params);
#endif
    if (!res) { return; }
    offs = frame_idx*anim->r.frag_count;
    frags = &anim->r.frags[offs];
#ifdef PSX
    res = (int)NSLookup(&anim->f.tpage);
    pginfo = *(tpageinfo*)&res;
#else
    tpag = anim->f.tpage;
#endif
    for (i=0;i<anim->r.frag_count;i++) {
      bound.p1.x = frags[i].x1 << shrink;
      bound.p1.y = frags[i].y1 << shrink;
      bound.p2.x = frags[i].x2 << shrink;
      bound.p2.y = frags[i].y2 << shrink;
#ifdef PSX
      GfxTransformFragment(&frags[i], obj->size, pginfo, &bound, ot);
#else
      GfxTransformFragment(&frags[i], obj->size, tpag, &bound, ot);
#endif
    }
  }
}

//----- (8001E3D4) --------------------------------------------------------
static int GoolTextStringTransform(
  gool_object *obj,
  char *str,
  int val,
  gool_font *font,
#ifdef PSX
  tpageinfo pginfo,
#else
  eid_t tpag,
#endif
  int size,
  void **prims_tail) {
  gool_glyph *glyph;
  bound2 bound, bound2;
  vec scale;
  int32_t y_offs, max_x;
  char c, c2, buf[16];
  int i, center, flag;

  flag = 1;
  center = 0;
  if (prims_tail && (obj->status_b & GOOL_FLAG_STRING_CENTER)) {
    center = 1;
    bound2.p1.x = 999999;
    bound2.p1.y = 999999;
    bound2.p2.x = -999999;
    bound2.p2.y = -999999;
  }
  bound.p1.x = val;
  max_x = 0;
  y_offs = 0;
  while (c = *(str++)) {
    scale.x = 400 << size;
    scale.y = 400 << size;
    if (c == '~') {
      c = *(str++);
      switch (c) {
      case 'n':
      case '%':
        c = *(str++);
        bound.p1.x = val;
        y_offs -= (int32_t)(font->glyphs[0].height << size);
        break;
      case 'p':
        c = *(str++);
        flag = obj->sp[-1-(c-'0')];
        if (flag) { str++; }
        c = *str;
        if (c == 0) { break; }
        if (c == '~') { continue; }
        break;
      case 's':
        c = *(str++);
        c2 = *(str);
        for (i=0;c2!='~';i++, c2=*(str++)) {
          buf[i] = c2;
        }
        buf[i] = 0;
        if (c == 'x')
          scale.x = atoi(buf) << size;
        else
          scale.y = atoi(buf) << size;
        c = *str;
        if (c == '~') { continue; }
        break;
      case 'c':
        c = *(str++);
        flag = c == '1';
        c = *str;
        if (c == '~') { continue; }
        break;
      }
    }
    glyph = &font->glyphs[c - 0x20];
    bound.p2.x = bound.p1.x + scale.x;
    bound.p2.y = y_offs + (int32_t)(glyph->height << size);
    bound.p1.y = bound.p2.y - scale.y;
    if (bound.p2.x > max_x)
      max_x = bound.p2.x;
    if (prims_tail && glyph->has_texture) {
      if (center) {
        if (bound.p1.x < bound2.p1.x)
          bound2.p1.x = bound.p1.x;
        if (bound.p1.y < bound2.p1.y)
          bound2.p1.y = bound.p1.y;
        if (bound.p2.x > bound2.p2.x)
          bound2.p2.x = bound.p2.x;
        if (bound.p2.y > bound2.p2.y)
          bound2.p2.y = bound.p2.y;
      }
#ifdef PSX
      GfxTransformFontChar(obj, glyph, obj->size, pginfo, &bound, prims_tail, flag);
#else
      GfxTransformFontChar(obj, glyph, obj->size, tpag, &bound, prims_tail, flag);
#endif
    }
    bound.p1.x += (int32_t)(glyph->width << size);
  }
  if (center && prims_tail && font->has_backdrop) {
    bound2.p1.x -= 100;
    bound2.p2.x += 100;
#ifdef PSX
    GfxTransformFragment(&font->backdrop, obj->size - 10, pginfo, &bound2, prims_tail);
#else
    GfxTransformFragment(&font->backdrop, obj->size - 10, tpag, &bound2, prims_tail);
#endif
  }
  return max_x;
}

//----- (8001E7D8) --------------------------------------------------------
void GoolTextObjectTransform(gool_object *obj, gool_text *text, int terms_skip, void *ot, int flag) {
  gool_font *font;
  tpageinfo pginfo;
  eid_t tpag;
  uint8_t *anims;
  char buf[256], *str;
  void **prims_tail;
  uint32_t size, font_offset;
  int32_t arg;
  int i, j, res, start;

  size = abs(obj->scale.x) / 27279; /* scale ignores flip */
#ifdef PSX
  res = RGteCalcSpriteRotMatrix(
    &obj->vectors,
    (gool_vectors*)&cam_trans_prev,
    !(obj->status_b & GOOL_FLAG_2D),
    size,
    &ms_rot,
    screen_proj,
    0x3C00);
#else
  res = SwCalcSpriteRotMatrix(
    &obj->vectors,
    (gool_vectors*)&cam_trans_prev,
    (obj->status_b & GOOL_FLAG_2D),
    size,
    &ms_rot,
    screen_proj,
    0x3C00,
    &params);
#endif
  if (!res) { return; }
  str = text->strings;
  for (i=0,j=0;j<terms_skip;i++) {
    if (str[i] == 0) { j++; }
  }
  start = i;
  anims = obj->global->items[5];
  font_offset = obj->invincibility_state >> 8;
  if (font_offset == 0)
    font = (gool_font*)&anims[text->glyphs_offset*4];
  else
    font = (gool_font*)&anims[font_offset*4];
  sprintf(buf, &str[i], obj->sp[-2], obj->sp[-3], obj->sp[-4], obj->sp[-5]);
  while(buf[i++] != 0); /* skip to first null term char */
  i--;
  if (i-- != start) { /* not a zero length str? */
    while (i!=start) { /* truncate trailing spaces */
      if (buf[i] != ' ') { break; }
      i--;
    }
    buf[i+1] = 0; /* place null term at end of truncated string */
  }
#ifdef PSX
  res = (int)NSLookup(&font->tpage);
  pginfo = *(tpageinfo*)&res;
  if (flag)
    arg = GoolTextStringTransform(obj, str, 0, font, pginfo, size, 0);
  else
    arg = 0;
  prims_tail = GpuGetPrimsTail();
  GoolTextStringTransform(obj, str, -arg/4, font, pginfo, size, prims_tail);
#else
  tpag = font->tpage;
  if (flag)
    arg = GoolTextStringTransform(obj, buf, 0, font, tpag, size, 0);
  else
    arg = 0;
  prims_tail = GLGetPrimsTail();
  GoolTextStringTransform(obj, buf, -arg/4, font, tpag, size, prims_tail);
#endif
}

static inline int GoolObjectColorByZone(gool_object *obj) {
  entry *zone;
  zone_header *header;
  gool_colors *colors;

  obj->invincibility_state = 0;
  zone = obj->zone ? obj->zone : cur_zone;
  header = (zone_header*)zone->items[0];
  colors = obj == crash ? &header->player_colors : &header->object_colors;
  obj->colors.intensity = colors->intensity;
}

//----- (8001EB28) --------------------------------------------------------
int GoolObjectColors(gool_object *obj) {
  entry *zone;
  zone_header *header;
  gool_header *g_header;
  int invincibility_state, elapsed_since;
  int mod, val;

  invincibility_state = obj->invincibility_state;
  elapsed_since = frames_elapsed - obj->invincibility_stamp;
  zone = obj->zone ? obj->zone : cur_zone;
  header = (zone_header*)zone->items[0];
  switch (invincibility_state) { /* cases 3, 4, 5 all fall down to case 2 */
  case 4:
    if (elapsed_since > 60) /* 1s at 60fps */
      GoolObjectColorByZone(obj);
    if (obj->collider) {
      g_header = (gool_header*)obj->global->items[0];
      if (g_header->category == 0x300)
        GoolSendEvent(obj, obj->collider, GOOL_EVENT_HIT_INVINCIBLE, 1, 0);
    }
  case 3:
    if (invincibility_state == 3 && elapsed_since > 451) /* 7.5s at 60fps */
      GoolObjectColorByZone(obj);
  case 5:
    if (invincibility_state == 5 && elapsed_since > 602) /* 10s at 60fps */
      GoolObjectColorByZone(obj);
  case 2:
    mod = (draw_count % 4) << 8;
    val = mod<0x100?mod+0x7F:0x47F-mod; /* cyclic pattern: 0.5, 3.5, 2.5, 1.5 */
    obj->colors.intensity.r=val;
    obj->colors.intensity.g=val;
    obj->colors.intensity.b=val;
    break;
  case 6:
    if (elapsed_since > 15) { /* 0.25s at 60 fps */
      obj->invincibility_state = 0;
      obj->status_b |= GOOL_FLAG_DPAD_CONTROL;
    }
    break;
  case 7:
    if (elapsed_since > 15 || (obj->status_a & 1)) {
      obj->invincibility_state = 0;
      obj->status_b |= GOOL_FLAG_DPAD_CONTROL;
    }
    break;
  default:
    if (obj == crash && (obj->status_b & 0x4000000))
      GoolObjectColorByZone(obj);
    break;
  }
  return SUCCESS;
}

int GoolObjectBound(gool_object *obj) {
  gool_bound *bound, crash_bound;
  gool_anim *anim;
  svtx_frame *frame;
  entry *entry;
  vec trans;
  int32_t rot_x;
  int bound_idx, frame_idx;

  bound_idx = object_bound_count++;
  bound = &object_bounds[bound_idx]; /* allocate a new bound for this object/frame */
  bound->obj = obj;
  anim = obj->anim_seq;
  frame_idx = obj->anim_frame >> 8;
  if (obj->status_a & GOOL_FLAG_LBOUND_INVALID) { /* local bounding volume needs recalc? */
    GoolObjectLocalBound(obj, &obj->scale); /* recalculate */
    obj->status_a &= ~GOOL_FLAG_LBOUND_INVALID; /* clear; no recalc until next set */
  }
  if (anim->type == 1) { /* if svtx/cvtx, calc bounding rect from current frame */
    entry = NSLookup(&anim->v.eid);
    frame = (svtx_frame*)entry->items[frame_idx];
    rot_x = angle12(obj->rot.x);
    /* no transform needed if oriented forward and standard scale */
    if (rot_x == 0 && obj->scale.x == 0x1000) {
      trans.x = obj->trans.x + frame->col_x;
      trans.y = obj->trans.y + frame->col_y;
      trans.z = obj->trans.z + frame->col_z;
    }
    else {
      GoolTransform(
        (vec*)&frame->col_x,
        &obj->trans,
        &obj->rot,
        &obj->scale,
        &trans);
    }
    if ((uint32_t)(rot_x-0x200) > 0xC00) { /* between 0 and 0x200 or 0xE00 and 0xFFF */
      bound->p1.x = trans.x + obj->bound.p1.x; /* i.e. 0 and 45 or 315 and 360 */
      bound->p1.y = trans.y + obj->bound.p1.y;
      bound->p1.z = trans.z + obj->bound.p1.z;
      bound->p2.x = trans.x + obj->bound.p2.x;
      bound->p2.y = trans.y + obj->bound.p2.y;
      bound->p2.z = trans.z + obj->bound.p2.z;
    }
    else if (rot_x < 0x600) { /* between 0x200 and 0x600 */
      bound->p1.x = trans.x + obj->bound.p1.z; /* i.e. 45 and 135 */
      bound->p1.y = trans.y + obj->bound.p1.y;
      bound->p1.z = trans.z - obj->bound.p2.x;
      bound->p2.x = trans.x + obj->bound.p2.z;
      bound->p2.y = trans.y + obj->bound.p2.y;
      bound->p2.z = trans.z - obj->bound.p1.x;
    }
    else if (rot_x < 0xA00) { /* between 0xA00 and 0xE00 */
      bound->p1.x = trans.x + obj->bound.p1.x; /* i.e. 225 and 315 */
      bound->p1.y = trans.y + obj->bound.p1.y;
      bound->p1.z = trans.z - obj->bound.p2.z;
      bound->p2.x = trans.x + obj->bound.p2.x;
      bound->p2.y = trans.y + obj->bound.p2.y;
      bound->p2.z = trans.z - obj->bound.p1.z;
    }
    else { /* between 0x600 and 0xA00 */
      bound->p1.x = trans.x - obj->bound.p2.z; /* i.e. 135 and 225 */
      bound->p1.y = trans.y + obj->bound.p1.y;
      bound->p1.z = trans.z + obj->bound.p1.x;
      bound->p2.x = trans.x - obj->bound.p1.z;
      bound->p2.y = trans.y + obj->bound.p2.y;
      bound->p2.z = trans.z + obj->bound.p2.x;
    }
  }
  else {
    bound->p1.x = obj->trans.x + obj->bound.p1.x;
    bound->p1.y = obj->trans.y + obj->bound.p1.y;
    bound->p1.z = obj->trans.z + obj->bound.p1.z;
    bound->p2.x = obj->trans.x + obj->bound.p2.x;
    bound->p2.y = obj->trans.y + obj->bound.p2.y;
    bound->p2.z = obj->trans.z + obj->bound.p2.z;
  }
  if (obj->anim_stamp == crash->anim_stamp) { /* rendered during same frame as crash? */
    crash_bound.p1.x = crash->trans.x + crash->bound.p1.x;
    crash_bound.p1.y = crash->trans.y + crash->bound.p1.y;
    crash_bound.p1.z = crash->trans.z + crash->bound.p1.z;
    crash_bound.p2.x = crash->trans.x + crash->bound.p2.x;
    crash_bound.p2.y = crash->trans.y + crash->bound.p2.y;
    crash_bound.p2.z = crash->trans.z + crash->bound.p2.z;
    if (TestBoundIntersection(&crash_bound, &bound->bound)) /* collision with crash? */
      GoolCollide(obj, &bound->bound, crash, &crash_bound); /* handle it (i.e. signal objs) */
    else
      obj->collider = 0;
  }
}

static inline int32_t GoolObjectControlDir(gool_object *obj, int scale) {
  const gool_accel_state *accel_state;
  const gool_move_state *move_state;
  uint32_t status_a,state_flags,dir;
  int32_t accel,decel,delta,cur_ang_xz,tgt_ang_xz;

  status_a = obj->status_a;
  state_flags = obj->state_flags;
  accel_state = &accel_states[0]; /* stopped */
  if (state_flags & GOOL_FLAG_GROUND_STATE) { /* ground state? */
    accel_state = &accel_states[1]; /* transitioning from ground to air */
    if (status_a & 0x2000) /* not transitioning? */
      accel_state = &accel_states[5]; /* moving on ground */
  }
  if (obj->invincibility_state == 5)
    accel_state = &accel_states[4];
  if (state_flags & GOOL_FLAG_AIR_STATE) { /* air state? */
    accel_state = &accel_states[2]; /* transitioning from air to ground */
    if (status_a & 0x2000) /* not transitioning? */
      accel_state = &accel_states[6]; /* moving in air */
  }
  if (state_flags & GOOL_FLAG_FLING_STATE) /* ??? */
    accel_state = &accel_states[3];
  dir = (pads[0].held >> 12) & 0xF; /* directional buttons */
  move_state = &move_states[dir];
  if (move_state->dir == 8) { /* none pressed or invalid combination? */
    decel = accel_state->decel;
    decel = (decel*scale)/1024;
    obj->speed -= decel; /* decelerate */
    if (obj->speed < 0)
      obj->speed = 0;
    return move_state->speed_scale;
  }
  /* else a valid combination of directional buttons */
  cur_ang_xz = obj->target_rot.x;
  tgt_ang_xz = angle12(move_state->angle + cam_rot_xz);
  delta = abs(tgt_ang_xz - cur_ang_xz);
  if (delta > 0x800)
    delta = 0x1000 - delta;
  if (state_flags & GOOL_FLAG_AIR_STATE) { /* in air? */
    accel = accel_state->accel;
    accel = ((cos(delta)>>6)*((accel*scale)/1024))>>6; /* angular acceleration */
    obj->speed += accel; /* accelerate */
    if (obj->speed <= 0x100) /* slow enough? */
      obj->target_rot.x = tgt_ang_xz; /* set directly to target angle */
    else if (delta < 0x7C8) /* fast enough but not turning completely around (<175deg) */
      /* linearly approach target angle (at speed 0xF00) */
      obj->target_rot.x = GoolObjectRotate(cur_ang_xz, tgt_ang_xz, 0xF00, 0);
    else { /* otherwise trying to turn around but moving too fast to do so */
      decel = accel_state->decel;
      decel = (decel*scale)/1024;
      obj->speed -= decel; /* decelerate */
      // obj->speed = max(obj->speed, 0);
      if (obj->speed < 0)
        obj->speed = 0;
    }
  }
  else { /* on ground */
     /* slow enough or at most 90deg from target angle? */
    if (obj->speed <= 0x100 || delta <= 0x400) {
      obj->target_rot.x = tgt_ang_xz; /* set directly to target angle */
      accel = accel_state->accel;
      accel = (accel*scale)/1024;
      obj->speed += accel; /* accelerate */
    }
    else { /* fast enough and trying to rotate more than 90deg */
      obj->speed = 0; /* stop */
    }
  }
  // obj->speed = min(obj->speed, accel_state->max_speed);
  if (obj->speed > accel_state->max_speed)
    obj->speed = accel_state->max_speed; /* terminal speed/velocity */
  return move_state->speed_scale;
}

//----- (8001F30C) --------------------------------------------------------
int GoolObjectPhysics(gool_object *obj) {
  vec trans, velocity;
  uint32_t status_a, status_b;
  int32_t scale, speed, speed_scale, tgt_ang_xz, lim;

  status_a = obj->status_a;
  status_b = obj->status_b;
#ifdef PSX
  scale = min(context->c1_p.ticks_per_frame, 0x66);
#else
  scale = min(context.ticks_per_frame, 0x66);
#endif
  if ((status_b & GOOL_FLAG_DPAD_CONTROL) && game_state == GAME_STATE_PLAYING) { /* dir controlled by joypad? */
    speed_scale = GoolObjectControlDir(obj, scale);
    /* calc velocity vector */
    tgt_ang_xz = obj->target_rot.x;
    speed = (obj->speed*speed_scale) >> 8;
    obj->velocity.x = ((sin(tgt_ang_xz)>>4)*speed)>>8;
    obj->velocity.z = ((cos(tgt_ang_xz)>>4)*speed)>>8;
  }
  if ((status_a & 1) && obj->event != 0x1200)  /* ??? */
    obj->status_a &= 0xFFFFDFFF; /* clear bit 14? */
  obj->status_a &= 0xFFCAA07E; /* clear bits 1,8-13,15,17,19,21,22 */
  if (status_b & GOOL_FLAG_ROT_Y) /* can rotate in xz? */
    obj->rot.x=GoolObjectRotate(obj->rot.x, obj->target_rot.x, obj->ang_velocity_x, obj);
  if (status_b & GOOL_FLAG_ROT_Y2) /* can rotate in xz (alt)? */
    obj->rot.x=GoolObjectRotate2(obj->rot.x, obj->target_rot.x, obj->ang_velocity_x, obj);
  if (status_b & GOOL_FLAG_ROT_X) /* can rotate in yz? */
    obj->rot.y=GoolObjectRotate(obj->rot.y, obj->target_rot.y, obj->ang_velocity_y, 0);
  if (((gool_header*)obj->global->items[0])->type != 5) /* not DoctC? */
    obj->collider = 0;
  if (status_b & GOOL_FLAG_TRANS_MOTION) { /* can move/translate? */
    velocity.x = (obj->velocity.x*scale)/1024;
    velocity.y = (obj->velocity.y*scale)/1024;
    velocity.z = (obj->velocity.z*scale)/1024;
    if (status_b & GOOL_FLAG_STOPPED_BY_SOLID) { /* stopped by solid matter? */
      obj->event = 0xFF;
      TransSmoothStopAtSolid(obj, &velocity, &cur_zone_query);
    }
    else { /* can move thru solid matter */
      obj->trans.x += velocity.x;
      obj->trans.y += velocity.y;
      obj->trans.z += velocity.z;
    }
    status_a=obj->status_a;
    status_b=obj->status_b;
    if (status_b & GOOL_FLAG_ORIENT_ON_PATH) { /* movement restricted to path? */
      GoolObjectOrientOnPath(obj, 0, &trans);
      obj->floor_y = trans.y;
      if (status_a & 0x200)
        obj->trans = trans;
    }
  }
  /* components of misc_a are limited to (-misc_b.z, misc_b.z)? */
  if (obj->status_b & 0x1000) {
    lim=obj->misc_b.z;
    obj->misc_a.x=limit(obj->misc_a.x, -lim, lim);
    obj->misc_a.y=limit(obj->misc_a.y, -lim, lim);
    obj->misc_a.z=limit(obj->misc_a.z, -lim, lim);
  }
  /* xz plane at height y=obj->process.floor_y is treated as solid ground? */
  /* beginning to touch or fall below it? */
  if ((status_b & GOOL_FLAG_SOLID_GROUND) && obj->trans.y <= obj->floor_y) {
    obj->trans.y = obj->floor_y; /* reposition */
    obj->status_a |= 1;
    obj->floor_impact_stamp = frames_elapsed; /* record timestamp */
    if (obj->velocity.y < 0) { /* falling? */
      obj->floor_impact_velocity = obj->velocity.y; /* record falling velocity */
      obj->velocity.y = 0; /* stop falling */
    }
  }
  if (status_b & GOOL_FLAG_GRAVITY) { /* affected by gravity? */
    obj->velocity.y -= 4000*scale; /* increase in negative y velocity */
    if (obj->velocity.y < -0x2EE000) /* terminal velocity = -750 */
      obj->velocity.y = -0x2EE000;
  }
  if (status_b & GOOL_FLAG_COLLIDABLE) { /* collidable? */
    /* not the same render frame as crash and within range of crash? */
    if (obj->anim_stamp != crash->anim_stamp &&
      !OutOfRange(obj, &obj->trans, &crash->trans, 0x7D000, 0xAF000, 0x7D000))
      GoolObjectBound(obj); /* compute and record object rect */
  }
}

//----- (8001FB20) --------------------------------------------------------
// unreferenced; exists as inline code in GoolTranslateGop
static inline int32_t GoolSignShift(uint32_t value, int bitlen) {
  return (int32_t)(value << (32 - bitlen)) >> (32 - bitlen);
}

// does not exist separately
static inline uint32_t* GoolTranslateGop(gool_object *obj, uint32_t gop, gool_const_buf *consts) {
  gool_object *link;
  int32_t val;
  uint32_t *data;
  int idx, link_idx, reg_idx;

  if (!(gop & 0x800)) { /* ireg or reg ref */
    idx = gop & 0x3FF;
    if (!(gop & 0x400))
      data = (uint32_t*)obj->global->items[2]; /* iref ref */
    else
      data = (uint32_t*)obj->external->items[2]; /* reg ref */
    return &data[idx];
  }
  else if (!(gop & 0x400)) {
    if (!(gop & 0x200)) { /* int ref */
      val = GoolSignShift(gop & 0x1FF, 9) << 8;
      consts->idx = !consts->idx;
      consts->buf[consts->idx] = val;
      return &consts->buf[consts->idx];
    }
    else if (!(gop & 0x100)) { /* frac ref */
      val = GoolSignShift(gop & 0xFF, 8) << 4;
      consts->idx = !consts->idx;
      consts->buf[consts->idx] = val;
      return &consts->buf[consts->idx];
    }
    else if (!(gop & 0x80)) { /* var ref */
      idx = GoolSignShift(gop & 0x3F, 6);
      return &obj->process.fp[idx];
    }
    else if (gop == 0xBE0) /* null ref */
      return (uint32_t*)0;
    else if (gop == 0xBF0) /* sp-double ref */
      return (uint32_t*)0xBF0;
    else /* invalid ref */
      return (uint32_t*)1;
  }
  else { /* pool ref */
    link_idx = (gop >> 6) & 0x7;
    reg_idx = gop & 0x3F;
    link = obj->links[link_idx];
    if (link)
      return &link->regs[reg_idx];
    else
      return (uint32_t*)0;
  }
}

//----- (8001FB34) --------------------------------------------------------
static uint32_t* GoolTranslateInGop(gool_object *obj, uint32_t gop) {
  int idx;
  if ((gop & 0xFFF) == 0xE1F) /* stack pop */
    return --obj->sp;
  else if ((gop & 0xE00) == 0xE00) { /* stack ref */
    idx = gop & 0x1FF;
    return &obj->regs[idx];
  }
  else /* other ref */
    return GoolTranslateGop(obj, gop, &in_consts);
}

//----- (8001FC4C) --------------------------------------------------------
static uint32_t* GoolTranslateOutGop(gool_object *obj, uint32_t gop) {
  int idx;
  if ((gop & 0xFFF) == 0xE1F) /* stack push */
    return obj->sp++;
  else if ((gop & 0xE00) == 0xE00) { /* stack ref */
    idx = gop & 0x1FF;
    return &obj->regs[idx];
  }
  else /* other ref */
    return GoolTranslateGop(obj, gop, &out_consts);
}

//----- (8001FDC4) --------------------------------------------------------
static int GoolTestControls(uint32_t instruction, int port) {
  uint32_t button_mask, dir_mask, test_type, dir_test, dir_test_type;
  uint32_t buttons, buttons_prev, dir_idx, dir, dir_prev;
  int not, cond;

  button_mask = instruction & 0xFFF;
  test_type = (instruction >> 12) & 3;
  dir_test_type = (instruction >> 14) & 3;
  dir_test = (instruction >> 16) & 0xF;
  not = (instruction >> 20) & 1;
  if (test_type == 0) /* directional test only? */
    cond = 1;
  else if (test_type == 1) { /* nondir test tapped */
    buttons = pads[port].tapped & 0xFFF;
    cond = buttons & button_mask;
    if (!cond) { return not; }
  }
  else if (test_type == 2) { /* nondir test held */
    buttons = pads[port].held & 0xFFF;
    cond = buttons & button_mask;
    if (!cond) { return not; }
  }
  else if (test_type == 3) { /* nondir test tapped (2-frame) */
    buttons = pads[port].tapped;
    buttons_prev = pads[port].tapped_prev;
    cond = button_mask & (buttons | buttons_prev);
    if (!cond) { return not; }
  }
  else if (test_type == 3) { /* nondir test held (2 frame) */
    buttons = pads[port].held;
    buttons_prev = pads[port].held_prev;
    cond = button_mask & (buttons | buttons_prev);
    if (!cond) { return not; }
  }
  if (dir_test_type == 0) /* nondir test only? */
    return cond ^ not; /* not=0=>cond, not=1=>!cond */
  else if (dir_test_type == 1) { /* single dir test tapped, multi-dir test held or released? */
    buttons = pads[port].tapped;
    if (dir_test == 9)
      cond = buttons & 0x1000;
    else if (dir_test == 10)
      cond = buttons & 0x2000;
    else if (dir_test == 11)
      cond = buttons & 0x4000;
    else if (dir_test == 12)
      cond = buttons & 0x8000;
    else {
      dir_idx = (pads[port].held & 0xF000) >> 12;
      dir = move_states[dir_idx].dir;
      dir_idx = (pads[port].held_prev & 0xF000) >> 12;
      dir_prev = move_states[dir_idx].dir;
      cond = (dir == dir_test) && (dir != dir_prev);
    }
  }
  else if (dir_test_type == 2) { /* single dir test held, multi-dir test held? */
    buttons = pads[port].held;
    if (dir_test == 9)
      cond = buttons & 0x1000;
    else if (dir_test == 10)
      cond = buttons & 0x2000;
    else if (dir_test == 11)
      cond = buttons & 0x4000;
    else if (dir_test == 12)
      cond = buttons & 0x8000;
    else {
      dir_idx = (pads[port].held & 0xF000) >> 12;
      dir = move_states[dir_idx].dir;
      cond = dir == dir_test;
    }
  }
  else if (dir_test_type == 3 || dir_test_type == 4) { /* multi-dir test held or pressed? */
    dir_idx = (pads[port].held & 0xF000) >> 12;
    dir = move_states[dir_idx].dir;
    dir_idx = (pads[port].held_prev & 0xF000) >> 12;
    dir_prev = move_states[dir_idx].dir;
    cond = (dir == dir_test) || (dir_prev == dir_test);
  }
  return cond ^ not; /* not=0=>cond, not=1=>!cond */
}

/* forward declarations */
static inline void GoolOp13unnamed(gool_object*,uint32_t);
static inline void GoolOp2225unnamed(gool_object*,uint32_t);
static inline void GoolOpMisc(gool_object*,uint32_t);
static inline int GoolOpControlFlow(gool_object*,uint32_t,uint32_t*,gool_state_ref*);
static inline int GoolOpChangeAnim(gool_object*,uint32_t,uint32_t*);
static inline int GoolOpChangeAnimFrame(gool_object*,uint32_t,uint32_t*);
static inline void GoolOpTransformVectors(gool_object*,uint32_t);
static inline void GoolOpJumpAndLink(gool_object*,uint32_t,uint32_t*);
static inline int GoolOpSendEvent(gool_object*,uint32_t,uint32_t*,gool_object*,uint32_t);
static inline int GoolOpReturnStateTransition(gool_object*,uint32_t,uint32_t*,
                                              gool_state_ref*,uint32_t);
static inline void GoolOpSpawnChildren(gool_object*,uint32_t,uint32_t);
static inline void GoolOpPaging(gool_object*,uint32_t);
static inline void GoolOpAudioVoiceCreate(gool_object*,uint32_t);
static inline void GoolOpAudioControl(gool_object*,uint32_t);
static inline void GoolOpReactSolidSurfaces(gool_object*,uint32_t);

/* shorthands */
#define G_OPCODE(ins) (ins >> 24)
#define G_OPA(ins) ((ins >> 12) & 0xFFF)
#define G_OPB(ins) ((ins      ) & 0xFFF)
#define G_OP(ins) G_OPB(ins)

#define G_TRANS_GOPA(obj,ins,a) \
a = *(GoolTranslateInGop(obj,G_OPA(ins)))
#define G_TRANS_GOPB(obj,ins,b) \
b = *(GoolTranslateInGop(obj,G_OPB(ins)))
#define G_TRANS_GOPA_IN(obj,ins,a) \
a = GoolTranslateInGop(obj,G_OPA(ins))
#define G_TRANS_GOPB_IN(obj,ins,b) \
b = GoolTranslateInGop(obj,G_OPB(ins))
#define G_TRANS_GOPA_OUT(obj,ins,a) \
a = GoolTranslateOutGop(obj,G_OPA(ins))
#define G_TRANS_GOPB_OUT(obj,ins,b) \
b = GoolTranslateOutGop(obj,G_OPB(ins))
#define G_TRANS_GOP     G_TRANS_GOPB
#define G_TRANS_GOP_IN  G_TRANS_GOPB_IN
#define G_TRANS_GOP_OUT G_TRANS_GOPB_OUT
#define G_TRANS_GOPS(obj,ins,a,b) \
G_TRANS_GOPA(obj,ins,a); \
G_TRANS_GOPB(obj,ins,b)
#define G_TRANS_REGREF(obj,a) \
(((a) == 0x1F) ? GoolObjectPop(obj) \
               : obj->regs[a])

#ifdef CFLAGS_GOOL_DEBUG
gool_debug_cache debug_cache = { 0 };
int GoolObjectTryBreak(gool_object *obj, uint32_t opcode, uint32_t flags, int res);
#endif /* CFLAGS_GOOL_DEBUG */

//----- (800201DC) --------------------------------------------------------
int GoolObjectInterpret(gool_object *obj, uint32_t flags, gool_state_ref *transition) {
  uint32_t instruction, opcode;
  gool_object *recipient;
  uint32_t arg;
  union {
    uint32_t a, r;
    int32_t sa, sr;
    uint32_t *src;
  } r1;
  union {
    uint32_t b, l;
    int32_t sb, sl;
    uint32_t *dst;
  } r2;
  int res;
#define a r1.a
#define r r1.r
#define sa r1.sa
#define sr r1.sr
#define src r1.src
#define b r2.b
#define l r2.l
#define sb r2.sb
#define sl r2.sl
#define dst r2.dst
#ifdef CFLAGS_GOOL_DEBUG
  gool_debug *dbg;

  dbg = GoolObjectDebug(obj);
  if (dbg && ((dbg->flags & GOOL_FLAGS_PAUSED)==GOOL_FLAGS_PAUSED)) { return SUCCESS; } /* paused */
#endif
  while (1) {
#ifdef CFLAGS_GOOL_DEBUG
    if (dbg) { dbg->obj_prev = *obj; }
#endif
    res = 0;
    instruction = *(obj->pc++);
    opcode = G_OPCODE(instruction);
    switch (opcode) {
    case 0:
      G_TRANS_GOPS(obj,instruction,r,l);
      GoolObjectPush(obj,l+r);
      break;
    case 1:
      G_TRANS_GOPS(obj,instruction,r,l);
      GoolObjectPush(obj,l-r);
      break;
    case 2:
      G_TRANS_GOPS(obj,instruction,r,l);
      GoolObjectPush(obj,l*r);
      break;
    case 3:
      G_TRANS_GOPS(obj,instruction,r,l);
      GoolObjectPush(obj,l/r);
      break;
    case 4:
      G_TRANS_GOPS(obj,instruction,a,b);
      GoolObjectPush(obj,!(a^b));
      break;
    case 5:
      G_TRANS_GOPS(obj,instruction,a,b);
      b ? GoolObjectPush(obj,a!=0)
        : GoolObjectPush(obj,0);
      break;
    case 6:
      G_TRANS_GOPS(obj,instruction,a,b);
      GoolObjectPush(obj,a||b);
      break;
    case 7:
      G_TRANS_GOPS(obj,instruction,a,b);
      GoolObjectPush(obj,a&b);
      break;
    case 8:
      G_TRANS_GOPS(obj,instruction,a,b);
      GoolObjectPush(obj,a|b);
      break;
    case 9:
      G_TRANS_GOPS(obj,instruction,sl,sr);
      GoolObjectPush(obj,sl>sr);
      break;
    case 0xA:
      G_TRANS_GOPS(obj,instruction,sl,sr);
      GoolObjectPush(obj,sl>=sr);
      break;
    case 0xB:
      G_TRANS_GOPS(obj,instruction,sl,sr);
      GoolObjectPush(obj,sl<sr);
      break;
    case 0xC:
      G_TRANS_GOPS(obj,instruction,sl,sr);
      GoolObjectPush(obj,sl<=sr);
      break;
    case 0xD:
      G_TRANS_GOPS(obj,instruction,r,l);
      GoolObjectPush(obj,l%r);
      break;
    case 0xE:
      G_TRANS_GOPS(obj,instruction,a,b);
      GoolObjectPush(obj,a^b);
      break;
    case 0xF:
      G_TRANS_GOPS(obj,instruction,a,b);
      GoolObjectPush(obj,(a&b)==a);
      break;
    case 0x10:
      G_TRANS_GOPS(obj,instruction,a,b);
      if (a == b)
        GoolObjectPush(obj,b);
      else {
        uint32_t rand;
        rand = randa(a-b);
        GoolObjectPush(obj,b+rand);
      }
      break;
    case 0x11:
      G_TRANS_GOPA(obj,instruction,a);
      G_TRANS_GOPB_OUT(obj,instruction,dst);
      if (dst) { *(dst) = a; }
      break;
    case 0x12:
      G_TRANS_GOPA(obj,instruction,a);
      G_TRANS_GOPB_OUT(obj,instruction,dst);
      *(dst) = !a;
      break;
    case 0x13:
      GoolOp13unnamed(obj,instruction);
      break;
    case 0x14:
      G_TRANS_GOPA_IN(obj,instruction,src);
      G_TRANS_GOPB_OUT(obj,instruction,dst);
      *(dst) = (uint32_t)src;
      break;
    case 0x15: /* signed = arithmetic shift */
      G_TRANS_GOPS(obj,instruction,sr,sl);
      sr < 0 ? GoolObjectPush(obj,sl>>-sr)
             : GoolObjectPush(obj,sl<<sr);
      break;
    case 0x16:
    case 0x26:
      G_TRANS_GOPA_IN(obj,instruction,src);
      G_TRANS_GOPB_IN(obj,instruction,dst);
      if (G_OPCODE(instruction) == 0x26) {
        if (!dst) { break; }
        GoolObjectPush(obj, (uint32_t)dst);
        if (src) { GoolObjectPush(obj,(uint32_t)src); }
      }
      else {
        if (!dst) { break; }
        if (src) { arg = *(src); }
        GoolObjectPush(obj,*(dst));
        if (src) { GoolObjectPush(obj,arg); }
      }
      break;
    case 0x17:
      G_TRANS_GOPA_IN(obj,instruction,src);
      G_TRANS_GOPB_OUT(obj,instruction,dst);
      if (dst) { *(dst) = ~*(src); }
      break;
    case 0x18: {
      uint32_t *code;
      uint32_t offset,reg;
      code = (uint32_t*)obj->global->items[1];
      offset = (instruction & 0x3FFF);
      reg = (instruction >> 14) & 0x3F;
      if (reg == 0x1F)
        GoolObjectPush(obj,(uint32_t)&code[offset]);
      else
        obj->regs[reg] = (uint32_t)&code[offset];
      break;
    }
    case 0x19:
      G_TRANS_GOPA(obj,instruction,sa);
      G_TRANS_GOPB_OUT(obj,instruction,dst);
      *(dst) = sa < 0 ? -sa : sa;
      break;
    case 0x1A: {
      uint32_t res;
      res = GoolTestControls(instruction&0x1FFFFF,0);
      GoolObjectPush(obj,res);
      break;
    }
    case 0x1B: {
      int32_t speed, velocity;
      G_TRANS_GOPS(obj,instruction,sa,sb);
#ifdef PSX
      speed = min(context.cur->ticks_per_frame, 0x66); /* offset 0x84 in disp */
#else
      speed = min(context.ticks_per_frame, 0x66);
#endif
      velocity = sb + (sa*speed)/1024;
      GoolObjectPush(obj, velocity);
      break;
    }
    case 0x1C:
      GoolOpMisc(obj, instruction);
      break;
    case 0x1D: {
      int32_t angle,sin_adj,sin_scaled;
      G_TRANS_GOPS(obj,instruction,sa,sb);
      angle = ((sb << 11) / sa) - 0x400;
      sin_adj = sin(angle) + 0x1000;
      sin_scaled = shr_mul(sin_adj,sa,0xFFFF,13,2);
      GoolObjectPush(obj, sin_scaled);
      break;
    }
    case 0x1E:
      G_TRANS_GOPS(obj,instruction,a,b);
      if (b == 0) { b = 1; }
      GoolObjectPush(obj, (a + draw_count) % b);
      break;
    case 0x1F:
      G_TRANS_GOP(obj,instruction,b);
      GoolObjectPush(obj,((uint32_t*)&globals)[b>>8]);
      break;
    case 0x20:
      G_TRANS_GOPS(obj,instruction,a,b);
      ((uint32_t*)&globals)[b>>8]=a;
      break;
    case 0x21: {
      int32_t ang_dist;
      G_TRANS_GOPS(obj,instruction,a,b);
      ang_dist = GoolAngDiff(b, a);
      GoolObjectPush(obj, ang_dist);
      break;
    }
    case 0x22:
    case 0x25:
      GoolOp2225unnamed(obj,instruction);
      break;
    case 0x23: {
      uint32_t link_idx,color_idx,color;
      gool_object *link;
      link_idx = (instruction >> 12) & 7;
      color_idx = (instruction >> 15) & 0x3F;
      link = obj->links[link_idx];
      color = link->colors_i[color_idx];
      GoolObjectPush(obj, color);
      break;
    }
    case 0x24: {
      uint32_t link_idx,color_idx,color;
      gool_object *link;
      G_TRANS_GOP(obj,instruction,b);
      color = b;
      link_idx = (instruction >> 12) & 7;
      color_idx = (instruction >> 15) & 0x3F;
      link = obj->links[link_idx];
      link->colors_i[color_idx] = color;
      break;
    }
    case 0x27: {
      uint8_t *anim_data;
      G_TRANS_GOPA(obj,instruction,a);
      G_TRANS_GOPB_OUT(obj,instruction,dst);
      anim_data = obj->global->items[5];
      *(dst) = (uint32_t)&anim_data[a>>6];
      break;
    }
    case 0x82:
      res = GoolOpControlFlow(obj,instruction,&flags,transition);
      break;
    case 0x83:
      res = GoolOpChangeAnim(obj,instruction,&flags);
      break;
    case 0x84:
      res = GoolOpChangeAnimFrame(obj,instruction,&flags);
      break;
    case 0x85:
      GoolOpTransformVectors(obj,instruction);
      break;
    case 0x86:
      GoolOpJumpAndLink(obj,instruction,&flags);
      break;
    case 0x87:
    case 0x90: {
      uint32_t idx;
      idx = (instruction >> 21) & 0x7;
      recipient = obj->links[idx];
    } /* fall through!!! */
    case 0x8F:
      res = GoolOpSendEvent(obj,instruction,&flags,recipient,opcode);
      break;
    case 0x88:
    case 0x89:
      /* rename: GoolOpStateReturn? */
      res = GoolOpReturnStateTransition(obj,instruction,&flags,transition,opcode);
      break;
    case 0x8A:
    case 0x91:
      GoolOpSpawnChildren(obj,instruction,opcode);
      break;
    case 0x8B:
      GoolOpPaging(obj,instruction);
      break;
    case 0x8C:
      GoolOpAudioVoiceCreate(obj,instruction);
      break;
    case 0x8D:
      GoolOpAudioControl(obj,instruction);
      break;
    case 0x8E:
      GoolOpReactSolidSurfaces(obj,instruction);
      break;
    }
#ifdef CFLAGS_GOOL_DEBUG
    res = GoolObjectTryBreak(obj,opcode,flags,res);
#endif
    if (ISCODE(res)) { return res; }
  }
}
#undef a
#undef r
#undef sa
#undef sr
#undef src
#undef b
#undef l
#undef sb
#undef sl
#undef dst

#ifdef CFLAGS_GOOL_DEBUG
#define GOOL_FLAGS_TRANS_BLOCK (GOOL_FLAG_SUSPEND_ON_RET | GOOL_FLAG_SUSPEND_ON_RETLNK)
#define GOOL_FLAGS_CODE_BLOCK GOOL_FLAG_SUSPEND_ON_ANIM
int GoolObjectTryBreak(gool_object *obj, uint32_t opcode, uint32_t flags, int res) {
  gool_debug *dbg;
  uint32_t offs, mask, idx;
  int brk, rtn;

  dbg = GoolObjectDebug(obj);
  if (!dbg) { return res; }
  brk = 0;
  rtn = ISCODE(res);
  if ((flags & GOOL_FLAGS_TRANS_BLOCK) == GOOL_FLAGS_TRANS_BLOCK) { /* trans block currently executing? */
    if ((dbg->flags & GOOL_FLAG_STEP_TRANS)
     && (!(dbg->flags & GOOL_FLAG_SERIAL_STEP) || !(dbg->flags & GOOL_FLAG_SERIAL_STATE))) {
      brk = 1;
      if (rtn && (dbg->flags & GOOL_FLAG_SERIAL_STEP))
        dbg->flags |= GOOL_FLAG_SERIAL_STATE;
    }
  }
  if ((flags & GOOL_FLAGS_CODE_BLOCK) == GOOL_FLAGS_CODE_BLOCK) {
    if ((dbg->flags & GOOL_FLAG_STEP_CODE)
     && (!(dbg->flags & GOOL_FLAG_SERIAL_STEP) || (dbg->flags & GOOL_FLAG_SERIAL_STATE))) {
      brk = 2;
      if (rtn && (dbg->flags & GOOL_FLAG_SERIAL_STEP))
        dbg->flags &= ~GOOL_FLAG_SERIAL_STATE;
    }
  }
  if (!brk) {
    offs = (uint32_t)(obj->pc-(uint32_t*)obj->external->items[1]);
    mask = 0x80000000 >> (offs % 32);
    idx = offs/32;
    if (dbg->breakpoints[idx] & mask) { brk=1; }
  }
  if (brk) {
    if (rtn && (opcode == 0x83 || opcode == 0x84)) {}
    else if (brk==2) {
      GoolObjectPush(obj, 0); /* wait value */
    }
    GoolObjectPause(obj, brk, ((brk==1) && !rtn));
    if (brk==1)
      dbg->flags &= ~GOOL_FLAG_STEP_TRANS;
    else if (brk==2)
      dbg->flags &= ~GOOL_FLAG_STEP_CODE;
    if (rtn) { return res; }
    return SUCCESS;
  }
  return res;
}
#endif

static inline
void GoolOp13unnamed(gool_object *obj, uint32_t instruction) {
  int32_t *inout,p1,p2,rate,prog,absprog;

  if (G_OPA(instruction) == 0xBF0) {
    p1 = GoolObjectPop(obj);
    p2 = GoolObjectPop(obj);
  }
  else {
    p1 = 0x100;
    p2 = *(GoolTranslateInGop(obj,G_OPA(instruction)));
  }
  G_TRANS_GOPB_IN(obj,instruction,inout);
  if (!inout) { return; }
  rate = *(inout);
  if (p2 >= 0) {
    if ((rate + p1) < p2)
      prog = rate + p1;
    else
      prog = (p1*2) - p2;
    absprog = abs(prog);
  }
  else {
    if (p2 >= (rate - p1))
      prog = -p2 - (p1*2);
    else
      prog = rate - p1;
    absprog = -abs(prog);
  }
  G_TRANS_GOPB_OUT(obj,instruction,inout); /* inout */
  *(inout) = prog;
  GoolObjectPush(obj, absprog);
}

static inline
void GoolOp2225unnamed(gool_object *obj, uint32_t instruction) {
  int32_t ang1,ang2,speed,angout;
  int32_t *p_ang1;
  if (G_OPA(instruction) == 0xBF0) {
    speed = GoolObjectPop(obj);
    ang2 = GoolObjectPop(obj);
  }
  else {
    speed = 0x100;
    ang2 = *(GoolTranslateInGop(obj,G_OPA(instruction)));
  }
  p_ang1 = GoolTranslateInGop(obj, G_OPB(instruction));
  if (p_ang1) {
    ang1 = *(p_ang1);
    if (G_OPCODE(instruction) == 0x22)
      angout = GoolSeek(ang1, ang2, abs(speed));
    else /* if G_OPCODE(instruction == 0x25) */
      angout = GoolObjectRotate(ang1, ang2, speed, 0);
    GoolObjectPush(obj, angout);
  }
  else
    GoolObjectPush(obj, 0);
}

static inline
void GoolOpMisc(gool_object *obj, uint32_t instruction) {
  uint32_t sop1, *ptr;
  union {
    int32_t sop2, arg, idx;
    uint32_t categories; /* bitfield */
  } r2;
#define sop2 r2.sop2
#define arg r2.arg
#define idx r2.idx
  int link_idx;
  gool_object *link;
  vec *src_trans, *dst_trans;

  G_TRANS_GOPB_IN(obj, instruction, ptr);
  sop1 = (instruction >> 20) & 0xF;
  sop2 = (int32_t)(instruction << 12) >> 27;
  switch (sop1) {
  case 0: { /* read event [service routine?] argument */
    uint32_t *args;
    if (!ptr) { break; }
    if (args = (uint32_t*)*ptr)
      GoolObjectPush(obj, args[idx]);
    else
      GoolObjectPush(obj, 0);
    break;
  }
  case 1:   /* [euclidian or manhattan] distance from obj(src) to link(dst) */
  case 6: { /* [euclidian or manhattan] distance from vec(src) to link(dst) */
    vec tmp[2];
    link_idx = (instruction >> 12) & 7;
    link = obj->links[link_idx];
    if (sop1 == 6)
      src_trans = (vec*)ptr; // ?
    else
      src_trans = &obj->trans;
    dst_trans = &link->trans;
    if (sop2 & 2) { /* restricts distance calculations to the XZ plane */
      tmp[0] = *src_trans;
      tmp[1] = *dst_trans;
      tmp[0].y = 0;
      tmp[1].y = 0;
      src_trans = &tmp[0];
      dst_trans = &tmp[1];
    }
    if (sop2 & 1)
      GoolObjectPush(obj, EucDist(src_trans, dst_trans)); /* eucDist(src_trans, dst_trans)); */
    else
      GoolObjectPush(obj, ApxDist(src_trans, dst_trans)); /* apxDist(srctrans, dsttrans));*/
    break;
  }
  case 5: { /* angle between [front face of obj] and link in XZ plane */
    uint32_t ang_xz;
    int32_t lang_xz;
    link_idx = (instruction >> 12) & 7;
    link = obj->links[link_idx];
    src_trans = &obj->trans;
    dst_trans = &link->trans;
    ang_xz = AngDistXZ(src_trans, dst_trans); /* angleXZ(src_trans, dst_trans); */
    lang_xz = (int32_t)ang_xz - angle12(obj->rot.x);
    if (abs(lang_xz) >= 0x800)
      lang_xz = angle12(-lang_xz);
    GoolObjectPush(obj, lang_xz);
    break;
  }
  case 2: { /* angle between vec and link vec */
    link_idx = (instruction >> 12) & 7;
    link = obj->links[link_idx];
    src_trans = &link->trans;
    dst_trans = (vec*)ptr;
    if (obj->status_b & 0x200200)
      GoolObjectPush(obj, AngDistXY(src_trans, dst_trans)); /* angleXY(src_trans, dst_trans)); */
    else
      GoolObjectPush(obj, AngDistXZ(src_trans, dst_trans)); /* angleXZ(src_trans, dst_trans)); */
    break;
  }
  case 3: { /* load link register */
    uint32_t reg_idx, data;
    link_idx = (instruction >> 12) & 7;
    link = obj->links[link_idx];
    reg_idx = *(ptr) >> 8;
    data = link->regs[reg_idx];
    GoolObjectPush(obj, data);
    break;
  }
  case 4: { /* set link register */
    uint32_t reg_idx, data;
    link_idx = (instruction >> 12) & 7;
    link = obj->links[link_idx];
    reg_idx = *(ptr) >> 8;
    data = GoolObjectPop(obj);
    link->regs[reg_idx] = data;
    break;
  }
  case 7: { /* find [spawned] obj with id */
    gool_handle *handle;
    gool_object *child, *found;
    uint32_t id;
    int i;
    if (!ptr) { break; }
    id = *ptr;
    if (!id || !(spawns[id>>8]&1)) { /* no obj with this id or obj with this id has not been spawned? */
      GoolObjectPush(obj, 0);
      break;
    }
    found = 0;
    for (i=3;i<5;i++) {
      handle = &handles[i];                  /* *** @ here = useless check done in the original: */
      child = GoolObjectGetChildren(handle); /* if (handle->type != 2 &&                         */
      while (child && !found) {              /*    !(found = hasPID(handle, pid_flags))) {       */
        found = (gool_object*)GoolObjectSearchTree(
          child,
          (gool_ifnptr_t)GoolObjectHasPidFlags,
          id); /*findObject(child, hasID, id);*/
        child = child->sibling;
      }
    }
    GoolObjectPush(obj, (uint32_t)found);
    break;
  }
  case 8: { /* set/clear spawn bit 2 */
    uint32_t id;
    if (!ptr) { break; }
    id = *(ptr) >> 8;
    if (sop2)
      spawns[id] &= 0xFFFFFFFD;
    else
      spawns[id] |= 2;
    break;
  }
  case 9: { /* set link zone */
    entry *zone;
    pnt *point;
    point = (pnt*)ptr;
    if (point)
      zone = ZoneFindNeighbor(cur_zone, point);
    else
      zone = cur_zone;
    if (ISSUCCESSCODE(zone)) {
      link_idx = (instruction >> 12) & 7;
      link = obj->links[link_idx];
      link->zone = zone;
    }
    break;
  }
  case 10: { /* write spawn bit */
    uint32_t id;
    if (!ptr) { break; }
    id = *(ptr) >> 8;
    switch (sop2) {
    case 0: /* clear bit 3 */
      spawns[id] &= 0xFFFFFFFB;
      break;
    case 1: /* set bit 3 */
      spawns[id] |= 4;
      break;
    case 4: { /* free spawn bits */
      lid_t lid;
      uint32_t tag;
      int i;
      lid = ns.ldat->lid;
      tag = (uint16_t)((lid << 9) | id);
      for (i=0;level_spawns[i];i++) {
        if (level_spawns[i] != tag) { continue; }
        if (level_spawns[i+1]) /* if following entry is a tail entry */
          level_spawns[i] = 1; /* replace current entry as a free entry */
        else
          level_spawns[i] = 0; /* else make current entry a tail entry*/
        break;
      }
    } /* fall through!!! */;
    case 2: /* clear bit 4 */
      spawns[id] &= 0xFFFFFFF7;
      break;
    case 5: { /* allocate spawn bits */
      zone_header *header;
      lid_t lid;
      uint32_t flags, tag, found;
      uint16_t *free_tag;
      int i;
      header = (zone_header*)cur_zone->items[0];
      if (!(header->flags & 0x2000)) { /* state can be saved in the current zone? */
        /* keeps a list of lid|oid(LLLLLLLPPPPPPPPP) */
        /* so we can record a list of the objects that have been encountered */
        lid = ns.ldat->lid;
        tag = (uint16_t)((lid << 9) | id);
        found = 0; free_tag = 0;
        for (i=0;level_spawns[i];i++) {  /* while a tail entry has not been reached */
          if (level_spawns[i] == tag) {  /* if the entry is found */
            found = 1;                   /* mark it as such */
            break;                       /* and stop the search */
          }
          if (level_spawns[i] == 1)      /* if a free entry is found */
            free_tag = &level_spawns[i]; /* remember its position but continue the search */
        }
        if (!found) { /* if entry was not found */
          if (!free_tag) /* if free spot was not found */
            free_tag = &level_spawns[i]; /* use tail as free spot */
          *(free_tag) = tag; /* put entry in free spot */
        }
      }
    } /* fall through!!! */
    case 3: /* set bit 4 */
      spawns[id] |= 8;
      break;
    case 6:
      break;
    case 7:
      break;
    case 8: /* clear bit 1 */
      spawns[id] &= 0xFFFFFFFE;
      break;
    case 9: /* set bit 1 */
      spawns[id] |= 1;
      break;
    }
    break;
  }
  case 11: { /* read spawn bit */
    uint32_t id;
    if (sop2 < 1 || sop2 > 3) { break; }
    if (!ptr) {
      GoolObjectPush(obj, 0);
      break;
    }
    id = *(ptr) >> 8;
    if (sop2 == 1) /* read bit 2 */
      GoolObjectPush(obj, !(spawns[id] & 2));
    else if (sop2 == 2) /* read bit 3 */
      GoolObjectPush(obj, spawns[id] & 4);
    else if (sop2 == 3) /* read bit 4 */
      GoolObjectPush(obj, spawns[id] & 8);
    break;
  }
  case 12: /* misc */
    switch (sop2) {
    case 0: /* save level state */
      LevelSaveState(obj, &savestate, 0); /* saveState(obj, &savestate, 0); */
      break;
    case 1: /* load level state */
      LevelRestart(&savestate); /* reloadLevel(savestate); */
      break;
    case 2: { /* add object to handle */
      gool_handle *handle;
      idx = *ptr >> 8;
      handle = &handles[idx];
      GoolObjectAddChild((gool_object*)handle, obj);
      break;
    }
    case 3:
      *(ptr); /* unused */
      break;
    case 4: { /* set object zone */
      gool_object *dst_obj;
      dst_obj = (gool_object*)(*ptr);
      dst_obj->zone = obj_zone; /* change zone */
      break;
    }
    case 5:
      MidiResetFadeStep(); /* reset midi fade step value */
      break;
    case 6:
      MidiTogglePlayback(*(ptr)); /* spu routine */
      break;
    case 7: { /* terminate all objects in cur zone neighbors */
      entry *neighbor;
      zone_header *header;
      int i, ii;
      if (!cur_zone) { break; }
      header = (zone_header*)cur_zone->items[0];
      for (i=0;i<header->neighbor_count;i++) {
        neighbor = NSLookup(&header->neighbors[i]);
        for (ii=0;ii<8;ii++)
          GoolObjectHandleTraverseTreePostorder(
            (gool_object*)&handles[ii],
            (gool_ifnptr_t)GoolZoneObjectsTerminate,
            (int)neighbor);
      }
      break;
    }
    case 8: {
      int32_t disty, distxz, atan;
      link_idx = (instruction >> 12) & 7;
      link = obj->links[link_idx];
      src_trans = (vec*)ptr;
      dst_trans = &link->trans;
      disty  = dst_trans->y - src_trans->y;
      distxz = EucDistXZ(dst_trans, src_trans); /* eucdistxz(dst_trans, src_trans); */
      atan = atan2(disty, distxz); /* atan2(disty, distxz); */
      GoolObjectPush(obj, atan);
      break;
    }
    case 9: { /* change level */
      uint32_t lid;
      lid = *(ptr) >> 8;
      next_lid = lid;
      break;
    }
    case 10: { /* seek disc to nsf */
      uint32_t lid;
      lid = *(ptr) >> 8;
#ifdef PSX
      NsCdSeek(lid); /* seek disc to location of NSF file for lid */
#endif
      break;
    }
    case 11: /* reset global variables */
      LevelResetGlobals(1);
      break;
    }
    break;
  case 13: { /* find nearest object */
    gool_nearest_query query;
    gool_handle *handle;
    gool_object *child, *sibling;
    int res;
    link_idx = (instruction >> 12) & 7;
    link = ((gool_object**)&obj->self)[link_idx];
    query.categories = (instruction >> 15) & 0x1F; /* bitfield, one bit per object category */
    query.obj = link;
    query.nearest_obj = 0;
    query.dist = 0x7FFFFFFF;
    query.event = *(ptr);
    handle = &handles[4]; /* async event subscribers */
    if (handle) {
      res = 0;
      child = GoolObjectGetChildren(handle);
      while (child && !res) {
        res = GoolObjectSearchTree(
          child,
          (gool_ifnptr_t)GoolFindNearestObject,
          (int)&query);
        child = child->sibling;
      }
    }
    GoolObjectPush(obj, (uint32_t)query.nearest_obj); /* is this correct? */
    break;
  }
  case 14: { /* test collision with point */
    bound bound;
    pnt *test;
    int res;
    link_idx = (instruction >> 12) & 7;
    link = obj->links[link_idx];
    bound.p1.x = link->trans.x + link->bound.p1.x;
    bound.p1.y = link->trans.y + link->bound.p1.y;
    bound.p1.z = link->trans.z + link->bound.p1.z;
    bound.p2.x = link->trans.x + link->bound.p2.x;
    bound.p2.y = link->trans.y + link->bound.p2.y;
    bound.p2.z = link->trans.z + link->bound.p2.z;
    test = (pnt*)ptr;
    res = TestPointInBound(test, &bound);
    GoolObjectPush(obj, res);
    break;
  }
  case 15: { /* mem card op */
    int part_idx;
    part_idx = *(ptr);
    obj->misc_memcard = CardControl(sop2, part_idx); // memory card routine
    break;
  }
  }
#undef sop2
#undef arg
#undef idx
}

static inline
int GoolOpControlFlow(gool_object *obj, uint32_t instruction, uint32_t *flags, gool_state_ref *transition) {
  uint32_t op_type, cond_type;
  int32_t offset, argc;
  gool_state *state;
  int state_idx, test, res;
  static int cond;
  static uint32_t flagspair;

  cond_type = (instruction >> 20) & 3;
  if (cond_type == 0)
    cond = 1;
  else if (cond_type == 1)
    cond = G_TRANS_REGREF(obj, (instruction >> 14) & 0x3F);
  else if (cond_type == 2)
    cond = !G_TRANS_REGREF(obj, (instruction >> 14) & 0x3F);
  /* else cond is unchanged; use val from previous invocation */
  op_type = (instruction >> 22) & 3;
  if (!cond || op_type == 3)
    return 0;
  if (op_type == 0) { /* branch */
    offset = (int32_t)((instruction & 0x3FF) << 22) >> 22;
    argc = (instruction >> 10) & 0xF;
    obj->pc += offset;
    obj->sp -= argc;
  }
  else if (op_type == 1) { /* state change */
    state_idx = instruction & 0x3FFF;
    state = (gool_state*)obj->global->items[4] + state_idx;
    if (obj->invincibility_state >= 2 && obj->invincibility_state <= 4)
      test = (obj->status_c | 0x1002) & state->flags;
    else
      test = obj->status_c & state->flags;
    if (!test) {
      res = GoolObjectChangeState(obj, state_idx, 0, (uint32_t*)0);
      if (ISERRORCODE(res) || !(*flags & GOOL_FLAG_SUSPEND_ON_RETLNK))
        GoolObjectPop(obj);
      else
        return res;
    }
  }
  else if (op_type == 2) { /* return */
    flagspair = *flags;
    res = GoolObjectPopFrame(obj, &flagspair);
    if (*flags & GOOL_FLAG_EVENT_SERVICE) {
      if (*flags & GOOL_FLAG_RETURN_EVENT) {
        transition->state = 0xFF;
        return SUCCESS;
      }
      else
        return ERROR_INVALID_STATERETURN;
    }
    if ((*flags & GOOL_FLAG_SUSPEND_ON_RET) || ISERRORCODE(res)) {
      if ((*flags & GOOL_FLAG_STATUS_PRESERVE) && ISSUCCESSCODE(res)) // if mode flags bit 5 set and valid return
        GoolObjectPush(obj, 0); // push initial value for wait
      return res;
    }
    else if ((flagspair & 0xFFFF) != 0xFFFF)
      *flags = flagspair;
  }
  return 0;
}

static inline
int GoolOpChangeAnim(gool_object *obj, uint32_t instruction, uint32_t *flags) {
  gool_anim *anim;
  uint32_t frame_idx, anim_offs, wait, flip;
  int out_of_range;

  frame_idx = instruction & 0x7F;
  anim_offs = (instruction >> 7) & 0x1FF;
  wait = (instruction >> 16) & 0x3F;
  flip = (instruction >> 22) & 0x3;
  anim = (gool_anim*)((uint32_t*)obj->global->items[5]+anim_offs);
  obj->anim_frame = frame_idx << 8;
  obj->anim_seq = anim;
  GoolObjectPush(obj, (wait << 24) | frames_elapsed);
  if (flip == 0)
    obj->scale.x = -abs(obj->scale.x);
  else if (flip == 1)
    obj->scale.x = abs(obj->scale.x);
  else if (flip == 2)
    obj->scale.x = -obj->scale.x;
  if (obj->status_b & 0x18) { /* object is solid and stopped by solid matter */
    out_of_range = OutOfRange(obj, &obj->trans, &crash->trans,
                              0x7D000, 0xAF000, 0x7D000);
    if (!out_of_range || (obj->status_b & 0x80000000))
      GoolObjectLocalBound(obj, &obj->scale);
  }
  if (*flags & GOOL_FLAG_SUSPEND_ON_ANIM)
    return SUCCESS;
  return 0;
}

static inline
int GoolOpChangeAnimFrame(gool_object *obj, uint32_t instruction, uint32_t *flags) {
  uint32_t frame_idx, anim_idx, wait, flip;
  frame_idx = *(GoolTranslateInGop(obj, G_OPB(instruction)));
  wait = (instruction >> 16) & 0x3F;
  flip = (instruction >> 22) & 0x3;
  obj->anim_frame = frame_idx;
  GoolObjectPush(obj, (wait << 24) | frames_elapsed);
  if (flip == 0)
    obj->scale.x = -abs(obj->scale.x);
  else if (flip == 1)
    obj->scale.x = abs(obj->scale.x);
  else if (flip == 2)
    obj->scale.x = -obj->scale.x;
  GoolObjectLocalBound(obj, &obj->scale);
  if (*flags & GOOL_FLAG_SUSPEND_ON_ANIM)
    return SUCCESS;
  return 0;
}

static inline
void GoolOpTransformVectors(gool_object *obj, uint32_t instruction) {
  uint32_t sop, *ptr;
  sop = (instruction >> 18) & 7;
  ptr = GoolTranslateInGop(obj, G_OPB(instruction));
  switch (sop) {
  case 0: { /* get nth point on the obj's path */
    int32_t progress;
    int trans_idx;
    vec *trans, trans_new;
    if (ptr && obj->entity) {
      progress = *(ptr); /* path progress value = point index */
      trans_idx = (instruction >> 12) & 7; /* vector in & out */
      trans = &obj->vectors_v[trans_idx];
      trans_new = *trans;
      GoolObjectOrientOnPath(obj, progress, &trans_new);
      *trans = trans_new;
      obj->floor_y = trans->y;
    }
    break;
  }
  case 1: { /* convert coords to screen coords */
    int persp_idx, ortho_idx;
    vec *persp, *ortho, *inout;
    int32_t z;
    persp_idx = (instruction >> 12) & 7; /* vector in */
    ortho_idx = (instruction >> 15) & 7; /* vector out */
    persp = &obj->vectors_v[persp_idx];
    ortho = &obj->vectors_v[ortho_idx];
    GoolProject(persp, ortho); /* transform persp to ortho */
    inout = (vec*)ptr;
    if (inout->x && (z=(ortho->z>>8))) {
      inout->x = (inout->x * 280) / z;
      inout->y = (inout->y * 280) / z;
      inout->z = (inout->z * 280) / z;
    }
    break;
  }
  case 2: { /* adjust velocity vector to point in target rot dir */
            /* spin(xz) or flip(xy) angle with input speed */
    int32_t speed, target_rotx;
    int velocity_idx;
    vec *velocity, target;
    speed = *(ptr);
    velocity_idx = (instruction >> 12) & 7; /* vector out */
    velocity = &obj->vectors_v[velocity_idx];
    target_rotx = obj->target_rot.x & 0xFFF; /* xz or xy plane */
    target.x = sin(target_rotx);
    velocity->x = ((target.x/16) * speed) >> 8;
    if (obj->status_b & 0x200200) {
      target.y = cos(target_rotx);
      velocity->y = ((target.y/16) * speed) >> 8;
    }
    else {
      target.z = cos(target_rotx);
      velocity->z = ((target.z/16) * speed) >> 8;
    }
    obj->speed = speed;
    break;
  }
  case 4:   /* linear transformation(trans, rot, and scale) */
  case 5: { /* linear transformation(trans and rot[with targetrot]) */
    int trans_idx, out_idx;
    vec *trans, in, *out;
    trans_idx = (instruction >> 12) & 7; /* trans vector in */
    out_idx = (instruction >> 15) & 7; /* vector out */
    trans = &obj->vectors_v[trans_idx];
    out = &obj->vectors_v[out_idx];
    in.z = *(ptr);
    in.y = GoolObjectPop(obj);
    in.x = GoolObjectPop(obj);
    if (sop == 4)
      GoolTransform(&in, trans, &obj->rot, &obj->scale, out);
    else
      GoolTransform(&in, trans, &obj->misc_b, 0, out);
    break;
  }
  case 6: { /* transform obj [model] vertex */
    int link_idx, out_idx, frame_idx, vert_idx;
    gool_object *link;
    gool_vertex_anim *anim;
    entry *svtx, *tgeo;
    svtx_frame *frame;
    svtx_vertex *vert;
    tgeo_header *header;
    vec in, *out, scale, model_trans, model_scale;
    link_idx = (instruction >> 21) & 7;
    link = obj->links[link_idx];
    anim = (gool_vertex_anim*)link->anim_seq;
    if (!anim || anim->type != 1) /* no anim or not a vertex anim? */
      break;
    out_idx = (instruction >> 12) & 7;
    out = &obj->vectors_v[out_idx];
    svtx = NSLookup(&anim->eid);
    frame_idx = link->anim_frame >> 8;
    frame = (svtx_frame*)svtx->items[frame_idx];
    tgeo = NSLookup(&frame->tgeo);
    header = (tgeo_header*)tgeo->items[0];
    vert_idx = *(ptr) >> 8; /* index of vert to transform */
    vert = &frame->vertices[vert_idx];
    model_trans.x = frame->bound_x1;
    model_trans.y = frame->bound_y1;
    model_trans.z = frame->bound_z1;
    model_scale.x = header->scale_x;
    model_scale.y = header->scale_y;
    model_scale.z = header->scale_z;
    in.x = (model_trans.x+(vert->x-0x80)) << 10;
    in.y = (model_trans.y+(vert->y-0x80)) << 10;
    in.z = (model_trans.z+(vert->z-0x80)) << 10;
    scale.x = (model_scale.x * link->scale.x) >> 12;
    scale.y = (model_scale.y * link->scale.y) >> 12;
    scale.z = (model_scale.z * link->scale.z) >> 12;
    GoolTransform(&in, &link->trans, &link->rot, &scale, out);
    break;
  }
  case 7: {
    int veca_idx, vecb_idx;
    vec *veca, *vecb;
    veca_idx = (instruction >> 12) & 7;
    vecb_idx = (instruction >> 15) & 7;
    veca = &obj->vectors_v[veca_idx];
    vecb = &obj->vectors_v[vecb_idx];
    GoolTransform2(veca, vecb, 0); /* audio-related vec func */
    break;
  }
  }
}

static inline
void GoolOpJumpAndLink(gool_object *obj, uint32_t instruction, uint32_t *flags) {
  uint32_t instr_idx, *code;
  int argc;
  instr_idx = instruction & 0x3FFF;
  argc = (instruction >> 20) & 0xF;
  GoolObjectPushFrame(obj, argc, *flags);
  if (instr_idx != 0x3FFF) {
    code = (uint32_t*)obj->global->items[1];
    obj->pc = &code[instr_idx];
  }
  else
    obj->pc = 0;
  *flags &= ~(GOOL_FLAG_SUSPEND_ON_RET | GOOL_FLAG_EVENT_SERVICE);
}

static inline
int GoolOpSendEvent(gool_object *obj, uint32_t instruction, uint32_t *flags, gool_object *recipient, uint32_t opcode) {
  uint32_t argv[64]; /* local stack variable (of GoolObjectInterpret) in orig impl. */
  uint32_t event, cond, argc, mode, *p_event;
  int i;
  p_event = GoolTranslateInGop(obj, G_OPB(instruction));
  obj->status_a &= ~GOOL_FLAG_KEEP_EVENT_STACK; /* clear bit 18 */
  cond = G_TRANS_REGREF(obj, (instruction >> 12) & 0x3F);
  argc = (instruction >> 18) & 0x7;
  mode = (instruction >> 21) & 0x7;
  if (p_event && cond && (recipient || opcode == 0x8F)) {
    for (i=0;i<argc;i++)
      argv[i] = obj->sp[-argc+i];
    event = *p_event;
    if (opcode == 0x8F)
      GoolSendToColliders(obj, event, mode, argc, argv);
    else if (opcode == 0x87)
      GoolSendEvent(obj, recipient, event, argc, argv);
    else if (opcode == 0x90)
      GoolSendToColliders2(obj, recipient, event, mode, argc, argv);
  }
  else
    obj->misc_flag = 0;
  if (obj->status_a & GOOL_FLAG_KEEP_EVENT_STACK) { /* if object has since changed state (as a result of the event) */
    if (*flags & GOOL_FLAG_SUSPEND_ON_RETLNK)
      return SUCCESS;
    else
      GoolObjectPop(obj);
  }
  else
    obj->sp -= argc;
  return 0;
}

static inline
int GoolOpReturnStateTransition(gool_object *obj,
  uint32_t instruction,
  uint32_t *flags,
  gool_state_ref *transition,
  uint32_t opcode) {
  static uint32_t cond;
  uint32_t cond_type, ret_type, res;
  int32_t offset;
  int argc;
  if (!(*flags & GOOL_FLAG_EVENT_SERVICE))
    return ERROR;
  cond_type = (instruction >> 20) & 3;
  if (cond_type == 0)
    cond = 1;
  else if (cond_type == 1)
    cond = G_TRANS_REGREF(obj, (instruction >> 14) & 0x3F);
  else if (cond_type == 2)
    cond = !G_TRANS_REGREF(obj, (instruction >> 14) & 0x3F);
  /* else cond is unchanged; use val from previous invocation */
  ret_type = (instruction >> 22) & 3;
  if (cond) {
    *flags |= GOOL_FLAG_RETURN_EVENT;
    transition->guard = (opcode == 0x89);
    if (ret_type == 1) {
      res = GoolObjectPopFrame(obj, flags);
      if (ISSUCCESSCODE(res))
        transition->state = (instruction & 0x3FFF);
      return res;
    }
    else if (ret_type == 2) {
      res = GoolObjectPopFrame(obj, flags);
      if (ISSUCCESSCODE(res))
        transition->state = 0xFF;
      return res;
    }
  }
  else if (ret_type == 0) {
    offset = ((int32_t)((instruction & 0x3FF) << 22)) >> 22;
    argc = (instruction >> 10) & 0xF;
    obj->pc += offset;
    obj->sp -= argc;
  }
  return 0;
}

static inline void GoolOpSpawnChildren(gool_object *obj, uint32_t instruction, uint32_t opcode) {
  int i, spawn_count, argc, flag;
  uint32_t type, subtype, *args;
  gool_object *child;

  argc = (instruction >> 20) & 0xF;
  type = (instruction >> 12) & 0xFF;
  subtype = (instruction >> 6) & 0x3F;
  spawn_count = (instruction & 0x3F);
  if (!spawn_count)
    spawn_count = obj->sp[--argc]; /* is this correct? */
  if (spawn_count > 0) {
    flag = (opcode == 0x91);
    for (i=0;i<spawn_count;i++) {
      args = &obj->sp[-argc];
      child = GoolObjectCreate(obj,type,subtype,argc,args,flag);
      if (ISERRORCODE(child))
        obj->misc_child = 0;
      else
        obj->misc_child = child;
      child->creator = obj;
    }
  }
  obj->sp -= argc; /* pop args */
}

static inline void GoolOpPaging(gool_object *obj, uint32_t instruction) {
  int sop, res, count;
  uint32_t *arg;

  sop = *GoolTranslateInGop(obj, G_OPA(instruction));
  arg = GoolTranslateInGop(obj, G_OPB(instruction));
  switch (sop) {
  case 1:
    obj->misc_entry = NSOpen((eid_t*)arg, 0, 1);
    break;
  case 2:
    obj->misc_flag = NSClose((entry*)arg, 1);
    break;
  case 3:
    res = NSClose((entry*)arg, 0);
    GoolObjectPush(obj, res);
  case 4:
    res = NSCountAvailablePages();
    GoolObjectPush(obj, res);
    break;
  case 5:
    count = *arg;
    res = NSCountAvailablePages2(&obj->sp[-count], count);
    obj->sp -= count;
    GoolObjectPush(obj, res);
    break;
  case 6:
    obj->misc_entry = NSOpen((entry*)arg, 1, 1);
    break;
  }
}

static inline void GoolOpAudioVoiceCreate(gool_object *obj, uint32_t instruction) {
  eid_t *adio;
  int vol;

  vol = *(int*)GoolTranslateInGop(obj, G_OPA(instruction));
  adio = (eid_t*)GoolTranslateInGop(obj, G_OPB(instruction));
  obj->voice_id = AudioVoiceCreate(obj, adio, vol);
}

static inline void GoolOpAudioControl(gool_object *obj, uint32_t instruction) {
  generic *arg;
  int sop, voice_id, reg_idx;
  uint32_t flags;

  arg = (generic*)GoolTranslateInGop(obj, G_OPB(instruction));
  sop = (instruction >> 20) & 0xF;
  flags = (instruction >> 18) & 0x3;
  reg_idx = (instruction >> 12) & 0x3F;
  if (reg_idx == 0)
    voice_id = 0;
  else
    voice_id = G_TRANS_REGREF(obj, reg_idx);
  if (sop == 15)
    sop = 0x80000000;
  if (flags & 1)
    sop |= 0x40000000;
  if (flags & 2)
    sop |= 0x20000000;
  AudioControl(voice_id, sop, arg, obj);
}

static inline void GoolOpReactSolidSurfaces(gool_object *obj, uint32_t instruction) {
  entry *zone;
  zone_header *header;
  vec *v_in, *v_out1, *v_out2, trans, trans2, cv_in;
  int sop, idx1, idx2, flag1, flag2, flags;
  static vec trans3, trans4; /* persists between instructions */

  idx1 = (instruction >> 12) & 7;
  idx2 = (instruction >> 15) & 7;
  sop = (instruction >> 18) & 7;
  GoolTranslateInGop(obj, G_OPB(instruction)); /* do not push or consume the in/out gop */
  v_in = &obj->vectors_v[idx1];
  v_out1 = (vec*)GoolTranslateOutGop(obj, G_OPB(instruction));
  v_out2 = &obj->vectors_v[idx2];
  cv_in = *v_in;
  trans = obj->trans;
  switch (sop) {
  case 0:
    trans.y += obj->bound.p2.y / 2;
    obj->misc_flag = ZoneReboundVector(&trans, &cv_in);
    if (G_OPB(instruction) != 0xBE0)
      *v_out1 = cv_in;
    *v_out2 = trans;
    break;
  case 1:
    trans = obj->trans;
    trans2 = obj->trans;
    if (obj == crash || obj->parent == crash)
      ZoneFindNearestObjectNode2(obj, &trans2);
    *((gool_objnode*)&obj->misc_node) = ZoneFindNearestObjectNode(obj, &trans);
    if (G_OPB(instruction) != 0xBE0)
      *v_out1 = trans3;
    *v_out2 = trans;
    break;
  case 2:
  case 3:
  case 4:
  case 5:
    flag1 = 0; flag2 = 0;
    if (sop == 3 || sop == 4) { flag1 = 1; }
    if (sop == 3 || sop == 5) { flag2 = 1; }
    trans3 = obj->trans;
    zone = obj->zone ? obj->zone : cur_zone;
    header = (zone_header*)zone->items[0];
    if (header->flags & 1)
      flags = flag2 ? 6 : 2;  /* flags identify the quadrant to test */
    else
      flags = flag2 ? 5 : 1;
    ZoneFindNearestObjectNode3(obj, &trans3, flags, !flag1);
    if (G_OPB(instruction) != 0xBE0)
      *v_out1 = trans4; /* unused? orig impl seems to store garbage if vec_out1 is used */
    *v_out2 = trans3;
    break;
  case 6:
    ZoneColorsScaleSeekByEntityNode(obj);
    break;
  }
}

int GoolSendEvent(gool_object *sender, gool_object *recipient, uint32_t event, int argc, uint32_t *argv) {
  entry *exec;
  gool_header *header;
  gool_state_maps *maps;
  gool_state *descs, *desc;
  gool_state_ref transition;
  uint32_t state, state_idx, subtype_map_idx;
  uint32_t offs, status_c;
  uint32_t *code, flags;
  int res, test, i;

  if (sender)
    sender->misc_flag = !!recipient;
  if (!recipient) { return SUCCESS; }
  recipient->interrupter = sender;
  res = ERROR_INVALID_STATERETURN; /* set default (for when no ESR) */
  if (recipient->ep) { /* recipient has event service routine? */
    GoolObjectPush(recipient, event);
    GoolObjectPush(recipient, (uint32_t)argv);
    GoolObjectPushFrame(recipient, 2, 0xFFFF);
    recipient->pc = recipient->ep;
    res = GoolObjectInterpret(recipient, GOOL_FLAG_EVENT_SERVICE, &transition); /* call it */
    if (res != ERROR_INVALID_STATERETURN) {
      if (ISERRORCODE(res))
        return res; /* return on error */
      state = transition.state; /* get response */
    }
    if (sender)
      sender->misc_flag = transition.guard;
  }
  if (ISERRORCODE(res)) { /* no ESR or invalid state return from ESR? */
    exec = recipient->global;
    header = (gool_header*)exec->items[0];
    subtype_map_idx = header->subtype_map_idx;
    state_idx = event >> 8;
    if (state_idx < subtype_map_idx) { /* index falls within event->state/interrupt map? */
      maps = (gool_state_maps*)exec->items[3];
      state = maps->event_map[state_idx];
    }
    else
      state = 0xFF; /* null state */
    if (sender)
      sender->misc_flag = (state != 0xFF);
    if (state & 0x8000) { /* is state actually an interrupt? */
      recipient->event = event;
      for (i=0;i<argc;i++)
        GoolObjectPush(recipient, argv[i]);
      GoolObjectPushFrame(recipient, argc, 0xFFFF);
      offs = state & 0x7FFF; /* offset of interrupt (in instructions) */
      code = (uint32_t*)exec->items[1];
      recipient->pc = &code[offs];
      flags = GOOL_FLAG_SUSPEND_ON_RET | GOOL_FLAG_SUSPEND_ON_RETLNK;
      res = GoolObjectInterpret(recipient, flags, 0); /* execute interrupt */
      return res; /* return */
    }
  }
  if (state != 0xFF) { /* state is non-null? */
    status_c = recipient->status_c;
    if (event == 0x1800 || event == GOOL_EVENT_SQUASH || event == GOOL_EVENT_BOULDER_SQUASH)
      status_c &= ~2;
    exec = recipient->global;
    descs = (gool_state*)exec->items[4];
    desc = &descs[state];
    if ((recipient == crash) &&
        (recipient->invincibility_state >= 2 && recipient->invincibility_state <= 4))
      status_c |= 0x1002;
    if (!(status_c & desc->flags)) {
      recipient->event = event;
      if (event == 0x1800 || event == GOOL_EVENT_SQUASH)
        recipient->status_a |= 0x10000;
      GoolObjectChangeState(recipient, state, argc, argv);
    }
    else if (sender)
      sender->misc_flag = 0;
  }
  return SUCCESS;
}

//----- (800243A0) --------------------------------------------------------
int16_t GoolObjectRotate(int16_t anga, int16_t angb, int32_t speed, gool_object *obj) {
  uint32_t scale;
  int32_t velocity, delta, abs_delta;

#ifdef PSX
  scale = min(context.c1_p->ticks_per_frame, 0x66);
#else
  scale = min(context.ticks_per_frame, 0x66);
#endif
  velocity = (speed*(int32_t)scale)/1024;
  anga = angle12(anga);
  angb = angle12(angb);
  delta = angb - anga;
  if (abs(delta) <= 0x800) /* between -180 and 180? */
    delta = delta; /* no adjustment */
  else if (delta > 0) /* between 180 and max=360? */
    delta -= 0x1000; /* adjust to -180 and 180 range */
  else /* between min=-360 and -180 */
    delta += 0x1000; /* adjust to -180 and 180 range */
  abs_delta = abs(delta);
  /* decelerate from src to target if bit 14 set;
     adjust angular velocity accordingly */
  if (obj && (obj->status_b & 0x20000000) && (abs_delta < velocity*4)) {
    if (abs_delta >= velocity*2) {  /* between 2v and 4v? */
      if (abs_delta >= velocity*3)  /* between 3v and 4v? */
        velocity = (velocity*3)/4;  /* multiply by 3/4 */
      else                          /* between 2v and 3v */
        velocity /= 2;              /* divide by 2 */
    }
    else if (abs_delta >= velocity) /* between 1v and 2v? */
      velocity /= 4;                /* divide by 4 */
    else                            /* between 0v and 1v */
      velocity /= 8;                /* divide by 8 */
  }
  if (abs_delta < velocity) { /* fast enough to reach target in 1 frame? */
    if (obj)
      obj->status_a |= GOOL_FLAG_REACHED_TROT; /* set 'rotated' flag */
    return angb; /* return target angle directly */
  }
  /* target angle is in exact opposite dir from source
     and points in the positive z direction? */
  if (abs_delta == 0x800 && angb >= 0x800)
    delta = -delta; /* favor clockwise rotation */
  if (delta >= 0) {
    if (obj)
      obj->status_a &= ~GOOL_FLAG_TROT_DIR; /* clear bit 4 (rotating counter-clockwise) */
    return angle12((int32_t)anga+velocity);
  }
  else {
    if (obj)
      obj->status_a |= GOOL_FLAG_TROT_DIR; /* set bit 4 (rotating clockwise) */
    return angle12((int32_t)anga-velocity);
  }
}

//----- (80024528) --------------------------------------------------------
int16_t GoolObjectRotate2(int16_t anga, int16_t angb, int32_t speed, gool_object *obj) {
  uint32_t scale;
  int32_t velocity, delta, abs_delta;

#ifdef PSX
  scale = min(context.c1_p->ticks_per_frame, 0x66);
#else
  scale = min(context.ticks_per_frame, 0x66);
#endif
  velocity = (speed*(int32_t)scale)/1024; /* angular speed */
  anga = angle12(anga);
  angb = angle12(angb);
  delta = angb - anga;
  if (abs(delta) <= 0x800) /* between -180 and 180? */
    delta = delta; /* no adjustment */
  else if (delta > 0) /* between 180 and max=360? */
    delta -= 0x1000; /* adjust to -180 and 180 range */
  else /* between min=-360 and -180 */
    delta += 0x1000; /* adjust to -180 and 180 range */
  abs_delta = abs(delta);
  /* delta is non-zero and either abs velocity is smaller than abs delta
     or velocity and delta have different signs? */
  if (delta != 0 && (abs_delta >= abs(velocity) || (delta^velocity)<0))
    return angle12((int32_t)anga + velocity); /* approach target angle */
  else {
    if (obj)
      obj->status_a |= GOOL_FLAG_REACHED_TROT; /* set 'reached target rot' flag */
    return angb; /* return target angle directly */
  }
}

//----- (800245F0) --------------------------------------------------------
int16_t GoolAngDiff(int16_t anga, int16_t angb) {
  int32_t delta;
  delta = angle12(angb) - angle12(anga);
  if (abs(delta) <= 0x800) /* between -180 and 180? */
    delta = delta; /* no adjustment */
  else if (delta > 0) /* between 180 and max=360? */
    delta -= 0x1000; /* adjust to -180 and 180 range */
  else /* between min=-360 and -180 */
    delta += 0x1000; /* adjust to -180 and 180 range */
  return delta;
}

//----- (80024628) --------------------------------------------------------
int32_t GoolSeek(int32_t a, int32_t b, int32_t delta) {
  int32_t diff;
  diff = b - a;
  if (delta > 0 && abs(diff) < delta)
    return b;
  if (diff > 0)
    return a + delta;
  else
    return a - delta;
}

//----- (8002465C) --------------------------------------------------------
/*
   in = fixed(16,16)
   trans = fixed(16,16)
   rot = angle12
   scale = fixed(20,12)
   out = fixed(16,16)
*/
int GoolTransform(vec *in, vec *trans, ang *rot, vec *scale, vec *out) {
  /*
     note: this is written under the assumption that
       rot->x = ((uint32_t*)&rot)[1]
       rot->y = ((uint32_t*)&rot)[0]
       rot->z = ((uint32_t*)&rot)[2]
     TODO: ensure that the ang structure has layout 'yxz'
           ensure that other code is written under this assumption
  */
  mat16 m1, m2;
  int16_t *m;
  int16_t rotx;
  vec s_in, s_out;
#ifdef PSX
  m=&m1.m;
  s=sin(-angle12(rot->z));
  c=cos(-angle12(rot->z));
  m[0][0]=c;m[0][1]=-s;m[0][2]=     0;
  m[1][0]=d;m[1][1]= c;m[1][2]=     0;
  m[2][0]=0;m[2][1]= 0;m[2][2]=0x1000;
  s=sin(-angle12(rot->y));
  c=cos(-angle12(rot->y));
  m=&m2.m;
  m[0][0]=0x1000;m[0][1]=0;m[0][2]= 0;
  m[1][0]=     0;m[1][1]=c;m[1][2]=-s;
  m[2][0]=     0;m[2][1]=s;m[2][2]= c;
  MulMatrix(&m1, &m2);
  rotx = rot->z-rot->x; /* tait-bryan angles !!! */
  s = sin(-angle12(rotx));
  c = cos(-angle12(rotx));
  m = &m2.m;
  m[0][0]= c;m[0][1]=     0;m[0][2]=s;
  m[1][0]= 0;m[1][1]=0x1000;m[1][2]=0;
  m[2][0]=-s;m[2][1]=     0;m[2][2]=c;
  MulMatrix(&m1, &m2);
  if (scale)
    ScaleMatrix(&m1, scale);
#else
  ang rot2;
  rot2.x=rot->x-rot->z;rot2.y=rot->y;rot2.z=rot->z;
  SwRotMatrixYXY(&m1, &rot2);
  if (scale) {
    SwScaleMatrix(&m2, scale);
    SwMulMatrix(&m1, &m2);
  }
#endif
  s_in.x = in->x>>4; /* 16 bit frac to 12 bit frac */
  s_in.y = in->y>>4;
  s_in.z = in->z>>4;
  GfxTransform(&s_in, &m1, &s_out);
  s_out.x <<= 4; /* 12 bit frac to 16 bit frac */
  s_out.y <<= 4;
  s_out.z <<= 4;
  s_out.x += trans->x;
  s_out.y += trans->y;
  s_out.z += trans->z;
  *out = s_out;
}

//----- (800248A0) --------------------------------------------------------
int GoolTransform2(vec *in, vec *out, int flag) {
  mat16 *m_rot;
  vec v_in, v_out;

  if (!flag) {
    m_rot = &ms_cam_rot;
    v_in.x = (in->x - cam_trans.x) >> 8;
    v_in.y = (in->y - cam_trans.y) >> 8;
    v_in.z = (in->z - cam_trans.z) >> 8;
  }
  else {
    m_rot = &ms_rot;
    v_in.x = (in->x - cam_trans_prev.x) >> 8;
    v_in.y = (in->y - cam_trans_prev.y) >> 8;
    v_in.z = (in->z - cam_trans_prev.z) >> 8;
  }
#ifdef PSX
  SetRotMatrix(m_rot);
  SetTransMatrix(&m_trans);
  RotTrans(v_in, out, &flag);
#else
  SwRotTrans(&v_in, &v_out, &mn_trans.t, m_rot);
#endif
  v_out.x <<= 8;
  v_out.y = -(out->y << 8);
  v_out.z <<= 8;
  *out = v_out;
  return SUCCESS;
}

//----- (800249E0) --------------------------------------------------------
int GoolProject(vec *in, vec *out) {
  vec v_in;
  int32_t p,flag;
  int32_t sz0,sz1,sz2;

  v_in.x = (in->x - cam_trans.x) >> 8;
  v_in.y = (in->y - cam_trans.y) >> 8;
  v_in.z = (in->z - cam_trans.z) >> 8;
#ifdef PSX
  vec16 r_out;
  SetRotMatrix(&ms_cam_rot);
  SetTransMatrix(&mn_trans);
  RotTransPers(&v_in, &r_out, &p, &flag);
  ReadSZfifo3(&sz0, &sz1, &sz2);
#else
  vec r_out;
  vec2 offs;
  offs.x=0;offs.y=0;
  SwRotTransPers(&v_in, &r_out, &mn_trans.t, &ms_cam_rot, &offs, screen_proj);
  sz2 = r_out.z;
#endif
  out->x = (r_out.x) << 8;
  out->y = -(r_out.y) << 8;
  out->z = sz2 << 8;
  return SUCCESS;
}

//----- (80024AD4) --------------------------------------------------------
int GoolCollide(gool_object *tgt, bound *tgt_bound, gool_object *src, bound *src_bound) {
  gool_object *cur_collider;
  uint32_t dist_cur, dist_new;
  bound test_bound;

  cur_collider = tgt->collider;
  if (cur_collider && cur_collider != src) {
    if (src->state_flags & 0x800) {
      src->collider = tgt;
      return ERROR_COLLISION_OVERRIDE;
    }
    else {
      dist_cur = ApxDist(&tgt->trans, &cur_collider->trans);
      dist_new = ApxDist(&tgt->trans, &src->trans);
      /* if source is further from target than target's current collider */
      /* and current collider does not have collision priority flag set */
      if (dist_new >= dist_cur && !(cur_collider->state_flags & 0x800))
        return ERROR_COLLISION_OVERRIDE;
    }
  }
  src->collider = tgt;
  tgt->collider = src;
  if (tgt->hotspot_size) {
    test_bound.p1.x = tgt_bound->p1.x + tgt->hotspot_size;
    test_bound.p1.y = tgt_bound->p1.y;
    test_bound.p1.z = tgt_bound->p1.z + tgt->hotspot_size;
    test_bound.p2.x = tgt_bound->p2.x - tgt->hotspot_size;
    test_bound.p2.y = tgt_bound->p2.y;
    test_bound.p2.z = tgt_bound->p2.z - tgt->hotspot_size;
    if (TestBoundIntersection(&test_bound, src_bound))
      tgt->status_a |= 0x1000;
  }
  if (src->hotspot_size) {
    test_bound.p1.x = src_bound->p1.x + src->hotspot_size;
    test_bound.p1.y = src_bound->p1.y;
    test_bound.p1.z = src_bound->p1.z + src->hotspot_size;
    test_bound.p2.x = src_bound->p2.x - src->hotspot_size;
    test_bound.p2.y = src_bound->p2.y;
    test_bound.p2.z = src_bound->p2.z - src->hotspot_size;
    if (TestBoundIntersection(&test_bound, tgt_bound))
      src->status_a |= 0x1000;
  }
  return SUCCESS;
}

//----- (80024CC8) --------------------------------------------------------
int GoolSendIfColliding(gool_object *recipient, gool_event_query *query) {
  gool_object *sender;
  uint32_t event, *argv;
  int argc;
  bound sender_bound, recipient_bound;
  gool_header *header;

  sender = query->sender;
  event = query->event;
  argc = query->argc;
  argv = query->argv;
  if (!query->type) {
    ++query->count;
    GoolSendEvent(sender, recipient, event, argc, argv);
    return SUCCESS;
  }
  switch (query->type) {
  case 4:
    header = (gool_header*)recipient->global->items[0];
    if (header->category != 0x300 && header->category != 0x400)
      return SUCCESS;
    /* fall through */
  case 1:
    GoolObjectCalcBound(sender, &sender_bound);
    GoolObjectCalcBound(recipient, &recipient_bound);
    if (TestBoundIntersection(&recipient_bound, &sender_bound)) {
      ++query->count;
      GoolSendEvent(sender, recipient, event, argc, argv);
    }
    break;
  case 3:
    header = (gool_header*)recipient->global->items[0];
    if (!(header->category == 0x300 || header->category == 0x400))
      return SUCCESS;
    /* fall through */
  case 2:
    GoolObjectCalcBound(sender, &sender_bound);
    if (TestPointInBound(&recipient->trans, &sender_bound)) {
      ++query->count;
      GoolSendEvent(sender, recipient, event, argc, argv);
    }
    break;
  case 5:
    header = (gool_header*)recipient->global->items[0];
    if (!(header->category == 0x300 || header->category == 0x400))
      return SUCCESS;
    GoolObjectCalcBound(recipient, &recipient_bound);
    sender_bound.p1.x = sender->trans.x - sender->misc_b.y;
    sender_bound.p1.y = sender->trans.y - sender->misc_b.x;
    sender_bound.p1.z = sender->trans.z - sender->misc_b.z;
    sender_bound.p2.x = sender->trans.x + sender->misc_b.y;
    sender_bound.p2.y = sender->trans.y + sender->misc_b.x;
    sender_bound.p2.z = sender->trans.z + sender->misc_b.z;
    if (TestBoundIntersection(&recipient_bound, &sender_bound)) {
      if (query->count < 3 || (query->count%5) == 0)
        GoolSendEvent(sender, recipient, event, argc, argv);
      ++query->count;
    }
    break;
  default:
    ++query->count;
    GoolSendEvent(sender, recipient, event, argc, argv);
  }
  return SUCCESS;
}

//----- (80025134) --------------------------------------------------------
int GoolSendToColliders(gool_object *sender, uint32_t event, int type, int argc, uint32_t *argv) {
  gool_event_query query;
  int i;

  query.sender = sender;
  query.event = event;
  query.type = type;
  query.argc = argc;
  query.argv = argv;
  query.count = 0;
  for (i=0;i<8;i++)
    GoolObjectHandleTraverseTreePostorder(
      (gool_object*)&handles[i],
      (gool_ifnptr_t)GoolSendIfColliding,
      (int)&query);
  return SUCCESS;
}

//----- (800251B8) --------------------------------------------------------
int GoolSendToColliders2(gool_object *sender, gool_object *recipients, uint32_t event, int type, int argc, uint32_t *argv) {
  gool_event_query query;
  gool_object *obj, *child, *sibling, *child_sibling;
  int res;

  query.sender = sender;
  query.event = event;
  query.type = type;
  query.argc = argc;
  query.argv = argv;
  query.count = 0;
  obj = GoolObjectGetChildren(recipients);
  while (obj) {
    child = obj->children;
    sibling = obj->sibling;
    while (child) {
      child_sibling = child->sibling;
      res = GoolObjectTraverseTreePostorder(
        child,
        (gool_ifnptr_t)GoolSendIfColliding,
        (int)&query);
      child = child_sibling;
      if (res == ERROR_INVALID_RETURN)
        GoolObjectKill(obj, 0);
    }
    res = GoolSendIfColliding(obj, &query);
    obj = sibling;
    if (res == ERROR_INVALID_RETURN)
      GoolObjectKill(recipients, 0);
  }
  return SUCCESS;
}

//----- (800252C0) --------------------------------------------------------
int GoolObjectInterrupt(gool_object *obj, uint32_t addr, int argc, uint32_t *argv) {
  int i, res; // $a0
  uint32_t *code, flags;

  for (i=0;i<argc;i++)
    *(obj->sp++) = argv[i];
  GoolObjectPushFrame(obj, 0, 0xFFFF);
  code = (uint32_t*)obj->global->items[1];
  obj->pc = &code[addr & 0x7FFF];
  flags = GOOL_FLAG_SUSPEND_ON_RET | GOOL_FLAG_SUSPEND_ON_RETLNK;
  res = GoolObjectInterpret(obj, flags, 0);
  return res;
}

//------ object debugging extensions -------------------------------------
#ifdef CFLAGS_GOOL_DEBUG

#define GOOL_DEBUG_DISLEN 8
extern char *GoolDisassemble(uint32_t, uint32_t);

void GoolObjectPrint(gool_object *obj, FILE *stream) {
  gool_header *header;
  uint8_t *stack;
  uint32_t fp, sp, base_sp, offs;
  int i;

  header = (gool_header*)obj->global->items[0];
  base_sp = header->init_sp*4;
  stack = ((uint8_t*)&obj->process) + base_sp;
  fp = (uint32_t)obj->fp - (uint32_t)stack;
  sp = (uint32_t)obj->sp - (uint32_t)stack;
  // fprintf(stream, "stack start: %x+60\n", base_sp);
  // fprintf(stream, "stack size: %x\n", sp);
  // fprintf(stream, "stack frame: %x | %x\n", base_sp+fp, base_sp+sp);
  fprintf(stream, "memory contents: \n");
  for (i=0;i<sp;i++) {
    offs = i % 16;
    if (offs == 0)
      fprintf(stream, "%03x+60:", base_sp+i);
    if (i == fp)
      fprintf(stream, "(%02x", stack[i]);
    else if (i == sp-1)
      fprintf(stream, " %02x)", stack[i]);
    else
      fprintf(stream, " %02x", stack[i]);
    if (offs == 15)
      fprintf(stream, "\n");
  }
  if (offs != 15)
    fprintf(stream, "\n");
  fprintf(stream, "\n");
}

void GoolObjectPrintDebug(gool_object *obj, FILE *stream) {
  char *line;
  uint32_t *code, ins, pc, pcins, pcs, pcn;

  GoolObjectPrint(obj, stream);
  code = (uint32_t*)obj->external->items[1];
  pcins = (uint32_t)obj->pc - (uint32_t)code;
  pcs = max(pcins - ((GOOL_DEBUG_DISLEN/2)*4), 0);
  pcn = pcs + (GOOL_DEBUG_DISLEN*4);
  // fprintf(stream, "current code window:\n");
  for (pc=pcs;pc<pcn;pc+=4) {
    ins = code[pc/4];
    line = GoolDisassemble(ins, pc);
    if (pc == pcins)
      fprintf(stream, "*** %s\n", line);
    else
      fprintf(stream, "    %s\n", line);
  }
}

static void GoolObjectPreserveFrames(gool_object *obj) {
  gool_debug *dbg;
  gool_frame *frame;
  uint32_t *pc, *fp, *sp, *psp;
  uint32_t flags, range;
  uint16_t rsp, rfp;
  uint32_t *prev_sp;
  int i,j,frame_count;

  dbg = GoolObjectDebug(obj);
  if (!dbg) { return; }
  dbg->prev_pc = obj->pc;
  fp = obj->fp; sp = obj->sp;
  frame_count = 0;
  do {
    flags = *fp;
    range = *(fp+2);
    rfp=range>>16;
    fp = (uint32_t*)((uint8_t*)&obj->process + rfp);
    ++frame_count;
  } while (flags != 0xFFFF);
  fp = obj->fp; sp = obj->sp;
  i=0;
  for (i=frame_count-1;i>=0;i--) {
    frame = &dbg->frames[i];
    flags = *fp;
    range = *(fp+2);
    rsp=range&0xFFFF;rfp=range>>16;
    psp = (uint32_t*)((uint8_t*)&obj->process + rsp);
    frame->argc = fp - psp;
    for (j=0;psp!=fp;j++)
      frame->argv[j] = *(psp++);
    pc = (uint32_t*)*(fp+1);
    frame->flags = flags;
    frame->pc = pc;
    if (i==0) { frame->pc = (uint32_t*)-1; }
    fp+=3;
    for (j=0;fp!=sp;j++)
      frame->data[j] = *(fp++);
    frame->len = j;
    fp = (uint32_t*)((uint8_t*)&obj->process + rfp);
    sp = (uint32_t*)((uint8_t*)&obj->process + rsp);
  }
  dbg->frame_count = frame_count;
  obj->pc = pc;
  obj->fp = fp;
  obj->sp = sp;
}

static void GoolObjectRestoreFrames(gool_object *obj) {
  gool_debug *dbg;
  gool_frame *frame;
  int i,j;

  dbg = GoolObjectDebug(obj);
  if (!dbg) { return; }
  for (i=0;i<dbg->frame_count;i++) {
    frame = &dbg->frames[i];
    for (j=0;j<frame->argc;j++)
      GoolObjectPush(obj, frame->argv[j]);
    GoolObjectPushFrame(obj, frame->argc, frame->flags);
    if ((int)frame->pc != -1)
      *(obj->fp+1) = (uint32_t)frame->pc;
    for (j=0;j<frame->len;j++)
      GoolObjectPush(obj, frame->data[j]);
  }
  obj->pc = dbg->prev_pc;
}

static gool_debug *GoolObjectDebugAlloc() {
  gool_debug_cache *cache;
  int i;

  cache = &debug_cache;
  if (!cache->inited) {
    for (i=0;i<512;i++)
      cache->free_dbgs[i] = &cache->debugs[i];
    cache->free_count=512;
    cache->inited = 1;
  }
  if (cache->free_count == 0) { return (gool_debug*)-1; }
  i = --cache->free_count;
  return cache->free_dbgs[i];
}

static void GoolObjectDebugFree(gool_debug *dbg) {
  gool_debug_cache *cache;
  int i;

  cache = &debug_cache;
  if (!cache->inited) { return; }
  i = cache->free_count++;
  cache->free_dbgs[i] = dbg;
}

gool_debug *GoolObjectDebug(gool_object *obj) {
  gool_debug *dbg;
  uint32_t id;
  int i;

  dbg = 0;
  if (obj->entity) {
    id = obj->pid_flags >> 8;
    dbg = debug_cache.by_id[id];
    if (!dbg) {
      dbg = debug_cache.by_id[id] = GoolObjectDebugAlloc();
    }
  }
  else {
    for (i=0;i<32;i++) {
      dbg = debug_cache.by_ptr[i];
      if (!dbg) {
        dbg = debug_cache.by_ptr[i] = GoolObjectDebugAlloc();
        break;
      }
      if (dbg->obj == obj) { break; }
    }
  }
  return dbg;
}

void GoolObjectPause(gool_object *obj, int type, int trans) {
  gool_debug *dbg;
  int block_type;

  dbg = GoolObjectDebug(obj);
  if (!dbg) { return; }
  if (type == 0)
    dbg->flags |= GOOL_FLAGS_PAUSED;
  else if (type == 1)
    dbg->flags |= GOOL_FLAG_PAUSED_TRANS;
  else if (type == 2)
    dbg->flags |= GOOL_FLAG_PAUSED_CODE;
  if (trans) {
    GoolObjectPreserveFrames(obj);
    dbg->flags |= GOOL_FLAG_SAVED_TRANS;
  }
}

void GoolObjectResume(gool_object *obj, int type) {
  gool_debug *dbg;

  dbg = GoolObjectDebug(obj);
  if (!dbg) { return; }
  if (type == 0)
    dbg->flags &= ~GOOL_FLAGS_PAUSED;
  else if (type == 1)
    dbg->flags &= ~GOOL_FLAG_PAUSED_TRANS;
  else if (type == 2)
    dbg->flags &= ~GOOL_FLAG_PAUSED_CODE;
  if (dbg->flags & GOOL_FLAG_SAVED_TRANS) {
    GoolObjectRestoreFrames(obj);
    dbg->flags &= ~GOOL_FLAG_SAVED_TRANS;
    dbg->flags |= GOOL_FLAG_RESTORED_TRANS;
  }
}

void GoolObjectStep(gool_object *obj, int type) {
  gool_debug *dbg;

  dbg = GoolObjectDebug(obj);
  if (!dbg) { return; }
  GoolObjectResume(obj, type);
  if (type == 0)
    dbg->flags |= GOOL_FLAGS_STEP;
  else if (type == 1)
    dbg->flags |= GOOL_FLAG_STEP_TRANS;
  else if (type == 2)
    dbg->flags |= GOOL_FLAG_STEP_CODE;
}

#endif
