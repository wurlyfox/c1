#include "title.h"
#include "globals.h"
#include "ns.h"
#include "gool.h"
#include "level.h"
#include "formats/mdat.h"
#include "formats/imag.h"

/* .sdata */
int title_once = 1;                            /* 80056500; gp[0x41] */
//char game_over_zone_name[8]      = "0b_pZ";  /* 80056504; gp[0x42] */
//char main_menu_zone_name[8]      = "0c_pZ";  /* 8005650C; gp[0x44] */
//char naughty_dog_zone_name[8]    = "0d_pZ";  /* 80056514; gp[0x46] */
//char options_menu_zone_name[8]   = "0e_pZ";  /* 8005651C; gp[0x48] */
//char unk_map_zone_name[8]        = "0f_pZ";  /* 80056524; gp[0x4A] */
//char island_1a_zone_name[8]      = "1e_pZ";  /* 8005652C; gp[0x4C] */
//char island_1b_zone_name[8]      = "1a_pZ";  /* 80056534; gp[0x4E] */
//char island_2_zone_name[8]       = "2b_pZ";  /* 8005653C; gp[0x50] */
//char island_3_zone_name[8]       = "3a_pZ";  /* 80056544; gp[0x52] */
//char scea_universal_zone_name[8] = "0a_pZ";  /* 8005654C; gp[0x54] */
//char mdat_name[8]                = "%cMapP"; /* 80056554; gp[0x56] */
//char mdat_1_name[8]              = "0MapP";  /* 8005655C; gp[0x58] */
/* .sbss */
int title_not_seen;                            /* 80056678; gp[0x9F] */
title_struct *title;                           /* 800566E0; gp[0xB9] */
#ifndef PSX
uint8_t image[512*256*2];
uint32_t image_cvt[512*240];
uint16_t *cluts[480];
#endif

extern ns_struct ns;
extern page_struct texture_pages[16];
extern int paused;
extern int pause_status;
extern lid_t cur_lid;

#ifdef PSX
#include "psx/gpu.h"
extern gfx_context_db context;
#else
#include "pc/gfx/gl.h"
#include "pc/gfx/tex.h"
extern gl_context context;
#endif

//----- (80031BAC) --------------------------------------------------------
int TitleInit() {
  if (cur_lid != LID_TITLE) { return 0; } /* main menu/map/title level only */
  title = calloc(1, sizeof(title_struct));
  if (title == 0) /* alloc failed? */
    exit(-1); /* abort */
  return 0;
}

//----- (80031BF8) --------------------------------------------------------
int TitleLoadNextState() {
  title_not_seen = title_once;
  title_once = 0;
  if (cur_lid != LID_TITLE) { return 0; }
  next_display_flags = (next_display_flags & ~GOOL_FLAG_DISPANIM_ALL) | GOOL_FLAG_DISPLAY_IMAGES;
  if (!title_not_seen) { /* already saw the scea/universal screens? */
    switch (game_state) {
    case GAME_STATE_GAMEOVER:
      title_state = 12;
      title->next_state = 12;
      title->state = 12;
      title->transition_state = 1;
      break;
    case GAME_STATE_CUTSCENE:
    case GAME_STATE_PLAYING:
    case GAME_STATE_CONTINUE:
    case 0x400:
      title->next_state = title_state;
      title->state = title_state;
      title->transition_state = 1;
      title->unk8 = dword_8006194C;
      title->unk7 = dword_8006194C;
      break;
    case GAME_STATE_TITLE:
      title_state = 5;
      title->next_state = 5;
      title->state = 5;
      title->transition_state = 1;
      break;
    }
  }
  else { /* set scea screen state */
    title_state = 10;
    title->next_state = 10;
    title->state = 10;
    title->transition_state = 0;
  }
  TitleLoadState();
  return 0;
}

