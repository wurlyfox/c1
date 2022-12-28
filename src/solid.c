#include "solid.h"
#include "math.h"
#include "geom.h"
#include "gool.h"
#include "level.h"

/* .data */
const uint32_t circle_bitmap[32] = {
  0b00000000000011111111000000000000,
  0b00000000001111111111110000000000,
  0b00000000111111111111111100000000,
  0b00000001111111111111111110000000,
  0b00000011111111111111111111000000,
  0b00000111111111111111111111100000,
  0b00001111111111111111111111110000,
  0b00011111111111111111111111111000,
  0b00111111111111111111111111111100,
  0b00111111111111111111111111111100,
  0b01111111111111111111111111111110,
  0b01111111111111111111111111111110,
  0b11111111111111111111111111111111,
  0b11111111111111111111111111111111,
  0b11111111111111111111111111111111,
  0b11111111111111111111111111111111,
  0b11111111111111111111111111111111,
  0b11111111111111111111111111111111,
  0b11111111111111111111111111111111,
  0b11111111111111111111111111111111,
  0b01111111111111111111111111111110,
  0b01111111111111111111111111111110,
  0b00111111111111111111111111111100,
  0b00111111111111111111111111111100,
  0b00011111111111111111111111111000,
  0b00001111111111111111111111110000,
  0b00000111111111111111111111100000,
  0b00000011111111111111111111000000,
  0b00000001111111111111111110000000,
  0b00000000111111111111111100000000,
  0b00000000001111111111110000000000,
  0b00000000000011111111000000000000
};

/* all integer-valued points in the upper right quadrant
   of a 32x32 square centered at the origin, such that x >= y,
   sorted by distance from the origin */
const vec28 sorted_points[152] ={
  { 1, 0},{ 1, 1},{ 2, 0},{ 2, 1},{ 2, 2},{ 3, 0},{ 3, 1},{ 3, 2},
  { 4, 0},{ 4, 1},{ 3, 3},{ 4, 2},{ 5, 0},{ 4, 3},{ 5, 1},{ 5, 2},
  { 4, 4},{ 5, 3},{ 6, 0},{ 6, 1},{ 6, 2},{ 5, 4},{ 6, 3},{ 7, 0},
  { 7, 1},{ 5, 5},{ 6, 4},{ 7, 2},{ 7, 3},{ 6, 5},{ 8, 0},{ 7, 4},
  { 8, 1},{ 8, 2},{ 6, 6},{ 8, 3},{ 7, 5},{ 8, 4},{ 9, 0},{ 9, 1},
  { 7, 6},{ 9, 2},{ 8, 5},{ 9, 3},{ 9, 4},{ 7, 7},{ 8, 6},{10, 0},
  {10, 1},{10, 2},{ 9, 5},{10, 3},{ 8, 7},{10, 4},{ 9, 6},{11, 0},
  {11, 1},{11, 2},{10, 5},{ 8, 8},{ 9, 7},{11, 3},{10, 6},{11, 4},
  {12, 0},{ 9, 8},{12, 1},{11, 5},{12, 2},{10, 7},{12, 3},{11, 6},
  {12, 4},{ 9, 9},{10, 8},{12, 5},{13, 0},{13, 1},{11, 7},{13, 2},
  {13, 3},{12, 6},{10, 9},{11, 8},{13, 4},{12, 7},{13, 5},{14, 0},
  {14, 1},{10,10},{14, 2},{11, 9},{14, 3},{13, 6},{12, 8},{14, 4},
  {13, 7},{14, 5},{11,10},{12, 9},{15, 0},{15, 1},{15, 2},{14, 6},
  {13, 8},{15, 3},{15, 4},{11,11},{12,10},{14, 7},{13, 9},{15, 5},
  {16, 0},{16, 1},{14, 8},{16, 2},{15, 6},{16, 3},{12,11},{13,10},
  {16, 4},{15, 7},{14, 9},{16, 5},{12,12},{15, 8},{13,11},{16, 6},
  {14,10},{16, 7},{15, 9},{13,12},{14,11},{16, 8},{15,10},{16, 9},
  {13,13},{14,12},{15,11},{16,10},{14,13},{15,12},{16,11},{14,14},
  {15,13},{16,12},{15,14},{16,13},{15,15},{16,14},{16,15},{16,16}
};

/* note: these consts and not originally part of the .data section
   they are embedded in load instructions */
#define vec_shl8(x,y,z) {(int)(x*256),(int)(y*256),(int)(z*256)}
bound test_bound_default = { vec_shl8( -100,      0,  -100),
                             vec_shl8(  100,      0,   100) };
bound test_bound_event   = { vec_shl8( -150,      0,  -150),
                             vec_shl8(  150,    665,   150) };
bound test_bound_surface = { vec_shl8(-6.25,      0, -6.25),
                             vec_shl8( 6.25,    665,  6.25) };
bound test_bound_zone    = { vec_shl8( -300, -267.5,  -300),
                             vec_shl8(  300,  932.5,   300) };
bound test_bound_obj     = { vec_shl8(  -75,      0,   -75),
                             vec_shl8(   75,    665,    75) };
bound test_bound_objtop  = { vec_shl8(  -75, 498.75,   -75),
                             vec_shl8(   75,    665,    75) };
bound test_bound_ceil    = { vec_shl8(-37.5, 498.75, -37.5),
                             vec_shl8( 37.5,    665,  37.5) };
/* .sdata */
int32_t land_offset = 62500;     /* 800564BC; gp[0x30] */
/* .sbss */
int being_stopped;               /* 800565DC; gp[0x78] */
uint32_t *wall_cache;            /* 800566AC; gp[0xAC] */
uint32_t *wall_bitmap;           /* 800566F4; gp[0xBE] */
uint32_t *p_circle_bitmap;       /* 8005670C; gp[0xC4] */
/* .bss */
vec prev_velocity;               /* 800567F8 */
zone_query cur_zone_query;       /* 8005CFEC */

extern ns_struct ns;
extern entry *cur_zone;
extern gool_bound object_bounds[28];
extern int object_bound_count;
extern int frames_elapsed;

