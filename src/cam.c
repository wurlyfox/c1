#include "cam.h"
#include "globals.h"
#include "math.h"
#include "geom.h"
#include "ns.h"
#include "pad.h"
#include "gool.h"
#include "level.h"
#include "formats/svtx.h"
#include "formats/tgeo.h"
#include "formats/zdat.h"

/* .sdata */
int32_t cam_offset_z  = -0x12C00; /* 800564A4; gp[0x2A] */
int32_t cam_zoom      =  0x6A400; /* 800564A8; gp[0x2B] */
int cam_offset_dir_z  =        0; /* 800564AC; gp[0x2C] */
int cam_offset_dir_x  =        0; /* 800564B0; gp[0x2D] */
int32_t cam_offset_y  =  0x3E800; /* 800564B4; gp[0x2E] */
int32_t cam_offset_x  =        0; /* 800564B8; gp[0x2F] */
/* .sbss */
int32_t cam_speed = 0;            /* 800565C0; gp[0x71] */
int32_t dcam_accel = 0;           /* 800565C4; gp[0x72] */
int32_t dcam_angvel = 0;          /* 800565C8; gp[0x73] */
int32_t dcam_angvel2 = 0;         /* 800565CC; gp[0x74] */
int32_t dcam_rot_y1 = 0;          /* 800565D0; gp[0x75] */
int32_t dcam_rot_y2 = 0;          /* 800565D4; gp[0x76] */
int32_t dcam_trans_z = 0;         /* 800565D8; gp[0x77] */

extern ns_struct ns;
extern level_state savestate;
extern vec cam_trans;
extern int frames_elapsed;

typedef struct {
  zone_path *cur_path;
  zone_path *next_path;
  int32_t progress;
  int exit;
  int entrance;
  int32_t delta_progress;
  uint32_t dist;
  int flags;
  int relation;
  int32_t pan_x;
  int32_t zoom;
  int32_t pan_y;
  int progress_made;
} cam_info;

extern entry *cur_zone;
extern zone_path *cur_path;
extern uint32_t cur_progress;

extern pad pads[2];
extern gool_object *crash;
extern ang cam_rot;

//----- (80029F6C) --------------------------------------------------------
static void CamAdjustProgress(int32_t speed, cam_info *cam) {
  entry *next_zone;
  int next_progress, next_idx;

#ifndef PSX
  speed *= 2;
#endif
  if (cam->flags & 2) { /* forward change in progress? */
    next_progress = cur_progress + speed; /* calc next progress */
    if (cam->next_path) { /* change to a new path? */
      next_idx = next_progress >> 8;
      /* since cam changes to a new path the next idx *should*
         exceed the cur path length; this check ensures it */
      if (next_idx >= cur_path->length) {
        /* subtract an entire current path length's worth of progress
           this makes it relative to the start of the next path */
        next_progress -= (cur_path->length<<8)+1;
        if (!(cam->relation & 2)) /* behind the preceding path? */
          next_progress = -next_progress; /* use the equivalent negative value */
        next_zone = cam->next_path->parent_zone;
        next_progress += cam->entrance; /* adjust by the entrance offset??? */
        LevelUpdate(next_zone, cam->next_path, min(next_progress, cam->progress), 0);
        return;
      }
    }
  }
  else { /* backward change in progress */
    next_progress = cur_progress - speed;
    if (cam->next_path) { /* change to a new path? */
      next_idx = next_progress >> 8;
      /* since cam changes to a new path the next idx *should*
         fall below 0; this check ensures it */
      if (next_idx < 0) {
        /* that negative idx plus the next path length gives the correct index
           however LevelUpdate is already written to handle negative indexes in this way */
        if (cam->relation & 2) /* in front of the preceding path? */
          next_progress = -next_progress;
        next_zone = cam->next_path->parent_zone;
        next_progress += cam->entrance; /* adjust by the entrance offset??? */
        LevelUpdate(next_zone, cam->next_path, max(next_progress, cam->progress), 0);
        return;
      }
    }
  }
  LevelUpdate(cur_zone, cur_path, next_progress, 0);
}

