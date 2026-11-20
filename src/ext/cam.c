#include "ext/ext.h"
#include "ext/gfx.h"
#include "ext/math.h"

#ifdef _WIN32
#include <windows.h>
#include <SDL_opengl.h>
#else
#include <GL/gl.h>
#endif

extern gool_object *crash;
extern entry *cur_zone;
extern zone_path *cur_path;
extern int32_t cur_progress;
extern mat16 ms_cam_rot, mn_cam_rot;
extern int GuiIgWantCaptureMouse(void);
extern int GuiIgWantCaptureKeyboard(void);

ext_cam_t ext_cam = {
  .options = { .enable_input = 1, .cam_zoom = 1.0f },
  .speed = 60.0,
  .sens = 0.15,
  .orbit_speed = 30.0,
  .orbit_radius = 2300.0,
};

#define CHASE_DIST 2300.0   /* fixed follow distance behind Crash (world units) */
#define CHASE_HEIGHT 430.0

/**
 * Get the unmodified game camera pose (without any extended camera offsets or orbit applied).
 */
static void CamExGamePose(cam_pose *p) {
  GfxVecToDVec(&p->trans, &cam.trans);
  p->rot = cam.rot;
  p->rot.x &= 0xFFF; p->rot.y &= 0xFFF; p->rot.z &= 0xFFF;
}

/**
 * Get the base camera pose for the current extended camera mode.
 * 
 * Can be independent of the current game camera state for some modes.
 * Also includes the effects of orbit if enabled.
 */
static void CamExBasePose(cam_pose *b) {
  ext_cam_state_t *state;
  dmat m_rot, m_head, m_orbit, m_rinv;
  ang yaw;
  dvec trans, pivot, axis;
  double r, d;

  state = &ext_cam.state;
  CamExGamePose(b);
  if (ext_cam.mode == CAM_MODE_FREE) { 
    *b = state->free_base; 
    return; 
  }
  if (ext_cam.mode == CAM_MODE_CHASE && crash) {
    GfxDMatrixFromAng(&m_rot, &b->rot);
    GfxVecToDVec(&trans, &crash->trans);
    b->trans = VecAdd(VecAdd(trans, 
                      VecMulS(m_rot.v[2], CHASE_DIST)),
                      VecMulS(m_rot.v[1], CHASE_HEIGHT));
  }
  if (ext_cam.options.enable_orbit && ext_cam.mode != CAM_MODE_FREE) {
    r = ext_cam.orbit_radius;
    GfxDMatrixFromAng(&m_rot, &b->rot);
    if (ext_cam.mode == CAM_MODE_CHASE && crash) {
      GfxVecToDVec(&pivot, &crash->trans);
    }
    else {
      /* position the pivot such that the orbit 
         starts at the base camera (rail) position */
      pivot = VecSub(b->trans, VecMulS(m_rot.v[2], r));
    }
    yaw = (ang){ .x = b->rot.x, .y = 0, .z = 0 };
    GfxDMatrixFromAng(&m_head, &yaw);
    /* get the orbit rotation axis from orbit direction 
       when facing forward this will be an axis in the XY plane
       at 0 degrees it will point along the Y axis
       at 90 degrees it will point along the X axis */
    d = state->orbit_dir * TWO_PI / 360.0;
    axis = VecSub(VecMulS(m_head.v[1], cos(d)),
                  VecMulS(m_head.v[0], sin(d)));
    /* get the matrix for rotation around the axis
       note that as the camera looks left, points in the world move to the right
       hence, as the matrix effectively moves world points in one direction
       the camera itself moves in the opposite direction
       m_rot is built from a basis that is the base camera direction, 
       so the inverse of the axis rotation matrix is needed 
       to change the camera direction */
    GfxRotDMatrixAxisAngle(&m_rinv, &axis, state->orbit_phi);
    GfxTransposeDMatrix(&m_rinv);
    m_orbit = m_rot;
    GfxMulDMatrix(&m_orbit, &m_rinv);
    /* the z-axis rotated by the orbit matrix is the direction
       from the pivot to the camera
       [when facing forward at an orbit rotation of 0, 
        the z-axis points to the pivot]
       the 3rd row of that matrix encodes said direction
       [because of how it transforms the unit (0,0,1)]
       scaling by the orbit radius and adding to the pivot point
       thus gives the camera position within the orbit */
    b->trans = VecAdd(pivot, VecMulS(m_orbit.v[2], r));
    /* recover the camera orientation 
       corresponding to the orbit rotation */
    GfxAngFromDMatrix(&b->rot, &m_orbit);
  }
}