//----- (8002BE8C) --------------------------------------------------------
void TransSmoothStopAtSolid(gool_object *obj, vec *velocity, zone_query *query) {
  vec cur_trans, cur_velocity;
  vec next_trans, next_velocity;
  vec delta_trans, slope_accel;
  uint32_t arg;

  if (!obj) {
    being_stopped = 0;
    return;
  }
  if (ns.ldat->lid == LID_HOGWILD || ns.ldat->lid == LID_WHOLEHOG) /* hog wild or whole hog? */
    land_offset = 162500; /* higher land */
  else
    land_offset =  62500;
  cur_velocity = *velocity;
  cur_trans = obj->trans;
  if (being_stopped) { /* supported by a solid surface? */
    slope_accel.x = prev_velocity.x - cur_velocity.x;
    slope_accel.y = prev_velocity.y - cur_velocity.y;
    slope_accel.z = prev_velocity.z - cur_velocity.z;
    if (abs(slope_accel.x) < 10
      && abs(slope_accel.y) < 10
      && abs(slope_accel.z) < 10) {
      cur_velocity.x = 0;
      cur_velocity.y = velocity->y;
      cur_velocity.z = 0;
    }
  }
  next_velocity = cur_velocity; /* inout */
  next_trans = cur_trans; /* inout */
  TransPullStopAtSolid(obj, query, &next_trans, &next_velocity);
  delta_trans.x = next_trans.x - cur_trans.x;
  delta_trans.y = next_trans.y - cur_trans.y;
  delta_trans.z = next_trans.z - cur_trans.z;
  if (!being_stopped && (velocity->x != 0 || velocity->z != 0)
    && abs(delta_trans.x) < 2
    && abs(delta_trans.y) < 2
    && abs(delta_trans.z) < 2) {
    prev_velocity = *velocity; /* save velocity for next run */
    being_stopped = 1;
  }
  else
    being_stopped = 0;
  obj->trans = next_trans; /* finally set the new trans */
  if (obj->status_a & 0x400) {
    arg = 0x6400;
    if (!(obj->status_a & 1))
      GoolSendEvent(0, obj, obj->event, 1, &arg);
    else if (obj->event != GOOL_EVENT_BOX_STACK_BREAK)
      GoolSendEvent(0, obj, obj->event, 1, &arg);
  }
}

//----- (8002C184) --------------------------------------------------------
void TransPullStopAtSolid(gool_object *obj, zone_query *query, vec *trans, vec *velocity) {
  vec delta_trans, max_delta_trans;
  vec next_trans;
  uvec abs_mag;
  uint32_t max_mag;

  query->i = 0; /* reset node counter */
  /* calculate the magnitudes which can be divided by invidually
     to get 3 scaled velocity vectors
     with x, y, and z maxed out, respectively
     (which will be used to set an upper limit on change in trans) */
  abs_mag.x = abs(velocity->x) / 25600;
  abs_mag.y = abs(velocity->y) / 153600;
  abs_mag.z = abs(velocity->z) / 25600;
  /* choose the max of the 3 mags */
  max_mag = max(max(abs_mag.x, abs_mag.y), abs_mag.z)+1;
  /* calculate upper limit on change in trans;
     either x or z will max out at 25600 or y at 153600 */
  max_delta_trans.x = velocity->x / (int)max_mag;
  max_delta_trans.y = velocity->y / (int)max_mag;
  max_delta_trans.z = velocity->z / (int)max_mag;
  /* velocity from hereon will hold the 'remaining trans' */
  while (velocity->x != 0 || velocity->y != 0 || velocity->z != 0) { /* trans remaining? */
    /* set delta trans initially to velocity (i.e. 'remaining trans') */
    delta_trans = *velocity;
    /* limit delta_trans */
    if (abs(delta_trans.x) >= abs(max_delta_trans.x))
      delta_trans.x = max_delta_trans.x;
    if (abs(delta_trans.y) >= abs(max_delta_trans.y))
      delta_trans.y = max_delta_trans.y;
    if (abs(delta_trans.z) >= abs(max_delta_trans.z))
      delta_trans.z = max_delta_trans.z;
    /* calculate next trans for this iteration */
    TransStopAtSolid(obj, query, trans, &delta_trans, &next_trans);
    /* set the calculated next trans */
    trans->x = next_trans.x;
    trans->y = next_trans.y;
    trans->z = next_trans.z;
    /* decrement velocity (i.e. 'remaining trans') */
    velocity->x -= delta_trans.x;
    velocity->y -= delta_trans.y;
    velocity->z -= delta_trans.z;
  }
}

/**
 * process a zone octree leaf node
 *
 * obj - object with node
 * node - leaf node
 * return - boolean (unknown)
 */
//----- (8002C3B8) --------------------------------------------------------
static int ProcessNode(gool_object *obj, uint32_t node) {
  int type, subtype;

  type = ((node & 0xE) >> 1) + 1;
  subtype = ((node & 0x3F0) >> 4);
  if (node == 0) { type = 0; }
  switch (subtype) {
  case 1:
    obj->event     = 0x700;
    obj->status_a |= 0x400;
    break;
  case 2:
    obj->event     = 0xC00;
    obj->status_a |= 0x400;
    break;
  case 3:
    obj->event     = GOOL_EVENT_DROWN;
    obj->status_a |= 0x400;
    break;
  case 4:
    obj->event     = GOOL_EVENT_BURN;
    obj->status_a |= 0x400;
    break;
  case 5:
    obj->event     = GOOL_EVENT_EXPLODE;
    obj->status_a |= 0x400;
    break;
  case 6:
    obj->event     = 0xD00;
    obj->status_a |= 0x400;
    break;
  case 7:
  case 8:
  case 9:
  case 10:
    obj->event     = 0x1200;
    obj->status_a |= 0x2000;
    break;
  case 11:
    obj->event     = GOOL_EVENT_FALL_KILL;
    obj->status_a |= 0x400;
    return 1;
  case 12:
    obj->event     = GOOL_EVENT_SHOCK;
    obj->status_a |= 0x400;
    break;
  }
  if (type == 4) /* pit death type a? */ {
    if (ns.ldat->lid == LID_CORTEXPOWER || ns.ldat->lid == LID_TOXICWASTE) { /* cortex power or toxic waste? */
      obj->process.event     = GOOL_EVENT_DROWN;
      obj->process.status_a |= 0x400;
      return 0;
    }
    else {
      obj->process.event     = GOOL_EVENT_FALL_KILL;
      obj->process.status_a |= 0x400;
      return 1;
    }
  }
  else if (type == 3 || type == 5) /* wall node or pit death type b? */
    return 1;
  return 0;
}

/*
  note: annotations for the limiting functions below are expressed in present tense (for brevity)
        but some actually refer to a possible future state of the object instead of its
        current state. this particular state is one which will occur in the next frame if
        the object continues to approach next_trans.
        any side-effects are in anticipation of that future state
*/
//----- (8002C660) --------------------------------------------------------
static int StopAtHighestObjectBelow(gool_object *obj, vec *trans, vec *next_trans, zone_query *query) {
  entry *exec;
  gool_header *header;
  gool_bound *bound, *found;
  int32_t max_y, test_y, delta_y;
  int i, colliding;

  max_y = -999999999;
  if (object_bound_count == 0) { return max_y; }
  delta_y = trans->y - next_trans->y;
  test_y = (delta_y > 0 ? trans->y : next_trans->y) + 62500;
  GoolCalcBound(&test_bound_obj, next_trans, &query->collider_bound);
  query->i += 16;
  found=0;
  for (i=0;i<object_bound_count;i++) {
    bound = &object_bounds[i];
    colliding = 0;
    if ((test_y >= bound->p2.y || (bound->obj->status_b & 0x400000))
      && max_y < bound->p2.y
      && TestBoundIntersection(&query->collider_bound, &bound->bound)) {
      found = bound;
      query->i += 4096;
      colliding = 1;
    }
    if (colliding && (bound->obj->status_b & GOOL_FLAG_SOLID_TOP)) {
      exec = bound->obj->global;
      header = (gool_header*)exec->items[0];
      if ((!(obj->state_flags & GOOL_FLAG_FLING_STATE)
       && obj->invincibility_state != 5)
       || header->category != 0x300
       || ((bound->obj->status_c & 0x1012) && !(bound->obj->state_flags & 10020)))
        max_y = bound->p2.y + 1;
    }
  }
  if (found)
    GoolCollide(obj, &query->collider_bound, found->obj, &found->bound);
  return max_y;
}