//----- (8002A0C4) --------------------------------------------------------
static int CamGetProgress(vec *trans, zone_path *path, cam_info *cam, int flags, int flag) {
  vec trans_path, rel_path, dot, adjust, v_dist;
  entry *zone;
  zone_header *header;
  zone_rect *rect;
  int dist, progress, pt_idx, entrance, exit;

  zone = path->parent_zone;
  header = (zone_header*)zone->items[0];
  rect = (zone_rect*)zone->items[1];
  cam->progress_made = 1;
  trans_path.x = rect->x + path->points[0].x;
  trans_path.y = rect->y + path->points[0].y;
  trans_path.z = rect->z + path->points[0].z;
  rel_path.x = (trans->x >> 4) - (trans_path.x << 4); /* 8 bit fractional */
  rel_path.y = (trans->y >> 4) - (trans_path.y << 4);
  rel_path.z = (trans->z >> 4) - (trans_path.z << 4);
  /* approximate camera's projected distance along the path by computing
     dot product of rel_path and the path's direction [vector] */
  dot.x = path->direction_x * rel_path.x; /* 12 bit frac * 8 bit frac = 20 bit frac */
  dot.y = path->direction_y * rel_path.y;
  dot.z = path->direction_z * rel_path.z;
  dist = (dot.x+dot.y+dot.z) >> 8; /* shift down to 12 bit frac */
  progress = dist / path->avg_node_dist; /* convert to a progress */
  pt_idx = progress >> 8;
  if (path == cur_path) /* already the current path? */
    cam->next_path = 0; /* no next path */
  else { /* new path */
    if (path->direction_x && rel_path.y < -12800 && (header->flags & 0x40000))
      return 0;
    cam->next_path = path; /* set the next path */
  }
  entrance = 0; exit = 0;
  if (flag) {
    if (flags & 1) { entrance = (int)path->entrance_index; }
    if (flags & 2) { exit = (int)path->exit_index; }
  }
  if (pt_idx >= entrance) { /* at or past the entrance? */
    if (pt_idx >= (path->length - exit)) { /* past or at the exit */
      /* exit is not at zero or path is not current? */
      if (exit || path != cur_path)
        return 0; /* abandon the camera */
      progress = (path->length << 8) - 1; /* force to end of path */
      pt_idx = path->length - 1;
      cam->progress_made = 0; /* no progress made */
    }
  }
  else { /* before the entrance */
    /* entrance is not at 0 or path is not current? */
    if (entrance || path != cur_path)
      return 0; /* abandon the camera */
    progress = 0; /* force to start of path */
    pt_idx = 0;
    cam->progress_made = 0; /* no progress made */
  }
  adjust.x = (cam->pan_x + crash->trans.x) >> 8;
  adjust.y = (cam->pan_y + crash->trans.y) >> 8;
  adjust.z = (cam->zoom + crash->trans.z) >> 8;
  v_dist.x = adjust.x - (rect->x + path->points[pt_idx].x);
  v_dist.y = adjust.y - (rect->y + path->points[pt_idx].y);
  v_dist.z = adjust.z - (rect->z + path->points[pt_idx].z);
  if (abs(v_dist.x) > 3200 || abs(v_dist.y) > 3200 || abs(v_dist.z) > 3200)
    return 0;
  cam->cur_path = path;
  cam->progress = progress;
  dist = sqrt((v_dist.x*v_dist.x)+(v_dist.y*v_dist.y)+(v_dist.z+v_dist.z));
  cam->dist = dist;
  return 1;
}

