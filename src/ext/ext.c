#include "ext/ext.h"
#include "ext/math.h"
#include "ext/gfx.h"
#include "ext/tex.h"
#include "ext/gool.h"
#include "ext/level.h"

#include "ext/lib/gui.h"

#include "pc/gfx/soft.h"
#include "pc/time.h"

#ifdef _WIN32
#include <windows.h>
#include <SDL_opengl.h>
#else
#include <GL/gl.h>
#endif

extern ns_struct ns;
extern entry *cur_zone;
extern zone_path *cur_path;
extern int32_t cur_progress;
extern rect2 screen;
extern uint32_t screen_proj;

gool_object *hover_obj; /* exported to gool.c */

ext_config_t ext_config = {
  .enable_textures = 1,
  .enable_gouraud = 1
};

ext_context_t ext_ctxt = {
  .options = {
    .enable_snap = 1,
    .enable_auto_edge = 1,
    .enable_hover = 1,
    .show_overlay = 1,
    .show_normals = 1,
    .show_vertices = 1
  },
  .json_path = "./geometry.json",
  .stats_zone_dirty = 1,
  .stats_level_dirty = 1,
  .state = {
    .sel_vert = -1,
    .inherit_target = -1,
    .inherit_color_target = -1,
    .inherit_color_vertex = -1,
    .snap_px = 14.0,
  },
};

/**
 * Compute the aspect ratio for the selected aspect mode.
 */
static double ExtViewAspect(void) {
  switch (ext_config.aspect_mode) {
  case 1: return (16.0 / 9.0) / (4.0 / 3.0);
  case 2: return (10.0 / 7.0) / (4.0 / 3.0);
  }
  return 1.0;
}

/**
 * Compute the zoom level for the view based on the camera options and configuration.
 */
double ExtViewZoom(void) {
  double z = ext_cam.options.cam_zoom;
  return (ext_config.enable_hwpersp && z >= 0.1 && z <= 16.0) ? z : 1.0;
}

/**
 * Get the common set of parameters used by many graphics-specific functions.
 */
static ext_viewparams_t ExtViewParams(const ext_context_t *ctxt) {
  ext_viewparams_t params;
  double sp;

  sp = (screen_proj > 0) ? (double)screen_proj : 512.0;
  params.mpolys = ctxt->mpolys;
  params.mpoly_count = ctxt->mpoly_count;
  params.hdim.w = 256.0 / sp * ExtViewAspect();
  params.hdim.h = 120.0 / sp;
  params.window_dim = ctxt->input.window.dim;
  params.zoom = ExtViewZoom();
  return params;
}

int ExtStatusV(ext_context_t *ctxt, const char *fmt, va_list args) {
  vsnprintf(ctxt->status, sizeof(ctxt->status), fmt, args);
  ctxt->status_time = GetTicksMonotonic();
  return 1;
}

/**
 * Set the status message for the extension context.
 * Returns 1 on success.
 */
int ExtStatus(ext_context_t *ctxt, const char *fmt, ...) {
  va_list args;
  int result;

  va_start(args, fmt);
  result = ExtStatusV(ctxt, fmt, args);
  va_end(args);
  return result;
}

/**
 * Get the name of the world with the specified EID.
 * Returns 1 on success, 0 if the world could not be found.
 */
int ExtWorldName(eid_t world_eid, char *name_out) {
  zone_world *world = ZoneGetWorld(cur_zone, world_eid);
  if (!world) 
    return 0;
  WorldName(cur_zone, world, name_out);
  return 1;
}

/**
 * Get the name of the polygon referenced by the given ext_polyref_t.
 */
static void ExtPolyName(const ext_polyref_t *ref, char *name_out) {
  char world[64];

  ExtWorldName(ref->world_eid, world);
  sprintf(name_out, "%spoly %d (%s)",
          ref->manual ? "new " : "", ref->poly_idx, world);
}

/**
 * Get the name of the edge referenced by the given ext_polyref_t 
 * and edge index.
 */
static void ExtEdgeName(const ext_polyref_t *ref, int idx, char *name_out) {
  char name[96];

  ExtPolyName(ref, name);
  sprintf(name_out, "%s edge %d", name, idx);
}

/**
 * Get the name of the vertex referenced by the given ext_polyref_t 
 * and vertex index.
 */
static void ExtVertName(const ext_polyref_t *ref, int idx, char *name_out) {
  char name[96];

  ExtPolyName(ref, name);
  sprintf(name_out, "%s vert %d", name, idx);
}

/**
 * Update the status message to indicate the currently selected polygon.
 */
static void ExtSelectedPolyStatus(ext_context_t *ctxt) {
  char name[96];

  ExtPolyName(&ctxt->state.pick, name);
  ExtStatus(ctxt, "Selected %s", name);
}

static void ExtRefreshTextureBlocks(ext_context_t *ctxt, entry *zone, int force) {
  if (!force && ctxt->texblock_zone == zone) 
    return;
  ctxt->texblock_count = ZoneTextureBlocks(zone, ctxt->texblocks);
  ctxt->texblock_zone = zone;
}

/* Recompute the polygon's atlas id + UVs from its subtexture and winding. */
static int ExtResolveManualTex(ext_mpoly_t *mp) {
  texinfo tinfo;
  fvec uvs[4];
  int i, id;

  if (!mp->has_tinfo) 
    return 1;
  tinfo = mp->tinfo;
  memset(uvs, 0, sizeof(uvs));
  id = TextureLoad(&tinfo, &uvs);
  if (id < 0) 
    return 0;
  mp->texid = id;
  for (i=0;i<3;i++) 
    mp->uvs[i] = uvs[i];
  mp->flags = tinfo.semi_trans;
  return 1;
}

static void ExtComputeZoneStats(ext_context_t *ctxt, entry *zone, ext_stats_t *out, ddim2 hdim) {
  ext_viewparams_t params;
  cv_grid_t grid;
  double c, c_sum, c_min;
  int i, cam_count;

  memset(out, 0, sizeof(*out));
  if (!zone || !zone->items[0]) 
    return;
  cam_count = ZonePathPointMatrices(zone, ctxt->cam_samples, EXT_MAX_CAM_SAMPLES);
  if (cam_count <= 0) 
    return;
  params = ExtViewParams(ctxt);
  c_sum = 0.0;
  c_min = 1.0;
  for (i=0;i<cam_count;i++) {
    ZoneBuildCoverageGrid(zone, &params, &ctxt->cam_samples[i], &hdim, &grid);
    c = GfxCvGetCoverage(&grid);
    c_sum += c;
    if (c < c_min) 
      c_min = c;
  }
  out->zone_avg_coverage = c_sum / (double)cam_count;
  out->zone_min_coverage = c_min;
  out->zone_samples = cam_count;
}

static void ExtRefreshZoneAndLevelStats(ext_context_t *ctxt, int recompute_level) {
  nsd *directory;
  nsd_pte *pte;
  entry *zone;
  ext_stats_t stats, tmp;
  ext_viewparams_t params;
  size_t i;
  char *eid_str;
  eid_t zone_eid;

  if (!ext_config.enable_hwpersp 
   || !cur_zone || !cur_zone->items[0]) 
    return;
  params = ExtViewParams(ctxt);
  ExtComputeZoneStats(ctxt, cur_zone, &stats, params.hdim);
  ctxt->stats = stats;
  ctxt->stats_zone = cur_zone;
  ctxt->stats_zone_dirty = 0;
  if (!recompute_level) 
    return;
  ctxt->stats.level_zones = 0;
  ctxt->stats.level_samples = 0;
  ctxt->stats.level_avg_coverage = 0.0;
  ctxt->stats.level_min_coverage = 1.0;
  directory = ns.nsd;
  if (!directory)
    return;
  for (i=0;i<directory->page_table_size;i++) {
    pte = &directory->page_table[i];
    eid_str = NSEIDToString(pte->eid);
    if (eid_str[4] != 'Z') 
      continue;
    zone_eid = pte->eid;
    zone = NSOpen(&zone_eid, 1, 1);
    if (!zone || ISERRORCODE(zone) || !zone->items[0]) 
      continue;
    tmp = (ext_stats_t){0};
    ExtComputeZoneStats(ctxt, zone, &tmp, params.hdim);
    if (tmp.zone_samples > 0) {
      ctxt->stats.level_zones++;
      ctxt->stats.level_samples += tmp.zone_samples;
      ctxt->stats.level_avg_coverage += tmp.zone_avg_coverage * (double)tmp.zone_samples;
      if (tmp.zone_min_coverage < ctxt->stats.level_min_coverage) 
        ctxt->stats.level_min_coverage = tmp.zone_min_coverage;
    }
    NSClose(&zone_eid, 1);
  }
  if (ctxt->stats.level_samples > 0)
    ctxt->stats.level_avg_coverage /= (double)ctxt->stats.level_samples;
  else
    ctxt->stats.level_min_coverage = 0.0;
  ctxt->stats_level_dirty = 0;
}

/**
 * Add a new candidate polygon for extrusion (to fill empty regions).
 * Only works if candidate generation is enabled.
 */
static void ExtAddCandidate(ext_context_t *ctxt, int manual, uint32_t world_eid, int poly_idx, int edge_idx, const dtri2_t ndc) {
  ext_cand_t *cand;
  int i;

  if (ctxt->candidate_count >= EXT_MAX_CANDS) 
    return;
  cand = &ctxt->candidates[ctxt->candidate_count++];
  memset(cand, 0, sizeof(*cand));
  cand->ref.manual = manual;
  cand->ref.world_eid = world_eid;
  cand->ref.poly_idx = poly_idx;
  cand->ref.edge_idx = edge_idx;
  /* ndc is permuted so that [0],[1] is the edge; proj is in polygon order */
  for (i=0;i<3;i++) 
    cand->proj[(i + edge_idx) % 3] = ndc[i];
}