//----- (8002C8EC) --------------------------------------------------------
static int StopAtFloor(gool_object *obj, vec *trans, vec *next_trans, zone_query *query, int *out) {
  int32_t avg_y[3], test_y, delta_y;
  int32_t floor_nodes_y, solid_nodes_y, solid_objs_y;
  int32_t floor, max_floor, floor_offset;
  int flags;

  floor_offset = 0;
  GoolCalcBound(&test_bound_event, next_trans, &query->collider_bound);
  if (!query->once
    || !TestBoundInBound(&query->collider_bound, &query->nodes_bound))
    ZoneQueryOctrees(next_trans, obj, query);
  delta_y = trans->y - next_trans->y;
  test_y = (delta_y > 0 ? trans->y : next_trans->y) + land_offset;
  GoolCalcBound(&test_bound_surface, next_trans, &query->collider_bound);
#ifdef PSX
  RFindFloorY(obj,
    query,
    &query->nodes_bound,
    &query->collider_bound,
    test_y,
    (zone_query_summary*)avg_y,
    -999999999,
    ProcessNode);
#else
  FindFloorY(obj,
    query,
    &query->nodes_bound,
    &query->collider_bound,
    test_y,
    (zone_query_summary*)avg_y,
    -999999999,
    ProcessNode);
#endif
  solid_nodes_y = avg_y[1]; /* avg. height of wall/scenery nodes */
  floor_nodes_y = avg_y[2]; /* avg. height of floor/valid event/level bound nodes */
  solid_objs_y = StopAtHighestObjectBelow(obj, trans, next_trans, query);
  query->floor_nodes_y = floor_nodes_y != -999999999 ? floor_nodes_y : 0;
  query->solid_nodes_y = solid_nodes_y != -999999999 ? solid_nodes_y : 0;
  query->solid_objs_y  = solid_objs_y  != -999999999 ? solid_objs_y  : 0;
  flags = 0x40001; /* assume on the floor and hitting the floor */
  if (solid_objs_y != -999999999) { /* solid object below? */
    floor_nodes_y = solid_objs_y; /* overrides the floor */
    flags = 0x200001; /* on an object and hitting at the top */
    if (obj->collider) { /* object is set as the collider? */
      if (obj->collider->status_b & 0x400000) /* object beneath us is a box? */
        floor_offset = 0x19000; /* set floor offset to box height */
      if ((obj->anim_stamp - obj->floor_impact_stamp) >= 4) /* more than 4 frames since last hitting floor? */
        flags |= 0x4000; /* breaking/bouncing(?) */
    }
  }
  if (obj->velocity.y > 0) { /* jumping/launching? */
    /* no toggle, no bounce/break box, not on ground, no collide box */
    obj->status_a &= ~0x244001; /* clear bits 1,15,19,22 */
  }
  if ((floor_nodes_y == -999999999 && solid_nodes_y == -999999999)
   || obj->velocity.y > 0) { /* not atop the floor or wall? */
    query->floor = 0; /* not on any kind of floor */
    return -999999999;
  }
  if (floor_nodes_y != -999999999) { /* collision with solid floor node? */
    floor = floor_nodes_y; /* takes precedence over solid non-floor node collision */
    max_floor = obj->trans.y + floor_offset + land_offset;
  }
  else { /* collision with solid non-floor node */
    floor = solid_nodes_y;
    max_floor = obj->trans.y;
    flags = 1;
  }
  query->floor = floor;
  floor += 1;
  if (floor > max_floor) { floor = obj->trans.y; }
  *out = max_floor;
  next_trans->y = floor; /* force new y position to the floor */
  query->collider_bound.p1.y = floor;
  query->collider_bound.p2.y = floor + 0x29900;
  if (!query->once
    || !TestBoundInBound(&query->collider_bound, &query->nodes_bound))
    ZoneQueryOctrees(next_trans, obj, query); /* requery with the new bounds */
  if (obj->velocity.y < 0 && (flags & 1)) { /* falling and just hit solid ground? */
    obj->floor_impact_velocity = obj->velocity.y; /* record y velocity at impact */
    obj->velocity.y = 0; /* stop falling */
  }
  obj->status_a |= flags;
  obj->floor_impact_stamp = frames_elapsed; /* record timestamp for this floor collision */
  return floor;
}

//----- (8002CD9C) --------------------------------------------------------
static int StopAtCeil(gool_object *obj, vec *next_trans, zone_query *query) {
  entry *neighbor;
  zone_header *header;
  zone_rect *rect;
  gool_bound *bound, *found;
  vec above_zone;
  int32_t min_y, ceil;
  uint32_t arg;
  int i;

  min_y = -999999999;
  found = 0;
  GoolCalcBound(&test_bound_objtop, next_trans, &query->collider_bound);
  for (i=0;i<object_bound_count;i++) {
    bound = &object_bounds[i];
    if (!(bound->obj->status_b & GOOL_FLAG_SOLID_BOTTOM)) { continue; } /* skip objects which are not solid at the bottom */
    if (TestBoundIntersection(&query->collider_bound, &bound->bound)) {
      found = bound;
      if (min_y == -999999999 || bound->p1.y <= min_y)
        min_y = bound->p1.y;
    }
  }
  GoolCalcBound(&test_bound_ceil, next_trans, &query->collider_bound);
#ifdef PSX
  ceil = RFindCeilY(obj,
    query,
    &query->nodes_bound,
    &query->collider_bound,
    2,
    1,
    -999999999);
#else
  ceil = FindCeilY(obj,
    query,
    &query->nodes_bound,
    &query->collider_bound,
    2,
    1,
    -999999999);
#endif
  header = (zone_header*)obj->zone->items[0];
  if (header->flags & 0x20000) { /* obj's zone is solid at the top? */
    /* first ensure that there is no other zone above it */
    /* find a neighbor zone which contains the pt 0x2580 units above the obj */
    above_zone.x = next_trans->x;
    above_zone.y = query->collider_bound.p2.y;
    above_zone.z = next_trans->z;
    neighbor = ZoneFindNeighbor(cur_zone, &above_zone);
    if (ISERRORCODE(neighbor)) { /* no such zone? */
      /* then it is ok to limit ceil (to the top of the obj zone)  */
      rect = (zone_rect*)obj->zone->items[1];
      if ((rect->y << 8) < above_zone.y)
        ceil = rect->y << 8;
    }
  }
  /* found an object that is hit from the bottom
     and either no ceiling found or the object is lower than the ceiling? */
  if (min_y != -999999999 && (ceil == -999999999 || min_y < ceil)) {
    found->obj->status_a |= GOOL_FLAG_HIT_AT_BOTTOM; /* set flag for 'hit from the bottom' */
    arg = 0x6400;
    GoolSendEvent(obj, found->obj, 0x1700, 1, &arg); /* ...and send the corresponding event */
    return min_y; /* return y location of the object bottom */
  }
  else /* the ceiling is hit */
    return ceil; /* so return that value */
}