//----- (8002A3EC) --------------------------------------------------------
static int CamGetProgress2(vec *trans, zone_path *path, cam_info *cam, int flags, int flag) {
  vec trans_pt, rel_path, dot, adjust, v_dist;
  entry *zone;
  zone_header *header;
  zone_rect *rect;
  int32_t dist, dist_nearest;
  int32_t rot_x, progress;
  int pt_idx, entrance, exit;
  int i, is_nearer;

  zone = path->parent_zone;
  header = (zone_header*)zone->items[0];
  rect = (zone_rect*)zone->items[1];
  cam->progress_made = 1;
  /* find the path point for which the near plane is nearest to the object */
  for (i=0;i<path->length;i++) {
    trans_pt.x = rect->x + path->points[i].x;
    trans_pt.z = rect->z + path->points[i].z;
    rel_path.x = (trans->x >> 8) - trans_pt.x;
    rel_path.z = (trans->z >> 8) - trans_pt.z;
    rot_x = path->points[i].rot_x;
    dist = (-16 * cam->zoom) - (rel_path.x*sin(rot_x)+rel_path.z*cos(rot_x));
    if (path->direction_z <= 0) {
      if (i==0 && dist < 0) {
        progress = 0;
        if (dist < -128000)
          cam->progress_made = 0;
        break;
      }
      else if (i==path->length-1 && dist > 0) {
        progress = (path->length << 8) - 1;
        if (dist > 128000)
          cam->progress_made = 0;
        break;
      }
    }
    else {
      if (i==0 && dist > 0) {
        progress = 0;
        if (dist > 128000)
          cam->progress_made = 0;
        break;
      }
      else if (i==path->length-1 && dist < 0) {
        progress = (path->length << 8) - 1;
        if (dist < -128000)
          cam->progress_made = 0;
        break;
      }
    }
    is_nearer = abs(dist) < abs(dist_nearest);
    if (i==0 || is_nearer) {
      if (i==0 || (dist ^ dist_nearest >= 0)) /* first point or no change in sign? */
        progress = i << 8;
      else /* account for sign changes when paths weave through near plane */
        progress += (abs(dist_nearest)<<8)/(abs(dist_nearest)+abs(dist));
      dist_nearest = dist;
    }
  }
  if (path == cur_path) /* already the current path? */
    cam->next_path = 0; /* no next path */
  else /* new path */
    cam->next_path = path; /* set the next path */
  entrance = 0; exit = 0;
  if (flag) {
    if (flags & 1) { entrance = (int)path->entrance_index; }
    if (flags & 2) { exit = (int)path->exit_index; }
  }
  pt_idx = progress >> 8;
  if (pt_idx >= entrance) { /* at or past the entrance? */
    if (pt_idx >= (int32_t)(path->length - exit)) { /* past or at the exit */
      if (exit || (path != cur_path)) /* exit is not at zero or not the current? */
        return 0; /* abandon the camera */
      progress = (path->length << 8) - 1; /* force to end of path */
      pt_idx = path->length - 1;
      cam->progress_made = 0; /* no progress made */
    }
  }
  else { /* before the entrance */
    /* entrance is not at zero or path is not current? */
    if (entrance || (path != cur_path))
      return 0; /* abandon the camera */
    progress = 0; /* force to start of path */
    pt_idx = 0;
    cam->progress_made = 0; /* no progress made */
  }
  adjust.x = (cam->pan_x + crash->trans.x) >> 8;
  adjust.y = (cam->pan_y + crash->trans.y) >> 8;
  adjust.z = (cam->zoom + crash->trans.z) >> 8;
  v_dist.x = adjust.x - (rect->x + path->points[pt_idx].x);
  v_dist.y = adjust.y - (rect->y + path->points[pt_idx].y);
  v_dist.z = adjust.z - (rect->z + path->points[pt_idx].z);
  cam->cur_path = path;
  cam->progress = progress;
  dist = sqrt((v_dist.x*v_dist.x)+(v_dist.y*v_dist.y)+(v_dist.z+v_dist.z));
  cam->dist = dist;
  return 1;
}