static void ExtBuildRuntimeCandidates(ext_context_t *ctxt, int rebuild) {
  ext_viewparams_t params;
  ext_edgeref_t *ref;
  zone_header *zh;
  zone_world *world;
  dtri2_t ndc;
  int i, refilter;

  ctxt->candidate_count = 0;
  if (!ctxt->options.enable_candidates 
   || !ctxt->options.enable_pick 
   || !cur_zone || !cur_zone->items[0]) {
    free(ctxt->cache.edges); 
    free(ctxt->cache.void_edges);
    memset(&ctxt->cache, 0, sizeof(ctxt->cache));
    return;
  }
  params = ExtViewParams(ctxt);
  zh = (zone_header *)cur_zone->items[0];
  if (ctxt->cache.zone != cur_zone) 
    rebuild = 1;
  refilter = rebuild || ctxt->cache.path != cur_path || ctxt->cache.progress != cur_progress;
  if (rebuild) {
    free(ctxt->cache.edges);
    ctxt->cache.edge_count = ZoneGetBoundaryEdges(cur_zone, &params, &ctxt->cache.edges);
    ctxt->cache.zone = cur_zone;
  }
  if (refilter) {
    free(ctxt->cache.void_edges);
    ctxt->cache.void_count = ZoneFilterVoidEdges(cur_zone, &params, 
      ctxt->cache.edges, ctxt->cache.edge_count, &ctxt->cache.void_edges);
    ctxt->cache.path = cur_path;
    ctxt->cache.progress = cur_progress;
  }

  for (i=0;i<ctxt->cache.void_count;i++) {
    ref = &ctxt->cache.void_edges[i];
    world = &zh->worlds[ref->world_idx];
    if (!GfxEdgeRefProject(world, ref, &params, ndc)
     || GfxTri2dEdgeOffscreen(ndc, 0)) 
      continue;
    ExtAddCandidate(ctxt, ref->manual, WorldEID(world), ref->poly_idx, ref->edge_idx, ndc);
  }
  if (!ctxt->state.pick_valid && ctxt->candidate_count > 0) {
    ctxt->state.pick = ctxt->candidates[0].ref;
    ctxt->state.pick_valid = 1;
    ctxt->state.sel_edge = ctxt->state.pick.edge_idx;
  }
}

static void ExtRefreshCandidates(ext_context_t *ctxt) {
  ExtBuildRuntimeCandidates(ctxt, 1);
}

/**
 * Get a reference to the selected poly 
 * provided it exists and, if a manual poly, provided it is active.
 */
static ext_polyref_t *ExtSelectedRef(ext_context_t *ctxt) {
  ext_polyref_t *ref;
  
  ref = &ctxt->state.pick;
  if (ctxt->state.pick_valid &&
      (!ref->manual || (ref->poly_idx >= 0 && ref->poly_idx < ctxt->mpoly_count &&
                        ctxt->mpolys[ref->poly_idx].active)))
    return ref;
  return 0;
}

/**
 * Get the index of the selected polygon as a candidate 
 * when the selected polygon is a candidate.
 */
static int ExtSelectedCandidate(ext_context_t *ctxt) {
  ext_polyref_t *ref, *cand;
  int i, idx;

  ref = ExtSelectedRef(ctxt);
  if (!ref) 
    return -1;
  idx = -1;
  for (i=0;i<ctxt->candidate_count;i++) {
    cand = &ctxt->candidates[i].ref;
    if (cand->manual == ref->manual 
     && cand->world_eid == ref->world_eid 
     && cand->poly_idx == ref->poly_idx
     && cand->edge_idx == ref->edge_idx) 
      return i;
    if (idx < 0) 
      idx = i;
  }
  return idx;
}

/**
 * Helper function to pick a manual polygon.
 */
static void ExtPickManual(ext_context_t *ctxt, int idx, int edge) {
  ext_polyref_t *ref;

  ref = &ctxt->state.pick;
  memset(ref, 0, sizeof(*ref));
  ref->manual = 1;
  ref->world_eid = ctxt->mpolys[idx].world_eid;
  ref->poly_idx = idx;
  ref->edge_idx = edge;
  ctxt->state.pick_valid = 1;
  ctxt->state.sel_edge = edge;
}

/**
 * Resolve the active polygon. 
 * 
 * The polygon vertices are permuted so that those of the
 * current selected edge are positioned at indices 0 and 1.
 */
static int ExtActivePoly(ext_context_t *ctxt, ext_poly_t *out) {
  const ext_polyref_t *src;
  ext_polyref_t ref;
  ext_viewparams_t params;

  src = ExtSelectedRef(ctxt);
  params = ExtViewParams(ctxt);
  if (!src) 
    return 0;
  ref = *src;
  ref.edge_idx = ctxt->state.sel_edge;
  return GfxPolyFromRef(&ref, &params, 1, out);
}

/**
 * Insert a new manual polygon into the context.
 */
static void ExtInsertPoly(ext_context_t *ctxt, const ext_poly_t *src) {
  ext_mpoly_t *mp;

  if (ctxt->mpoly_count >= EXT_MAX_MANUAL_POLYS) 
    return;
  mp = &ctxt->mpolys[ctxt->mpoly_count++];
  memset(mp, 0, sizeof(*mp));
  mp->poly = *src;
  mp->active = 1;
}

/**
 * Delete the current selected polygon (manual only). 
 * (private variant)
 */
static void ExtDeleteSelectedPoly(ext_context_t *ctxt) {
  ext_polyref_t *ref;

  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual) 
    return;
  ctxt->mpolys[ref->poly_idx].active = 0;
  ctxt->state.pick_valid = 0;
  ctxt->state.drag_active = 0;
  ctxt->state.inherit_target = -1;
  ctxt->state.inherit_color_target = -1;
  ctxt->stats_zone_dirty = 1;
  ctxt->stats_level_dirty = 1;
  ExtRefreshCandidates(ctxt);
}

/**
 * Check if moving vertex vert_idx of polygon poly_idx 
 * to the new position 'view' would collapse any welded polygon 
 * to a line or point.
 */
static int ExtVertCanMove(ext_context_t *ctxt, int poly_idx, int vert_idx, vec view) {
  ext_mpoly_t *mp, *mpo;
  tri_t tri;
  vec old;
  double area;
  int i, j, k;
  
  mp = &ctxt->mpolys[poly_idx];
  old = mp->verts[vert_idx];
  /* for all other manual polygons */
  for (i=0;i<ctxt->mpoly_count;i++) {
    mpo = &ctxt->mpolys[i];
    /* skip if not active or not in the same world */
    if (!mpo->active || mpo->world_eid != mp->world_eid) 
      continue;
    /* check all vertices of the other polygon */
    for (j=0;j<3;j++) {
      if (mpo->vertex_locked[j]) continue;
      if (!VecEq(mpo->verts[j], old)) continue;
      /* vertex j of the other polygon (unlocked) 
         is shared with vertex vert_idx of this polygon; 
         check if moving said vertex would collapse the other polygon 
         to a line or point */
      for (k=0;k<3;k++) 
        tri[k] = mpo->verts[k];
      tri[j] = view;
      area = GfxTriArea(tri);
      if (area < 16.0) 
        return 0;
    }
  }
  return 1;
}

/**
 * Move vertex vert_idx of polygon poly_idx to the new position 'view',
 * updating all welded instances of the vertex.
 */
static void ExtVertMove(ext_context_t *ctxt, int poly_idx, int vert_idx, vec view) {
  ext_mpoly_t *mp, *mpo;
  vec old;
  int i, j;

  /* make sure to move all instances of the vertex
     across welded polygons */
  mp = &ctxt->mpolys[poly_idx];
  old = mp->verts[vert_idx];
  for (i=0;i<ctxt->mpoly_count;i++) {
    mpo = &ctxt->mpolys[i];
    if (!mpo->active || mpo->world_eid != mp->world_eid) continue;
    for (j=0;j<3;j++) {
      if (mpo->vertex_locked[j]) continue;
      if (VecEq(mpo->verts[j], old)) 
        mpo->verts[j] = view;
    }
  }
  ctxt->stats_zone_dirty = 1;
  ctxt->stats_level_dirty = 1;
  ExtRefreshCandidates(ctxt);
}

/**
 * Find the vertex closest to the mouse cursor,
 * amongst vertices for all world and manual polygons 
 * in the current zone.
 * 
 * @param ctxt The extension context.
 * @param dst The destination world.
 * @param skip A vertex to skip (usually the currently selected vertex).
 * @param out_local Output for the local coordinates of the found vertex.
 * @param out_ndc Output for the NDC coordinates of the found vertex.
 * @param out_col Output for the color of the found vertex.
 * @return 1 if a vertex was found, 0 otherwise.
 */
static int ExtFindSnap(ext_context_t *ctxt, const zone_world *dst, ext_vertref_t *skip,
                       vec *local_out, dvec2 *ndc_out, rgba *col_out) {
  ext_viewparams_t params;
  ext_vertref_t ref;
  zone_header *zh;
  zone_world *world;
  vec local;
  rgba color;

  params = ExtViewParams(ctxt);
  if (!ZoneNearestVert(cur_zone, &params, skip, &ctxt->input.mouse.pos, ctxt->state.snap_px, &ref, ndc_out))
    return 0;
  if (!GfxVertFromRef(&ref, &params, &local, &color))
    return 0;
  zh = (zone_header *)cur_zone->items[0];
  world = &zh->worlds[ref.world_idx];
  if (col_out) *col_out = color;
  local_out->x = local.x + (world->header->trans_x - dst->header->trans_x);
  local_out->y = local.y + (world->header->trans_y - dst->header->trans_y);
  local_out->z = local.z + (world->header->trans_z - dst->header->trans_z);
  return 1;
}

/**
 * Pick the vertex of the currently selected polygon 
 * closest to the mouse cursor.
 */
