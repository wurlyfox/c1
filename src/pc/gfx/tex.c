/*
 * texture cache, for paletted texture support
 */
#include "tex.h"
#include "ns.h"
#include "gfx.h"

extern quad28_t uv_map[600];

uint32_t pixel_5551_8888(uint16_t p, int mode) {
  int r, g, b, a;

  r = (p >> 0) & 0x1F;
  g = (p >> 5) & 0x1F;
  b = (p >> 10) & 0x1F;
  a = (1-(p>>15));

  r = ((r*510)+31)/62;
  g = ((g*510)+31)/62;
  b = ((b*510)+31)/62;

  switch (mode) {
  case 0: a = a == 1 ? 0x7F : (r|g|b) == 0 ? 0 : 0xFF; break;
  case 1: a = a == 1 ? 0xFF : (r|g|b) == 0 ? 0 : 0xFF; break;
  case 2: a = a == 1 ? 0xFF : (r|g|b) == 0 ? 0 : 0xFF; break;
  case 3: a = a == 1 ? 0xFF : (r|g|b) == 0 ? 0 : 0xFF; break;
  }
  return (a << 24) | (b << 16) | (g << 8) | r;
}

typedef struct _tex_cache_entry {
  struct _tex_cache_entry *next;
  texinfo texinfo;
  int texid;
  fvec uvs[4];
  rect2 rect;
} tex_cache_entry;

typedef struct {
  uint8_t *data;
  rect2 rect;
  int invalid;
  int tpage_id;
  int texid;
} tex_atlas;

#define TEX_TPAGE_LOCAL_COUNT  8
#define TEX_TPAGE_GLOBAL_COUNT 8
#define TEX_TPAGE_COUNT        TEX_TPAGE_LOCAL_COUNT+TEX_TPAGE_GLOBAL_COUNT

typedef struct {
  tex_cache_entry *table[0x40000];
  int entry_count;
  tex_cache_entry entries[0x10000];
  int tpage_count;
  tpage *tpages[TEX_TPAGE_COUNT];
  tex_atlas atlases[TEX_TPAGE_COUNT*6];
  uint8_t data[TEX_TPAGE_COUNT*((1024+512+256)*128)*2*sizeof(uint32_t)];
  dim2 dim;  /* dim of the atlas group texture */
  int texid; /* hw texture id for the atlas group */
  tex_create_callback_t create;
  tex_delete_callback_t delete;
  tex_subimage_callback_t subimage;
  int global_count;
  eid_t global_eids[TEX_TPAGE_GLOBAL_COUNT];
  tpage *global_tpages[TEX_TPAGE_GLOBAL_COUNT];
} tex_cache;

tex_cache cache;

extern page_struct texture_pages[16];

void TexturesInit(
  tex_create_callback_t create,
  tex_delete_callback_t delete,
  tex_subimage_callback_t subimage) {
  tex_atlas *atlas;
  rect2 rect;
  uint8_t *data;
  int idx, i, j;

  cache.create = create;
  cache.delete = delete;
  cache.subimage = subimage;
  memset(cache.table, sizeof(cache.table), 0);
  // AvlTreeInit(&cache.tree);
  cache.entry_count = 0;
  cache.tpage_count = 0;
  cache.dim.w = 2048; cache.dim.h = 2048;
  data = cache.data;
  for (i=0;i<TEX_TPAGE_COUNT;i++) {
    for (j=0;j<6;j++) {
      idx = (i*6)+j;
      atlas = &cache.atlases[idx];
      rect.w = 1 << (10 - (j%3));
      rect.h = 128;
      //rect.x = 2048 - rect.w*2;
      //rect.y = rect.h*((i*2)+(j/3));
      atlas->data = data;
      atlas->rect = rect;
      atlas->invalid = 0;
      atlas->tpage_id = -1; /* not associated with a tpage yet...*/
      data += 1 << (19 - (j%3));
      atlas->texid = (*cache.create)(atlas->rect.dim, 0);
    }
  }
  // cache.texid = (*cache.create)(cache.dim, 0);
  cache.global_count = 0;
}

void TexturesKill() {
  tex_atlas *atlas;
  int i;

  (*cache.delete)(cache.texid);
}