/**
*  load a title screen
*
*  a title screen is described by a zone entry;
*  in particular, the first path and path point of the zone give the
*  location of the camera with respect to any gool objects in the zone
*
*  eid - screen zone eid
*  type - screen type
*
*  type 0 - image only, no gool objects and no camera
*           (scea/universal screens)
*  type 1 - includes [backdrop] image, gool objects, and/or camera
*           (map, load game/password/game over screen, options menu)
*  type 3 - includes [backdrop] image and gool objects; no camera
*           (main menu, naughty dog)
*/
//----- (80031D50) --------------------------------------------------------
static void TitleLoadScreen(eid_t *eid, int type) {
  entry *zone;
  zone_header *header;
  zone_path *path;
  int i;

  paused = 0;
  pause_status = -1;
  GoolForEachObjectHandle(
    (gool_ifnptr2_t)GoolObjectHandleTraverseTreePostorder,
    (int)GoolObjectTerminate,
    0);
  if (type == 0) {
    next_display_flags = (next_display_flags & ~GOOL_FLAG_DISPANIM_ALL) | GOOL_FLAG_DISPLAY_IMAGES;
    title->at_title = 1;
  }
  else if (type == 1) {
    next_display_flags = GOOL_FLAG_DISPLAY_WORLDS
                       | GOOL_FLAG_DISPANIM_OBJECTS
                       | GOOL_FLAG_CAM_UPDATE;
    title->at_title = 0;
    for (i=8;i<15;i++) {
      if (texture_pages[i].state == 30)
        texture_pages[i].state = 1;
    }
  }
  else if (type == 2 || type == 3) {
    next_display_flags = GOOL_FLAG_DISPANIM_OBJECTS | GOOL_FLAG_DISPLAY_IMAGES;
    title->at_title = 1;
  }
  zone = NSOpen(eid, 1, 1);
  header = (zone_header*)zone->items[0];
  path = (zone_path*)zone->items[header->paths_idx]; /* first and only path */
  LevelUpdate(zone, path, 0, 2);
  NSClose(eid, 1);
}

//----- (80031EB4) --------------------------------------------------------
int TitleLoadState() {
  entry *mdat;
  mdat_header *header;
  mdat_entity *entity;
  eid_t eid;
  char eid_str[8], eid_char;
  uint32_t fov;
  int i, state, type;

  title_pause_state = 0;
  switch (title->state) { /* get fov for this menu state */
  case 1: case 2: case 3: case 4:
    exit(0);
  case 6: case 15:
    fov = 37;
    break;
  case 5: case 8: case 12: case 13: case 14:
    fov = 55;
    break;
  default:
    fov = 90;
    break;
  }
  ns.ldat->fov = fov;
  GfxInitMatrices();
  switch (title->state) {
  case 5: /* main menu */
    LevelResetGlobals(1);
    eid = NSStringToEID("0c_pZ");
    type = 3;
    break;
  case 6: /* options menu */
    eid = NSStringToEID("0e_pZ");
    type = 1;
    break;
  case 8: /* Naughty Dog */
    eid = NSStringToEID("0d_pZ");
    type = 3;
    break;
  case 12: /* game over screen */
    eid = NSStringToEID("0b_pZ");
    type = 1;
    break;
  case 13: case 14: /* password/load game menu */
    eid = NSStringToEID("0b_pZ");
    type = 1;
    break;
  case 15: /* world map */
    if (cur_map_level == 99
      || cur_map_level < 9) {
      eid = NSStringToEID("1e_pZ");
    }
    else if (cur_map_level == 9) { eid = NSStringToEID("1a_pZ"); }
    else if (cur_map_level < 18) { eid = NSStringToEID("2b_pZ"); }
    else if (cur_map_level < 50) { eid = NSStringToEID("3a_pZ"); }
    type = 1;
    break;
  default: /* Sony Computer Entertainment America & Universal Interactive screens */
    eid = NSStringToEID("0a_pZ");
    type = 0;
    break;
  }
  if (type == 0) /* SCEA or universal screen? */
    TitleLoadScreen(&eid, 0);
  screen_ro.x = 0;
  screen_ro.y = 0;
  switch (title->state) {
  case 1: case 2: case 3: case 4:
    dword_800618B8 = ~state;
    break;
  case 5: case 6: case 8: case 12: case 13: case 14: case 15:
    TitleLoadScreen(&eid, type); /* load title-specific zone */
    break;
  default:
    dword_800618B8 = 0;
    title->at_title = 1; /* we are at a title card */
    break;
  }
  next_display_flags |= 0x200000;
  /* get mdat entry for this title state */
  eid_char = NSAlphaNumChar(title->state);
  sprintf(eid_str, "%cMapP", eid_char);
  eid = NSStringToEID(eid_str);
  title->mdat = eid;
  if (next_display_flags & GOOL_FLAG_DISPLAY_IMAGES && title->at_title) { /* title state has entities (to spawn)? */
    mdat = NSLookup(&title->mdat);
    TitleLoadEntries(title->state, 1, 1); /* load mdat ipal cluts and preload other entries in mdat  */
    header = (mdat_header*)mdat->items[0];
    title->w = header->w_idx << 4;
    title->h = header->h_idx << 4;
    title->w_idx = header->w_idx;
    title->h_idx = header->h_idx;
    title->x_offs = 0;
    title->y_offs = 0;
    for (i=0;i<header->entity_count;i++) { /* spawn entities */
      entity = (mdat_entity*)mdat->items[2+i];
      if (entity->loc.z <= levels_unlocked) /* is loc.z really a z loc if this 2d? */
        GoolObjectSpawn(mdat, i);
    }
  }
  return 0;
}

