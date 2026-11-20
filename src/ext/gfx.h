#ifndef _EXT_GFX_H_
#define _EXT_GFX_H_

#include "common.h"
#include "ext/ext.h"

#define MIN_Z 10.0

#define CV_N 256
#define CV_RANGE 1.25

typedef struct {
  uint8_t map[CV_N * CV_N];
} cv_grid_t;

#define GfxMatDet(m) ( (m)[0][0]*((m)[1][1]*(m)[2][2]-(m)[1][2]*(m)[2][1]) \
                     - (m)[0][1]*((m)[1][0]*(m)[2][2]-(m)[1][2]*(m)[2][0]) \
                     + (m)[0][2]*((m)[1][0]*(m)[2][1]-(m)[1][1]*(m)[2][0]) )
int GfxMatSolve(double m[3][3], const dvec *b, dvec *result_out);

/* conversion between fixed point and floating point */
void GfxVecToDVec(dvec *out, const vec *in);
void GfxDVecToVec(vec *out, const dvec *in);
void GfxAngToDAng(dang *out, const ang *in);
void GfxDAngToAng(ang *out, const dang *in);
void GfxMatrix16ToDMatrix(dmat *out, const mat16 *in);
void GfxDMatrixToMatrix16(mat16 *out, const dmat *in);
/* composite conversion between ang and dmat */
void GfxDMatrixFromAng(dmat *m_rot, const ang *rot);
void GfxAngFromDMatrix(ang *rot, dmat *m_rot);

/* floating point variants of vector and matrix operations 
   from soft.c */
int GfxRotD(dvec *in, dvec *out, dmat *m_rot);
int GfxRotTransD(dvec *in, dvec *out, dvec *trans, dmat *m_rot);
void GfxTransposeDMatrix(dmat *m);
void GfxMulDMatrix(dmat *m_l, dmat *m_r);
void GfxRotDMatrixYXY(dmat *m_rot, dang *rot);
void GfxRotDMatrixZXY(dmat *m_rot, dang *rot);
void GfxRotDMatrixAxisAngle(dmat *m_rot, dvec *axis, double angle);

/* coordinate transformations */
vec GfxRoundVec(const dvec *pos);
int GfxTransformLocalToView(const vec *local, const vec *trans, vec *view_out);
void GfxTransformLocalToCam(const vec *local, const mat16 *cam, const vec *trans, vec *view_out);
int GfxTransformWorldToView(const vec *world, vec *view_out);
void GfxTransformWorldToViewD(const dvec *world, dvec *view_out);
int GfxTransformViewToWorld(const vec *view, vec *world_out);
int GfxTransformViewToWorldD(const dvec *view, dvec *world_out);
int GfxProjectViewToNDC(const vec *view, const ddim2 *hdim, dvec2 *ndc_out);
int GfxProjectViewToNDCD(const dvec *view, const ddim2 *hdim, dvec2 *ndc_out);
int GfxDeprojectNDCToView(const dvec2 *ndc, const ext_viewparams_t *params, double z, dvec *view_out);
int GfxDeprojectNDCToTri(const tri_t tri, const dvec2 *ndc, const ddim2 *hdim,
                         dvec *view_out, double *depth_out);
int GfxDeprojectNDCToBound(const bound *bound, const dvec2 *ndc, const ddim2 *hdim,
                           vec *view_out, double *depth_out);
int GfxDeprojectNDCToLocal(const dvec2 *ndc, const ext_viewparams_t *params,
                           const vec *trans, const vec *base,
                           vec *local_out);
int GfxTransformPxToNDC(const vec2 *px, const ext_viewparams_t *params, dvec2 *ndc_out);
void GfxTransformNDCToDPx(const dvec2 *ndc, const ext_viewparams_t *params, dvec2 *px_out);

#define GFX_BOUND_FACE_COUNT 12
#define GFX_BOUND_EDGE_COUNT 12
#define GFX_BOUND_POLY_VERT_MAX (GFX_BOUND_FACE_COUNT * 4)

/* bounding box geometry */
void GfxBoundGeom(const bound *box, tri_t tris_out[GFX_BOUND_FACE_COUNT],
                  line_t edges_out[GFX_BOUND_EDGE_COUNT]);
int GfxBoundGeomClipped(const bound *box, dvec verts_out[GFX_BOUND_POLY_VERT_MAX],
                        int *poly_count_out, int vert_counts_out[GFX_BOUND_FACE_COUNT],
                        dline_t edges_out[GFX_BOUND_EDGE_COUNT], int *edge_count_out);
int GfxBoundGeomClippedProject(const bound *box, const ext_viewparams_t *params,
                               dvec2 verts_out[GFX_BOUND_POLY_VERT_MAX], int *poly_count_out,
                               int vert_counts_out[GFX_BOUND_FACE_COUNT],
                               dline2_t edges_out[GFX_BOUND_EDGE_COUNT], int *edge_count_out);

/* edges */
int GfxEdgeNearClip(const vec *a, const vec *b, dvec *verts_out);