static int ExtPickSelPolyVertex(ext_context_t *ctxt) {
  ext_viewparams_t params;
  ext_polyref_t *ref;
  ext_mpoly_t *mp;
  char name[128];
  int idx;

  /* get view params */
  params = ExtViewParams(ctxt);
  /* get ref to selected poly */
  ref = ExtSelectedRef(ctxt);
  if (!ref) 
    return 0;
  /* get nearest vert of the selected poly */
  idx = GfxPolyNearestVert(ref, &ctxt->input.mouse.pos, &params);
  if (idx < 0) 
    return 0;
  /* get the selected manual poly in case this is one */
  mp = 0;
  if (ref->manual)
    mp = &ctxt->mpolys[ref->poly_idx];
  /* set selected vertex and activate dragging if allowed */
  ctxt->state.sel_vert = idx;
  ExtVertName(ref, idx, name);
  ExtStatus(ctxt, "Selected %s", name);
  if (ctxt->options.enable_edit && mp && !mp->vertex_locked[idx])
    ctxt->state.drag_active = 1; /* can only drag unlocked manual poly verts */
  return 1;
}

/**
 * Get the vertex currently hovered by the mouse cursor.
 */
static int ExtHoveredVertex(ext_context_t *ctxt, dvec2 *ndc_out, rgba *col_out) {
  ext_mpoly_t *dst;
  ext_vertref_t skip;
  zone_world *world;
  vec tmp;

  dst = &ctxt->mpolys[ctxt->state.inherit_color_target];
  world = ZoneGetWorld(cur_zone, dst->world_eid);
  if (!world) 
    return 0;
  skip.world_eid = dst->world_eid;
  skip.poly_idx = ctxt->state.inherit_color_target;
  skip.vert_idx = ctxt->state.inherit_color_vertex;
  return ExtFindSnap(ctxt, world, &skip, &tmp, ndc_out, col_out);
}

/**
 * Pick the polygon under the mouse cursor.
 */
static void ExtPickPolygon(ext_context_t *ctxt) {
  cv_grid_t grid;
  ext_viewparams_t params;
  ext_polyref_t pick; /*, target; */
  ext_mpoly_t /* *mp,*/ *dst;
  /* zone_world *world;
  wgeo_polygon *poly; */
  texinfo tinfo;
  dtri2_t tri;
  dvec2 ndc;
  char src_name[128]; /*, tgt_name[128]; */
  int found, has_tinfo;

  /* transform mouse pos to NDC */
  params = ExtViewParams(ctxt);
  if (!GfxTransformPxToNDC(&ctxt->input.mouse.pos, &params, &ndc))
    return;
  /* get poly at NDC mouse pos */
  found = ZonePolyAtPoint(cur_zone, &params, &ndc, &pick, tri);
  if (!found) {
    if (ctxt->state.inherit_target >= 0 || ctxt->state.hover_mode) 
      return; 
    /* reset selection */
    if (ctxt->state.pick_valid)
      ExtStatus(ctxt, "Cleared polygon selection");
    ctxt->state.pick_valid = 0;
    ctxt->state.sel_vert = -1; 
    return; 
  }
  /* when hovering, set tri under the mouse into the state */
  if (ctxt->state.hover_mode) {
    memcpy(ctxt->state.hover_tri, tri, sizeof(ctxt->state.hover_tri));
    ctxt->state.hover_tri_valid = 1;
    return;
  }
  if (ctxt->state.inherit_target >= 0) { /* is there a target polygon for inheritance? */
    /* we must be choosing a source to inherit from;
      if that source is not the target polygon and it has texinfo
      then the target should inherit it */
    has_tinfo = GfxPolyRefTexinfo(&pick, &params, &tinfo);
    if (ctxt->state.inherit_target < ctxt->mpoly_count 
      && has_tinfo) {
      dst = &ctxt->mpolys[ctxt->state.inherit_target];
      dst->tinfo = tinfo; 
      dst->has_tinfo = 1;
      /*
      target = (ext_polyref_t){ .manual = 1, .world_eid = dst->world_eid,
                                .poly_idx = ctxt->state.inherit_target };
      ExtPolyName(&target, tgt_name);
      */
      ExtPolyName(&pick, src_name);
      ExtStatus(ctxt, "Inherited texture from %s", src_name);
    }
    if (has_tinfo) 
      ctxt->state.inherit_target = -1;
    return;
  }

  /* mark this polygon (and the edge to extrude from) as the picked polygon */
  ctxt->state.pick = pick;
  if (pick.manual) 
    ctxt->state.pick.world_eid = ctxt->mpolys[pick.poly_idx].world_eid;
  if (ctxt->options.enable_auto_edge) {
    ZoneBuildCoverageGrid(cur_zone, &params, 0, &params.hdim, &grid);
    ctxt->state.pick.edge_idx = GfxTriBestVoidEdge(&grid, tri);
  }
  ctxt->state.sel_edge = ctxt->state.pick.edge_idx;
  ctxt->state.sel_vert = GfxTri2dNearestVertex(tri, &ndc, &params);
  ctxt->state.pick_valid = 1;
  ExtSelectedPolyStatus(ctxt);
}

/**
 * Update the drag state for the currently selected vertex, 
 * handling snapping and movement within the world.
 */
static void ExtDragUpdate(ext_context_t *ctxt) {
  ext_viewparams_t params;
  ext_polyref_t *ref;
  ext_vertref_t skip;
  ext_mpoly_t *mp;
  zone_world *world;
  vec cur, local;
  dvec2 ndc, snap;
  rgba snap_col;
  char name[128];
  int snapped, poly_idx, vert_idx;

  ctxt->state.snap_valid = 0;
  ref = ExtSelectedRef(ctxt);
  /* make sure a selected manual poly and selected vertex exist */
  if (!ref || !ref->manual) {
    ctxt->state.drag_active = 0;
    return;
  }
  vert_idx = ctxt->state.sel_vert;
  if (vert_idx < 0 || vert_idx >= 3) {
    ctxt->state.drag_active = 0;
    return; 
  }
  poly_idx = ref->poly_idx;
  mp = &ctxt->mpolys[poly_idx];
  /* if the manual poly is inactive or the selected vertex is locked, stop dragging */
  if (!mp->active || mp->vertex_locked[vert_idx]) {
    ctxt->state.drag_active = 0;
    return;
  }
  cur = mp->verts[vert_idx];
  /* otherwise get the world for the manual poly */
  world = ZoneGetWorld(cur_zone, mp->world_eid);
  /* if the world is not found, stop dragging */
  if (!world) { 
    ctxt->state.drag_active = 0; 
    return; 
  }
  /* when snapping is enabled, find a closest vertex (snap point) */
  snapped = 0;
  if (ctxt->options.enable_snap) {
    skip.world_eid = mp->world_eid;
    skip.poly_idx = poly_idx;
    skip.vert_idx = vert_idx;
    snapped = ExtFindSnap(ctxt, world, &skip, &local, &snap, &snap_col);
    /* do not snap if moving to the snap position would result in a degenerate polygon */
    if (snapped && !ExtVertCanMove(ctxt, poly_idx, vert_idx, local))
      snapped = 0;
  }
  if (snapped) { /* vertex snapped? */
    /* update snap state */
    ctxt->state.snap_valid = 1; 
    ctxt->state.snap_pt = snap;
    if (ctxt->options.enable_snap_color) { 
      mp->colors[vert_idx] = snap_col;
      mp->colors[vert_idx].a = ~0u;
    }
  } 
  else { /* vertex not snapped */
    params = ExtViewParams(ctxt);
    /* get mouse pos in NDC */
    if (!GfxTransformPxToNDC(&ctxt->input.mouse.pos, &params, &ndc))
      return;
    /* deproject the mouse position to local space
       using the current local vertex position as the base */
    if (!GfxDeprojectNDCToLocal(&ndc, &params, &world->trans, &cur, &local))
      return;
  }
  /* move the vertex if there is any change in position */
  if (!VecEq(local, cur)) {
    ExtVertMove(ctxt, poly_idx, vert_idx, local);
    ExtVertName(ref, vert_idx, name);
    ExtStatus(ctxt, "Moved %s to (%d,%d,%d)",
              name, local.x, local.y, local.z);
  }
}

/**
 * Process mouse input for the extension context, 
 * handling hover, clicks, and dragging of vertices.
 */
static void ExtProcessMouse(ext_context_t *ctxt) {
  ext_state_t *state;
  ext_polyref_t target;
  ext_mpoly_t *mp;
  dvec2 ndc;
  rgba col;
  char name[128];

  if (!cur_zone || !cur_zone->items[0]) 
    return;
  state = &ctxt->state;
  /* check inherit [color] targets for validity against the manual poly count
     and reset if invalid */
  if (state->inherit_color_target >= ctxt->mpoly_count 
   || (state->inherit_color_target >= 0 && !ctxt->mpolys[state->inherit_color_target].active))
    state->inherit_color_target = -1;
  if (state->inherit_target >= ctxt->mpoly_count) 
    state->inherit_target = -1;
  state->hover_tri_valid = 0;
  state->hover_vtx_valid = 0;
  /* if mouse is in the game view/outside of the GUI 
     and not dragging an item */
  if (!state->ui_capture && !state->drag_active) {
    /* handle mouse hover type picks */
    /* in 'inherit [poly] texture' state? */
    if (state->inherit_target >= 0) { 
      state->hover_mode = 1;
      ExtPickPolygon(ctxt);
      state->hover_mode = 0;
    } 
    /* in 'inherit [vertex] color' state? */
    else if (state->inherit_color_target >= 0)
      state->hover_vtx_valid = ExtHoveredVertex(ctxt, &state->hover_vtx, &col);
    /* 'highlight poly on hover' enabled? */
    else if (ctxt->options.enable_hover) {
      state->hover_mode = 1;
      ExtPickPolygon(ctxt);
      state->hover_mode = 0;
      state->hover_kind = 1;
    }
    if (state->inherit_target >= 0) 
      state->hover_kind = 0;
  }
  /* mouse click? */
  if (ctxt->input.mouse.press) {
    if (!state->ui_capture) { /* mouse in game view/not in GUI view? */
      if (state->inherit_color_target >= 0) { /* vertex color to inherit? */
        if (ExtHoveredVertex(ctxt, &ndc, &col)) {
          mp = &ctxt->mpolys[state->inherit_color_target];
          mp->colors[state->inherit_color_vertex] = col;
          mp->colors[state->inherit_color_vertex].a = ~0u;
          target = (ext_polyref_t){ .manual = 1, .world_eid = mp->world_eid,
                                    .poly_idx = state->inherit_color_target };
          ExtVertName(&target, state->inherit_color_vertex, name);
          ExtStatus(ctxt, "Inherited color to %s", name);
        }
        state->inherit_color_target = -1;
      } 
      /* else try handle starting a vertex drag? (edit mode only) */
      else if (state->inherit_target < 0 && ExtPickSelPolyVertex(ctxt)) { /* dragging a vertex of the selected poly */ }
      else 
        ExtPickPolygon(ctxt);
    }
  }
  if (state->drag_active) {
    if (ctxt->input.mouse.held) 
      ExtDragUpdate(ctxt);
    else {
      state->drag_active = 0; 
      state->snap_valid = 0; 
    }
  }
}