//----- (80032298) --------------------------------------------------------
int TitleKill() {
  if (title)
    free(title);
  cur_lid = 0;
  return 0;
}

//----- (800322CC) --------------------------------------------------------
int TitleLoading(lid_t lid, uint8_t *image_data, nsd *nsd) {
#ifdef PSX
  rect216 rect;
#else
  dim2 dim;
  rect2 rect;
#endif
  int32_t x, y;
  uint32_t w, h;
  uint8_t *buf;
  uint16_t *palette, *src, *dst;
  int i,j,k,l;

  if (!nsd->has_loading_image || !nsd->loading_image_width || !nsd->loading_image_height)
    return SUCCESS;
  if (game_state == GAME_STATE_GAMEOVER
   || game_state == GAME_STATE_NEW_GEM
   || game_state == GAME_STATE_TITLE)
    return SUCCESS;
  if (game_state == GAME_STATE_CUTSCENE && lid == LID_TITLE)
    return SUCCESS;
  w = nsd->loading_image_width;
  h = nsd->loading_image_height;
  buf = malloc(0x4000);
#ifdef PSX
  DrawSync(0);
  for (i=0;i<2;i++) {
    palette = (uint16_t*)&image_data[0];
    src = (uint16_t*)&image_data[512]; /* skip palette */
    dst = (uint16_t*)buf;
    GpuFillDisplay();
    x = context.c2_p->draw.clip.x - (w/2) - 256;
    y = context.c2_p->draw.clip.y - (h/2) + 108;
    for (j=0;j<h/8;j++) { /* load the image in w x 8 pixel blocks */
      for (k=0;k<8;k++) {
        for (l=0;l<w;l++)
          *(dst++) = palette[*(src++)];
      }
      rect.x = x;
      rect.y = y;
      rect.w = w;
      rect.h = 8;
      LoadImage((RECT*)&rec, buf);
      DrawSync(0);
      y += 8;
    }
    GpuSwapBuffers();
  }
#else
  palette = (uint16_t*)&image_data[0];
  src = (uint16_t*)&image_data[512]; /* skip palette */
  dst = (uint16_t*)image_cvt;
  rect.x = context.draw_clip.x - (w/2) - 256;
  rect.y = context.draw_clip.y - (h/2) + 108;
  rect.w = w; rect.h = h;
  TextureCopy((uint8_t*)src, (uint8_t*)dst, &rect.dim, 0, 0, 0, palette, 2, 2, 3);
  GLDrawImage(&rect.dim, (uint8_t*)image_cvt, &rect.loc);
#endif
  ns.draw_skip_counter = 2;
  free(buf);
  return SUCCESS;
}