/**
 * Get the effective camera pose for the current extended camera mode.
 * 
 * This includes translation and rotation offsets applied to the base pose.
 */
static void CamExEffectivePose(const cam_pose *b, cam_pose *e) {
  dmat m_rot;
  dang offs_rot;
  dvec offs, w;

  /* get the effective translation and rotation offsets */
  /* get base translation offset 
     then rotate it using the transpose/inverse
     of the base pose rotation matrix */
  offs = ext_cam.offs_trans;
  GfxDMatrixFromAng(&m_rot, &b->rot);
  GfxTransposeDMatrix(&m_rot);
  GfxRotD(&offs, &w, &m_rot);
  /* then add it to the base translation */
  e->trans = VecAdd(b->trans, w);
  /* convert base rotation offset to radians */
  offs_rot = VecMulS(ext_cam.offs_rot, (TWO_PI / 360.0));
  /* convert it to the game's Q4.12 representation 
     and add to the base rotation */
  GfxDAngToAng(&e->rot, &offs_rot);
  e->rot = VecAdd(e->rot, b->rot);
  e->rot.x &= 0xFFF; e->rot.y &= 0xFFF; e->rot.z &= 0xFFF;
}

static int CamExActive(void) {
  return ext_config.enable_hwpersp 
      && cur_zone && cur_zone->items[0];
}

/**
 * Check if the extended camera has any translation or rotation offsets applied.
 * (In other words, whether the effective pose is equal to the base pose.)
 */
static int CamExHasOffsets(void) {
  return !VecEq(ext_cam.offs_trans, VecConst(dvec, 0.0))
      || !VecEq(ext_cam.offs_rot, VecConst(dang, 0.0));
}

/**
 * Reset the translation and rotation offsets of the extended camera to zero.
 */
void CamExResetOffsets(void) {
  memset(&ext_cam.offs_trans, 0, sizeof(ext_cam.offs_trans));
  memset(&ext_cam.offs_rot, 0, sizeof(ext_cam.offs_rot));
}

/**
 * Set the extended camera mode.
 * 
 * This will update the internal state and reset offsets if switching to or from free mode.
 */
void CamExSetMode(int mode) {
  ext_cam_state_t *state;
  
  state = &ext_cam.state;
  if (mode == ext_cam.mode) 
    return;
  if (mode == CAM_MODE_FREE) {
    state->free_base = state->last_eff;
    CamExResetOffsets();
  } 
  else if (ext_cam.mode == CAM_MODE_FREE) {
    CamExResetOffsets();
  }
  ext_cam.mode = mode;
}

/**
 * Apply the extended camera to the game camera state.
 * 
 * Effectively overwrites the game camera state (trans and rot).
 */
void CamExApply(void) {
  ext_cam_state_t *state;
  cam_pose base, eff;

  state = &ext_cam.state;
  state->applied = 0;
  if (!CamExActive()) 
    return;
  /* get new base and effective poses */
  CamExBasePose(&base);
  CamExEffectivePose(&base, &eff);
  state->last_eff = eff;
  /* in game/rail mode when there are no offsets and orbit is disabled
     the game has already positioned the camera correctly, so return */
  if (ext_cam.mode == CAM_MODE_GAME 
   && !ext_cam.options.enable_orbit && !CamExHasOffsets()) {
    CamExGamePose(&state->last_eff);
    return;
  }
  /* save the [unmodified] game camera state */
  state->cam = cam;
  state->cam_prev = cam_prev;
  state->applied = 1;
  /* modify game camera state */
  GfxDVecToVec(&cam.trans, &eff.trans);
  cam.rot = eff.rot;
}