static int ExtGoolOnKill(gool_object *obj) {
  ext_state_t *state;
  
  state = &ext_ctxt.state;
  if (state->obj_sel && (!obj || state->obj_sel == obj))
    ExtSelectObject(&ext_ctxt, 0);
  if (!obj || state->obj_drag == obj)
    state->obj_drag = 0;
  if (!obj || state->obj_hover == obj)
    state->obj_hover = 0;
  if (!obj || hover_obj == obj)
    hover_obj = 0;
  return 0;
}

/**
 * Process mouse input for the extension context, 
 * handling selection, hover, and dragging of objects.
 */
static int ExtProcessObjects(ext_context_t *ctxt) {
  ext_viewparams_t params;
  ext_state_t *state;
  ext_mouse_t *mouse;
  zone_world *world;
  gool_object *obj;
  gool_bound bound;
  dvec2 ndc;
  vec surface, center, offset;
  char name[64], zone_name[16], world_name[64];
  int picked;

  GoolOnKill(ExtGoolOnKill);
  state = &ctxt->state;
  mouse = &ctxt->input.mouse;
  params = ExtViewParams(ctxt);
  picked = 0;
  /* when the zone changes, cancel interactions tied to the 
     previous zone */
  if (state->obj_zone != cur_zone) {
    state->obj_zone = cur_zone;
    state->obj_hover = 0;
    state->obj_drag = 0;
  }
  /* when the selected object is no longer a type 1 handle, 
     deselect it */
  if (state->obj_sel 
   && state->obj_sel->handle.type != 1) {
    state->obj_sel = 0;
    state->obj_drag = 0;
  }
  state->obj_hover = 0;
  hover_obj = 0;
  /* if mouse control is disabled, reset drag state and return */
  if (!ctxt->options.object_mouse_control 
   || !cur_zone || !cur_zone->items[0]) {
    state->obj_drag = 0;
    return 0;
  }
  /* otherwise, still reset drag state if the mouse isn't held
     or the UI is capturing the mouse */
  if (!mouse->held || state->ui_capture)
    state->obj_drag = 0;
  /* transform the mouse position to NDC */
  if (!GfxTransformPxToNDC(&mouse->pos, &params, &ndc)) 
    return 0;
  /* if UI is not capturing the mouse and not in a drag state... */
  if (!state->ui_capture && !state->obj_drag) {
    /* get the object under the mouse and set it as the hover object */
    obj = GoolObjectByPoint2d(ndc, params.hdim);
    state->obj_hover = obj;
    hover_obj = obj;
    /* if the mouse is pressed and there is an object under it... */
    if (mouse->press && obj) {
      /* set it as the selected object */
      ExtSelectObject(ctxt, obj);
      picked = 1;
      /* pre-emptively initiate a drag;
         will be cancelled if the mouse is released */
      state->obj_drag = obj;
      state->obj_drag_start = mouse->pos;
      state->obj_drag_moved = 0;
    } 
  }
  /* if an object is being dragged 
     and the mouse has moved from the initial drag point... */
  obj = state->obj_drag;
  if (obj && !Vec2Eq(mouse->pos, state->obj_drag_start))
    state->obj_drag_moved = 1;
  if (obj && state->obj_drag_moved) {
    GoolObjectName(obj, name);
    strcpy(zone_name, NSEIDToString(cur_zone->eid));
    /* attempt to place the object on the world surface under the mouse */
    if (!GoolObjectBound2(obj, &bound))
      ExtStatus(ctxt, "Cannot move %s: bound unavailable in %s", name, zone_name);
    /* get the projected world surface point under the mouse */
    else if (ZoneDeprojectPoint(cur_zone, &params.hdim, &ndc, &surface, &world)) {
      /* get the bottom center point of the object bounding box */
      center = VecDivS(VecAdd(bound.p1, bound.p2), 2);
      center.y = min(bound.p1.y, bound.p2.y);
      /* get its offset from the object position 
         and adjust surface position by that offset */
      offset = VecSub(center, obj->trans);
      obj->trans = VecSub(surface, offset);
      obj->floor_y = obj->trans.y;
      WorldName(cur_zone, world, world_name);
      ExtStatus(ctxt, "Placed %s at (%d,%d,%d) in %s",
                name, obj->trans.x, obj->trans.y, obj->trans.z, world_name);
    }
    else
      ExtStatus(ctxt, "Cannot place %s: no surface under mouse in %s",
                name, zone_name);
  }
  return picked || state->obj_drag != 0;
}

int ExtSetFullscreen(ext_context_t *ctxt, int fullscreen) {
  int actual;
  
  actual = GLFullscreen(fullscreen);
  if (actual < 0) {
    ExtStatus(ctxt, "Unable to change fullscreen mode");
    return 0;
  }
  ext_config.fullscreen = actual;
  if (actual != !!fullscreen) {
    ExtStatus(ctxt, "Unable to change fullscreen mode");
    return 0;
  }
  return 1;
}

/**
 * Update the extension context based on the current input state.
 * Handles mouse input, object selection, and dragging.
 */
extern int pad_lock;
void ExtUpdate(ext_context_t *ctxt, const gl_input *input) {
  ext_input_t *xinput;
  ext_mouse_t *mouse;
  int object_mouse, press, fullscreen;

  ExtGetStatus(ctxt);
  xinput = &ctxt->input;
  fullscreen = GLFullscreen(-1);
  if (fullscreen >= 0)
    ext_config.fullscreen = fullscreen;
  if (input->keys[0xC4] && !xinput->keys[0xC4])
    ExtSetFullscreen(ctxt, !ext_config.fullscreen);
  if ((input->keys[0x160] && !xinput->keys[0x160]) 
    ||(input->keys[0x164] && !xinput->keys[0x164]))
    pad_lock = 3;
  else if ((!input->keys[0x160] && xinput->keys[0x160]) 
        || (!input->keys[0x164] && xinput->keys[0x164]))
    pad_lock = 0;

  xinput->window = input->window;
  xinput->keys_ui_capture = GuiIgWantCaptureKeyboard();
  memcpy(xinput->keys, input->keys, sizeof(xinput->keys));

  mouse = &xinput->mouse;
  mouse->ui_capture = GuiIgWantCaptureMouse();
  ctxt->state.ui_capture = mouse->ui_capture;
  mouse->prev_pos = mouse->pos;
  mouse->pos = input->mouse;
  mouse->held = input->buttons;
  mouse->wheel = input->wheel;
  mouse->delta = Vec2Sub(mouse->pos, mouse->prev_pos);

  CamExUpdate(xinput);
  if (CamExModifierHeld(input->keys))
    mouse->held = 0;
  mouse->press = (mouse->prev_held ^ mouse->held) & mouse->held;
  mouse->release = (mouse->prev_held ^ mouse->held) & mouse->prev_held;
  mouse->prev_held = mouse->held;

  object_mouse = ExtProcessObjects(ctxt);
  if (!ctxt->options.enable_pick || !cur_zone || !cur_zone->items[0]) {
    ctxt->state.drag_active = 0;
    ctxt->state.snap_valid = 0;
    return;
  }

  /* now process mouse */
  ExtRefreshTextureBlocks(ctxt, cur_zone, 0);
  ExtBuildRuntimeCandidates(ctxt, 0);
  press = xinput->mouse.press;
  if (object_mouse) 
    xinput->mouse.press = 0;
  ExtProcessMouse(ctxt);
  xinput->mouse.press = press;
}