//----- (8003263C) --------------------------------------------------------
static inline void sub_8003263C(int next_state) {
  uint32_t arg;

  if (paused && pause_obj) {
    arg = 0;
    GoolSendEvent(0, pause_obj, 0xC00, 1, &arg);
    pause_status = -1;
    paused = 0;
  }
  fade_counter = -256;
  title_state = next_state;
  title->next_state = next_state;
  title->transition_state = 5;
}

/*
transition states
  0 = start of game
  1 = blank screen
  2 = ?
  3 = full screen/finished fading in
  4 = unused?
  5 = started fading out
  6 = started fading in
  7 = finished fading out
*/

//----- (800326D8) --------------------------------------------------------
int TitleUpdate(void *ot) {
#ifdef PSX
  SPRT_16 *sprite;
  DR_MODE *dr_mode;
#else
  uint32_t tile_cvt[16*16];
  uint32_t *subimage;
  rect2 rect;
  dim2 sdim, ddim;
  pnt2 dloc;
  int x, y;
#endif
  tileinfo *cur;
  int transition_state;
  int i, ii;

#ifndef PSX
  sdim.w=1024;sdim.h=256;
  ddim.w=512;ddim.h=240;
#endif
  transition_state = title->transition_state;
  switch (transition_state) {
  case 0: /* start of game? */
    title->unk7 = 0;
    title->unk8 = 0;
  case 1: /* blank screen? */
    next_display_flags |= GOOL_FLAG_DISPLAY | GOOL_FLAG_ANIMATE; /* set display flags */
    fade_counter = 288; /* reset fade counter */
    title->transition_state = 6; /* change transition state to 'started fading in' */
    if (next_display_flags & GOOL_FLAG_DISPLAY_IMAGES) { /* screen image in this state? */
      TitleCalcTiles(title, title->x_offs, title->y_offs); /* calculate infos for each tile, set draw modes, and get tpage ids */
      TitleLoadImages(title, title->x_offs, title->y_offs); /* load tile textures into vram */
    }
    fade_counter = 288; /* reset fade counter again (in case it was changed) */
    break;
  case 2:
    title->transition_state = 6; /* change transition state to 'started fading in' */
    fade_counter = 288;
    break;
  }
  if (cur_display_flags & GOOL_FLAG_DISPLAY_IMAGES && title->at_title) { /* main menu or title screens? */
    cur = &title->tileinfos[0];
    for (i=0;i<15;i++) { /* draw the tiled splash screen image */
      for (ii=0;ii<32;ii++) {
        cur = &title->tileinfos[i+(ii*16)];
#ifdef PSX
        sprite = (SPRT_16*)context.cur->prims_tail;
        context.cur->prims_tail += sizeof(SPRT_16);
        SetSprt16(sprite);
        sprite->r = 128;
        sprite->g = 128;
        sprite->b = 128;
        sprite->x0 = (cur->x_idx*16) - title->x_offs - 256;
        sprite->y0 = (cur->y_idx*16) - title->y_offs - 120;
        sprite->u0 = cur->u_idx*16;
        sprite->v0 = cur->v_idx*16;
        sprite->clut = title->clut_ids[cur->clut_idx];
        AddPrim(ot, (void*)sprite);
        dr_mode = (DR_MODE*)context.cur->prims_tail;
        context.cur->prims_tail += sizeof(DR_MODE);
        *dr_mode = title->dr_modes[cur->type];
        AddPrim(ot, (void*)dr_mode);
#else
        rect.w=16;rect.h=16;
        rect.x=cur->x;
        rect.y=cur->y;
        dloc.x=cur->x;
        dloc.y=cur->y;
        /* src, dst, src dim, dst dim, src rect, dst loc, ... */
        TextureCopy(image, (uint8_t*)image_cvt, &sdim, &ddim,
          &rect, &dloc, cluts[cur->clut_idx], 2, 1, 3);
#endif
      }
    }
#ifndef PSX
    GLDrawImage(&ddim, (uint8_t*)image_cvt, 0);
#endif
  }
  if (title->transition_state == 5 && fade_counter == 0) { /* currently fading out and fade counter has reached 0? */
    title->transition_state = 7; /* change transition state to 'finished fading out' */
    next_display_flags &= ~(GOOL_FLAG_DISPLAY | GOOL_FLAG_ANIMATE);
#ifdef PSX
    GpuDrawOverlay(255); /* draw semi-trans overlay that will cause primitive to fade */
#else
    GLDrawOverlay(255);
#endif
  }
  else if (title->transition_state == 6 && fade_counter == 0) { /* currently fading in and fade counter has reached 0? */
    title->transition_state = 3; /* change transition state to 'finished fading in' */
  }
  else if (title->transition_state == 7) { /* finished fading out? */
    GoolForEachObjectHandle( /* kill all objects spawned in this title state */
      (gool_ifnptr2_t)GoolObjectHandleTraverseTreePostorder,
      (int)GoolObjectTerminate,
      0);
    if (title->at_title) /* at a title card/splash? */
      TitleLoadEntries(title->state, 0, -1); /* close entries opened in this title state */
    title->transition_state = 1; /* change transition state to 'blank screen' */
    title->state = title->next_state; /* go to the next title state */
#ifdef PSX
    GpuDrawOverlay(255); /* draw semi-trans overlay that will cause primitives to fade */
#else
    GLDrawOverlay(255);
#endif
    TitleLoadState(); /* load the next state */
  }
  if (title->next_state != title_state) { /* pending request to change (to next) state? */
    fade_counter = -256; /* reset fade counter */
    title->next_state = title_state; /* set the next state */
    title->transition_state = 5; /* start fading out; change transition state to 'fading out' */
  }
  return 0;
}