//----- (8002A82C) --------------------------------------------------------
static void CamFollow(gool_object *obj, uint32_t flag) {
  cam_info cameras[5], *cam_nearest;
  entry *zone;
  zone_header *header;
  zone_path *path, *n_path;
  zone_neighbor_path neighbor_path;
  vec trans, cam_offset_new;
  int32_t progress, new_progress, old_progress;
  int32_t seek_pan, seek_zoom, new_pan, new_zoom, total_zoom;
  int32_t delta_dist, dist_exit, dist_nearest, cam_speed;
  int i, path_idx, icam, flags, progress_made, same_dir, n_end;

  flags = 0; // for shorter paths (< 50) both flags should set
  path_idx = cur_progress >> 8;
  header = (zone_header*)cur_zone->items[0];
  /* less than half way but max of 50 into path? */
  if (path_idx < (cur_path->length/2) || path_idx < 50)
    flags |= 1;
  /* more than halfway but max of 50 from end of path */
  if (path_idx >= (cur_path->length/2) || (cur_path->length-path_idx) < 50)
    flags |= 2;
  if (!(header->flags & 0x80) && (cam_zoom > 0x31FFF)) { /* first person zone? */
    if (!(pads[0].held & 0x1000)) { /* up not pressed? */
      if (pads[0].held & 0x4000) /* down pressed? */
        cam_offset_dir_z = 1; /* offset cam backward */
      /* else no adjust */
    }
    else
      cam_offset_dir_z = 0; /* offset cam forward */
  }
  else /* cam z dir depends on zone flag */
    cam_offset_dir_z = header->flags & 0x8000;
  if (!(pads[0].held & 0x8000)) { /* left not pressed? */
    if (pads[0].held & 0x2000)  /* right pressed? */
      cam_offset_dir_x = 1; /* offset cam right */
    /* else no adjust */
  }
  else
    cam_offset_dir_x = 0; /* offset cam left */
  if (cam_offset_dir_z == 0) /* cam offset is forward? */
    cam_offset_z = max(cam_offset_z - 0x3200, -0x12C00); /* seek towards -0x12C00 */
  else /* cam offset is backward; so seek towards 0x12C00, cortex power 0x4B000 */
    cam_offset_z = min(cam_offset_z + 0x3200, ns.ldat->lid != 3 ? 0x12C00: 0x4B000);
  if (!(header->flags & 0x4000)) /* not a left/right direction zone? */
    cam_offset_x = 0; /* no x offset */
  else if (cur_path->direction_x) { /* path runs left or right? */
    if (cam_offset_dir_x == 0) { /* cam offset is left? */
      if (cur_path->direction_x < 0) /* path runs left? */
        cam_offset_x = max(cam_offset_x - 25600, -307200); /* seek towards -307200 */
      else
        cam_offset_x = max(cam_offset_x - 25600, 0); /* seek towards 0 */
    }
    else { /* cam offset is right */
      if (cur_path->direction_x > 0) /* path runs right? */
        cam_offset_x = min(cam_offset_x + 25600, 307200); /* seek towards 307200 */
      else
        cam_offset_x = min(cam_offset_x + 25600, 0); /* seek towards 0 */
    }
  }
  seek_pan = 0; new_pan = 0; /* seek_pan/seek_zoom boolean vals are inverted in orig impl */
  seek_zoom = 0; new_zoom = 0;
  for (i=0;i<cur_path->neighbor_path_count;i++) { /* get nearest neighbor paths */
    neighbor_path = cur_path->neighbor_paths[i];
    if (neighbor_path.relation & flags) {
      n_path = ZoneGetNeighborPath(cur_zone, cur_path, i);
      if (n_path->direction_y)
        new_pan = n_path->cam_zoom << 8;
      if (n_path->direction_z)
        new_zoom = n_path->cam_zoom << 8;
    }
  }
  if (cur_path->direction_y) {
    seek_pan = 1;
    new_pan = cur_path->cam_zoom << 8;
  }
  if (cur_path->direction_z) {
    seek_zoom = 1;
    new_zoom = cur_path->cam_zoom << 8;
  }
  /* current or at least one neighbor path is forward/backward
     and we are at least more than 10 units from either end of the path? */
  if (new_zoom && (path_idx >= 11) && (path_idx < (cur_path->length-10))) {
    if (seek_zoom)
      cam_zoom = GoolSeek(cam_zoom, new_zoom, 0x1900); /* seek to new_zoom 25.0 increments */
    else
      cam_zoom = new_zoom;
  }
  if (new_pan) { /* current or at least one neighbor path is up/down? */
    if (seek_pan)
      cam_offset_y = GoolSeek(cam_offset_y, new_pan, 0x6400); /* seek to new_pan at 100.0 increments */
    else
      cam_offset_y = new_pan;
  }
  total_zoom = cam_offset_z + cam_zoom + obj->cam_zoom;
  icam = 0; /* cam iterator */
  cameras[icam].pan_x = cam_offset_x; /* record values for first camera */
  cameras[icam].pan_y = cam_offset_y;
  cameras[icam].zoom = total_zoom;
  if (cur_path->direction_z != 0) {
    trans = obj->trans;
    progress_made = CamGetProgress2(&trans, cur_path, &cameras[icam], 0, 0);
  }
  else {
    trans.x = obj->trans.x + cam_offset_x;
    trans.y = obj->trans.y + cam_offset_y;
    trans.z = obj->trans.z + total_zoom;
    progress_made = CamGetProgress(&trans, cur_path, &cameras[icam], 0, 0);
  }
  if (progress_made) { /* was progress made? */
    new_progress = cameras[icam].progress;
    old_progress = cur_progress;
    cameras[icam].delta_progress = abs(new_progress - old_progress); /* record change */
    if (new_progress < old_progress)
      cameras[icam].flags = 1;
    else
      cameras[icam].flags = 2;
    icam++;
  }
  for (i=0;i<cur_path->neighbor_path_count;i++) {
    neighbor_path = cur_path->neighbor_paths[i];
    if ((neighbor_path.goal & 4) /* timed override of neighbor cam? */
      && frames_elapsed - gem_stamp > 15) {
      continue;
    }
    if (!(neighbor_path.relation & flags)) { continue; } /* skip the further neighbor paths */
    n_path = ZoneGetNeighborPath(cur_zone, cur_path, i);
    same_dir = (abs(cur_path->direction_x) == abs(n_path->direction_x))
            && (abs(cur_path->direction_y) == abs(n_path->direction_y))
            && (abs(cur_path->direction_z) == abs(n_path->direction_z));
    cameras[icam].pan_x = cam_offset_x;
    cameras[icam].pan_y = cam_offset_y;
    cameras[icam].zoom = total_zoom;
    flags = neighbor_path.goal;
    if (n_path->direction_z != 0) {
      trans = obj->trans;
      progress_made = CamGetProgress2(&trans, n_path, &cameras[icam], flags, !same_dir);
    }
    else {
      trans.x = obj->trans.x + cam_offset_x;
      trans.y = obj->trans.y + cam_offset_y;
      trans.z = obj->trans.z + total_zoom;
      progress_made = CamGetProgress(&trans, n_path, &cameras[icam], flags, !same_dir);
    }
    if (progress_made) { /* was progress made? */
      /* calculate exit point of the current path and distance from it */
      if (neighbor_path.relation & 1) { /* neighbor path before cur path? */
        cameras[icam].exit = 0; /* exiting at the start of the path */
        dist_exit = cur_progress;
      }
      else {
        cameras[icam].exit = ((cur_path->length<<8)-1); /* exiting at the end */
        dist_exit = cameras[icam].exit - cur_progress;
      }
      /* calculate entry point of neighbor path and distance from it */
      if (neighbor_path.goal & 1) { /* goal is in front of current path? */
        cameras[icam].entrance = 0; /* entering at start of the path */
        cameras[icam].relation = 2; /* current path behind neighbor path */
        cameras[icam].delta_progress = dist_exit + cameras[icam].progress + 0x100;
      }
      else { /* goal is behind the current path */
        n_end = (n_path->length << 8) - 1;
        cameras[icam].entrance = n_end; /* entering at end */
        cameras[icam].relation = 1; /* current path in front of neighbor path */
        cameras[icam].delta_progress = dist_exit + (n_end-(cameras[icam].progress)) + 0x100;
      }
      cameras[icam].flags = neighbor_path.relation;
      icam++;
    }
  }
  if (!icam) { return; } /* return if there are no cameras/potential viewpoints */
  dist_nearest = 0x7FFFFFFF;
  cam_nearest = &cameras[0]; /* initially assume the current path camera is nearest */
  /* find the camera nearest to crash or at least one that changes in progress */
  for (i=0;i<icam;i++) {
    if (cameras[i].cur_path->cam_mode == 1) {
      cam_nearest = &cameras[i];
      break;
    } /* break on cams for paths w/cam mode 1 */
    if ((!cam_nearest->progress_made && cameras[i].progress_made)
      || (cameras[i].dist < dist_nearest
        && cam_nearest->progress_made == cameras[i].progress_made)) {
      cam_nearest = &cameras[i];
      dist_nearest = cam_nearest->dist;
    }
  }
  if (!cam_nearest->delta_progress) { return; }
  /* scale change in progress to change in distance */
  delta_dist = (int32_t)cam_nearest->delta_progress * (int32_t)cur_path->avg_node_dist;
  if (delta_dist <= 30000) { /* small change in distance? */
    path = cam_nearest->cur_path;
    zone = path->parent_zone;
    progress = cam_nearest->progress;
    LevelUpdate(zone, path, progress, 0); /* update level with target zone/path/progress */
    cam_speed = cam_nearest->delta_progress;
  }
  else {
    if (cam_nearest->delta_progress <= 0x200) /* change <= 2? */
      cam_speed = cam_nearest->delta_progress / 2; /* halve progress */
    else if (cam_nearest->delta_progress < 0x500) /* from 2 to 5 non-inclusive? */
      cam_speed = 0x200; /* limit to 2 */
    else /* > 5 */
      cam_speed = min((cam_nearest->delta_progress/2), cam_speed)+0x100;
    CamAdjustProgress(cam_speed, cam_nearest);
  }
}

