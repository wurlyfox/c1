#include "ext/level.h"
#include "ext/gfx.h"
#include "ext/tex.h"

#include "pc/gfx/soft.h"

extern entry *cur_zone;

eid_t WorldEID(const zone_world *world) {
  if (!world->eid) return (eid_t)0;
  if (world->eid & 1) return world->eid;
  return world->en->eid;
}

void WorldName(entry *zone, const zone_world *world, char *name_out) {
  eid_t world_eid;
  
  world_eid = WorldEID(world);
  sprintf(name_out, "%s", NSEIDToString(world_eid));
  return;
  /*
  zone_header *zh;
  char zn[16];
  int idx;
  zh = (zone_header *)zone->items[0];
  idx = (int)(world - zh->worlds);
  sprintf(zn, "%s", NSEIDToString(zone->eid));
  sprintf(name_out, "%s world %d", zn, idx);
  */
}

/**
 * Get a world from a zone by its corresponding EID.
 */
zone_world *ZoneGetWorld(entry *zone, eid_t world_eid) {
  zone_header *zh;
  zone_world *world;
  eid_t eid;
  int i;

  if (!zone || !zone->items[0]) return 0;
  zh = (zone_header *)zone->items[0];
  for (i=0;i<(int)zh->world_count;i++) {
    world = &zh->worlds[i];
    eid = WorldEID(world);
    if (eid == world_eid) 
      return world;
  }
  return 0;
}

/**
 * Get the index of a path within a zone.
 * Returns -1 if the path is not found.
 */
int ZonePathIdx(entry *zone, zone_path *path) {
  zone_header *header;
  int idx, paths_idx, path_count;

  if (!zone || !zone->items[0] || !path) 
    return -1;
  header = (zone_header*)zone->items[0];
  paths_idx = header->paths_idx;
  path_count = header->path_count;
  for (idx=0;idx<path_count;idx++)
    if (zone->items[paths_idx+idx] == (uint8_t*)path) break;
  if (idx==path_count)
    idx = -1;
  return idx;
}

/**
 * Deproject a point from normalized device coordinates (NDC) 
 * onto the surface of a zone's geometry in view space.
 */
int ZoneDeprojectPoint(entry *zone, const ddim2 *hdim, const dvec2 *ndc,
                       vec *view_out, zone_world **world_out) {
  zone_header *zh;
  zone_world *world, *best_world;
  wgeo_header *wh;
  wgeo_polygon *poly;
  tri_t tri;
  dtri2_t ptri;
  dvec view, best_view, world_point;
  double depth, nearest = 1e300;
  int i, j, found = 0;

  if (!zone || !zone->items[0] || !view_out || hdim->w <= 0 || hdim->h <= 0)
    return 0;
  zh = (zone_header *)zone->items[0];
  for (i=0;i<(int)zh->world_count;i++) {
    world = &zh->worlds[i];
    if (!world->header || !world->vertices || !world->polygons) continue;
    wh = world->header;
    if (wh->is_backdrop) continue;
    for (j=0;j<(int)wh->poly_count;j++) {
      poly = &world->polygons[j];
      /* first check that the projected triangle contains the point */
      if (GfxWorldPolyProject(world, poly, hdim, 0, 0, 0, ptri)
       && !GfxPointInTri2d(ptri, ndc))
        continue;
      /* if so, then deproject the point to the triangle in view space */
      GfxWorldPolyTransform(world, poly, 0, 0, 0, tri);
      if (GfxDeprojectNDCToTri(tri, ndc, hdim, &view, &depth) && depth < nearest) {
        nearest = depth;
        found = 1;
        best_view = view;
        best_world = world;
      }
    }
  }
  if (!found)
    return 0;
  if (!GfxTransformViewToWorldD(&best_view, &world_point)) return 0;
  if (world_point.x < (double)INT32_MIN / 256.0 || world_point.x > (double)INT32_MAX / 256.0
   || world_point.y < (double)INT32_MIN / 256.0 || world_point.y > (double)INT32_MAX / 256.0
   || world_point.z < (double)INT32_MIN / 256.0 || world_point.z > (double)INT32_MAX / 256.0)
    return 0;
  GfxDVecToVec(view_out, &world_point);
  if (world_out)
    *world_out = best_world;
  return 1;
}

/**
 * Find the vertex in a zone that is nearest to a given screen-space position 
 * (i.e. mouse) within a specified radius.
 */