static void ExtJsonWrite(ext_context_t *ctxt) {
  FILE *f;
  ext_mpoly_t *mp;
  uint32_t rgn, ci;
  int i, wrote;

  f = fopen(ctxt->json_path, "w");
  if (!f) { 
    ExtStatus(ctxt,
      "failed to write %.180s", ctxt->json_path); 
    return; 
  }
  /* write the root object fields;
     (this line is skipped by the reader) */
  fprintf(f, "{\n  \"version\": 2,\n  \"polys\": [\n");
  wrote = 0;
  for (i=0;i<ctxt->mpoly_count;i++) {
    mp = &ctxt->mpolys[i];
    if (!mp->active) continue;
    /* get texture region and color info */
    memcpy(&rgn, &mp->tinfo.rgninfo, sizeof(rgninfo));
    memcpy(&ci, &mp->tinfo.colinfo, sizeof(colinfo));
    /* add newline after non-root lines */
    if (wrote) 
      fprintf(f, ",\n");
    /* simply write all fields */
    fprintf(f,
      "    {\"world_eid\":%u,"
      "\"tx\":%d,\"ty\":%d,\"tz\":%d,"
      "\"v\":[[%d,%d,%d],[%d,%d,%d],[%d,%d,%d]],"
      "\"uv\":[[%g,%g],[%g,%g],[%g,%g]],"
      "\"c\":[[%u,%u,%u,%u],[%u,%u,%u,%u],[%u,%u,%u,%u]],"
      "\"texid\":%d,\"flags\":%d,\"l\":[%u,%u,%u],"
      "\"ht\":%d,\"tp\":%u,\"rgn\":%u,\"ci\":%u}",
      mp->world_eid, mp->trans_x, mp->trans_y, mp->trans_z,
      mp->verts[0].x, mp->verts[0].y, mp->verts[0].z,
      mp->verts[1].x, mp->verts[1].y, mp->verts[1].z,
      mp->verts[2].x, mp->verts[2].y, mp->verts[2].z,
      mp->uvs[0].x, mp->uvs[0].y, mp->uvs[1].x, mp->uvs[1].y, mp->uvs[2].x, mp->uvs[2].y,
      mp->colors[0].r, mp->colors[0].g, mp->colors[0].b, mp->colors[0].a,
      mp->colors[1].r, mp->colors[1].g, mp->colors[1].b, mp->colors[1].a,
      mp->colors[2].r, mp->colors[2].g, mp->colors[2].b, mp->colors[2].a,
      mp->texid, mp->flags,
      mp->vertex_locked[0], mp->vertex_locked[1], mp->vertex_locked[2],
      mp->has_tinfo, (unsigned)mp->tinfo.tpage, rgn, ci);
    wrote++;
  }
  /* close the polys array and the root object */
  fprintf(f, "\n  ]\n}\n");
  fclose(f);
  /* update status */
  ExtStatus(ctxt,
    "saved %d polygons to %.160s", wrote, ctxt->json_path);
}

static void ExtJsonRead(ext_context_t *ctxt) {
  FILE *f;
  ext_mpoly_t mp;
  char line[2048], fmt[1024];
  uint32_t tp, rgn, ci;
  int loaded, ht, count;

  /* world_eid, trans, vert locs, vert uvs, vert colors, texid, flags, l (vertex_locked) */
  static const char *pfx = " {\"world_eid\":%u,"
    "\"tx\":%d,\"ty\":%d,\"tz\":%d,"
    "\"v\":[[%d,%d,%d],[%d,%d,%d],[%d,%d,%d]],"
    "\"uv\":[[%f,%f],[%f,%f],[%f,%f]],"
    "\"c\":[[%u,%u,%u,%u],[%u,%u,%u,%u],[%u,%u,%u,%u]],"
    "\"texid\":%d,\"flags\":%d,"
    "\"l\":[%hhu,%hhu,%hhu]";
  
  f = fopen(ctxt->json_path, "r");
  if (!f) { 
    ExtStatus(ctxt, "failed to read %.180s", ctxt->json_path);
    return; 
  }
  ctxt->mpoly_count = 0;
  if (ctxt->state.pick_valid && ctxt->state.pick.manual)
    ctxt->state.pick_valid = 0;
  ctxt->state.sel_vert = -1;
  ctxt->state.drag_active = 0;
  ctxt->state.snap_valid = 0;
  ctxt->state.inherit_target = -1;
  ctxt->state.inherit_color_target = -1;
  loaded = 0; 
  /* ensure all texture blocks are loaded */
  ExtRefreshTextureBlocks(ctxt, cur_zone, 1);
  while (fgets(line, sizeof(line), f)) {
    memset(&mp, 0, sizeof(mp));
    snprintf(fmt, sizeof(fmt), "%s,\"ht\":%%d,\"tp\":%%u,\"rgn\":%%u,\"ci\":%%u}", pfx);
    /* polys with texinfo have ht==1 and specify the texinfo fields in tp, rgn, ci
       so try parsing a js object with these fields first */
    count = sscanf(line, fmt,
      &mp.world_eid, &mp.trans_x, &mp.trans_y, &mp.trans_z,
      &mp.verts[0].x, &mp.verts[0].y, &mp.verts[0].z,
      &mp.verts[1].x, &mp.verts[1].y, &mp.verts[1].z,
      &mp.verts[2].x, &mp.verts[2].y, &mp.verts[2].z,
      &mp.uvs[0].x, &mp.uvs[0].y, &mp.uvs[1].x, &mp.uvs[1].y, &mp.uvs[2].x, &mp.uvs[2].y,
      &mp.colors[0].r, &mp.colors[0].g, &mp.colors[0].b, &mp.colors[0].a,
      &mp.colors[1].r, &mp.colors[1].g, &mp.colors[1].b, &mp.colors[1].a,
      &mp.colors[2].r, &mp.colors[2].g, &mp.colors[2].b, &mp.colors[2].a,
      &mp.texid, &mp.flags, &mp.vertex_locked[0], &mp.vertex_locked[1], &mp.vertex_locked[2],
      &ht, &tp, &rgn, &ci);
    if (count == 40) {
      /* copy parsed texinfo data into the texinfo fields */
      mp.has_tinfo = ht;
      mp.tinfo.tpage = tp;
      memcpy(&mp.tinfo.rgninfo, &rgn, sizeof(rgninfo));
      memcpy(&mp.tinfo.colinfo, &ci, sizeof(colinfo));
      count = 36;
    } 
    else {
      /* just parse the basic poly fields without texinfo */
      snprintf(fmt, sizeof(fmt), "%s}", pfx);
      count = sscanf(line, fmt,
      &mp.world_eid, &mp.trans_x, &mp.trans_y, &mp.trans_z,
      &mp.verts[0].x, &mp.verts[0].y, &mp.verts[0].z,
      &mp.verts[1].x, &mp.verts[1].y, &mp.verts[1].z,
      &mp.verts[2].x, &mp.verts[2].y, &mp.verts[2].z,
      &mp.uvs[0].x, &mp.uvs[0].y, &mp.uvs[1].x, &mp.uvs[1].y, &mp.uvs[2].x, &mp.uvs[2].y,
      &mp.colors[0].r, &mp.colors[0].g, &mp.colors[0].b, &mp.colors[0].a,
      &mp.colors[1].r, &mp.colors[1].g, &mp.colors[1].b, &mp.colors[1].a,
      &mp.colors[2].r, &mp.colors[2].g, &mp.colors[2].b, &mp.colors[2].a,
      &mp.texid, &mp.flags, &mp.vertex_locked[0], &mp.vertex_locked[1], &mp.vertex_locked[2]);
    }
    if (count == 36 && ctxt->mpoly_count < EXT_MAX_MANUAL_POLYS) {
      /* append the new/manual poly */
      mp.active = 1;
      ctxt->mpolys[ctxt->mpoly_count++] = mp;
      loaded++;
    }
  }
  fclose(f);
  ctxt->stats_zone_dirty = 1;
  ctxt->stats_level_dirty = 1;
  ExtRefreshCandidates(ctxt);
  ExtStatus(ctxt,
    "loaded %d polygons from %.120s", loaded, ctxt->json_path);
}

/**
 * Append triangle primitives for manual polygons 
 * to the list used by the game rendering pipeline.
 * 
 * (This way manual polygons are included in the rendering process 
 * alongside regular geometry.)
 */
void ExtAppendTris(ext_context_t *ctxt, poly3i *tris, int *tri_count) {
  ext_mpoly_t *mp;
  poly3i *dst;
  int i;

  if (!ctxt->options.enable_pick)
    return;
  if (!cur_zone || !cur_zone->items[0]) 
    return;
  for (i=0;i<ctxt->mpoly_count;i++) {
    mp = &ctxt->mpolys[i];
    if (!mp->active) continue;
    if (!ExtResolveManualTex(mp)) continue;
    dst = &tris[*tri_count];
    if (GfxPolyToPrim(mp, dst)) 
      (*tri_count)++;
  }
}

static void GLDrawBox2d(double x, double y, double r) {
  glBegin(GL_LINE_LOOP);
  glVertex2d(x - r, y - r); 
  glVertex2d(x + r, y - r); 
  glVertex2d(x + r, y + r); 
  glVertex2d(x - r, y + r);
  glEnd();
}

static void GLDrawObjectBound(gool_object *obj, const ext_viewparams_t *params,
                              unsigned char alpha, unsigned char line) {
  gool_bound bound;
  dvec2 polys[GFX_BOUND_POLY_VERT_MAX];
  dline2_t edges[GFX_BOUND_EDGE_COUNT];
  int vert_counts[GFX_BOUND_FACE_COUNT];
  int poly_count, edge_count;
  int i, j, pi;

  if (!obj || obj->handle.type != 1 
   || !GoolObjectBound2(obj, &bound)) 
    return;
  if (!GfxBoundGeomClippedProject(&bound.bound, params, 
                                  polys, &poly_count, vert_counts, 
                                  edges, &edge_count))
    return;
  pi = 0;
  for (i=0;i<poly_count;i++) {
    glColor4ub(0, 0, 0, alpha);
    glBegin(GL_TRIANGLE_FAN);
    for (j=0;j<vert_counts[i];j++,pi++)
      glVertex2d(polys[pi].x / params->zoom, -polys[pi].y / params->zoom);
    glEnd();
  }
  glColor4ub(line, line, line, 255);
  glBegin(GL_LINES);
  for (i=0;i<edge_count;i++) {
    for (j=0;j<2;j++)
      glVertex2d(edges[i][j].x / params->zoom, -edges[i][j].y / params->zoom);
  }
  glEnd();
}

void GLDrawObjectHighlights(ext_context_t *ctxt) {
  ext_viewparams_t params;
  gool_object *obj_sel;

  obj_sel = ctxt->state.obj_sel;
  if (!obj_sel) 
    return;
  params = ExtViewParams(ctxt);
  if (params.hdim.w <= 0 || params.hdim.h <= 0 || params.zoom <= 0) 
    return;
  glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glLineWidth(1.5f);
  GLDrawObjectBound(obj_sel, &params, 25, 100);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopAttrib();
}