/**
* calculate uv coordinate indices for each imag tile
*
* the actual uv coordinates are 16 times the index values
*
* uvinfos - output uv coordinate indices for tiles
*/
//----- (80032AF4) --------------------------------------------------------
static void TitleCalcUvs(uvinfo *uvinfos) {
  uvinfo *dst;
  int i, ii;
  for (i=0;i<16;i++) {
    for (ii=0;ii<33;ii++) {
      dst = &uvinfos[i+(ii*16)];
      if (i >= 15) { /* last row? */
        dst->u_idx = (ii % 15) + 48;   /* 48, 49, 50, ... 64, 48, 49, 50, ... 64, 48, 49 */
        dst->v_idx = ii / 15;          /*  0   0   0  ...  0,  1,  1,  1, ...  1,  2,  2 */
        dst->mode = 3;
      }
      else if (ii >= 30) { /* last 3 columns? */
        dst->u_idx = i + 32;           /* 32, 32, 32 |  33, 33, 33  |        47, 47, 47 */
        dst->v_idx = ii - 30;          /*  0,  1,  2 |   0,  1,  2  |   ....  0,  1,  2 */
        dst->mode = 2;
      }
      else {
        dst->u_idx = ii + (ii >= 15);  /* 0, 1, 2, ..., 14, 16, 17, 18, 19, 20 ... 29, 0 ... 29 */
        dst->v_idx = i;                /* 0 .....                                   0, 1 ... 14 */
        dst->mode = ii >= 15;          /* 0  0  0  ....  0   1,  1,  1, ..........  1, 0 ...  1 */
      }
    }
  }
}

/**
* calculate infos for each tile, set draw modes, and get tpage ids
*/
//----- (80032BB4) --------------------------------------------------------
void TitleCalcTiles(title_struct *ts, int x_offs, int y_offs) {
  tileinfo *tileinfo;
  uvinfo *uvinfo;
  int i, ii, idx;

  TitleCalcUvs(ts->uvinfos);
#ifdef PSX
  RECT tw;
  ts->tpage_ids[0] = GetTPage(1, 1, 256, 256);
  ts->tpage_ids[1] = GetTPage(1, 1, 384, 256);
  ts->tpage_ids[2] = GetTPage(1, 1, 512, 256);
  ts->tpage_ids[3] = GetTPage(1, 1, 640, 256);
  tw.w = 256; tw.h = 256;
  tw.x = 256; tw.y = 256;
  SetDrawMode(&ts->dr_modes[0], 1, 1, ts->tpage_ids[0], &tw);
  tw.x = 384; tw.y = 256;
  SetDrawMode(&ts->dr_modes[1], 1, 1, ts->tpage_ids[1], &tw);
  tw.x = 512; tw.y = 256;
  SetDrawMode(&ts->dr_modes[2], 1, 1, ts->tpage_ids[2], &tw);
  tw.x = 768; tw.y = 256;
  SetDrawMode(&ts->dr_modes[3], 1, 1, ts->tpage_ids[3], &tw);
#endif
  for (i=0;i<16;i++) {
    for (ii=0;ii<33;ii++,idx++) {
      tileinfo = &ts->tileinfos[i+(ii*16)];
      uvinfo = &ts->uvinfos[i+(ii*16)];
      tileinfo->x_idx = ii + (x_offs >> 4);
      tileinfo->y_idx = i + (y_offs >> 4);
      tileinfo->u_idx = uvinfo->u_idx;
      tileinfo->v_idx = uvinfo->v_idx;
      tileinfo->mode = uvinfo->mode;
    }
  }
}