int ZoneNearestVert(entry *zone, const ext_viewparams_t *params, const ext_vertref_t *skip,
                    const vec2 *mouse, double radius, ext_vertref_t *vert_out, dvec2 *ndc_out) {
  const ext_mpoly_t *mp, *skipped;
  zone_header *zh;
  zone_world *world;
  vec local, view;
  rgba color;
  dvec2 mpx, px, ndc;
  double dist, min_dist;
  int i, j, found;

  if (!zone || !zone->items[0]) 
    return 0;
  zh = (zone_header *)zone->items[0];
  skipped = 0;
  if (skip 
   && skip->poly_idx >= 0 && skip->poly_idx < params->mpoly_count 
   && skip->vert_idx >= 0 && skip->vert_idx < 3)
    skipped = &params->mpolys[skip->poly_idx];
  mpx.x = (double)mouse->x;
  mpx.y = (double)mouse->y;
  min_dist = radius * radius;
  found = 0;
  for (i=0;i<(int)zh->world_count;i++) {
    world = &zh->worlds[i];
    if (!world->header || !world->vertices)
      continue;
    if (world->header->is_backdrop) 
      continue;
    for (j=0;j<(int)world->header->vert_count;j++) {
      GfxWorldVertUnpack(&world->vertices[j], &local, &color);
      if (!GfxTransformLocalToView(&local, &world->trans, &view)
       || !GfxProjectViewToNDC(&view, &params->hdim, &ndc)) 
        continue;
      GfxTransformNDCToDPx(&ndc, params, &px);
      dist = Vec2SqrDist(px, mpx);
      if (dist >= min_dist) 
        continue;
      min_dist = dist;
      vert_out->world_idx = i;
      vert_out->poly_idx = -1;
      vert_out->vert_idx = j;
      vert_out->manual = 0;
      *ndc_out = ndc;
      found = 1;
    }
  }
  for (i=0;i<params->mpoly_count;i++) {
    mp = &params->mpolys[i];
    if (!mp->active) 
      continue;
    world = ZoneGetWorld(zone, mp->world_eid);
    if (!world) 
      continue;
    for (j=0;j<3;j++) {
      /* if there is a vertex to skip, check if this is 
         that vertex or any welded duplicate and skip it */
      if (skip) {
        if (i == skip->poly_idx && j == skip->vert_idx)
          continue;
        if (skipped && mp->world_eid == skipped->world_eid 
         && !mp->vertex_locked[j]
         && VecEq(mp->verts[j], skipped->verts[skip->vert_idx])) 
          continue;
      }
      if (!GfxTransformLocalToView(&mp->verts[j], &world->trans, &view)
       || !GfxProjectViewToNDC(&view, &params->hdim, &ndc)) 
        continue;
      GfxTransformNDCToDPx(&ndc, params, &px);
      dist = Vec2SqrDist(px, mpx);
      if (dist >= min_dist) 
        continue;
      min_dist = dist;
      vert_out->world_idx = (int)(world - zh->worlds);
      vert_out->poly_idx = i;
      vert_out->vert_idx = j;
      vert_out->manual = 1;
      *ndc_out = ndc;
      found = 1;
    }
  }
  return found;
}

/**
 * Find the first polygon in a zone that contains a given point in 
 * normalized device coordinates (NDC).
 */
int ZonePolyAtPoint(entry *zone, const ext_viewparams_t *params,
                    const dvec2 *ndc, ext_polyref_t *poly_out, dtri2_t tri_out) {
  const ext_mpoly_t *mp;
  zone_header *zh;
  zone_world *world, *best_world;
  wgeo_header *wh;
  wgeo_polygon *poly;
  dtri2_t tri, best_tri;
  tri_t mv;
  double depth, min_depth;
  int i, j, best_idx;

  if (!zone || !zone->items[0]) return 0;
  /* Manual polygons take precedence over world polygons. */
  for (i=0;i<params->mpoly_count;i++) {
    mp = &params->mpolys[i];
    if (!mp->active) continue;
    world = ZoneGetWorld(zone, mp->world_eid);
    if (!world) continue;
    for (j=0;j<3;j++)
      GfxTransformLocalToView(&mp->verts[j], &world->trans, &mv[j]);
    if (GfxPointInNearClippedTri(mv, ndc, &params->hdim, &depth, tri)) {
      poly_out->manual = 1;
      poly_out->world_eid = mp->world_eid;
      poly_out->poly_idx = i;
      poly_out->edge_idx = 0;
      memcpy(tri_out, tri, sizeof(tri));
      return 1;
    }
  }
  min_depth = 1e30;
  best_world = 0;
  best_idx = -1;
  zh = (zone_header *)zone->items[0];
  for (i=0;i<(int)zh->world_count;i++) {
    world = &zh->worlds[i];
    if (!world->header || !world->polygons || !world->vertices)
      continue;
    wh = world->header;
    if (wh->is_backdrop)
      continue;
    for (j=0;j<(int)wh->poly_count;j++) {
      poly = &world->polygons[j];
      GfxWorldPolyTransform(world, poly, 0, 0, 0, mv);
      if (GfxPointInNearClippedTri(mv, ndc, &params->hdim, &depth, tri) 
       && depth < min_depth) {
        min_depth = depth;
        best_world = world;
        best_idx = j;
        memcpy(best_tri, tri, sizeof(tri));
      }
    }
  }
  if (!best_world) 
    return 0;
  poly_out->manual = 0;
  poly_out->world_eid = WorldEID(best_world);
  poly_out->poly_idx = best_idx;
  poly_out->edge_idx = 0;
  memcpy(tri_out, best_tri, sizeof(best_tri));
  return 1;
}

