#include "ext/tex.h"

/**
 * Force-load the specified texture for the given texinfo
 * and return the UV coordinates of the loaded texture block.
 */
int TextureBlockLoad(const texinfo *tinfo, fbound *ext) {
  fvec uvs[4];
  texinfo t;
  int res;

  /* force cache the unflipped variant of the block
     with the default 0123 winding */
  t = *tinfo;
  t.uv_idx %= 25;
  res = TextureLoad(&t, &uvs);
  if (res < 0)
    return res;
  /* just get upper-left and lower-right */
  ext->p1 = uvs[0];
  ext->p2 = uvs[3];
  return res;
}

/**
 * Determine whether two texinfos refer to the same texture block of 
 * their atlas.
 * 
 * (Ignores flip and winding since those target a triangular region 
 * of the block.)
 */
int TextureBlockEq(const texinfo *a, const texinfo *b) {
  return a->tpage == b->tpage
      && a->segment == b->segment
      && a->offs_x == b->offs_x
      && a->offs_y == b->offs_y
      && a->color_mode == b->color_mode
      && a->uv_idx % 25 == b->uv_idx % 25;
}

/**
 * Copy the specified texture block into the provided pixel buffer,
 * optionally scaling it to the given width and height.
 */
int TextureBlockCopy(texinfo *info, tpage *page, int width, int height, uint8_t *pixels_out) {
  rect2 rect;
  pnt2 clut;
  uint32_t pixels[64*64];
  uint32_t *s, *ts, *d;
  double sx, sy, stepx, stepy;
  int x, y;

  if (!page) {
    page = TexturePage((entry_ref*)&info->tpage);
    if (!page) 
      return 0;
  }
  TextureBlock(info, &rect, &clut);
  TextureCopy((uint8_t*)page, (uint8_t*)pixels, 0, &rect.dim, &rect, 0,
    &clut, 1, info->color_mode, info->semi_trans);
  if (width == 0 || height == 0) {
    memcpy(pixels_out, pixels, sizeof(pixels));
    return 1;
  }
  ts = pixels;
  d = (uint32_t*)pixels_out;
  stepx = rect.w / (double)width;
  stepy = rect.h / (double)height;
  sy = 0;
  for (y=0;y<height;y++) {
    s = ts;
    sx = 0;
    for (x=0;x<width;x++) {
      *(d++) = *s;
      s += (int)(sx+stepx) - (int)sx;
      sx += stepx;
    }
    ts += ((int)(sy+stepy) - (int)sy) * rect.w;
    sy += stepy;
  }
  return 1;
}

int Uv(int w, int h, int xflip, int yflip, int winding) {
  int uv_idx;
  int lw, lh;

  for (lw=0;w>4;w>>=1,lw++);
  for (lh=0;h>4;h>>=1,lh++);
  uv_idx = (lh*5 + lw) + 25*(winding + 6*(2*yflip + xflip));
  return uv_idx;
}

void UvGetDimensions(int uv_idx, int *w, int *h) {
  int lw, lh;

  lw = uv_idx % 5;
  lh = (uv_idx / 5) % 5;
  *w = 4 << lw;
  *h = 4 << lh;
}

void UvGetFlipWinding(int uv_idx, int *flip_winding) {
  *flip_winding = uv_idx / 25;
}

void UvGetFlip(int uv_idx, int *xflip, int *yflip) {
  int flip;

  flip = (uv_idx / 150);
  *xflip = flip % 2;
  *yflip = (flip / 2) % 2;
}

void UvGetWinding(int uv_idx, int *winding) {
  *winding = (uv_idx / 25) % 6;
}

int UvSetDimensions(int uv_idx, int w, int h) {
  int lw, lh;

  for (lw=0;w>4;w>>=1,lw++);
  for (lh=0;h>4;h>>=1,lh++);
  uv_idx = (lh*5 + lw) + 25*(uv_idx / 25);
  return uv_idx;
}

int UvSetFlipWinding(int uv_idx, int flip_winding) {
  return (uv_idx % 25) + 25 * flip_winding;
}

int UvSetFlip(int uv_idx, int xflip, int yflip) {
  int winding;
  winding = uv_idx / 25 % 6;
  uv_idx = (uv_idx % 25) + 25*(winding + 6*(2*yflip + xflip));
  return uv_idx;
}

int UvSetWinding(int uv_idx, int winding) {
  int flip;
  
  flip = (uv_idx / 150);
  uv_idx = (uv_idx % 25) + 25*(winding + 6*flip);
  return uv_idx;
}

int UvPermuteWinding(int uv_idx, int s0, int s1, int s2) {
  int winding;

  UvGetWinding(uv_idx, &winding);
  winding = (2*s0 + (s1-s0+3)%3 - 1);
  return UvSetWinding(uv_idx, winding);
}