static void GLDrawFrameClips(dvec2 a, dvec2 b, double z) {
  dvec2 d, p;
  double ts[4];
  double t;
  int i, n;

  n = 0;
  d = Vec2Sub(b, a);
  if (abs(d.x) > 1e-12) { 
    ts[n++] = (-1.0 - a.x) / d.x; 
    ts[n++] = (1.0 - a.x) / d.x; 
  }
  if (abs(d.y) > 1e-12) { 
    ts[n++] = (-1.0 - a.y) / d.y; 
    ts[n++] = (1.0 - a.y) / d.y; 
  }
  for (i=0;i<n;i++) {
    t = ts[i];
    if (t < 0.0 || t > 1.0) 
      continue;
    p = Vec2Add(a, Vec2MulS(d, t));
    if (p.x < -1.0001 || p.x > 1.0001 
     || p.y < -1.0001 || p.y > 1.0001) 
      continue;
    GLDrawBox2d(p.x / z, -p.y / z, 0.012);
  }
}

static void GLDrawViewportOverlay(ext_context_t *ctxt) {
  double f, g;
  double zoom, mult;

  zoom = ExtViewZoom();
  mult = (ext_config.aspect_mode == 1) ? (10.0 / 7.0) / (4.0 / 3.0)
       : (ext_config.aspect_mode == 2) ? (16.0 / 9.0) / (4.0 / 3.0) : 1.0;
  f = 1.0 / zoom;
  /* dim the area outside the 1x zoom viewport */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (zoom > 1.001) {
    glColor4ub(0, 0, 0, 110);
    glBegin(GL_QUADS);
    glVertex2d(-1, -1); glVertex2d(-f, -1); glVertex2d(-f, 1); glVertex2d(-1, 1);
    glVertex2d(f, -1); glVertex2d(1, -1); glVertex2d(1, 1); glVertex2d(f, 1);
    glVertex2d(-f, -1); glVertex2d(f, -1); glVertex2d(f, -f); glVertex2d(-f, -f);
    glVertex2d(-f, f); glVertex2d(f, f); glVertex2d(f, 1); glVertex2d(-f, 1);
    glEnd();
  }
  glDisable(GL_BLEND);
  /* outline the screen edge */
  glLineWidth(2.0f);
  glColor3ub(0, 255, 120);
  GLDrawBox2d(0, 0, 1);
  /* outline the 1x zoom viewport area */
  glBegin(GL_LINE_LOOP);
  glVertex2d(-1.0 / zoom, -1.0 / zoom); 
  glVertex2d(1.0 / zoom, -1.0 / zoom); 
  glVertex2d(1.0 / zoom, 1.0 / zoom); 
  glVertex2d(-1.0 / zoom, 1.0 / zoom);
  glEnd();
  /* outline the 4:3 aspect ratio [at 1x zoom] area */
  if (mult > 1.001) {
    g = f / mult;
    glColor3ub(120, 120, 255);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2d(-g, -f); glVertex2d(g, -f); glVertex2d(g, f); glVertex2d(-g, f);
    glEnd();
  }
  glLineWidth(1.0f);
}

static void GLDrawCandidateEdges(ext_context_t *ctxt) {
  ext_cand_t *cand;
  dvec2 *a, *b;
  double zoom;
  int idx, i;

  zoom = ExtViewZoom();
  for (i=0;i<ctxt->candidate_count;i++) {
    cand = &ctxt->candidates[i];
    idx = cand->ref.edge_idx;
    a = &cand->proj[idx];
    b = &cand->proj[(idx + 1) % 3];
    glColor3ub(255, 150, 30);
    glBegin(GL_LINES);
    glVertex2d(a->x / zoom, -a->y / zoom);
    glVertex2d(b->x / zoom, -b->y / zoom);
    glEnd();
    glColor3ub(255, 0, 255);
    GLDrawFrameClips(*a, *b, zoom);
  }
}

static void GLDrawHoverItems(ext_context_t *ctxt) {
  double zoom;
  dvec2 *p;
  int i;
  
  zoom = ExtViewZoom();
  if (ctxt->state.hover_tri_valid) {
    /* dim the area of the hovered polygon
       with color dependent on hover kind */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (ctxt->state.hover_kind) 
      glColor4ub(60, 200, 255, 70); 
    else 
      glColor4ub(255, 230, 0, 110);
    glBegin(GL_TRIANGLES);
    for (i=0;i<3;i++) {
      p = &ctxt->state.hover_tri[i];
      glVertex2d(p->x / zoom, -p->y / zoom);
    }
    glEnd();
    glDisable(GL_BLEND);
    /* draw a thicker outline over the hovered polygon */
    if (ctxt->state.hover_kind) 
      glColor3ub(60, 200, 255); 
    else 
      glColor3ub(255, 230, 0);
    glLineWidth(ctxt->state.hover_kind ? 2.0f : 3.0f);
    glBegin(GL_LINE_LOOP);
    for (i=0;i<3;i++) {
      p = &ctxt->state.hover_tri[i];
      glVertex2d(p->x / zoom, -p->y / zoom);
    }
    glEnd();
  }
  /* draw a box around the hovered vertex if any */
  p = &ctxt->state.hover_vtx;
  if (ctxt->state.hover_vtx_valid) {
    glColor3ub(255, 230, 0);
    glLineWidth(3.0f);
    GLDrawBox2d(p->x / zoom, -p->y / zoom, 0.03);
  }
  /* draw a box around the relevant snap point if any */
  p = &ctxt->state.snap_pt;
  if (ctxt->state.snap_valid) {
    glColor3ub(0, 255, 0);
    glLineWidth(2.0f);
    GLDrawBox2d(p->x / zoom, -p->y / zoom, 0.02);
  }
}

static void GLDrawFrameOverlay(ext_context_t *ctxt) {
  GLDrawViewportOverlay(ctxt);
  GLDrawCandidateEdges(ctxt);
  GLDrawHoverItems(ctxt);
}

static void GLDrawTriOutline(tri_t tri) {
  int i;

  glLineWidth(2.0f);
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  for (i=0;i<3;i++) 
    glVertex3d(tri[i].x, tri[i].y, (double)-tri[i].z);
  glEnd();
  /* also draw the selected edge (from v0 to v1) 
     as a seperate color */
  glLineWidth(3.0f);
  glColor3ub(255, 64, 64);
  glBegin(GL_LINES);
  glVertex3d(tri[0].x, tri[0].y, (double)-tri[0].z);
  glVertex3d(tri[1].x, tri[1].y, (double)-tri[1].z);
  glEnd();
}

static void GLDrawTriVerts(tri_t tri, int sel, const uint8_t *locked) {
  int i;

  glPointSize(8.0f);
  glBegin(GL_POINTS);
  for (i=0;i<3;i++) {
    if (i == sel) 
      glColor3ub(0, 255, 0);
    else if (locked && locked[i]) 
      glColor3ub(160, 160, 160);
    else
      glColor3ub(255, 200, 0);
    glVertex3d(tri[i].x, tri[i].y, (double)-tri[i].z);
  }
  glEnd();
}

static void GLDrawTriNormal(tri_t tri) {
  dvec c, u, w, n, tip;
  double len;
  const double scale = 320.0;

  /* get centroid */
  c = VecDivS(VecAdd(VecAdd(VecCast(dvec, tri[0]), 
                            VecCast(dvec, tri[1])), 
                            VecCast(dvec, tri[2])), 3);
  /* compute normal */
  u = VecSub(VecCast(dvec, tri[1]), VecCast(dvec, tri[0]));
  w = VecSub(VecCast(dvec, tri[2]), VecCast(dvec, tri[0]));
  n = VecCross(u, w);
  len = VecMag(n);
  if (len <= 1e-4)
    return;
  n = VecMulS(VecDivS(n, len), scale);
  /* get tip of the normal */
  tip = VecAdd(c, n);
  /* draw line from centroid to tip */
  glLineWidth(2.0f);
  glColor3ub(80, 220, 255);
  glBegin(GL_LINES);
  glVertex3d(c.x, c.y, (double)-c.z);
  glVertex3d(tip.x, tip.y, (double)-tip.z);
  glEnd();
}

static void GLDrawTriOverlay(const ext_context_t *ctxt, tri_t tri, int sel, const uint8_t locked[3]) {
  GLDrawTriOutline(tri);
  if (ctxt->options.show_vertices)
    GLDrawTriVerts(tri, sel, locked);
  if (ctxt->options.show_normals)
    GLDrawTriNormal(tri);
}

/**
 * Draw the main extension overlay, including:
 * - Selected polygon outline, vertices, and normal
 * - Viewport bounds
 * - Hovered polygons
 * - Hovered vertices and snap points [boxes]
 * - Frame clips [boxes]
 * - Camera paths and axes
 * - Candidate edges 
 */
