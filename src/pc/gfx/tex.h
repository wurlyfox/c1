#ifndef _TEX_H_
#define _TEX_H_

#include "common.h"
#include "geom.h"
#include "ns.h"
#include "pcgfx.h"

typedef int (*tex_create_callback_t)(dim2, uint8_t*);
typedef void (*tex_delete_callback_t)(int);
typedef void (*tex_subimage_callback_t)(int, rect2, uint8_t*);

extern void TexturesInit(tex_create_callback_t create,
  tex_delete_callback_t delete, tex_subimage_callback_t subimage,
  int max_texture_size);
extern void TexturesKill();
extern void TexturesUpdate();
extern int TextureId();
extern tpage* TexturePage(entry_ref *tpag);
extern void TextureBlock(texinfo *texinfo, rect2 *rect, pnt2 *clut);
extern int TextureLoad(texinfo *texinfo, fvec(*uvs)[4]);
extern void TextureCopy(uint8_t *src, uint8_t *dst, dim2 *sdim, dim2 *ddim,
  rect2 *srect, pnt2 *dloc,
  void *clut, int clut_type, int color_mode, int semi_trans);
#endif /* _TEX_H_ */