#define EDGE_MAP_SIZE 131072
#define EDGE_MAP_MASK (EDGE_MAP_SIZE - 1)

typedef struct edge_entry_s {
  uint64_t key;
  ext_edgeref_t ref;
  vec points[3];
  uint8_t ref_count;
  struct edge_entry_s *next;
} edge_entry_t;

typedef struct {
  edge_entry_t *hash[EDGE_MAP_SIZE];
  edge_entry_t entries[EDGE_MAP_SIZE];
  int entry_count;
} edge_table_t;

static inline uint64_t PointHash(const vec *v) {
  uint64_t ux = (uint64_t)(uint16_t)v->x;
  uint64_t uy = (uint64_t)(uint16_t)v->y;
  uint64_t uz = (uint64_t)(uint16_t)v->z;
  return (ux << 32) ^ (uy << 16) ^ uz;
}

static inline uint64_t EdgeHash(const vec v[2]) {
  uint64_t k0, k1, mink, maxk;
  uint64_t hash;

  k0 = PointHash(&v[0]);
  k1 = PointHash(&v[1]);
  mink = min(k0, k1);
  maxk = max(k0, k1);
  hash = (mink * 0x517cc1b727220a95ULL) ^ (maxk * 0x4f1bbcdcbfa540ULL);
  return hash;
}

static void EdgeTableInsert(edge_table_t *table, 
                            int world_idx, int poly_idx, int edge_idx, 
                            int manual, const vec v[3]) {
  edge_entry_t *entry;
  uint64_t key;
  uint32_t slot;

  key = EdgeHash(v);
  slot = (uint32_t)(key & EDGE_MAP_MASK);
  entry = table->hash[slot];
  while (entry) {
    if (entry->key == key) {
      if (entry->ref_count < 255) entry->ref_count++;
      return;
    }
    entry = entry->next;
  }
  if (table->entry_count >= EDGE_MAP_SIZE) return;
  /* create a new entry */
  entry = &table->entries[table->entry_count++];
  entry->key = key;
  entry->ref.world_idx = world_idx;
  entry->ref.poly_idx = poly_idx;
  entry->ref.edge_idx = edge_idx;
  entry->ref.manual = manual;
  memcpy(entry->points, v, sizeof(entry->points));
  entry->ref_count = 1;
  entry->next = table->hash[slot];
  table->hash[slot] = entry;
}

/**
 * Get the boundary edges of a zone's geometry.
 * 
 * A boundary edge is an edge that is not shared by any other polygon.
 */