//----- (8002B2BC) --------------------------------------------------------
int CamUpdate() {
  entry *zone, *n_zone, *zone_s1;
  zone_path *path, *n_path, *path_s1;
  int32_t progress, n_progress, angle;
  zone_header *header, *n_header;
  zone_neighbor_path neighbor_path;
  zone_path_point *point;
  int next_island_cam_state;
  int i, cam_mode, skip, n_path_idx, pt_idx;

  if (!crash) { return 0; }
  if (cur_display_flags & GOOL_FLAG_SPIN_DEATH) {
    CamDeath(&i_death_cam);
    return 0;
  }
  if (!(cur_display_flags & GOOL_FLAG_CAM_UPDATE)) { return 0; }
  cam_mode = cur_path->cam_mode;
  switch (cam_mode) {
  case 5: /* follow crash */
  case 6:
    game_state = GAME_STATE_PLAYING;
    CamFollow(crash, 0);
    return 1;
  case 0: /* no camera */
    game_state = GAME_STATE_PLAYING;
    return 1;
  case 1: /* auto-cam */
  case 3:
    header = (zone_header*)cur_zone->items[0];
    skip = 0;
    if ((pads[0].tapped & 0xF0) && !(header->flags & 0x81000))
      skip = 1; /* skip the transition */
    do { /* auto-increment progress and update; or skip to next non-auto-cam path */
      if (!(header->flags & 0x1000))
        game_state = GAME_STATE_CUTSCENE;
      pt_idx = (cur_progress >> 8) + 1;
      /* auto-cam hasn't reached end of path and not skipping? */
      if (pt_idx < cur_path->length && !skip) {
        LevelUpdate(cur_zone, cur_path, cur_progress + 0x100, 0);
        return 1;
      }
      neighbor_path = cur_path->neighbor_paths[0];
      n_path = ZoneGetNeighborPath(cur_zone, cur_path, 0);
      if (!n_path) { return 1; } /* TODO: make ZoneGetNeighborPath return 0 if no neighbors */
      if (neighbor_path.goal & 1)
        n_progress = 0;
      else
        n_progress = (n_path->length - 1) << 8;
      n_zone = n_path->parent_zone;
      n_header = (zone_header*)n_zone->items[0];
      LevelUpdate(n_zone, n_path, n_progress, 0); /* skip forward to next path */
      if (!(n_header->flags & 0x1000))
        LevelSaveState(crash, &savestate, 1);
      if (!skip) { return 1; }
      /* continue skipping while 1,3 */
    } while (cur_path->cam_mode == 1 || cur_path->cam_mode == 3);
    return 1;
  case 7:
    path = 0;
    next_island_cam_state = island_cam_state;
    if (island_cam_state == 0) { return 0; }
    else if (island_cam_state == -1) {
      angle = -angle12(island_cam_rot_x);
      next_island_cam_state = 1;
    }
    else if (island_cam_state == 5 || island_cam_state == 6)
      angle = 0x22;
    else {
      angle = GoolAngDiff(cam_rot.x, island_cam_rot_x);
      if (abs(angle) < 0x17)
        angle = 0;
      else
        angle = sign(angle);
    }
    path_s1 = cur_path;
    do {
      if (angle == 0) { break; }
      path = path_s1; /* s2 */
      progress = cur_progress;
      if (island_cam_state == -1) {
        pt_idx = progress >> 8;
        point = &cur_path->points[pt_idx];
        if (abs(GoolAngDiff(point->rot_y, abs(angle))) < 0x17) { break; }
      }
      if (island_cam_state & 4)
        n_progress = progress + 0x400;
      else
        n_progress = progress + 0x100;
      n_path_idx = -1;
      if (n_progress >= (path_s1->length << 8)) {
        for (i=0;i<path_s1->neighbor_path_count;i++) {
          neighbor_path = path_s1->neighbor_paths[i];
          if (neighbor_path.goal == next_island_cam_state
           || (n_path_idx == -1 && (path_s1->neighbor_path_count == 1
           || (neighbor_path.goal & 3) == (next_island_cam_state & 3))))
            n_path_idx = i;
        }
        if (n_path_idx == -1) {
          for (i=0;i<path_s1->neighbor_path_count;i++) {
            neighbor_path = path_s1->neighbor_paths[i];
            if (!(neighbor_path.goal & 4)) {
              n_path_idx = i;
              break;
            }
          }
          /* orig impl. otherwise erroneously goes to next code with n_path_idx = -1 */
        }
        zone_s1 = path_s1->parent_zone;
        path_s1 = ZoneGetNeighborPath(zone_s1, path_s1, n_path_idx);
        if (neighbor_path.goal & 1)
          n_progress = 0;
        else
          n_progress = (path_s1->length - 1) << 8;
        if (island_cam_state & 4 || angle > 0) {
          path = path_s1;
          progress = n_progress;
          break;
        }
      }
      else if (island_cam_state & 4 || angle > 0) {
        if (angle > 0)
          path = path_s1;
        progress = n_progress;
        break;
      }
    } while (path_s1 != cur_path && (n_progress >> 8) != (cur_progress >> 8));
    if (island_cam_state == -1)
      island_cam_state = next_island_cam_state;
    if (path && (path != cur_path || progress != cur_progress)) {
      zone = path->parent_zone;
      LevelUpdate(zone, path, progress, 0);
    }
    return 1;
  case 8:
    n_path_idx = -1;
    for (i=0;i<cur_path->neighbor_path_count;i++) {
      neighbor_path = cur_path->neighbor_paths[i];
      if (neighbor_path.relation & 3 == (island_cam_state & 3)^3) { /* i.e. 3 - level_count */
        n_path_idx = i;
        break;
      }
    }
    if (island_cam_state & 1) {
      pt_idx = (cur_progress >> 8) + 1;
      if (pt_idx < cur_path->length)
        LevelUpdate(cur_zone, cur_path, cur_progress+0x100, 0);
      else if (n_path_idx != -1) {
        neighbor_path = cur_path->neighbor_paths[n_path_idx];
        n_path = ZoneGetNeighborPath(cur_zone, cur_path, n_path_idx);
        if (neighbor_path.goal & 1)
          n_progress = 0;
        else
          n_progress = (n_path->length-1) << 8;
        n_zone = n_path->parent_zone;
        LevelUpdate(n_zone, n_path, n_progress, 0);
      }
    }
    else {
      pt_idx = (cur_progress >> 8) - 1;
      if (pt_idx >= 0)
        LevelUpdate(cur_zone, cur_path, cur_progress-0x100, 0);
      else if (n_path_idx != -1) {
        neighbor_path = cur_path->neighbor_paths[n_path_idx];
        n_path = ZoneGetNeighborPath(cur_zone, cur_path, n_path_idx);
        if (neighbor_path.goal & 1)
          n_progress = 0;
        else
          n_progress = (n_path->length-1) << 8;
        n_zone = n_path->parent_zone;
        LevelUpdate(n_zone, n_path, n_progress, 0);
      }
    }
    if (cur_path->cam_mode == 8)
      island_cam_state = 1;
    return 1;
  default:
    return 1;
  }
}