void GLDrawExtOverlay(ext_context_t *ctxt) {
  GLboolean depth;
  ext_viewparams_t params;
  ext_polyref_t *ref;
  ext_poly_t poly;
  zone_world *world;
  dtri2_t ndc;
  vec tri[3];
  double zoom;
  const uint8_t *locked;
  int i, valid;

  if (!ctxt->options.enable_pick)
    return;
  if (!ctxt->options.show_overlay)
    return;
  if (!cur_zone || !cur_zone->items[0]) 
    return;
  
  depth = glIsEnabled(GL_DEPTH_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  
  valid = 0;
  ref = ExtSelectedRef(ctxt);
  world = ExtActivePoly(ctxt, &poly) ? ZoneGetWorld(cur_zone, poly.world_eid) : 0;
  if (world) {
    valid = 1;
    for (i=0;i<3;i++) {
      GfxTransformLocalToView(&poly.verts[i], &world->trans, &tri[i]);
    }
  } 
  else
    valid = 0;

  if (valid) {
    locked = 0;
    if (ref && ref->manual)
      locked = ctxt->mpolys[ref->poly_idx].vertex_locked;
    GLDrawTriOverlay(ctxt, tri, ctxt->state.sel_vert, locked);
  }
    
  /* 2D pass in GL NDC */
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  GLDrawFrameOverlay(ctxt);
  
  zoom = ExtViewZoom();
  if (valid && zoom > 1.001) {
    /* mark where the selected polygon's edges are clipped by the frustum */
    params = ExtViewParams(ctxt);
    for (i=0;i<3;i++) {
      if (!GfxProjectViewToNDC(&tri[i], &params.hdim, &ndc[i])) {
        valid = 0;
        break;
      }
    }
    if (valid) {
      glLineWidth(2.0f);
      glColor3ub(255, 0, 255);
      for (i=0;i<3;i++) 
        GLDrawFrameClips(ndc[i], ndc[(i + 1) % 3], zoom);
    }
  }
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  if (depth) 
    glEnable(GL_DEPTH_TEST);
  glColor3ub(255, 255, 255);

  GLDrawCamOverlay();
  GLDrawObjectHighlights(ctxt);
}

/**
 * Get the currently selected polygon and its reference.
 */
int ExtGetSelectedPoly(ext_context_t *ctxt, ext_polyref_t *ref, ext_poly_t *poly) {
  ext_viewparams_t params;
  const ext_polyref_t *sel;

  sel = ExtSelectedRef(ctxt);
  if (!cur_zone || !cur_zone->items[0] || !sel) 
    return 0;
  params = ExtViewParams(ctxt);
  if (!GfxPolyFromRef(sel, &params, 0, poly)) 
    return 0;
  *ref = *sel;
  return 1;
}

/**
 * Get the center point of the currently selected polygon.
 */
int ExtGetSelectionCenter(ext_context_t *ctxt, dvec *pt_out) {
  ext_poly_t poly;
  int i;

  if (!ctxt->options.enable_pick)
    return 0;
  if (!cur_zone || !cur_zone->items[0]) 
    return 0;
  if (!ExtActivePoly(ctxt, &poly)) 
    return 0;
  pt_out->x = poly.trans_x;
  pt_out->y = poly.trans_y;
  pt_out->z = poly.trans_z;
  for (i=0;i<3;i++)
    pt_out->c[i] += ((double)poly.verts[0].c[i] + poly.verts[1].c[i] + poly.verts[2].c[i]) / 3.0;
  return 1;
}

/**
 * Select the specified polygon.
 */
int ExtSelectPoly(ext_context_t *ctxt, const ext_polyref_t *ref) {
  if (!ref)
    return 0;
  ctxt->state.pick = *ref;
  ctxt->state.sel_edge = ctxt->options.enable_auto_edge ? ctxt->state.pick.edge_idx : 0;
  ctxt->state.pick_valid = 1;
  ctxt->state.sel_vert = -1;
  ExtSelectedPolyStatus(ctxt);
  return 1;
}

/**
 * Extrude the currently selected polygon, creating new polygons.
 * The last new polygon becomes the current selection.
 */
int ExtExtrudeSelected(ext_context_t *ctxt) {
  ext_poly_t poly, res[2];
  ext_polyref_t source;
  char world_name[128], source_name[128];
  int i, n, first;

  if (!ctxt->options.enable_edit) {
    ExtStatus(ctxt, "Cannot extrude: edit mode disabled");
    return 0;
  }
  if (!ExtActivePoly(ctxt, &poly)) {
    ExtStatus(ctxt, "Cannot extrude: no polygon selected");
    return 0;
  }
  source = *ExtSelectedRef(ctxt);
  ExtPolyName(&source, source_name);
  n = GfxPolyExtrude(&poly, res);
  if (!n) {
    ExtStatus(ctxt, "Cannot extrude %s: degenerate polygon", source_name);
    return 0;
  }
  if (n > EXT_MAX_MANUAL_POLYS - ctxt->mpoly_count) {
    ExtStatus(ctxt, "Cannot extrude %s: polygon limit reached", source_name);
    return 0;
  }
  first = ctxt->mpoly_count;
  for (i=0;i<n;i++) 
    ExtInsertPoly(ctxt, &res[i]);
  /* the last new manual poly becomes the current selection */
  ExtPickManual(ctxt, ctxt->mpoly_count - 1, 0);
  ctxt->stats_zone_dirty = 1;
  ctxt->stats_level_dirty = 1;
  ExtRefreshCandidates(ctxt);
  ExtWorldName(res[0].world_eid, world_name);
  if (n == 1)
    ExtStatus(ctxt, "Created new poly %d (%s) from %s", first, world_name, source_name);
  else
    ExtStatus(ctxt, "Created new polys %d, %d (%s) from %s", first, first+n-1, world_name, source_name);
  return 1;
}

/**
 * Delete the currently selected polygon.
 */
int ExtDeleteSelected(ext_context_t *ctxt) {
  ext_polyref_t *ref;
  char name[128];

  if (!ctxt->options.enable_edit) {
    ExtStatus(ctxt, "Cannot delete: edit mode disabled");
    return 0;
  }
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual) {
    ExtStatus(ctxt, "Cannot delete: select a new polygon");
    return 0;
  }
  ExtPolyName(ref, name);
  ExtDeleteSelectedPoly(ctxt);
  ctxt->state.sel_vert = -1;
  ExtStatus(ctxt, "Deleted %s", name);
  return 1;
}

/**
 * Get the index of the currently selected vertex. 
 * Returns -1 if no vertex is selected.
 */
int ExtGetSelectedVertex(ext_context_t *ctxt) {
  return ctxt->state.sel_vert;
}

/**
 * Select the specified vertex by index. 
 * If the index is invalid, no vertex is selected.
 */
void ExtSelectVertex(ext_context_t *ctxt, int idx) {
  ext_polyref_t *ref;
  char name[128];

  ctxt->state.sel_vert = (idx >= 0 && idx < 3) ? idx : -1;
  ref = ExtSelectedRef(ctxt);
  if (ref && ctxt->state.sel_vert >= 0) {
    ExtVertName(ref, ctxt->state.sel_vert, name);
    ExtStatus(ctxt, "Selected %s", name);
  }
}

/**
 * Get the position of the specified vertex of the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtGetVertexPos(ext_context_t *ctxt, int idx, vec *pos_out) {
  ext_mpoly_t *mp; 
  ext_polyref_t *ref;
  ext_viewparams_t params;
  vec verts[3];
  rgba cols[3];

  if (idx < 0 || idx >= 3 || !cur_zone || !cur_zone->items[0])
    return 0;
  ref = ExtSelectedRef(ctxt);
  if (!ref)
    return 0;
  if (ref->manual) {
    mp = &ctxt->mpolys[ref->poly_idx];
    *pos_out = mp->verts[idx];
  }
  else {
    params = ExtViewParams(ctxt);
    if (!GfxPolyRefVerts(ref, &params, verts, cols))
      return 0;
    *pos_out = verts[idx];
  }
  return 1;
}

/**
 * Set the position of the specified vertex of the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtSetVertexPos(ext_context_t *ctxt, int idx, const vec *pos) {
  ext_mpoly_t *mp; 
  ext_polyref_t *ref;
  char name[128];

  if (!ctxt->options.enable_edit) 
    return 0;
  if (idx < 0 || idx >= 3)
    return 0;
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual)
    return 0;
  mp = &ctxt->mpolys[ref->poly_idx];
  if (mp->vertex_locked[idx]) 
    return 0;
  if (!ExtVertCanMove(ctxt, ref->poly_idx, idx, *pos))
    return 0;
  ExtVertMove(ctxt, ref->poly_idx, idx, *pos);
  ExtVertName(ref, idx, name);
  ExtStatus(ctxt, "Moved %s to (%d,%d,%d)",
            name, pos->x, pos->y, pos->z);
  return 1;
}

/**
 * Get the color of the specified vertex of the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtGetVertexColor(ext_context_t *ctxt, int idx, rgba *col_out) {
  ext_mpoly_t *mp; 
  ext_polyref_t *ref;
  ext_viewparams_t params;
  vec verts[3];
  rgba cols[3];

  if (idx < 0 || idx >= 3 
   || !cur_zone || !cur_zone->items[0])
    return 0;
  ref = ExtSelectedRef(ctxt);
  if (!ref)
    return 0;
  if (ref->manual) {
    mp = &ctxt->mpolys[ref->poly_idx];
    *col_out = mp->colors[idx];
  }
  else {
    params = ExtViewParams(ctxt);
    if (!GfxPolyRefVerts(ref, &params, verts, cols))
      return 0;
    *col_out = cols[idx];
  }
  return 1;
}

/**
 * Set the color of the specified vertex of the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtSetVertexColor(ext_context_t *ctxt, int idx, const rgba *col) {
  ext_mpoly_t *mp; 
  ext_polyref_t *ref;
  char name[128];

  if (!ctxt->options.enable_edit)
    return 0;
  if (idx < 0 || idx >= 3)
    return 0;
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual)
    return 0;
  mp = &ctxt->mpolys[ref->poly_idx];
  mp->colors[idx] = *col;
  ExtVertName(ref, idx, name);
  ExtStatus(ctxt, "Changed %s color", name);
  return 1;
}

/**
 * Get the index of the currently selected edge.
 * Returns -1 if no edge is selected.
 */
int ExtGetSelectedEdge(ext_context_t *ctxt) {
  return ctxt->state.sel_edge;
}

