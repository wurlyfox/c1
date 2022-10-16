#ifndef _GFX_H_
#define _GFX_H_

#include "common.h"
#include "geom.h"
#include "ns.h"
#include "gool.h"

#include "formats/gool.h"
#include "formats/svtx.h"
#include "formats/cvtx.h"
#include "formats/tgeo.h"
#include "formats/zdat.h"

extern int TgeoOnLoad(entry *tgeo);

extern int GfxInitMatrices();
extern void GfxUpdateMatrices();
extern int GfxResetCam(vec *trans);
extern int GfxCalcObjectMatrices(svtx_frame *frame, tgeo_header *t_header,
  gool_object *obj, int flag, int32_t *zdist);
extern void GfxTransformSvtx(svtx_frame *frame, void *ot, gool_object *obj);
extern void GfxTransformCvtx(cvtx_frame *frame, void *ot, gool_object *obj);
#ifdef PSX
extern void GfxTransformFragment(
  gool_frag *frag, int32_t z, tpageinfo pginfo, bound2 *bound, void *ot);
extern void GfxTransformFontChar(gool_object *obj,
  gool_glyph *glyph, int32_t z, tpageinfo pginfo, bound2 *bound, void *ot,
  int gouraud);
#else
extern void GfxTransformFragment(
  gool_frag *frag, int32_t z, eid_t tpag, bound2 *bound, void *ot);
extern void GfxTransformFontChar(gool_object *obj,
  gool_glyph *glyph, int32_t z, eid_t tpag, bound2 *bound, void *ot,
  int gouraud);
#endif
extern void GfxTransform(vec *in, mat16 *mat, vec *out);
extern void GfxLoadWorlds(zone_header *header);
extern void GfxTransformWorlds(void *ot);
extern void GfxTransformWorldsFog(void *ot);
extern void GfxTransformWorldsRipple(void *ot);
extern void GfxTransformWorldsLightning(void *ot);
extern void GfxTransformWorldsDark(void *ot);
extern void GfxTransformWorldsDark2(void *ot);
extern void sub_8001A460(uint32_t flags_a, uint32_t flags_b);
extern int sub_8001A5F4(int type, uint8_t *icon, int idx, rect2 *rect);
extern void sub_8001A754(char *dst, int id, char *src, int len);

#endif /* _GFX_H_ */