//----- (8002D18C) --------------------------------------------------------
static int StopAtZone(gool_object *obj, vec *next_trans) {
  entry *neighbor;
  zone_header *header;
  zone_rect *rect;
  uint32_t arg;

  neighbor = ZoneFindNeighbor(cur_zone, next_trans);
  if (ISERRORCODE(neighbor)) { /* obj is not within the current zone or any of its neighbors? */
    rect = (zone_rect*)obj->zone->items[1];
    if (next_trans->y + obj->bound.p1.y < (rect->y << 8)) { /* below the top of the zone? */
      if (ns.ldat->lid == LID_RIPPERROO) { /* ripper roo? */
        arg = 0;
        GoolSendEvent(0, obj, GOOL_EVENT_DROWN, 1, &arg);
      }
      header = (zone_header*)obj->zone->items[0];
      if ((header->flags & 2) && obj->invincibility_state != 2) { /* zone not solid at bottom? */
        arg = 0x6400;
        GoolSendEvent(0, obj, GOOL_EVENT_FALL_KILL, 1, &arg); /* fall into a hole and die */
      }
      else { /* zone is solid at bottom */
        next_trans->y = (rect->y << 8) - obj->bound.p1.y; /* readjust to bottom of zone */
        obj->floor_impact_velocity = obj->velocity.y; /* record y velocity of impact */
        obj->velocity.y = 0; /* stop falling */
        obj->status_a |= GOOL_FLAG_GROUNDLAND; /* set flag for 'stopped by floor' */
        obj->floor_impact_stamp = frames_elapsed; /* record timestamp for this floor collision */
      }
    }
  }
  else
    obj->zone = neighbor; /* set objects zone to correct */
  if (!obj->zone) { return -255; } /* return error if still no zone */
  header = (zone_header*)obj->zone->items[0];
  if ((header->flags & 4) && obj->trans.y < header->water_y /* lower than water? */
    && (ns.ldat->lid == LID_UPSTREAM || ns.ldat->lid == LID_UPTHECREEK)) { /* upstream or up the creek? */
    arg = 0x27100;
    GoolSendEvent(0, obj, GOOL_EVENT_DROWN, 1, &arg); /* water death */
  }
  return SUCCESS;
}

//----- (8002D384) --------------------------------------------------------
void TransStopAtSolid(gool_object *obj, zone_query *query, vec *trans, vec *delta_trans, vec *next_trans) {
  vec bit_idx, adj_bit_idx, adj_trans;
  int32_t floor, ceil;
  int found;

  adj_trans.x = trans->x + delta_trans->x;
  adj_trans.y = trans->y + delta_trans->y;
  adj_trans.z = trans->z + delta_trans->z;
  StopAtFloor(obj, trans, &adj_trans, query, &floor);
  PlotWalls(trans, obj, query); /* 1) */
  bit_idx.x = (((adj_trans.x-trans->x)<<2)/8192) + 16; /* 2) */
  bit_idx.z = (((adj_trans.z-trans->z)<<2)/8192) + 16;
  found = StopAtWalls(trans, bit_idx.x, bit_idx.z, &adj_bit_idx.x, &adj_bit_idx.z, obj, 0); /* 3) */
  if (found) { /* was a non-solid bit found (in place of or near the desired dest location)? */
    /* convert [adjusted] index back to a relative location and add to trans */
    adj_trans.x = trans->x + (((adj_bit_idx.x - 16)*8192)>>2); /* readjust */
    adj_trans.z = trans->z + (((adj_bit_idx.z - 16)*8192)>>2);
  }
  else {
    adj_trans.x = trans->x; /* cannot move in xz plane */
    adj_trans.z = trans->z;
  }
  query->i++;
  if ((bit_idx.x != 16 || bit_idx.z != 16) /* does this count as movement at the bitmap scale? 4) */
    && adj_bit_idx.x == bit_idx.x && adj_bit_idx.z == bit_idx.z) /* no readjustment due to solid wall? 5) */
    obj->status_a |= 0x100; /* set flag for 'not stopped by a wall' */
  ceil = StopAtCeil(obj, &adj_trans, query); /* check for solids above crash */
  if (ceil != -999999999) /* was a solid found? */
    query->ceil = ceil; /* record its y location */
  else
    query->ceil = 0;
  /* ceiling y location above bottom of object bound box? */
  if (ceil != -999999999 && ceil < (adj_trans.y+0x29901)) {
    if (trans->y < ceil-0x29901) /* top of object above ceiling? */
      adj_trans.y = ceil-0x29901; /* stop object */
    if (obj->velocity.y > 0) /* also clear positive y velocity */
      obj->velocity.y = 0;
    obj->status_a |= 0x80; /* set flag for 'hit the ceiling' */
  }
  StopAtZone(obj, &adj_trans); /* stop at solid zone floors */
  *next_trans = adj_trans;
}

//----- (8002D638) --------------------------------------------------------
int BinfInit() {
#ifdef PSX
  int i;
  wall_bitmap = scratch.wall_bitmap;
  wall_cache = scratch.wall_cache;
  p_circle_bitmap = scratch.circle_bitmap;
  for (i=0;i<32;i++)
    p_circle_bitmap[i] = circle_bitmap[i];
#else
  wall_bitmap = (uint32_t*)malloc(32*sizeof(uint32_t));
  wall_cache = (uint32_t*)malloc(2*64*sizeof(uint32_t));
  p_circle_bitmap = (uint32_t*)circle_bitmap;
#endif
  return SUCCESS;
}

//----- (8002D694) --------------------------------------------------------
int BinfKill() {
#ifndef PSX
  free(wall_bitmap);
  free(wall_cache);
#endif
  return SUCCESS;
}

/**
 * set a 16 unit radius circular region of bits in the wall bitmap
 *
 * x - center of circle x index
 * z - center of circle z index
 */
//----- (8002D69C) --------------------------------------------------------
static void PlotWall(int x, int z) {
  int i, idx, start, end;
  uint32_t bits, bit;

  if (abs(x) >= 32 || abs(z) >= 32) { return; }
  if (x < 0) { /* left half of the plot? */
    idx = ((z+32)*2) + 0; /* z'th row from center, left set of 32 bits in row */
    bit = 1 << (x+32); /* (32-abs(x))'th bit */
  }
  else {
    idx = ((z+32)*2) + 1; /* z'th row from center, right set of 32 bits in row */
    bit = 1 << x; /* x'th bit */
  }
  if (wall_cache[idx] & bit) /* already plotted a wall with center at x,z? */
    return;
  wall_cache[idx] |= bit;
  /* plot circle in wall bitmap with center at x,z */
  if (z<0) {
    start = -z;
    end = 32;
    idx = 0;
  }
  else {
    start = 0;
    end = 32-z;
    idx = z;
  }
  if (x < 0) {
    for (i=start;i<end;i++,idx++) {
      bits = circle_bitmap[i];
      wall_bitmap[idx] |= bits << -x;
    }
  }
  else {
    for (i=start;i<end;i++,idx++) {
      bits = circle_bitmap[i];
      wall_bitmap[idx] |= bits >> x;
    }
  }
}