int ZoneGetBoundaryEdges(entry *zone, const ext_viewparams_t *params, ext_edgeref_t **edges_out) {
  edge_table_t *edge_table;
  edge_entry_t *entry;
  zone_header *zh;
  zone_world *world;
  wgeo_header *wh;
  wgeo_polygon *poly;
  wgeo_vertex *verts;
  const ext_mpoly_t *mp;
  vec v[3], ev[3];
  rgba col[3];
  int i, j, k, edge_count;

  *edges_out = 0;
  if (!zone || !zone->items[0]) 
    return 0;
  zh = (zone_header *)zone->items[0];
  edge_table = calloc(1, sizeof(*edge_table));
  if (!edge_table) return 0;
  for (i=0;i<(int)zh->world_count;i++) {
    world = &zh->worlds[i];
    if (!world->header || !world->polygons || !world->vertices)
      continue;
    wh = world->header;
    if (wh->is_backdrop)
      continue;
    verts = world->vertices;
    /* insert edges into the edge table for each world poly
       each edge stores the permutation of vertices
       with first 2 verts being said edge and last being the opposite vert */
    for (j=0;j<(int)wh->poly_count;j++) {
      poly = &world->polygons[j];
      GfxWorldVertUnpack(&verts[poly->a_idx], &v[0], &col[0]);
      GfxWorldVertUnpack(&verts[poly->b_idx], &v[1], &col[1]);
      GfxWorldVertUnpack(&verts[poly->c_idx], &v[2], &col[2]);
      for (k=0;k<3;k++) {
        ev[0] = v[k];
        ev[1] = v[(k + 1) % 3];
        ev[2] = v[(k + 2) % 3];
        EdgeTableInsert(edge_table, i, j, k, 0, ev);
      }
    }
  }
  /* manual polygons take part like world polys: shared edges with them cancel out */
  if (params) {
    for (j=0;j<params->mpoly_count;j++) {
      mp = &params->mpolys[j];
      world = mp->active ? ZoneGetWorld(zone, mp->world_eid) : 0;
      if (!world) continue;
      i = (int)(world - zh->worlds);
      for (k=0;k<3;k++) {
        ev[0] = mp->verts[k];
        ev[1] = mp->verts[(k + 1) % 3];
        ev[2] = mp->verts[(k + 2) % 3];
        EdgeTableInsert(edge_table, i, j, k, 1, ev);
      }
    }
  }
  edge_count = 0;
  if (edge_table->entry_count > 0)
    *edges_out = malloc(edge_table->entry_count * sizeof(ext_edgeref_t));
  if (*edges_out) {
    for (i=0;i<edge_table->entry_count;i++) {
      entry = &edge_table->entries[i];
      if (entry->ref_count != 1) continue;
      (*edges_out)[edge_count++] = entry->ref;
    }
  }
  free(edge_table);
  return edge_count;
}

/**
 * Build a coverage grid for a zone, marking the areas of the screen
 * that are covered by the zone's geometry and manual polygons.
 */
void ZoneBuildCoverageGrid(entry *zone, const ext_viewparams_t *params, 
                           const mat16 *cam, const ddim2 *hdim, cv_grid_t *grid) {
  const ext_mpoly_t *mp;
  zone_header *zh;
  zone_world *world;
  wgeo_header *header;
  wgeo_polygon *poly;
  mat16 cam0;
  const mat16 *tcam;
  vec trans, zero;
  int i, j;

  memset(grid, 0, sizeof(*grid));
  zero = (vec){{ 0, 0, 0 }};
  if (!cam) zone = cur_zone;
  else {
    cam0 = *cam;
    cam0.t = zero;
  }
  if (!zone || !zone->items[0]) return;
  zh = (zone_header *)zone->items[0];
  for (i=0;i<(int)zh->world_count;i++) {
    world = &zh->worlds[i];
    if (!world->header || !world->polygons || !world->vertices) continue;
    header = world->header;
    if (header->is_backdrop) {
      trans = zero;
      tcam = cam ? &cam0 : 0;
    }
    else {
      trans = (vec){{ header->trans_x, header->trans_y, header->trans_z }};
      tcam = cam;
    }
    for (j=0;j<(int)header->poly_count;j++) {
      poly = &world->polygons[j];
      GfxCvFillWorldPoly(grid, world, poly, hdim, tcam, &trans);
    }
  }
  if (params) {
    for (i=0;i<params->mpoly_count;i++) {
      mp = &params->mpolys[i];
      if (!mp->active) continue;
      /* get the world associated with this manual poly */
      world = ZoneGetWorld(zone, mp->world_eid);
      if (!world) continue;
      /* transform the manual poly vertices to camera space
          and project them to NDC */
      GfxCvFillManualPoly(grid, world, mp, hdim, cam);
    }
  }
}

/**
 * Filter/select edges from the given array of boundary edges for
 * a zone that are considered "void".
 * 
 * [Uses coverage grid to determine if edges are in void areas]
 */