void TexturesUpdate() {
  tex_atlas *atlas;
  rect2 rect;
  int i;

  for (i=0;i<TEX_TPAGE_COUNT*6;i++) {
    atlas = &cache.atlases[i];
    if (atlas->invalid) {
      rect.x=0;rect.y=0;rect.dim=atlas->rect.dim;
      (*cache.subimage)(atlas->texid, rect, atlas->data);
      atlas->invalid = 0;
    }
  }
}

/**
 * copies a rect of pixels inside a texture into a 32-bit color texture
 *
 * src        - source texture pixel buffer
 * sdim       - dimensions of source texture, or 0 for default
 * srect      - subrect of pixels to copy, or 0 for entire texture
 * dst        - destination texture pixel buffer
 * ddim       - dimensions of destination texture, or 0 if same as source texture
 * dloc       - location in destination at which to copy pixels, or 0 for (0,0)
 * clut       - generic; see below
 * clut_type  - see below
 * color_mode - color mode of the subrect of pixels in the source texture
 * semi_trans - semi-trans mode of the subrect of pixels in the source texture
 *
 * clut is generic and can be one of the following based on clut_type:
 *
 *   clut_type = 0 => no clut; clut = 0
 *   clut_type = 1 => clut is a pnt2* with the 2 dimensional location
 *                    of the clut in the source texture
 *   clut_type = 2 => clut is a direct pointer to clut data
 */
void TextureCopy(uint8_t *src, uint8_t *dst, dim2 *sdim, dim2 *ddim,
  rect2 *srect, pnt2 *dloc,
  void *clut, int clut_type, int color_mode, int semi_trans) {
  uint32_t *data, palette[256], *dst32;
  uint16_t *clut_data, *src16;
  pnt2 *clut_loc;
  int i, idx, si, di, x, y;
  dim2 _dim;
  rect2 _rect;
  pnt2 _pnt;

  src16 = (uint16_t*)src;
  dst32 = (uint32_t*)dst;
  if (!sdim) { /* use the defaults if no source dimensions specified */
    sdim = &_dim;
    sdim->w = 1024 >> color_mode;
    sdim->h = 128;
  }
  if (!ddim) { ddim = sdim; }
  if (!srect) {
    srect = &_rect;
    srect->x = 0; srect->y = 0;
    srect->dim = *sdim;
  }
  if (!dloc) {
    dloc = &_pnt;
    dloc->x = 0; dloc->y = 0;
  }
  if (clut_type == 0) {
    clut = 0;
  }
  else if (clut_type == 1) {
    clut_loc = (pnt2*)clut;
    idx = (clut_loc->x*16)+(clut_loc->y*(sdim->w>>(2-color_mode)));
    clut_data = &src16[idx];
  }
  else if (clut_type == 2) {
    clut_data = (uint16_t*)clut;
  }
  switch (color_mode) {
  case 0:
    for (i=0;i<16;i++)
      palette[i] = pixel_5551_8888(clut_data[i], semi_trans);
    for (y=0;y<srect->h;y++) {
      for (x=0;x<srect->w;x++) {
        si = (srect->x+x)+(srect->y+y)*sdim->w;
        di = (dloc->x+x)+(dloc->y+y)*ddim->w;
        if (si%2) { idx = (src[si/2] >> 4) & 0xF; }
        else      { idx = (src[si/2] >> 0) & 0xF; }
        dst32[di] = palette[idx];
      }
    }
    break;
  case 1:
    for (i=0;i<256;i++)
      palette[i] = pixel_5551_8888(clut_data[i], semi_trans);
    for (y=0;y<srect->h;y++) {
      for (x=0;x<srect->w;x++) {
        si = (srect->x+x)+((srect->y+y)*sdim->w);
        di = (dloc->x+x)+((dloc->y+y)*ddim->w);
        dst32[di] = palette[src[si]];
      }
    }
    break;
  case 2:
    for (y=0;y<srect->h;y++) {
      for (x=0;x<srect->w;x++) {
        si = (srect->x+x)+(srect->y+y)*sdim->w;
        di = (dloc->x+x)+(dloc->y+y)*ddim->w;
        dst32[di] = pixel_5551_8888(src16[si], semi_trans);
      }
    }
    break;
  }
}