/**
 * set or clear a rectangular region of bits in the wall bitmap
 *
 * x1 - rect x1
 * z1 - rect z1
 * x2 - rect x2
 * z2 - rect z2
 */
//----- (8002D7D4) --------------------------------------------------------
static void PlotWallB(int x1, int z1, int x2, int z2, int op) {
  int i;
  uint32_t bits;

  x1 += 15; z1 += 16; x2 += 16; z2 += 16; /* move range (-16,-16)-(16,16) to (0,0)-(32,32) */
  if (x2 <= 0 || x1 >= 32 || z1 >= 32) { return; } /* return if 2d range is out of bounds */
  bits = 0xFFFFFFFF;
  if (x2 < 32) { bits <<= (32 - x2); }
  if (x1 >= 0) { bits &= ~((int)0x80000000 >> x1); } /* force arithmetic shift */
  if (bits) {
    if (z1 < 0) { z1 = 0; }
    if (z2 > 32) { z2 = 32; }
    if (op) { /* op 1? */
      for (i=z1;i<z2;i++)
        wall_bitmap[i] |= bits; /* set */
    }
    else { /* op 0 */
      for (i=z1;i<z2;i++)
        wall_bitmap[i] &= ~bits; /* clear */
    }
  }
}

//----- (8002D8B8) --------------------------------------------------------
static void PlotObjWalls(vec *next_trans, gool_object *obj, zone_query *query, int flag) {
  zone_header *header;
  bound obj_bound, test_bound, node_bound;
  gool_bound *bound;
  vec delta;
  uint32_t dist_xz;
  entry *exec;
  gool_header *g_header;
  int zone_has_walls;
  int i, ii, x1, z1, x2, z2;

  if (!object_bound_count) { return; }
  header = (zone_header*)cur_zone->items[0];
  zone_has_walls = !(header->flags & 0x100000);
  GoolCalcBound(&obj->bound, next_trans, &obj_bound);
  test_bound.p1.x = obj_bound.p1.x - (100<<8);
  test_bound.p1.y = obj_bound.p1.y;
  test_bound.p1.z = obj_bound.p1.z - (100<<8);
  test_bound.p2.x = obj_bound.p2.x + (100<<8);
  test_bound.p2.y = obj_bound.p2.y;
  test_bound.p2.z = obj_bound.p2.z + (100<<8);
  for (i=0;i<object_bound_count;i++) {
    bound = &object_bounds[i];
    if (flag && bound->obj == obj->collider && test_bound.p1.y >= bound->p2.y) { continue; }
    if (!TestBoundIntersection(&test_bound, &bound->bound)) { continue; }
    node_bound.p1.x = bound->p1.x + bound->obj->hotspot_size;
    node_bound.p1.y = bound->p1.y;
    node_bound.p1.z = bound->p1.z + bound->obj->hotspot_size;
    node_bound.p2.x = bound->p2.x - bound->obj->hotspot_size;
    node_bound.p2.y = bound->p2.y;
    node_bound.p2.z = bound->p2.z - bound->obj->hotspot_size;
    exec = bound->obj->global;
    g_header = (gool_header*)exec->items[0];
    if (zone_has_walls && (bound->obj->status_b & 0x10000)
      && ((!(obj->state_flags & 0x10) && obj->invincibility_state != 5)
        || g_header->category != 0x300
        || ((bound->obj->status_c & 0x1012) && !(bound->obj->state_flags & 0x10020)))) {
      if (bound->p2.y < test_bound.p1.y || test_bound.p2.y <= bound->p1.y) { continue; }
      if (g_header->type == 11) { /* PoPIC? */
        delta.x = (next_trans->x - bound->obj->trans.x) >> 8;
        delta.z = (next_trans->z - bound->obj->trans.z) >> 8;
        dist_xz = sqrt((delta.x*delta.x)+(delta.z*delta.z)) << 8;
        if (dist_xz <= 0x19000) { continue; }
      }
      x1 = ((node_bound.p1.x-next_trans->x)*4) >> 13;
      z1 = ((node_bound.p1.z-next_trans->z)*4) >> 13;
      x2 = ((node_bound.p2.x-next_trans->x)*4) >> 13;
      z2 = ((node_bound.p2.z-next_trans->z)*4) >> 13;
      PlotWallB(x1, z1, x2, z2, 1);
      if (flag) {
        for (ii=x1;ii<x2;ii+=8)
          PlotWall(ii, z1);
        for (ii=z1;ii<z2;ii+=8)
          PlotWall(x2, ii);
        for (ii=x2;ii>=x1;ii-=8)
          PlotWall(ii, z2);
        for (ii=z2;ii>=z1;ii-=8)
          PlotWall(x1, ii);
      }
    }
    if (flag && TestBoundIntersection(&obj_bound, &bound->bound))
      GoolCollide(obj, &obj_bound, bound->obj, &bound->bound);
  }
}

//----- (8002DF50) --------------------------------------------------------
void PlotWalls(vec *next_trans, gool_object *obj, zone_query *query) {
  zone_header *header;
  int i, flags;

  header = (zone_header*)cur_zone->items[0];
  if (!(header->flags & 0x100000)) {
    for (i=0;i<32;i++)
      wall_bitmap[i] = 0;
    for (i=0;i<128;i++)
      wall_cache[i] = 0;
    flags = 0;
    if ((obj->status_c & 2) || (obj->invincibility_state >= 2 && obj->invincibility_state <= 4))
      flags = 2;
#ifdef PSX
    RPlotQueryWalls(query,
      &query->nodes_bound,
      flags,
      obj->trans.y+land_offset,
      obj->trans.y+(50<<8),
      next_trans->y+(665<<8),
      next_trans->x,
      next_trans->z);
#else
    PlotQueryWalls(query,
      &query->nodes_bound,
      flags,
      obj->trans.y+land_offset,
      obj->trans.y+(50<<8),
      next_trans->y+(665<<8),
      next_trans->x,
      next_trans->z);
#endif
  }
  PlotObjWalls(next_trans, obj, query, 1);
}

static inline int TestWall(int x, int z) {
  if (x >= 0 && z >= 0 && x < 32 && z < 32)
    return !(wall_bitmap[z] & (0x80000000 >> x));
  return 0;
}