/**
 * Restore the game camera state to what it was before CamExApply was called.
 */
void CamExRestore(void) {
  ext_cam_state_t *state;

  state = &ext_cam.state;
  if (!state->applied) 
    return;
  cam = state->cam;
  cam_prev = state->cam_prev;
  state->applied = 0;
}

/**
 * Check if any of the camera modifier keys are currently held.
 * 
 * Used to prevent conflicts with other input handling when modifier keys are held.
 */
int CamExModifierHeld(const uint8_t *keys) {
  if (!ext_config.enable_hwpersp || !ext_cam.options.enable_input) 
    return 0;
  return keys[0x161] || keys[0x165] || keys[0x160] || keys[0x164]
      || keys[0x162] || keys[0x166];
}

/**
 * Update the extended camera state based on the current input.
 * 
 * This includes updating the orbit state and handling mouse and keyboard input 
 * for camera movement and zoom.
 */
void CamExUpdate(const ext_input_t *input) {
  const uint8_t *keys;
  ext_cam_state_t *state;
  cam_pose base, eff;
  dmat me, mb;
  dvec d, w, tgt, dir;
  double z, dist, step, speed, diff;
  int alt, shift, ctrl, look, pan;
  int i, mouse, held, dx, dy, wheel;

  keys = input->keys;
  wheel = input->mouse.wheel;
  held = input->mouse.held;
  dx = input->mouse.delta.x;
  dy = input->mouse.delta.y;

  if (!ext_config.enable_hwpersp
   || !CamExActive()) 
    return;

  state = &ext_cam.state;
  /* update orbit state */
  if (ext_cam.options.enable_orbit) {
    /* smooth out any jumps in orbit direction */
    diff = ext_cam.orbit_dir - state->orbit_dir;
    diff = fmod(diff + 540.0, 360.0) - 180.0; /* map to (-180.0, 180.0) */
    state->orbit_dir += fabs(diff) < 0.05 ? diff : diff * 0.08;
    /* increment orbit phase */
    state->orbit_phi += (ext_cam.orbit_speed / 30.0) * (TWO_PI / 360.0);
    state->orbit_phi = fmod(state->orbit_phi, TWO_PI);
  } 
  else {
    state->orbit_phi = 0.0;
    state->orbit_dir = ext_cam.orbit_dir;
  }

  if (!ext_cam.options.enable_input)
    return;

  /* get base and effective poses and their rotation matrices */
  d = (dvec){0, 0, 0};
  CamExBasePose(&base);
  CamExEffectivePose(&base, &eff);
  GfxDMatrixFromAng(&mb, &base.rot);
  GfxDMatrixFromAng(&me, &eff.rot);

  /* check if mouse is in game area */
  mouse = !input->mouse.ui_capture;
  /* control camera zoom with mouse wheel */
  if (mouse && wheel) {
    z = ext_cam.options.cam_zoom > 0.0 ? ext_cam.options.cam_zoom : 1.0;
    for (i=0;i<abs(wheel);i++) 
      z *= wheel > 0 ? 0.9 : 1.0 / 0.9;
    ext_cam.options.cam_zoom = limit(z, 0.25, 4.0);
  }
  /* when holding alt and dragging the mouse up/down, 
     seek the camera towards the center of the selected object */
  alt = keys[0x162] || keys[0x166];
  shift = keys[0x161] || keys[0x165];
  ctrl = keys[0x160] || keys[0x164];
  if (mouse && alt && (held & 1) && dy) {
    if (ExtGetSelectionCenter(&ext_ctxt, &tgt)) {
      /* get direction and distance from camera to target */
      dir = VecSub(tgt, eff.trans);
      dist = VecMag(dir);
      /* step by a distance proportional to the mouse movement */
      step = -dy * (dist * 0.005 > 2.0 ? dist * 0.005 : 2.0);
      if (step > dist - 64.0) 
        step = dist - 64.0;
      if (dist > 1.0 && step != 0.0) {
        /* undo base rotation */
        GfxRotD(&dir, &dir, &mb);
        /* scale direction by step size and apply to camera offset */
        dir = VecMulS(dir, step / dist);
        ext_cam.offs_trans = VecAdd(ext_cam.offs_trans, dir);
      }
    }
    return;
  }
  look = (held & 4) || ((held & 1) && shift);
  pan = (held & 2) || ((held & 1) && ctrl);
  /* when holding shift and dragging,
     change x, y rotation offset based on mouse movement */
  if (mouse && look) {
    ext_cam.offs_rot.x += dx * ext_cam.sens;
    ext_cam.offs_rot.y += dy * ext_cam.sens;
    ext_cam.offs_rot.y = limit(ext_cam.offs_rot.y, -89.0, 89.0);
  }
  /* when holding ctrl and dragging,
     change effective translation based on mouse movement */
  if (mouse && pan) {
    d.x -= dx * ext_cam.speed * 0.15;
    d.y += dy * ext_cam.speed * 0.15;
  }
  /* control effective translation with keyboard */
  if (!input->keys_ui_capture) {
    speed = ext_cam.speed * (shift ? 4.0 : 1.0);
    if (keys['i'] || (ctrl && keys['a'])) d.z -= speed;
    if (keys['k'] || (ctrl && keys['z'])) d.z += speed;
    if (keys['j'] || (ctrl && keys[0x80+80])) d.x -= speed;
    if (keys['l'] || (ctrl && keys[0x80+79])) d.x += speed;
    if (keys['o'] || (ctrl && keys[0x80+82])) d.y += speed;
    if (keys['u'] || (ctrl && keys[0x80+81])) d.y -= speed;
  }
  /* apply effective translation to camera offset */
  if (d.x != 0 || d.y != 0 || d.z != 0) {
    /* base rotation does not contribute to the movement direction
       so rotate by the transpose of the effective rotation first
       for the camera translation direction in sight, then apply the 
       counter rotation to offset movement from sight direction */
    GfxTransposeDMatrix(&me);
    GfxRotD(&d, &w, &me);
    GfxRotD(&w, &d, &mb);
    ext_cam.offs_trans = VecAdd(ext_cam.offs_trans, d);
  }
}