/**
 * Select the specified edge of the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtSelectEdge(ext_context_t *ctxt, int idx) {
  ext_polyref_t *ref;
  char name[128];

  if (!ctxt->options.enable_edit) 
    return 0;
  idx = (idx+3)%3;
  ref = ExtSelectedRef(ctxt);
  if (!ref)
    return 0;
  ctxt->state.sel_edge = idx;
  ExtEdgeName(ref, idx, name);
  ExtStatus(ctxt, "Selected %s", name);
  return 1;
}

/**
 * Get the texture information for the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtGetSelectedTexture(ext_context_t *ctxt, texinfo *tex) {
  const ext_polyref_t *ref;
  const ext_mpoly_t *mp;
  zone_world *world;
  wgeo_header *wh;
  wgeo_polygon *poly;

  ref = ExtSelectedRef(ctxt);
  if (!ref || !cur_zone || !cur_zone->items[0]) 
    return 0;
  if (ref->manual) {
    mp = &ctxt->mpolys[ref->poly_idx];
    if (!mp->has_tinfo) return 0;
    *tex = mp->tinfo;
    return 1;
  }
  world = ZoneGetWorld(cur_zone, ref->world_eid);
  if (!world || !world->header || !world->polygons)
    return 0;
  wh = world->header;
  if (ref->poly_idx < 0 || ref->poly_idx >= (int)wh->poly_count) 
    return 0;
  poly = &world->polygons[ref->poly_idx];
  return GfxWorldPolyTexinfo(world, poly, tex);
}

/**
 * Get the texture information for the specified texture index.
 * Returns a pointer to the texture info, or 0 if the index is invalid.
 */
const texinfo *ExtGetTexture(const ext_context_t *ctxt, int idx) {
  if (idx < 0 || idx >= ctxt->texblock_count)
    return 0;
  return &ctxt->texblocks[idx];
}

/**
 * Get the index of the specified texture info in the texture blocks.
 * Returns -1 if the texture info is not found.
 */
int ExtGetTextureIndex(const ext_context_t *ctxt, const texinfo *tex) {
  int i;

  for (i=0;i<ctxt->texblock_count;i++)
    if (TextureBlockEq(&ctxt->texblocks[i], tex)) 
      return i;
  return -1;
}

/**
 * Get the number of texture blocks.
 */
int ExtGetTextureCount(const ext_context_t *ctxt) {
  return ctxt->texblock_count;
}

/**
 * Apply the specified texture to the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtApplyTexture(ext_context_t *ctxt, int idx) {
  ext_mpoly_t *mp; 
  ext_polyref_t *ref;
  char name[128];
  
  if (!ctxt->options.enable_edit)
    return 0;
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual)
    return 0;
  if (idx < 0 || idx >= ctxt->texblock_count)
    return 0;
  mp = &ctxt->mpolys[ref->poly_idx];
  mp->tinfo = ctxt->texblocks[idx];
  mp->has_tinfo = 1;
  ExtPolyName(ref, name);
  ExtStatus(ctxt, "Applied texture #%d to %s", idx, name);
  return 1;
}

/**
 * Set the flip/winding of the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtSetFlipWinding(ext_context_t *ctxt, int flip_winding) {
  ext_mpoly_t *mp; 
  ext_polyref_t *ref;
  char name[128];

  if (!ctxt->options.enable_edit)
    return 0;
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual)
    return 0;
  mp = &ctxt->mpolys[ref->poly_idx];
  if (!mp->has_tinfo)
    return 0;
  flip_winding = limit(flip_winding, 0, 23);
  mp->tinfo.uv_idx = UvSetFlipWinding(mp->tinfo.uv_idx, flip_winding);
  ExtPolyName(ref, name);
  ExtStatus(ctxt, "Changed flip/winding to %d on %s", flip_winding, name);
  return 1;
}

/**
 * Set the flip and winding parts of the currently selected polygon.
 * Returns 1 on success, 0 on failure.
 */
int ExtSetWindingParts(ext_context_t *ctxt, int xflip, int yflip, int winding) {
  ext_mpoly_t *mp; 
  ext_polyref_t *ref;
  int uv_idx;
  char name[128];

  if (!ctxt->options.enable_edit)
    return 0;
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual)
    return 0;
  mp = &ctxt->mpolys[ref->poly_idx];
  if (!mp->has_tinfo)
    return 0;
  uv_idx = mp->tinfo.uv_idx;
  uv_idx = UvSetFlip(uv_idx, xflip, yflip);
  uv_idx = UvSetWinding(uv_idx, winding);
  mp->tinfo.uv_idx = uv_idx;
  ExtPolyName(ref, name);
  ExtStatus(ctxt, "Changed texture on %s: flip (%d,%d), winding %d",
            name, xflip, yflip, winding);
  return 1;
}

/**
 * Get the currently selected object.
 * Returns a pointer to the object, or 0 if no object is selected.
 */
gool_object *ExtGetSelectedObject(const ext_context_t *ctxt) {
  return ctxt->state.obj_sel;
}

/**
 * Select the specified object.
 * If obj is 0, clears the selection.
 */
void ExtSelectObject(ext_context_t *ctxt, gool_object *obj) {
  char name[64];

  ctxt->state.obj_sel = obj;
  ctxt->state.obj_drag = 0;
  if (obj) {
    GoolObjectName(obj, name);
    ExtStatus(ctxt, "Selected %s", name);
  }
  else
    ExtStatus(ctxt, "Cleared object selection");
}

/**
 * Get the candidate polygon at the specified index.
 * Returns a pointer to the candidate, or 0 if the index is invalid.
 */
const ext_polyref_t *ExtGetCandidate(const ext_context_t *ctxt, int idx) {
  return idx >= 0 && idx < ctxt->candidate_count ? &ctxt->candidates[idx].ref : 0;
}

/**
 * Get the number of candidate polygons.
 * Returns the count as an integer.
 */
int ExtGetCandidateCount(const ext_context_t *ctxt) {
  return ctxt->candidate_count;
}

/**
 * Get the index of the currently selected candidate polygon.
 * Returns -1 if no candidate is selected.
 */
int ExtGetSelectedCandidate(ext_context_t *ctxt) {
  return ExtSelectedCandidate(ctxt);
}

/**
 * Select the candidate polygon at the specified index.
 * Returns 1 on success, 0 on failure.
 */
int ExtSelectCandidate(ext_context_t *ctxt, int idx) {
  if (idx < 0 || idx >= ctxt->candidate_count) 
    return 0;
  ctxt->state.pick = ctxt->candidates[idx].ref;
  ctxt->state.sel_edge = ctxt->options.enable_auto_edge ? ctxt->state.pick.edge_idx : 0;
  ctxt->state.pick_valid = 1;
  ctxt->state.sel_vert = -1;
  ExtSelectedPolyStatus(ctxt);
  return 1;
}

/**
 * Get the current state for extended functionality.
 * Returns a pointer to the state structure.
 */
const ext_state_t *ExtGetState(const ext_context_t *ctxt) {
  return &ctxt->state;
}

/**
 * Get the current status message.
 * Returns a pointer to the status string.
 */
const char *ExtGetStatus(ext_context_t *ctxt) {
  if (ctxt->status[0] &&
      GetTicksMonotonic() - ctxt->status_time >= EXT_STATUS_DURATION_TICKS)
    ExtStatus(ctxt, "%s", "");
  return ctxt->status;
}

/**
 * Toggle texture inheritance mode for the currently selected polygon.
 * Returns 1 if the mode was successfully toggled, 0 otherwise.
 */
int ExtInheritTexToggle(ext_context_t *ctxt) {
  ext_polyref_t *ref;
  char name[128];

  if (!ctxt->options.enable_edit) 
    return 0;
  if (ctxt->state.inherit_target >= 0) { 
    ctxt->state.inherit_target = -1; 
    ExtStatus(ctxt, "Cancelled texture inheritance");
    return 1; 
  }
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual) 
    return 0;
  ctxt->state.inherit_target = ref->poly_idx;
  ctxt->state.inherit_color_target = -1;
  ExtPolyName(ref, name);
  ExtStatus(ctxt, "Select a source poly to inherit texture for %s", name);
  return 1;
}

/**
 * Toggle vertex color inheritance mode for the currently selected vertex.
 * Returns 1 if the mode was successfully toggled, 0 otherwise.
 */
int ExtInheritColorToggle(ext_context_t *ctxt) {
  ext_polyref_t *ref;
  char name[128];

  if (!ctxt->options.enable_edit) 
    return 0;
  if (ctxt->state.inherit_color_target >= 0) { 
    ctxt->state.inherit_color_target = -1; 
    ExtStatus(ctxt, "Cancelled vertex color inheritance");
    return 1; 
  }
  ref = ExtSelectedRef(ctxt);
  if (!ref || !ref->manual) 
    return 0;
  if (ctxt->state.sel_vert < 0 || ctxt->state.sel_vert >= 3) {
    ExtStatus(ctxt, "Select a vertex to inherit color");
    return 0;
  }
  ctxt->state.inherit_color_target = ref->poly_idx;
  ctxt->state.inherit_color_vertex = ctxt->state.sel_vert;
  ctxt->state.inherit_target = -1;
  ExtVertName(ref, ctxt->state.sel_vert, name);
  ExtStatus(ctxt, "Select a source vertex to inherit color for %s",
            name);
  return 1;
}

/**
 * Load manual polygon data from a JSON file.
 * Only works if editing is enabled.
 */
void ExtLoadJson(ext_context_t *ctxt) { 
  if (ctxt->options.enable_edit) 
    ExtJsonRead(ctxt); 
}

/**
 * Save manual polygon data to a JSON file.
 * Only works if editing is enabled.
 */
void ExtSaveJson(ext_context_t *ctxt) { 
  ExtJsonWrite(ctxt); 
}

void ExtRecomputeStats(ext_context_t *ctxt, int level) {
  if (level) { 
    ctxt->stats_level_dirty = 1; 
    ExtRefreshZoneAndLevelStats(ctxt, 1); 
  }
  else { 
    ctxt->stats_zone_dirty = 1; 
    ExtRefreshZoneAndLevelStats(ctxt, 0); 
  }
}

int ExtZoneStatsValid(const ext_context_t *ctxt) {
  return ctxt->stats.zone_samples > 0 
      && ctxt->stats_zone == cur_zone
      && !ctxt->stats_zone_dirty;
}

const ext_stats_t *ExtGetStats(const ext_context_t *ctxt) {
  return &ctxt->stats;
}