int ZoneFilterVoidEdges(entry *zone, const ext_viewparams_t *params,
                        const ext_edgeref_t *edges, int edge_count,
                        ext_edgeref_t **edges_out) {
  cv_grid_t *grid;
  const ext_edgeref_t *edge;
  zone_header *zh;
  zone_world *world;
  dtri2_t ndc;
  int i, count, empty, total;

  *edges_out = 0;
  if (!zone || !zone->items[0] || edge_count <= 0)
    return 0;
  zh = (zone_header *)zone->items[0];
  grid = malloc(sizeof(cv_grid_t));
  *edges_out = malloc(edge_count * sizeof(**edges_out));
  if (!grid || !*edges_out) {
    free(grid); 
    free(*edges_out);
    *edges_out = 0;
    return 0;
  }
  ZoneBuildCoverageGrid(zone, params, 0, &params->hdim, grid);
  count = 0;
  for (i=0;i<edge_count;i++) {
    edge = &edges[i];
    world = &zh->worlds[edge->world_idx];
    if (!GfxEdgeRefProject(world, edge, params, ndc)) 
      continue;
    if ((ndc[0].y < -1.0 && ndc[1].y < -1.0) || (ndc[0].y > 1.0 && ndc[1].y > 1.0)
     || (ndc[0].x < -1.0 && ndc[1].x < -1.0) || (ndc[0].x > 1.0 && ndc[1].x > 1.0))
      continue;
    empty = GfxTriEdgeVoidAmt(grid, ndc, 0, 0, &total);
    if (total > 0 && empty * 2 >= total) 
      (*edges_out)[count++] = *edge;
  }
  free(grid);
  return count;
}

/**
 * Extract camera rail sample poses from the zone.
 *
 * Generates transformation matrices for points along the
 * camera paths defined in the zone.
 *
 * Returns the number of matrices generated.
 */
int ZonePathPointMatrices(entry *zone, mat16 *mats_out, int max_mats) {
  zone_header *header;
  zone_rect *rect;
  zone_path *path;
  zone_path_point *point;
  mat16 *out_mat;
  ang rot;
  int i, j, count, step;

  if (!zone || !zone->items[0]) 
    return 0;
  header = (zone_header *)zone->items[0];
  rect = (zone_rect *)zone->items[1];
  count = 0;
  for (i=0;i<(int)header->path_count;i++) {
    path = (zone_path *)zone->items[header->paths_idx + i];
    if (path->length == 0) continue;
    step = (path->length > 32) ? 2 : 1;
    for (j=0;j<(int)path->length;j+=step) {
      if (count >= max_mats) return count;
      point = &path->points[j];
      out_mat = &mats_out[count++];
      rot.x = angle12(-point->rot_x);
      rot.y = angle12(-point->rot_y);
      rot.z = angle12(-point->rot_z);
      SwRotMatrixZXY(out_mat, &rot);
      out_mat->m[1][0] = (out_mat->m[1][0] / 8) * -5;
      out_mat->m[1][1] = (out_mat->m[1][1] / 8) * -5;
      out_mat->m[1][2] = (out_mat->m[1][2] / 8) * -5;
      out_mat->m[2][0] = -out_mat->m[2][0];
      out_mat->m[2][1] = -out_mat->m[2][1];
      out_mat->m[2][2] = -out_mat->m[2][2];
      out_mat->t.x = rect->x + point->x;
      out_mat->t.y = rect->y + point->y;
      out_mat->t.z = rect->z + point->z;
    }
  }

  return count;
}

/**
 * Get the set of unique texture block texinfos used by a given zone.
 * 
 * (That is, by polygons of worlds within the zone).
 */
int ZoneTextureBlocks(entry *zone, texinfo *blocks_out) {
  zone_header *zh;
  zone_world *world;
  wgeo_header *wh;
  wgeo_polygon *poly;
  texinfo tinfo;
  int i, j, k, count, has, found;

  if (!zone || !zone->items[0]) 
    return 0; 
  count = 0;
  zh = (zone_header *)zone->items[0];
  for (i=0;i<(int)zh->world_count;i++) {
    world = &zh->worlds[i];
    if (!world->header || !world->polygons || !world->vertices) 
      continue;
    wh = world->header;
    if (wh->is_backdrop) 
      continue;
    for (j=0;j<(int)wh->poly_count;j++) {
      poly = &world->polygons[j];
      has = GfxWorldPolyTexinfo(world, poly, &tinfo);
      if (!has) continue;
      tinfo.uv_idx = tinfo.uv_idx % 25;
      found = 0;
      for (k=0;k<count;k++) {
        if (TextureBlockEq(&blocks_out[k], &tinfo)) { 
          found = 1; 
          break; 
        }
      }
      if (!found && count < EXT_MAX_TEXTURES) 
        blocks_out[count++] = tinfo;
    }
  }
  return count;
}