//----- (8002E0A0) --------------------------------------------------------
static int ReplotWalls(int op, int flags, vec *next_trans, gool_object *obj) {
  zone_query *query;
  zone_query_result *result;
  zone_query_result_rect *result_rect;
  vec max_depth;
  dim zone_dim;
  int16_t node;
  rect node_rect;
  bound node_bound, dist;
  int x1, x2, z1, z2; /* bit index */
  int level, type, result_count;
  int plot, i;

  result_count = 0;
  query = &cur_zone_query;
  result = query->results;
  for (i=0;i<query->result_count;i++) { /* while not done iterating results */
    result = &query->results[i];
    if (!result->is_node) {
      result_rect = (zone_query_result_rect*)result;
      zone_dim.w = result_rect->w << 8;
      zone_dim.h = result_rect->h << 8;
      zone_dim.d = result_rect->d << 8;
      max_depth.x = result_rect->max_depth_x;
      max_depth.y = result_rect->max_depth_y;
      max_depth.z = result_rect->max_depth_z;
      result += sizeof(zone_query_result_rect)/sizeof(zone_query_result);
      result_count += 2;
    }
    else {
      node = (result->node << 1) | 1;
      level = result->level;
      type = ((node & 0xE) >> 1) + 1;
      if (type == 0 || type == 5
        || (flags == 0 && type != 2)
        || (flags == 1 && type == 2)) {
        ++result;
        continue;
      }
      node_rect.loc.y = result->y << 4; /* recover original scale */
      node_rect.dim.h = zone_dim.h >> min(level, max_depth.y);
      node_bound.p1.y = node_rect.loc.y + query->nodes_bound.p1.y; /* local bound to bound */
      node_bound.p2.y = node_bound.p1.y + (int32_t)node_rect.dim.h;
      dist.p1.y = node_bound.p1.y - next_trans->y;
      dist.p2.y = node_bound.p2.y - next_trans->y;
      plot = ((flags == 0 && dist.p1.y<=(100<<8) && dist.p2.y>=(-400<<8)))
           || (flags == 1 && dist.p2.y>=0)
           || (flags != 0 && flags != 1);
      if (!plot) {
        ++result;
        continue;
      }
      node_rect.loc.x = result->x << 4;
      node_rect.loc.z = result->z << 4;
      node_rect.dim.w = zone_dim.w >> min(level, max_depth.x);
      node_rect.dim.d = zone_dim.d >> min(level, max_depth.z);
      node_bound.p1.x = node_rect.loc.x + query->nodes_bound.p1.x;
      node_bound.p1.z = node_rect.loc.z + query->nodes_bound.p1.z;
      node_bound.p2.x = node_bound.p1.x + (int32_t)node_rect.dim.w;
      node_bound.p2.z = node_bound.p1.z + (int32_t)node_rect.dim.d;
      dist.p1.x = node_bound.p1.x - next_trans->x;
      dist.p2.x = node_bound.p2.x - next_trans->x;
      dist.p1.z = node_bound.p1.z - next_trans->z;
      dist.p2.z = node_bound.p2.z - next_trans->z;
      x1 = (dist.p1.x*4) >> 13;
      z1 = (dist.p1.z*4) >> 13;
      x2 = (dist.p2.x*4) >> 13;
      z2 = (dist.p2.z*4) >> 13;
      PlotWallB(x1, z1, x2, z2, 1);
      ++result;
      ++result_count;
    }
  }
  return result_count;
}

/**
 * adjust the bit index (x,z) to the index of the nearest non-solid bit in the wall bitmap
 *
 * trans - obj current trans
 * x - x index
 * z - z index
 * adj_x - (output) adjusted x index
 * adj_z - (output) adjusted z index
 * obj - the object
 * ret - should be set to 0;
 *       bit 8 of this value is set in a recursive call to the function
 *       and bit 2 is set upon finding a non-solid bit, in which case the full ret value
 *       (with possibly bit 8 set) is returned from the function
 *
 * return - bitfield: bit 2 = readjustment occured/found a nearest non-solid bit
 *                     bit 8 = a secondary check was necessary (resulting in a recursive call)
*/
//----- (8002E3F8) --------------------------------------------------------
int StopAtWalls(vec *trans, int x, int z, int *adj_x, int *adj_z, gool_object *obj, int ret) {
  zone_header *header;
  gool_header *collider_header;
  vec28 offs;
  int done;
  int i,ox,oz,tx,tz;

  done = 0;
  while (!done) {
    header = (zone_header*)cur_zone->items[0];
    if (TestWall(x, z) || (header->flags & 0x100000)) {
      *adj_x = x;
      *adj_z = z;
      return ret+2;
    }
    for (i=0;i<sizeof(sorted_points)/sizeof(vec28);i++) {
      offs = sorted_points[i];
      ox = offs.x; oz = offs.y;
      if  (TestWall(tx=x+ox,tz=z+oz)
        || TestWall(tx=x+ox,tz=z-oz)
        || TestWall(tx=x-ox,tz=z+oz)
        || TestWall(tx=x-ox,tz=z-oz)) { /* is there a wall at any offset (px,pz) s.t. abs(px)==ox, abs(pz)==oz? */
        *adj_x = tx;
        *adj_z = tz;
        return ret+2;
      }
      if (ox == oz) { continue; } /* skip next part if coord pair does not have an inverse */
      /*
         sorted_points only includes points (x,y) s.t. x >= y;
         that is-integer valued points in the 16x16 upper-left right triangular portion
         of the upper right quadrant of the 32x32 square centered at the origin
         are not included;
         inclusion of these points is not necessary as they can easily be derived from the others
         by reflecting across the y=x line,
         i.e. interchanging the components (x,y) to get the inverse coord pair (y,x)
         therefore, when ox!=oz it is also necessary to check for walls
         at offsets (pz,px) s.t. abs(px)==ox, abs(pz)==oz
      */
      if  (TestWall(tx=x+oz, tz=z+ox)
        || TestWall(tx=x+oz, tz=z-ox)
        || TestWall(tx=x-oz, tz=z+ox)
        || TestWall(tx=x-oz, tz=z-ox)) {
        *adj_x = tx;
        *adj_z = tz;
        return ret+2;
      }
    }
    done = 1; /* assume done */
    if (x == 16 || z == 16) /* check was done w.r.t. the origin? */
      break; /* all bits in the bitmap have been tested, so break */
    if (!obj->collider) /* no collider? */
      break; /* shouldn't expect to find any non-solid bits, so break */
    collider_header = (gool_header*)obj->collider->global->items[0];
    if (collider_header->type == 0x22) /* collider is a box? */
      break; /* shouldn't expect to find any non-solid bits, so break */
    for (i=0;i<32;i++) {
      if (wall_bitmap[i] != 0xFFFFFFFF) { /* found a row with non-solid bit? */
        x += 16; z += 16; /* offset */
        done = 0; /* redo check at new location to ensure that rest of 32x32 points are tested */
        break;
      }
    }
  }
  if (ret < 0x100) { /* function has not been called recursively? */
    if (obj->collider)
      collider_header = (gool_header*)obj->collider->global->items[0];
    if (!obj->collider || collider_header->type != 0x22) { /* no collider or its a non-box? */
      if (ReplotWalls(0, 0, trans, obj)) {
        ReplotWalls(1, 1, trans, obj);
        PlotObjWalls(trans, obj, &cur_zone_query, 0);
      }
      return StopAtWalls(trans, x, z, adj_x, adj_z, obj, 0x100);
    }
  }
  obj->status_a |= 0x100000; /* set flag for 'bounce from box/enemy'???? */
  *adj_x = 16; /* no movement */
  *adj_z = 16;
  return 0;
}