/**
* load color mode 1 textures into the appropriate locations of vram
* for each splash screen image tile, based on the computed uv indices,
* offset by the specified x,y location
*
* there is one imag entry per column of tiles for a splash screen image
* each item of an imag entry contains the image data (i.e. a color mode 1 texture)
* for a tile in the column (starting at offset 0x4 of the item)
*
* info   - 'tiled image info' with computed uv indices, etc.
* x_offs - x offset at which to load tiles
* y_offs - y offset at which to load tiles
*/
//----- (80032DFC) --------------------------------------------------------
void TitleLoadImages(timginfo *info, int x_offs, int y_offs) {
  entry *mdat, *imag;
  mdat_header *header;
  imag_tile *tile;
  tileinfo *tileinfo;
  uvinfo *uvinfo;
  rect216 rect;
  int x_idx, y_idx, w_idx, h_idx;
  int i, ii, idx;
#ifdef PSX
  DrawSync(0);
#else
  int x, y;
#endif
  w_idx = title->w_idx;
  h_idx = title->h_idx;
  for (i=0;i<15;i++) { /* y */
    for (ii=0;ii<32;ii++,idx++) { /* x */
      x_idx = ii+(x_offs>>4);
      y_idx = i+(y_offs>>4);
      if (x_idx >= w_idx || y_idx >= h_idx) { continue; }
      tileinfo = &info->tileinfos[i+(ii*16)];
      uvinfo = &info->uvinfos[i+(ii*16)];
      tileinfo->x = x_offs+(ii*16);
      tileinfo->y = y_offs+(i*16);
      mdat = NSLookup(&title->mdat);
      header = (mdat_header*)mdat->items[0];
      imag = NSLookup(&header->imags[x_idx]);
      tile = (imag_tile*)imag->items[y_idx];
      tileinfo->clut_idx = tile->clut_id;
#ifdef PSX
      rect.x = (uvinfo->u_idx*8)+256; /* color mode 1; actual memory size of pixel region is half that of 16 bit pixel region */
      rect.y = (uvinfo->v_idx*16)+256;
      rect.w = 8; /* color mode 1 */
      rect.h = 16;
      LoadImage((RECT*)&rect, &tile->data);
#else
      uint8_t *subimage;
      subimage = &image[tileinfo->x+(tileinfo->y*1024)];
      for (y=0;y<16;y++) {
        for (x=0;x<16;x++)
          subimage[x+(y*1024)] = tile->data[x+(y*16)];
      }
#endif
    }
  }
}