/* Helper function to convert a local path point to world coordinates. */
static void CamExPathPoint(zone_rect *r, zone_path_point *pt, dvec *out) {
  *out = Vec(dvec, r->x + pt->x, r->y + pt->y, r->z + pt->z);
}

static void GLDrawCamSegment(const dvec a, const dvec b) {
  dvec pa, pb;
  double t, nz;

  nz = 64.0;
  GfxTransformWorldToViewD(&a, &pa);
  GfxTransformWorldToViewD(&b, &pb);
  if (pa.z <= nz && pb.z <= nz) return;
  /* clip relevant end of the segment to the near plane */
  if (pa.z <= nz) {
    t = (nz - pa.z) / (pb.z - pa.z);
    pa = VecAdd(pa, VecMulS(VecSub(pb, pa), t));
  }
  if (pb.z <= nz) {
    t = (nz - pb.z) / (pa.z - pb.z);
    pb = VecAdd(pb, VecMulS(VecSub(pa, pb), t));
  }
  glVertex3d(pa.x, pa.y, -pa.z);
  glVertex3d(pb.x, pb.y, -pb.z);
}

/* Helper function for drawing arrow constituents. */
static void GLDrawArrowTri(dvec v0, dvec v1, dvec v2, dvec col, double shade) {
  dvec p0, p1, p2;

  GfxTransformWorldToViewD(&v0, &p0);
  if (p0.z <= 64.0) return;
  GfxTransformWorldToViewD(&v1, &p1);
  if (p1.z <= 64.0) return;
  GfxTransformWorldToViewD(&v2, &p2);
  if (p2.z <= 64.0) return;
  glColor3d(col.x * shade, col.y * shade, col.z * shade);
  glVertex3d(p0.x, p0.y, -p0.z);
  glVertex3d(p1.x, p1.y, -p1.z);
  glVertex3d(p2.x, p2.y, -p2.z);
}

