#ifndef _EXT_LEVEL_H_
#define _EXT_LEVEL_H_

#include "common.h"
#include "ext/ext.h"
#include "ext/gfx.h"

eid_t WorldEID(const zone_world *world);
void WorldName(entry *zone, const zone_world *world, char *name_out);

zone_world *ZoneGetWorld(entry *zone, eid_t world_eid);
int ZonePathIdx(entry *zone, zone_path *path);
int ZoneNearestVert(entry *zone, const ext_viewparams_t *params, const ext_vertref_t *skip,
                    const vec2 *mouse, double radius, ext_vertref_t *vert_out, dvec2 *ndc_out);
int ZonePolyAtPoint(entry *zone, const ext_viewparams_t *params,
                       const dvec2 *ndc, ext_polyref_t *poly_out, dtri2_t tri_out);
int ZoneDeprojectPoint(entry *zone, const ddim2 *hdim, const dvec2 *ndc,
                       vec *view_out, zone_world **world_out);
int ZoneGetBoundaryEdges(entry *zone, const ext_viewparams_t *params, 
                            ext_edgeref_t **edges_out);
void ZoneBuildCoverageGrid(entry *zone, const ext_viewparams_t *params, 
                              const mat16 *cam, const ddim2 *hdim, cv_grid_t *grid);
int ZoneFilterVoidEdges(entry *zone, const ext_viewparams_t *params, 
                           const ext_edgeref_t *edges, int edge_count,
                           ext_edgeref_t **edges_out);
int ZonePathPointMatrices(entry *zone, mat16 *mats_out, int max_mats);
int ZoneTextureBlocks(entry *zone, texinfo *blocks_out);

#endif /* _EXT_LEVEL_H_ */