#if (!defined (PSX) || defined (PSX_NOASM))
/* for psx builds, the default implementations for these functions are in
   psx/r3000a.s.
   if noasm is enabled for a psx build then the below implementations are
   used in place of the pure asm ones; they are referenced in psx/r3000a.c
   for pc builds these are the default
*/
static void ZoneQueryOctreeR(
  zone_rect *zone_rect,
  uint16_t node,
  rect n_rect,
  rect *z_rect,
  vec *max_depth,
  zone_query_result *results,
  int *result_count,
  int level) {
  int16_t *children;
  zone_query_result *result;
  rect subrect;
  int i, j, k, idx, flags, mask;
  vec flag, mid;

  if (!node) { return; }
  if (node & 1) {
    if (*result_count >= 512) { return; }
    result = &results[*result_count];
    result->node = node >> 1;
    result->level = level;
    result->x = n_rect.x >> 4;
    result->y = n_rect.y >> 4;
    result->z = n_rect.z >> 4;
    (*result_count)++;
    return;
  }
  children = (uint16_t*)((uint8_t*)zone_rect + node);
  flag.x = level < max_depth->x;
  flag.y = level < max_depth->y;
  flag.z = level < max_depth->z;
  subrect.dim.w = n_rect.w >> flag.x;
  subrect.dim.h = n_rect.h >> flag.y;
  subrect.dim.d = n_rect.d >> flag.z;
  mid.x = n_rect.x + (int32_t)subrect.dim.w;
  mid.y = n_rect.y + (int32_t)subrect.dim.h;
  mid.z = n_rect.z + (int32_t)subrect.dim.d;
  flags = ((flag.x ? (0 <= mid.x) | ((mid.x <= (int32_t)z_rect->w)<<1) : 3) << 0)
    | ((flag.y ? (0 <= mid.y) | ((mid.y <= (int32_t)z_rect->h)<<1) : 3) << 2)
    | ((flag.z ? (0 <= mid.z) | ((mid.z <= (int32_t)z_rect->d)<<1) : 3) << 4);
  idx = 0;
  for (i=0;i<1+flag.x;i++) {
    for (j=0;j<1+flag.y;j++) {
      for (k=0;k<1+flag.z;k++) {
        mask = ((k+flag.z)<<4)+((j+flag.y)<<2)+((i+flag.x)<<0);
        if ((flags & mask) == mask) {
          subrect.x = i ? mid.x : n_rect.x;
          subrect.y = j ? mid.y : n_rect.y;
          subrect.z = k ? mid.z : n_rect.z;
          ZoneQueryOctreeR(zone_rect, children[idx], subrect, z_rect, max_depth,
            results, result_count, level+1);
        }
        idx++;
      }
    }
  }
}

int ZoneQueryOctree(zone_rect *zone_rect, bound *bound, zone_query_results *results) {
  rect n_rect, t_rect;
  int result_count;
  vec max_depth;
  uint16_t node;

  node = zone_rect->octree.root;
  max_depth.x = zone_rect->octree.max_depth_x;
  max_depth.y = zone_rect->octree.max_depth_y;
  max_depth.z = zone_rect->octree.max_depth_z;
  n_rect.dim.w = zone_rect->w << 8;
  n_rect.dim.h = zone_rect->h << 8;
  n_rect.dim.d = zone_rect->d << 8;
  n_rect.loc.x = (zone_rect->x << 8) - bound->p1.x;
  n_rect.loc.y = (zone_rect->y << 8) - bound->p1.y;
  n_rect.loc.z = (zone_rect->z << 8) - bound->p1.z;
  t_rect.dim.w = bound->p2.x - bound->p1.x;
  t_rect.dim.h = bound->p2.y - bound->p1.y;
  t_rect.dim.d = bound->p2.z - bound->p1.z;
  results->rect.w = zone_rect->w;
  results->rect.h = zone_rect->h;
  results->rect.d = zone_rect->d;
  results->rect.max_depth_x = max_depth.x;
  results->rect.max_depth_y = max_depth.y;
  results->rect.max_depth_z = max_depth.z;
  results->rect.zero = 0;
  result_count = 0;
  ZoneQueryOctreeR(zone_rect, node, n_rect, &t_rect, &max_depth, results->r, &result_count, 0);
  return result_count+2;
}

void PlotQueryWalls(
  zone_query *query,
  bound *nodes_bound,
  int flag,
  int test_y1_t1,  // a3
  int test_y1,     // gp
  int test_y2,     // sp
  int trans_x,
  int trans_z) {
  zone_query_result *result;
  zone_query_result_rect *result_rect;
  bound nbound;
  dim zone_dim;
  vec max_depth;
  uint16_t node;
  int i, ii, level, type, subtype;

  result = &query->results[0];
  for (i=0;i<query->result_count;i++) {
    if (result->value == -1) { return; }
    if (!result->is_node) {
      result_rect = (zone_query_result_rect*)result;
      zone_dim.w = result_rect->w << 8; // s4
      zone_dim.h = result_rect->h << 8; // s5
      zone_dim.d = result_rect->d << 8; // s6
      max_depth.x = result_rect->max_depth_x; // s7
      max_depth.y = result_rect->max_depth_y; // t8
      max_depth.z = result_rect->max_depth_z; // t9
      result += 2;
      continue;
    }
    node = (result->node << 1) | 1;
    level = result->level;
    type = (node & 0xE) >> 1;
    subtype = (node & 0x3F0) >> 4;
    nbound.p1.x = ((int32_t)result->x << 4) + nodes_bound->p1.x; // t6
    nbound.p1.y = ((int32_t)result->y << 4) + nodes_bound->p1.y; // t7
    nbound.p1.z = ((int32_t)result->z << 4) + nodes_bound->p1.z; // s0
    nbound.p2.x = nbound.p1.x + (int32_t)(zone_dim.w >> min(level, max_depth.x)); // s1
    nbound.p2.y = nbound.p1.y + (int32_t)(zone_dim.h >> min(level, max_depth.y)); // s2
    nbound.p2.z = nbound.p1.z + (int32_t)(zone_dim.d >> min(level, max_depth.z)); // s3
    ++result;
    if (type == 3 || type == 4) { continue; }
    if (type == 1) {
      if (nbound.p2.y <= test_y1_t1 || test_y2 <= nbound.p1.y)
        continue;
    }
    else if (subtype == 0 || subtype > 38 || flag) {
      if (nbound.p2.y <= test_y1 || test_y2 <= nbound.p1.y)
        continue;
    }
    else
      continue;
    nbound.p1.x = (int)((nbound.p1.x - trans_x) << 2) >> 13;
    nbound.p2.x = (int)((nbound.p2.x - trans_x) << 2) >> 13;
    nbound.p1.z = (int)((nbound.p1.z - trans_z) << 2) >> 13;
    nbound.p2.z = (int)((nbound.p2.z - trans_z) << 2) >> 13;
    if (-32 < nbound.p1.z && nbound.p1.z < 32) {
      for (ii=nbound.p1.x;ii<nbound.p2.x;ii+=8) {
        PlotWall(ii, nbound.p1.z);
      }
    }
    if (-32 < nbound.p2.x && nbound.p2.x < 32) {
      for (ii=nbound.p1.z;ii<nbound.p2.z;ii+=8)
        PlotWall(nbound.p2.x, ii);
    }
    if (-32 < nbound.p2.z && nbound.p2.z < 32) {
      for (ii=nbound.p2.x;ii>nbound.p1.x;ii-=8)
        PlotWall(ii, nbound.p2.z);
    }
    if (-32 < nbound.p1.x && nbound.p1.x < 32) {
      for (ii=nbound.p2.z;ii>nbound.p1.z;ii-=8)
        PlotWall(nbound.p1.x, ii);
    }
  }
}

