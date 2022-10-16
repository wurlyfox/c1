#include "pbak.h"
#include "globals.h"
#include "math.h"
#include "ns.h"
#include "gool.h"
#include "level.h"

/* .sbss */
pbak_frame *cur_pbak_frame;   /* 800566A8; gp[0xAB] */
pbak_header *cur_pbak_header; /* 80056704; gp[0xC2] */

extern ns_struct ns;
extern lid_t cur_lid;
extern gool_object *crash;
extern gool_handle handles[8];
#ifdef PSX
#include "psx/gpu.h"
extern gfx_context_db context;
#else
#include "pc/gfx/gl.h"
extern gl_context context;
#endif

/* note: return types for PbakInit and PbakKill are void in orig impl;
 *       made int for subsys map compatibility */
//----- (8002E8A4) --------------------------------------------------------
int PbakInit() {
  cur_pbak_header = 0;
  pbak_state = 0;
  return SUCCESS;
}

//----- (8002E8BC) --------------------------------------------------------
int PbakKill() {
  return SUCCESS;
}

//----- (8002E8CC) --------------------------------------------------------
static inline void PbakStart() {
  uint32_t seed;

  pbak_state = 2;
  cur_pbak_frame = &cur_pbak_header->frames[0];
  seed = cur_pbak_header->seed;
  sranda(seed);
#ifdef PSX
  context.c2_p->draw_stamp = cur_pbak_header->draw_stamp;
#else
  context.draw_stamp = cur_pbak_header->draw_stamp;
#endif
  crash->bound = cur_pbak_header->crash_bound;
  checkpoint_id = -1;
  LevelRestart(&cur_pbak_header->savestate);
}

//----- (8002E98C) --------------------------------------------------------
void PbakPlay(eid_t *eid) {
  entry *pbak;
  int argv[2];

  ns.draw_skip_counter = 0;
  pbak = NSOpen(eid, 1, 1);
  cur_pbak_header = (pbak_header*)pbak->items[0];
  *eid = EID_NONE;
  argv[0] = 2279;
  argv[1] = 19993;
  GoolObjectCreate(&handles[1], 4, 8, 2, argv, 1);
  PbakStart();
}

//----- (8002EABC) --------------------------------------------------------
void PbakChoose(eid_t *pbak) {
  char buf[8];
  int count;

  count = NSCountEntries(&ns, 19);
  strcpy(buf, "pb??B");
  memset(buf+6, 0, 2);
  if (count) {
    buf[2] = randb(count) + '0';
    buf[3] = NSAlphaNumChar(ns.lid);
    *pbak = NSStringToEID(buf);
    ns.draw_skip_counter = -1;
    pbak_state = 3;
  }
  else {
    *pbak = EID_NONE;
    if (cur_lid != LID_TITLE)
      game_state = GAME_STATE_CUTSCENE; /* treat like a cutscene */
  }
}