/**
* load all entries for a given mdat state
*
* 1) get the mdat entry for the current title state
* 2) load each item, i.e. a clut, of each ipal entry in the mdat header
* into vram at the locations described by the corresponding *'clut lines',
* also in the mdat header
* 3) reserve texture pages for the cluts loaded into vram
* 4) preload all image entries in the mdat header
* 5) preload all tgeo entries in the mdat header
* * each clut line specifies a number of following ipal items (cluts) to
* load into vram and the location at which to load those cluts, or the
* location at which to load the first clut in the line. cluts in the
* line are stacked so that the y location of a clut is one more than
* the y location of the previous clut in that line (if any).
*
* cluts are read sequentially from first to last ipal entry in the mdat
* header, and first to last item (clut) of each ipal entry. each ipal
* entry in the mdat header should have exactly 120 cluts/items, unless
* it is the last one listed in which case it should have at most 120
* cluts/items
*
* state - current mdat state
* flag  - enables pre-loading into virtual pages instead of physical
* count - increment amount for page struct reference counters;
*         this can be negative in which case the function closes
*         instead of opens (loads) the entries
*/
//----- (80032FDC) --------------------------------------------------------
void TitleLoadEntries(int state, int flag, int count) {
  entry *mdat, *ipal, *tgeo;
  mdat_header *header;
  clut_line *line;
  uint8_t *clut;
  char eid_str[6];
  eid_t eid;
  int i, ipal_idx, clut_idx, line_idx;
  int end;

  /* load the mdat entry for this state */
  strcpy(eid_str, "0MapP");
  eid_str[0] = NSAlphaNumChar(state);
  eid = NSStringToEID(eid_str);
  if (eid != EID_NONE) {
    if (count > 0)
      NSOpen(&eid, flag, count);
    else
      NSClose(&eid, -count);
  }
  mdat = NSLookup(&title->mdat);
  header = (mdat_header*)mdat->items[0];
  if (count > 0) {
    clut_idx = 0;
    line_idx = 0;
    for (i=0;i<header->ipal_count;i++) {
      ipal_idx = i / 120;
      if (header->ipals[ipal_idx] != EID_NONE)
        NSOpen(&header->ipals[ipal_idx], flag, count);
      end = min(i+120, header->ipal_count);
      for (;i<end;i++) {
        ipal_idx = i / 120;
        ipal = NSLookup(&header->ipals[ipal_idx]);
        clut = ipal->items[i % 120];
        line = &header->clut_lines[line_idx];
        if (clut_idx >= line->clut_count) {
          clut_idx = 0;
          line_idx++;
        }
#ifdef PSX
        clut_id = LoadClut(clut, line->clut_x, line->clut_y + clut_idx);
        title->clut_ids[i] = clut_id;
#else
        cluts[i] = (uint16_t*)clut; /* ??? */
#endif
        ++clut_idx;
      }
      if (header->ipals[ipal_idx] != EID_NONE)
        NSClose(&header->ipals[ipal_idx], count);
    }

  }
#ifdef PSX
  /* ensure that texture pages are reserved for the allocated cluts */
  NSTexturePageFree(9);
  texture_pages[9].state = 30;
  NSTexturePageFree(10);
  texture_pages[10].state = 30;
  NSTexturePageFree(13);
  texture_pages[13].state = 30;
  if (header->ipal_count > 160) {
    NSTexturePageFree(8);
    texture_pages[8].state = 30;
  }
  else if (texture_pages[8].state == 30)
    texture_pages[8].state = 1;
  if (header->ipal_count > 288) {
    NSTexturePageFree(12);
    texture_pages[12].state = 30;
  }
  else if (texture_pages[12].state == 30)
    texture_pages[12].state = 1;
  if (header->ipal_count >= 417) {
    NSTexturePageFree(14);
    texture_pages[14].state = 30;
  }
  else if (texture_pages[14].state == 30)
    texture_pages[14].state = 1;
#endif
  /* pre-load the imag entries */
  for (i=0;i<header->w_idx;i++) {
    if (header->imags[i] == EID_NONE) { continue; }
    if (count > 0)
      NSOpen(&header->imags[i], flag, count);
    else
      NSClose(&header->imags[i], -count);
  }
  /* pre-load any tgeos for this menu state */
  for (i=0;i<header->tgeo_count;i++) {
    if (header->tgeos[i] == EID_NONE) { tgeo = 0; }
    else if (count > 0)
      tgeo = NSOpen(&header->tgeos[i], flag, count);
    else {
      NSClose(&header->tgeos[i], -count);
      tgeo = 0;
    }
    if (flag && count > 0 && tgeo && tgeo->type == 2) /* orig impl did not check if tgeo is non-null */
      TgeoOnLoad(tgeo);
  }
}