// void RPlotWall(int x, int z) {
//   int i, idx, start, end;
//   uint32_t bits, bit;
//
//   if (x < 0) { /* left half of the plot? */
//     idx = ((z+32)*2) + 0; /* z'th row from center, left set of 32 bits in row */
//     bit = 1 << (x+32); /* (32-abs(x))'th bit */
//   }
//   else {
//     idx = ((z+32)*2) + 1; /* z'th row from center, right set of 32 bits in row */
//     bit = 1 << x; /* x'th bit */
//   }
//   if (scratch.wall_cache[idx] & bit) /* already plotted a wall with center at x,z? */
//     return;
//   /* plot circle in wall bitmap with center at x,z */
//   if (z<0) {
//     start = -z;
//     end = 32;
//     idx = 0;
//   }
//   else {
//     start = 0;
//     end = 32-z;
//     idx = z;
//   }
//   if (x < 0) {
//     for (i=start;i<end;i++, idx++) {
//       bits = scratch.circle_bitmap[i];
//       scratch.wall_bitmap[idx] |= bits << -x;
//     }
//   }
//   else {
//     for (i=start;i<end;i++, idx++) {
//       bits = scratch.circle_bitmap[i];
//       scratch.wall_bitmap[idx] |= bits >> x;
//     }
//   }
// }

void FindFloorY(
  gool_object *obj,
  zone_query *query,
  bound *nodes_bound,
  bound *collider_bound,
  int32_t max_y,
  zone_query_summary *summary,
  int32_t default_y,
  int (*func)(gool_object*, uint32_t)) {
  int32_t sum_y2[2];
  int count[2];
  zone_query_result *result;
  zone_query_result_rect *result_rect;
  bound nbound;
  dim zone_dim;
  vec max_depth;
  int16_t node;
  int i, level, type, subtype;

  sum_y2[0] = 0; count[0] = 0;
  sum_y2[1] = 0; count[1] = 0;
  result = &query->results[0];
  for (i=0;i<query->result_count;i++) {
    if (result->value == -1) { break; }
    if (!result->is_node) {
      result_rect = (zone_query_result_rect*)result;
      zone_dim.w = result_rect->w << 8;
      zone_dim.h = result_rect->h << 8;
      zone_dim.d = result_rect->d << 8;
      max_depth.x = result_rect->max_depth_x;
      max_depth.y = result_rect->max_depth_y;
      max_depth.z = result_rect->max_depth_z;
      result += 2;
      continue;
    }
    node = (result->node << 1) | 1;
    level = result->level;
    type = (node & 0xE) >> 1;
    subtype = (node & 0x3F0) >> 4;
    nbound.p1.x = ((int32_t)result->x << 4) + nodes_bound->p1.x;
    nbound.p1.y = ((int32_t)result->y << 4) + nodes_bound->p1.y;
    nbound.p1.z = ((int32_t)result->z << 4) + nodes_bound->p1.z;
    ++result;
    if (collider_bound->p2.x < nbound.p1.x
      || collider_bound->p2.y < nbound.p1.y
      || collider_bound->p2.z < nbound.p1.z)
      continue;
    nbound.p2.x = nbound.p1.x + (int32_t)(zone_dim.w >> min(level, max_depth.x));
    nbound.p2.y = nbound.p1.y + (int32_t)(zone_dim.h >> min(level, max_depth.y));
    nbound.p2.z = nbound.p1.z + (int32_t)(zone_dim.d >> min(level, max_depth.z));
    if (nbound.p2.x < collider_bound->p1.x
      || nbound.p2.y < collider_bound->p1.y
      || nbound.p2.z < collider_bound->p1.z)
      continue;
    if (type == 3 || type == 4 || (subtype > 0 && subtype <= 38)) {
      if ((*func)(obj, node/*, count[0], sum_y2[0]*/))
        continue;
    }
    if (nbound.p2.y > max_y) { continue; }
    if (type == 0) { /* solid wall or scenery? */
      count[0] += 1;
      sum_y2[0] += nbound.p2.y;
    }
    else { /* solid floor, level bound, or event */
      count[1] += 1;
      sum_y2[1] += nbound.p2.y;
    }
  }
  if (count[0] == 0)
    summary->solid_nodes_y = default_y;
  else
    summary->solid_nodes_y = sum_y2[0]/count[0];
  if (count[1] == 0)
    summary->floor_nodes_y = default_y;
  else
    summary->floor_nodes_y = sum_y2[1]/count[1];
}

int FindCeilY(
  gool_object *obj,
  zone_query *query,
  bound *nodes_bound,
  bound *collider_bound,
  int type_a,
  int type_b,
  int32_t default_y) {
  int32_t sum_y1;
  int count;
  zone_query_result *result;
  zone_query_result_rect *result_rect;
  bound nbound;
  dim zone_dim;
  vec max_depth;
  int16_t node;
  int i, level, type, subtype;
  sum_y1 = 0; count = 0;
  result = &query->results[0];
  for (i=0;i<query->result_count;i++) {
    if (result->value == -1) { break; }
    if (!result->is_node) {
      result_rect = (zone_query_result_rect*)result;
      zone_dim.w = result_rect->w << 8;
      zone_dim.h = result_rect->h << 8;
      zone_dim.d = result_rect->d << 8;
      max_depth.x = result_rect->max_depth_x;
      max_depth.y = result_rect->max_depth_y;
      max_depth.z = result_rect->max_depth_z;
      result += 2;
      continue;
    }
    node = (result->node << 1) | 1;
    level = result->level;
    type = (node & 0xE) >> 1;
    subtype = (node & 0x3F0) >> 4;
    if (type != (type_a-1) || type != (type_b-1)) { continue; }
    nbound.p1.x = (result->x << 4) + nodes_bound->p1.x;
    nbound.p1.y = (result->y << 4) + nodes_bound->p1.y;
    nbound.p1.z = (result->z << 4) + nodes_bound->p1.z;
    ++result;
    if (collider_bound->p2.x < nbound.p1.x
      || collider_bound->p2.y < nbound.p1.y
      || collider_bound->p2.z < nbound.p1.z)
      continue;
    nbound.p2.x = nbound.p1.x + (int32_t)(zone_dim.w >> min(level, max_depth.x));
    nbound.p2.y = nbound.p1.y + (int32_t)(zone_dim.h >> min(level, max_depth.y));
    nbound.p2.z = nbound.p1.z + (int32_t)(zone_dim.d >> min(level, max_depth.z));
    if (nbound.p2.x < collider_bound->p1.x
      || nbound.p2.y < collider_bound->p1.y
      || nbound.p2.z < collider_bound->p2.z)
      continue;
    count += 1;
    sum_y1 += nbound.p1.y;
  }
  if (count == 0)
    return default_y;
  else
    return sum_y1/count;
}

#endif

