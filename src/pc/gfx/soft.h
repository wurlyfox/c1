#ifndef _SOFT_H_
#define _SOFT_H_

#include "common.h"
#include "geom.h"
#include "gool.h"
#include "formats/svtx.h"
#include "formats/cvtx.h"
#include "formats/tgeo.h"
#include "formats/wgeo.h"
#include "formats/slst.h"
#include "formats/zdat.h"

/* this structure is a combination of values
   some are the equivalent of GTE state/register values used in the psx implementation
   the rest are the equivalent of shader parameters passed in scratch memory in
   the psx implementation

   it does not include poly ids, ordering table, or pointer to tail of primitive memory */
typedef struct {
  union {
    zone_world *worlds;
    /* etc... */
  };
  vec2 screen;
  uint32_t screen_proj;
  vec trans;
  mat16 m_rot;
  mat16 m_light;
  mat16 m_color;
  rgb8 far_color;
  rgb8 back_color;
  int32_t far_t1;
  int32_t far_t2;
  rgb8 far_color1;
  rgb8 far_color2;
  int32_t tri_wave[16];
  vec illum;
  uint32_t shamt_add;
  uint32_t shamt_sub;
  uint32_t amb_fx0;
  uint32_t amb_fx1;
} sw_transform_struct;

extern uint32_t SwSqrMagnitude2(int32_t a, int32_t b);
extern uint32_t SwSqrMagnitude3(int32_t a, int32_t b, int32_t c);
extern void SwRot(vec *in, vec *out, mat16 *m_rot);
extern void SwRotTrans(vec *in, vec *out, vec *trans, mat16 *m_rot);
extern void SwRotTransPers(vec *in, vec *out, vec *trans, mat16 *m_rot,
  vec2 *offs, uint32_t proj);
extern void SwTransposeMatrix(mat16 *m);
extern void SwMulMatrix(mat16 *m_l, mat16 *m_r);
extern void SwRotMatrixYXY(mat16 *m_rot, ang *rot);
extern void SwRotMatrixZXY(mat16 *m_rot, ang *rot);
extern void SwScaleMatrix(mat16 *m_scale, vec *v);
extern void SwCalcObjectMatrices(mat16 *m_rot, tgeo_header *header,
  gool_vectors *vectors, gool_colors *colors, sw_transform_struct *params);
extern void SwCalcObjectRotMatrix(mat16 *m_rot, tgeo_header *header,
  gool_vectors *vectors, sw_transform_struct *params);
extern int SwCalcSpriteRotMatrix(gool_vectors *obj_vectors,
  gool_vectors *cam_vectors, int flag, int shrink, mat16 *m_rot,
  int32_t depth, int32_t max_depth, sw_transform_struct *params);
extern void SwTransformSvtx(svtx_frame *frame, void *ot,
  tgeo_polygon *polys, tgeo_header *header, uint32_t cull_mask,
  uint32_t far, void **prims_tail, rect28 *regions, int flag,
  sw_transform_struct *params);
extern void SwTransformCvtx(cvtx_frame *frame, void *ot,
  tgeo_polygon *polys, tgeo_header *header, uint32_t cull_mask,
  uint32_t far, void **prims_tail, rect28 *regions, int shamt,
  sw_transform_struct *params);
extern void SwTransformSprite(void *ot, texinfo2 *info, eid_t tpag,
  int size, uint32_t far, void **prims_tail, rect28 *regions,
  sw_transform_struct *params);
extern void SwProjectBound(bound *lbound, vec *col,
  vec *trans, vec *_cam_trans, bound2 *extents);
extern void SwTransformWorlds(poly_id_list *poly_id_list, void *ot,
  int32_t proj, int anim_phase, void **prims_tail, sw_transform_struct *params);
extern void SwTransformWorldsFog(poly_id_list *poly_id_list, void *ot,
  int32_t proj, int anim_phase, void **prims_tail, sw_transform_struct *params);
extern void SwTransformWorldsRipple(poly_id_list *poly_id_list, void *ot,
  int32_t proj, int anim_phase, void **prims_tail, sw_transform_struct *params);
extern void SwTransformWorldsLightning(poly_id_list *poly_id_list, void *ot,
  int32_t proj, int anim_phase, void **prims_tail, sw_transform_struct *params);
extern void SwTransformWorldsDark(poly_id_list *poly_id_list, void *ot,
  int32_t proj, int anim_phase, void **prims_tail, sw_transform_struct *params);
extern void SwTransformWorldsDark2(poly_id_list *poly_id_list, void *ot,
  int32_t proj, int anim_phase, void **prims_tail, sw_transform_struct *params);

#ifdef CFLAGS_DRAW_OCTREES
#include "level.h"
extern void SwTransformZoneQuery(zone_query *query, void *ot, void **prims_tail);
#endif

#endif /* _SOFT_H_ */