//----- (8002BAB4) --------------------------------------------------------
int CamDeath(int *count) {
  entry *svtx, *tgeo;
  gool_object *obj;
  gool_anim *anim;
  svtx_frame *frame;
  svtx_vertex *vert;
  tgeo_header *t_header;
  vec u_vert, r_vert, v_vert;
  vec scale, u_trans, trans;
  ang rot;
  uint32_t dist_xz;
  int32_t ang_xz, ang_yz;

  obj = cam_spin_obj;
  anim = obj->anim_seq;
  svtx = NSLookup(&anim->eid);
  frame = (svtx_frame*)svtx->items[obj->anim_frame >> 8];
  tgeo = NSLookup(&frame->tgeo);
  /* get [unrotated] vertex in the current frame referenced by the gool object */
  vert = &frame->vertices[cam_spin_obj_vert >> 8];
  t_header = (tgeo_header*)&tgeo->items[1];
  u_vert.x = (vert->x - 128 + frame->x) << 10; /* 10 bit frac to 20 bit frac */
  u_vert.y = (vert->y - 128 + frame->y) << 10;
  u_vert.z = (vert->z - 128 + frame->z) << 10;
  /* calculate scale */
  scale.x = (obj->scale.x * t_header->scale_x) >> 12;
  scale.y = (obj->scale.y * t_header->scale_y) >> 12;
  scale.z = (obj->scale.z * t_header->scale_z) >> 12;
  /* trans, rotate, and scale the vertex */
  GoolTransform(&u_vert, &obj->trans, &obj->rot, &scale, &r_vert);
  /* get relative location of transformed vertex w.r.t camera */
  v_vert.x = (cam_trans.x - r_vert.x) >> 8;
  v_vert.y = (cam_trans.y - r_vert.y) >> 8;
  v_vert.z = (cam_trans.z - r_vert.z) >> 8;
  /* calculate the xz distance and angle in the xz plane */
  dist_xz = sqrt(v_vert.x*v_vert.x+v_vert.z*v_vert.z);
  ang_xz = atan2(-v_vert.x, -v_vert.z) + 0x800;
  if (*count < 9) {
    if (*count == 0) /* calculate delta xz angle between transformed vertex and camera, for 9 iterations */
      dcam_angvel = GoolAngDiff(cam_rot.x, ang_xz) / 9;
    dcam_trans_z = dist_xz << 8; /* radius of the below circle (this seeks towards 175000 for count >= 9 */
    dcam_rot_y1 = 0; /* base angular distance around the below circle circumference */
    dcam_rot_y2 = ang_xz; /* angular distance around the below circle circumference */
    dcam_accel = 22;
    cam_rot.x += dcam_angvel; /* rotate cam in xz plane towards the vertex */
    *(count)++;
  }
  /* calculate the angle in the yz plane */
  ang_yz = atan2(v_vert.y, dist_xz);
  if (GoolAngDiff(cam_rot.y, -ang_yz) < 0x72) /* less than 10 degrees? */
    dcam_angvel2 = dcam_flip_speed / 2; /* slower yz rot rate */
  else
    dcam_angvel2 = dcam_flip_speed; /* normal yz rot rate */
  /* rotate cam in yz plane towards the vertex */
  cam_rot.y = GoolObjectRotate(cam_rot.y, -ang_yz, dcam_angvel2, 0);
  if (cur_display_flags & GOOL_FLAG_SPIN_ACCEL) {
    dcam_angvel2 += dcam_accel; /* accelerate?? no further usage of this value... */
    cam_rot.x += dcam_accel; /* additional rotation??? */
  }
  /* move camera up towards the vertex */
  cam_trans.y = GoolSeek(cam_trans.y, r_vert.y + 120000, 102400);
  /* seek the xz distance toward 175000 */
  dcam_trans_z = GoolSeek(dist_xz << 8, 175000, dcam_zoom_speed);
  /* translate cam around the circle of radius equal to the xz distance in the xz plane */
  u_trans.x = 0;
  u_trans.y = 0;
  u_trans.z = dcam_trans_z;
  trans.x = r_vert.x;
  trans.y = cam_trans.y;
  trans.z = r_vert.z;
  rot.x = 0;
  rot.y = dcam_rot_y1 + dcam_rot_y2;
  rot.z = 0;
  GoolTransform(&u_trans, &trans, &rot, 0, &cam_trans);
  return SUCCESS;
}