/**
 * retrieves the corresponding texture page [struct] index for a tpag eid
 */
static int TexturePageIdx(entry_ref *tpag) {
  eid_t eid;
  int i;

  if (tpag->is_eid) { eid = tpag->eid; }
  else { eid = tpag->en->eid; }
  for (i=0;i<16;i++) {
    if (i<cache.global_count && cache.global_eids[i] == eid) { break; }
    if (texture_pages[i].eid == eid) { break; }
  }
  return i < 16 ? i : -1;
}

/**
 * retrieves the corresponding texture page for a tpag eid
 */
static tpage *TexturePage(entry_ref *tpag) {
  eid_t eid;
  int i, idx;

  eid = tpag->is_eid ? tpag->eid : tpag->en->eid;
  idx = TexturePageIdx(tpag);
  if (idx == -1) { return 0; }
  if (idx < cache.global_count)
    return cache.global_tpages[idx];
  return (tpage*)texture_pages[idx].page;
}

/**
 * retrieves the corresponding texture atlas for a texture page
 */
static tex_atlas *TextureAtlas(tpage *tpage, int color_mode, int semi_trans) {
  tex_atlas *atlas;
  int i, idx;

  for (i=0;i<cache.tpage_count;i++)
    if (cache.tpages[i] == tpage) break;
  if (i == cache.tpage_count && i < TEX_TPAGE_COUNT) {
    cache.tpages[i] = tpage;
    ++cache.tpage_count;
  }
  idx = i*6 + (color_mode + (semi_trans == 0 ? 3 : 0));
  atlas = &cache.atlases[idx];
  if (atlas->tpage_id == -1)
    atlas->tpage_id = TexturePageIdx((entry_ref*)&tpage->eid);
  return atlas;
}

/**
 * copies a rect of pixels inside a texture page into the texture cache
 *
 * returns id of the main atlas group texture
 */
static int TextureData(tpage *tpage, rect2 rect, pnt2 clut,
  int color_mode, int semi_trans) {
  tex_atlas *atlas;
  uint8_t *data;

  atlas = TextureAtlas(tpage, color_mode, semi_trans);
  data = atlas->data;
  TextureCopy((uint8_t*)tpage, data, 0, 0, &rect, &rect.loc,
    &clut, 1, color_mode, semi_trans);
  atlas->invalid = 1;
  return atlas->texid;
  // return cache.texid;
}