/* triangles */
double GfxTriArea(tri_t tri);
int GfxTriNearClip(const tri_t tri, dvec *verts_out);
int GfxTri2dClip(const dtri2_t tri, dtri2_t tris_out[5]);
int GfxPointInTri2d(const dtri2_t tri, const dvec2 *ndc);
int GfxPointInNearClippedTri(const tri_t tri, const dvec2 *ndc, const ddim2 *hdim,
                             double *depth_out, dtri2_t tri_out);
int GfxPointInPoly2d(const dvec2 *verts, const dvec2 *ndc, int count);
int GfxTriEdgeExtrusionVector(tri_t tri, int idx, dvec *offset_out);
int GfxTri2dNearestVertex(const dtri2_t tri, const dvec2 *ndc, const ext_viewparams_t *params);
int GfxTri2dOffscreen(const dtri2_t ndc);
int GfxTri2dEdgeOffscreen(const dtri2_t ndc, int idx);

/* worlds/polygons */
rgba GfxRgb8ToA32(uint8_t r, uint8_t g, uint8_t b);
void GfxWorldVertUnpack(const wgeo_vertex *vert, vec *local_out, rgba *col_out);
int GfxWorldVertTransform(const zone_world *world, const wgeo_vertex *vert, const mat16 *cam,
                          const vec *trans, vec *view_out);
int GfxWorldVertProject(const zone_world *world, const wgeo_vertex *vert, const ddim2 *hdim,
                        const mat16 *cam, const vec *trans, dvec2 *ndc_out);
int GfxWorldPolyTexinfo(const zone_world *world, const wgeo_polygon *poly, texinfo *tinfo_out);
void GfxWorldPolyLoad(const zone_world *world, const wgeo_polygon *poly,
                      vec verts_out[3], fvec uvs_out[3], rgba cols_out[3],
                      texinfo *tinfo_out, int *is_textured_out, int *texid_out, int *flags_out);
int GfxWorldPolyTransform(const zone_world *world, const wgeo_polygon *poly, const mat16 *cam,
                          const vec *trans, int permute, vec verts_out[3]);
int GfxWorldPolyProject(const zone_world *world, const wgeo_polygon *poly, const ddim2 *hdim,
                        const mat16 *cam, const vec *trans, int permute, dtri2_t tri_out);
int GfxWorldPointProject(const zone_world *world, const vec *local, const ddim2 *hdim,
                         vec *view_out, dvec2 *ndc_out);

/* reference-based polygon and vertex utilities */
int GfxPolyFromRef(const ext_polyref_t *ref, 
                   const ext_viewparams_t *params,
                   int rotate, ext_poly_t *poly_out);
int GfxVertFromRef(const ext_vertref_t *ref, 
                   const ext_viewparams_t *params,
                   vec *pos_out, rgba *col_out);

void GfxEdgeRefVerts(const zone_world *world, const ext_edgeref_t *e, 
                     const ext_mpoly_t *mpolys, vec verts_out[3]);
int GfxEdgeRefProject(const zone_world *world, const ext_edgeref_t *e, 
                      const ext_viewparams_t *params, dtri2_t tri_out);

int GfxPolyRefVerts(const ext_polyref_t *ref, 
                    const ext_viewparams_t *params,
                    vec verts_out[3], rgba cols_out[3]);
int GfxPolyRefTexinfo(const ext_polyref_t *ref, 
                      const ext_viewparams_t *params, 
                      texinfo *tinfo_out);
int GfxPolyRefTransform(const ext_polyref_t *ref, 
                        const ext_viewparams_t *params,
                        const mat16 *cam, const vec *trans, vec verts_out[3]);
int GfxPolyRefProject(const ext_polyref_t *ref, 
                      const ext_viewparams_t *params,
                      const mat16 *cam, const vec *trans, dtri2_t tri_out);
int GfxPolyNearestVert(const ext_polyref_t *ref, const vec2 *px, const ext_viewparams_t *params);

/* polygon manipulation */
void GfxPolyPermute(const ext_poly_t *poly, int v0, int v1, int v2, ext_poly_t *poly_out);
int GfxPolyExtrude(const ext_poly_t *src, ext_poly_t polys_out[2]);
int GfxPolyToPrim(ext_poly_t *poly, poly3i *tri_out);

/* coverage grid */
void GfxCvFillTri(cv_grid_t *grid, const dtri2_t tri);
void GfxCvFillWorldPoly(cv_grid_t *grid, const zone_world *world,
                        const wgeo_polygon *poly, const ddim2 *hdim, const mat16 *cam, const vec *trans);
void GfxCvFillManualPoly(cv_grid_t *grid, const zone_world *world,
                         const ext_mpoly_t *mp, const ddim2 *hdim, const mat16 *cam);
int GfxCvPointCovered(const cv_grid_t *grid, const dvec2 *ndc);
double GfxCvGetCoverage(const cv_grid_t *grid);

int GfxTriEdgeVoidAmt(const cv_grid_t *grid, const dtri2_t tri, int idx, int flags, int *total_out);
int GfxTriBestVoidEdge(const cv_grid_t *grid, const dtri2_t tri);

#endif /* _EXT_GFX_H_ */