/**
 * Draw a solid arrow.
 * 
 * @param o The origin point of the arrow.
 * @param dir The direction of the arrow.
 * @param u A vector orthogonal to dir.
 * @param v A vector orthogonal to u and dir. Also contributes, along with v,
 *          the scaling of the cross-section of the arrow shaft that intersects
 *          the plane defined by u and v.
 * @param len The length of the arrow (magnitude of dir also contributes).
 * @param rad The radius of the arrow shaft after scaling by u and v.
 * @param col The base color for the arrow.
 * @note The vectors u and v must be orthogonal to dir and to each other.
 */
static void GLDrawArrow(dvec o, dvec dir, dvec u, dvec v,
                        double len, double rad, dvec col) {
  enum { SH = 6, HD = 8 };
  dvec tip, base, ring0[SH], ring1[SH], hring[HD];
  double shaft_end, a, c, s, sh;
  int k, n;

  /* get base and tip positions for the arrow */
  shaft_end = len * 0.72;
  base = VecAdd(o, VecMulS(dir, shaft_end));
  tip = VecAdd(o, VecMulS(dir, len));
  /* compute vert positions for rings around the bases [front and rear] of the shaft */
  for (k=0;k<SH;k++) {
    a = k * TWO_PI / SH;
    c = cos(a) * rad;
    s = sin(a) * rad;
    ring0[k] = VecAdd(VecAdd(o, VecMulS(u, c)), VecMulS(v, s));
    ring1[k] = VecAdd(VecAdd(base, VecMulS(u, c)), VecMulS(v, s));
  }
  /* compute vert positions for a ring around the back of the arrow head */
  for (k=0;k<HD;k++) {
    a = k * TWO_PI / HD;
    c = cos(a) * rad * 2.6;
    s = sin(a) * rad * 2.6;
    hring[k] = VecAdd(VecAdd(base, VecMulS(u, c)), VecMulS(v, s));
  }
  /* fill quads around the shaft */
  for (k=0;k<SH;k++) {
    n = (k + 1) % SH;
    /* shade from half dim to full brightness based on ring angle */
    sh = 0.55 + 0.45 * cos(k * TWO_PI / SH + 0.8);
    GLDrawArrowTri(ring0[k], ring0[n], ring1[k], col, sh);
    GLDrawArrowTri(ring0[n], ring1[n], ring1[k], col, sh);
  }
  /* fill triangles around the back of the arrow head
     and around the cone of the arrow head */
  for (k=0;k<HD;k++) {
    n = (k + 1) % HD;
    /* shade from half dim to full brightness based on ring angle */
    sh = 0.6 + 0.4 * cos((k + 0.5) * TWO_PI / HD + 0.8);
    GLDrawArrowTri(hring[k], hring[n], tip, col, sh);
    /* base is shaded darker */
    GLDrawArrowTri(hring[n], hring[k], base, col, sh * 0.6);
  }
}

static void GLDrawCamPaths(zone_header *header, zone_rect *rect) {
  zone_path *path;
  dvec a, b;
  int pi, i, cur;

  /* draw segments between consecutive path points 
     segments on the current path are drawn as yellow lines 
     segments on other paths are drawn as thinner white lines */
  for (pi=0;pi<(int)header->path_count;pi++) {
    path = (zone_path *)cur_zone->items[header->paths_idx + pi];
    if (!path || !path->length) continue;
    cur = (path == cur_path);
    glLineWidth(cur ? 3.0f : 2.0f);
    glBegin(GL_LINES);
    if (cur) 
      glColor3ub(255, 220, 40); 
    else 
      glColor3ub(230, 230, 230);
    for (i=0;i<((int)path->length)-1;i++) {
      CamExPathPoint(rect, &path->points[i], &a);
      CamExPathPoint(rect, &path->points[i+1], &b);
      GLDrawCamSegment(a, b);
    }
    glEnd();
  }
}