/**
* creates a new texture for the given texinfo
* this may require creating a parent atlas for the texture
*
* returns the index of main atlas group texture
* and the uv coordinates of the texture within the main atlas group
* (OLD: returns the index of the parent atlas
*       and the uv coordinates of the texture within the parent atlas)
*/
static int TextureNew(texinfo *texinfo, fvec (*uvs)[4]) {
  tex_cache_entry *entry, *prev;
  tex_atlas *atlas;
  tpage *tpage;
  quad28 quad;
  vec2 offs;
  rect2 rect;
  pnt2 clut;
  uint32_t hash;
  float page_width;
  int i, idx, ppi, fw, texid;

  ppi = (2 << (2 - texinfo->color_mode));
  page_width = (float)(ppi << 7);
  /* // alternative variant; avoids the large blob of precomputed uvs, but slower
  fw = rgninfo.uv_idx / 25;
  rect.x = ((rgninfo.segment * 32) + rgninfo.xoffs) * ppi;
  rect.y = rgninfo.offs * 4;
  rect.w = 4 << (rgninfo.uv_idx % 5);
  rect.h = 4 << ((rgninfo.uv_idx / 5) % 5);
  for (i=0;i<3;i++) {
    uvs[i].x = (rect.x+(rect.w*((pos_masks[(i<<1)+0]>>fw)&1))) / page_width;
    uvs[i].y = (rect.y+(rect.h*((pos_masks[(i<<1)+1]>>fw)&1))) / 128.0;
  }
  */
  quad = *(quad28*)&uv_map[texinfo->uv_idx];
  offs.x = ((texinfo->segment*32)+texinfo->offs_x)*ppi;
  offs.y = texinfo->offs_y*4;
  rect.w = max3(quad.p[0].x,quad.p[1].x,quad.p[2].x)+1;
  rect.h = max3(quad.p[0].y,quad.p[1].y,quad.p[2].y)+1;
  rect.x = offs.x; /*+min4(quad.p[0].x,quad.p[1].x,quad.p[2].x,quad.p[3].x);*/
  rect.y = offs.y; /*+min4(quad.p[0].y,quad.p[1].y,quad.p[2].y,quad.p[3].y);*/
  for (i=0;i<4;i++) {
    (*uvs)[i].x = offs.x+quad.p[i].x;
    (*uvs)[i].y = offs.y+quad.p[i].y;
  }
  clut.x = texinfo->clut_x;
  clut.y = texinfo->clut_y;
  /* tpage = NSLookup(&texinfo->tpage); */
  tpage = TexturePage((entry_ref*)&texinfo->tpage);
  /* change here */
  atlas = TextureAtlas(tpage, texinfo->color_mode, texinfo->semi_trans);
  for (i=0;i<4;i++) {
    //(*uvs)[i].x += atlas->rect.x;
    //(*uvs)[i].y += atlas->rect.y;
    //(*uvs)[i].x /= 2048;
    //(*uvs)[i].y /= 2048;
    (*uvs)[i].y = (*uvs)[i].y;
    (*uvs)[i].x /= atlas->rect.w;
    (*uvs)[i].y /= atlas->rect.h;
  }
  /* ----------- */
  texid = TextureData(tpage, rect, clut,
    texinfo->color_mode, texinfo->semi_trans);
  entry = &cache.entries[cache.entry_count++];
  entry->texinfo = *texinfo;
  entry->texid = texid;
  for (i=0;i<4;i++)
    entry->uvs[i] = (*uvs)[i];
  entry->rect.x = rect.x;
  entry->rect.y = rect.y;
  entry->rect.w = rect.w;
  entry->rect.h = rect.h;
  entry->next = 0;
  hash = atlas->tpage_id<<14|texinfo->color_mode<<12|texinfo->segment<<10
       |texinfo->offs_x<<5|texinfo->offs_y;
  if (cache.table[hash]) {
    prev = cache.table[hash];
    entry->next = prev;
  }
  cache.table[hash] = entry;
  return entry->texid;
  // return cache.texid;
}

/**
* retrieves the uv coordinates for the texture referenced by the given texinfo
* these coordinates are relative to the main atlas group texture
*
* returns id of the main atlas group
* and the uv coordinates of the texture within the main atlas group
* (OLD: retrieves the index of the atlas
*       which contains the texture referenced by the given texinfo)
*/
static int TextureLookup(texinfo *texinfo, fvec(*uvs)[4]) {
  tex_cache_entry *entry;
  entry_ref *tpag;
  eid_t eid;
  uint32_t hash;
  int i, idx;

  idx = TexturePageIdx((entry_ref*)&texinfo->tpage);
  hash = idx<<14|texinfo->color_mode<<12|texinfo->segment<<10
       |texinfo->offs_x<<5|texinfo->offs_y;
  entry = cache.table[hash];
  while (entry && texinfo->uv_idx != entry->texinfo.uv_idx)
    entry = entry->next;
  if (!entry) { return -1; }
  for (i=0;i<4;i++)
    (*uvs)[i] = entry->uvs[i];
  return entry->texid;
  // return cache.texid;
}

int TextureId() {
  return cache.texid;
}

/**
* creates a new texture for the given texinfo if one does not exist
*
* returns id of the main atlas group
* and the uv coordinates of the texture within the main atlas group
* (OLD: returns the index of the texture's parent atlas)
*/
int TextureLoad(texinfo *texinfo, fvec(*uvs)[4]) {
  int texid;

  texid = TextureLookup(texinfo, uvs);
  if (texid != -1) return texid;
  texid = TextureNew(texinfo, uvs);
  return texid;
}

/**
* loads a global texture page which persists between game states
*
* this page will be chosen over ones which are dynamically loaded/unloaded
* by the game
*/

/*
int TexturePageGlobal(tpage *tpage) {
  int idx;

  idx = cache.global_count++;
  cache.global_eids[idx] = tpage->eid;
  cache.global_tpages[idx] = tpage;
  return idx;
}
*/
