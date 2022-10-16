#ifndef _TITLE_H_
#define _TITLE_H_

#include "common.h"
#include "ns.h"

typedef struct { /* u,v coordinate indices for a 16x16 color mode 1 texture */
  uint32_t u_idx;
  uint32_t v_idx;
  uint32_t mode; /* i.e. dr mode idx */
} uvinfo;

typedef struct { /* describes a 16x16 tile, with a color mode 1 texture */
  uint32_t u_idx:4;
  uint32_t v_idx:4;
  uint32_t x_idx:9;
  uint32_t y_idx:8;
  uint32_t mode:2; /* i.e. dr mode idx */
  uint32_t :5;
  uint32_t clut_idx:9;
  uint32_t x:14;
  uint32_t :9;
  uint32_t y:14;
  uint32_t :18;
} tileinfo;

typedef struct {
  uvinfo uvinfos[33*16];
  tileinfo tileinfos[33*16];
} timginfo; /* tiled image info */

typedef struct {
  eid_t mdat;
  uint32_t w; /* width of display */
  uint32_t h; /* height of display */
  uint32_t w_idx; /* width of display in 16x16 tiles */
  uint32_t h_idx; /* height of display in 16x16 tiles */
  uint32_t x_offs; /* x offset of display */
  uint32_t y_offs; /* y offset of display */
  union {                 /* 7 */
    timginfo;
    timginfo info;
  };
  uint16_t clut_ids[480]; /* C68 */
  uint16_t tpage_ids[4];  /* D58 */
#ifdef PSX
  DR_MODE dr_modes[4];    /* D5A */
#else
  uint32_t dr_modes[4][3];
#endif
  int transition_state;   /* D66 */
  int at_title;           /* D67 */
  uint32_t unk7;          /* D68 */
  uint32_t unk8;          /* D69 */
  uint32_t unk9;          /* D6A */
  uint32_t unk10;         /* D6B */
  uint32_t unk11;         /* D6C */
  uint32_t unk12;         /* D6D */
  uint32_t unk13;         /* D6E */
  uint32_t unk14;         /* D6F */
  uint32_t unk15;         /* D70 */
  uint32_t unk16;         /* D71 */
  uint32_t unk17;         /* D72 */
  uint32_t unk18;         /* D73 */
  int state;              /* D74 */
  int next_state;         /* D75 */
} title_struct;

extern int TitleInit();
extern int TitleLoadNextState();
extern int TitleLoadState();
extern int TitleKill();
extern int TitleLoading(lid_t lid, uint8_t *image_data, nsd *nsd);
extern int TitleUpdate(void *ot);
extern void TitleCalcTiles(title_struct *title, int x_offs, int y_offs);
extern void TitleLoadImages(timginfo *info, int x_offs, int y_offs);
extern void TitleLoadEntries(int state, int flag, int count);

#endif /* _TITLE_H_ */