static void GLDrawCamPathAxes(zone_header *header, zone_rect *rect) {
  enum { MAX_TRIS = 512 };
  static const dvec red = {1.0, 0.25, 0.25};
  static const dvec green = {0.25, 1.0, 0.3};
  static const dvec blue = {0.3, 0.45, 1.0};
  static dvec drawn[MAX_TRIS];
  zone_path *path;
  zone_path_point *pt;
  dmat mat;
  dvec prev, p, q, right, up, fwd;
  ang rot;
  double cum[2048];
  double total, target, want, sc;
  int i, j, d, n, pi;
  int ndrawn, count, last_idx, idx, dup;

  ndrawn = 0;
  target = 1500.0;
  glBegin(GL_TRIANGLES);
  for (pi=0;pi<(int)header->path_count;pi++) {
    path = (zone_path *)cur_zone->items[header->paths_idx + pi];
    n = path ? (int)path->length : 0;
    if (n < 1) continue;
    total = 0;
    if (n > 2048) n = 2048;
    /* get cumulative distances between consecutive path points */
    for (i=0;i<n;i++) {
      CamExPathPoint(rect, &path->points[i], &q);
      if (i) total += VecDist(q, prev);
      cum[i] = total;
      prev = q;
    }
    /* divided into n segments of approximately target length
       there are n + 1 total points connecting them */
    count = (int)(total / target + 0.5) + 1;
    /* ensure at least a start and end point */
    if (count < 2) count = 2;
    last_idx = -1;
    for (j=0;j<count;j++) {
      /* map [0, count-1] to [0, total] */
      want = total * j / (count - 1);
      idx = 0;
      /* find the index of the path point with 
         the closest cumulative distance to the desired point */
      for (i=0;i<n;i++) {
        if (fabs(cum[i] - want) < 
            fabs(cum[idx] - want)) 
          idx = i;
      }
      /* do not process the same index twice */
      if (idx == last_idx) continue;
      last_idx = idx;
      /* get world coordinates for the path point */
      pt = &path->points[idx];
      CamExPathPoint(rect, pt, &p);
      dup = 0;
      /* paths may loop around and revisit areas near earlier points
         ensure that axes are drawn at sufficiently spaced points 
         i.e. > 1/2 * target */
      for (d=0;d<ndrawn;d++) {
        if (VecSqrDist(p, drawn[d]) < (target * 0.5) * (target * 0.5)) {
          dup = 1;
          break;
        }
      }
      if (dup) continue;
      /* save this point for the test above in the next iterations */
      if (ndrawn < MAX_TRIS) 
        drawn[ndrawn++] = p;
      /* transform the point to view space */
      GfxTransformWorldToViewD(&p, &q);
      /* reject points behind the near plane */
      if (q.z <= 64.0) continue;
      /* scale axes so that they become smaller
         the closer they are to the near plane */
      sc = q.z * 0.12;
      sc = limit(sc, 10.0, 600.0);
      /* get bases for the orientation at this point */
      rot = (ang){ .x = pt->rot_x, .y = pt->rot_y, .z = pt->rot_z };
      GfxDMatrixFromAng(&mat, &rot);
      right = mat.v[0];
      up = mat.v[1];
      fwd = VecMulS(mat.v[2], -1.0);
      /* draw right, up, and forward arrows (red, green, blue)
         [using the orthogonal vectors for each arrow as required by the function]
         use scaling factor for both the radius and arrow lengths
         forward arrow is scaled 1.7 times the base */
      GLDrawArrow(p, right, up, fwd, sc, sc * 0.035, red);
      GLDrawArrow(p, up, fwd, right, sc, sc * 0.035, green);
      GLDrawArrow(p, fwd, right, up, sc * 1.7, sc * 0.035, blue);
    }
  }
  glEnd();
}

/**
 * Draw the camera overlay, including camera paths and axes.
 */
void GLDrawCamOverlay(void) {
  zone_header *header;
  zone_rect *rect;
  GLboolean depth;

  if (!ext_cam.options.show_paths || !CamExActive()) 
    return;
  header = (zone_header *)cur_zone->items[0];
  rect = (zone_rect *)cur_zone->items[1];
  /* disable depth test, textures, and blend for cam path overlay */
  depth = glIsEnabled(GL_DEPTH_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  /* draw the camera paths and axes */
  GLDrawCamPaths(header, rect);
  GLDrawCamPathAxes(header, rect);
  /* restore GL state */
  glLineWidth(1.0f);
  if (depth) 
    glEnable(GL_DEPTH_TEST);
}
