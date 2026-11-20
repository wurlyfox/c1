#ifndef _EXT_TEX_H_
#define _EXT_TEX_H_

#include "common.h"
#include "geom.h"
#include "ns.h"
#include "pc/gfx/pcgfx.h"
#include "pc/gfx/tex.h"

int TextureBlockLoad(const texinfo *tinfo, fbound *ext);
int TextureBlockEq(const texinfo *a, const texinfo *b);
int TextureBlockCopy(texinfo *info, tpage *page, int width, int height, uint8_t *pixels_out);

int Uv(int w, int h, int xflip, int yflip, int winding);
void UvGetDimensions(int uv_idx, int *w, int *h);
void UvGetFlipWinding(int uv_idx, int *flip_winding);
void UvGetFlip(int uv_idx, int *xflip, int *yflip);
void UvGetWinding(int uv_idx, int *winding);
int UvSetDimensions(int uv_idx, int w, int h);
int UvSetFlipWinding(int uv_idx, int flip_winding);
int UvSetFlip(int uv_idx, int xflip, int yflip);
int UvSetWinding(int uv_idx, int winding);
int UvPermuteWinding(int uv_idx, int s0, int s1, int s2);

#endif /* _EXT_TEX_H_ */
