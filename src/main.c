#include "common.h"
#include "globals.h"
#include "ns.h"
#include "pad.h"
#include "gfx.h"
#include "misc.h"
#include "gool.h"
#include "level.h"
#include "slst.h"
#include "solid.h"
#include "pbak.h"
#include "audio.h"
#include "midi.h"
#include "title.h"

#ifdef PSX
#include "psx/init.h"
#include "psx/gpu.h"
#include "psx/r3000a.h"
#else
#include "pc/init.h"
#include "pc/time.h"
#include "pc/gfx/gl.h"
#include "pc/gfx/soft.h" // for ext only
#endif

/* .data */
const ns_subsystem subsys[21] = {
#ifdef PSX
  { .name = "NONE", .init =      GpuSetupPrims, .init2 =                  0, .on_load =          0, .unsued = 0, .kill =            GpuKill },
#else
  { .name = "NONE", .init =       GLSetupPrims, .init2 =                  0, .on_load =          0, .unused = 0, .kill =             GLKill },
#endif
  { .name = "SVTX", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "TGEO", .init =                  0, .init2 =                  0, .on_load = TgeoOnLoad, .unused = 0, .kill =                  0 },
  { .name = "WGEO", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "SLST", .init =           SlstInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           SlstKill },
  { .name = "TPAG", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "LDAT", .init =                  0, .init2 =           LdatInit, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "ZDAT", .init =                  0, .init2 =                  0, .on_load = ZdatOnLoad, .unused = 0, .kill =                  0 },
  { .name = "CPAT", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "BINF", .init =           BinfInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           BinfKill },
  { .name = "OPAT", .init = GoolInitAllocTable, .init2 =        GoolInitLid, .on_load =          0, .unused = 0, .kill = GoolKillAllocTable },
  { .name = "GOOL", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "ADIO", .init =          AudioInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =          AudioKill },
  { .name = "MIDI", .init =           MidiInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           MidiKill },
  { .name = "INST", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "IMAG", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "LINK", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "MDAT", .init =          TitleInit, .init2 = TitleLoadNextState, .on_load = MdatOnLoad, .unused = 0, .kill =          TitleKill },
  { .name = "IPAL", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "PBAK", .init =           PbakInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           PbakKill }
};
/* .sdata */
int wgeom_disabled = 0;         /* 800563FC; gp[0x0]  */
int paused = 0;                 /* 80056400; gp[0x1]  */
int pause_status = 0;           /* 8005640C; gp[0x4]  */
int use_cd = 1;                 /* 80056410; gp[0x5]  */
int done = 0;                   /* 80056428; gp[0xB]  */
/* .sbss */
uint32_t pause_stamp;           /* 800565B8; gp[0x6F] */
uint32_t pause_draw_stamp;      /* 800565BC; gp[0x70] */

extern ns_struct ns;
extern pad pads[2];
extern lid_t cur_lid, next_lid;
extern entry *cur_zone;
extern gool_object *crash;
extern eid_t crash_eid;
extern gool_handle handles[8];
extern int bonus_return;
extern level_state savestate;

#ifdef CFLAGS_DRAW_OCTREES
extern zone_query cur_zone_query;
int draw_octrees = 0;
#endif

#ifdef PSX
extern gfx_context_db context;
extern int ticks_elapsed;
#else
extern gl_context context;
#endif

void CoreLoop(lid_t lid);

//----- (80011D88) --------------------------------------------------------
int main() {
#ifdef PSX
  use_cd = 1;
#else
  use_cd = 0;
#endif
  init();
  CoreLoop(LID_BOOTLEVEL);
  _kill();
  return 0;
}

//----- (80011DD0) --------------------------------------------------------
void CoreObjectsCreate() {
  PadUpdate();
  pause_obj = 0;
  if (cur_lid == LID_TITLE) { /* title level? */
    NSOpen(&ns.ldat->exec_map[4], 0, 1);
    NSOpen(&ns.ldat->exec_map[52], 0, 1);
  }
  else if (cur_lid == LID_LEVELEND) { /* level completion screen? */
    NSOpen(&ns.ldat->exec_map[29], 0, 1);
    NSOpen(&ns.ldat->exec_map[30], 0, 1);
    NSOpen(&ns.ldat->exec_map[3], 0, 1);
  }
  else if (cur_lid != LID_INTRO && cur_lid != LID_GAMEWIN) { /* not intro or ending? */
    life_hud = GoolObjectCreate(&handles[1], 4, 0, 0, 0, 0);
    fruit_hud = GoolObjectCreate(&handles[1], 4, 1, 0, 0, 0);
    pickup_hud = GoolObjectCreate(&handles[1], 4, 5, 0, 0, 0);
    NSOpen(&ns.ldat->exec_map[0], 0, 1);
    NSOpen(&ns.ldat->exec_map[5], 0, 1);
    NSOpen(&ns.ldat->exec_map[29], 0, 1);
    if (cur_lid != LID_THEGREATHALL) /* not the great hall? */
      NSOpen(&ns.ldat->exec_map[34], 0, 1); /* load boxes code */
    NSOpen(&ns.ldat->exec_map[3], 0, 1);
    NSOpen(&ns.ldat->exec_map[4], 0, 1);
  }
  LevelInitMisc(1);
}

//----- (80011FC4) --------------------------------------------------------
void CoreLoop(lid_t lid) {
  int is_pause_lid, can_pause;
  zone_header *header;
  void *ot;
  uint32_t arg;
#ifndef PSX
  int ticks_elapsed;
#endif

#if (LID_BOOTLEVEL!=LID_TITLE)
  //life_count=3;
  //init_life_count=3;
  sfx_vol=255;
#endif
  NSInit(&ns, lid);
  CoreObjectsCreate();
  do {
    lid = ns.ldat->lid;
    is_pause_lid = lid != LID_TITLE && lid != LID_LEVELEND && lid != LID_INTRO;
    can_pause = (pbak_state == 0) && ((is_pause_lid && title_pause_state != -1) || title_pause_state > 0);
    if ((pads[0].tapped & 0x800) && can_pause) {
      if (paused = 1 - paused) {
        if (!pause_obj) {
          pause_obj = GoolObjectCreate(&handles[7], 4, 4, 0, 0, 0);
          if (ISSUCCESSCODE(pause_obj)) {
            pause_status = 1;
#ifndef PSX
            ticks_elapsed = GetTicksElapsed();
#endif
            pause_stamp = ticks_elapsed;
#ifdef PSX
            pause_draw_stamp = context.c2_p->draw_stamp;
#else
            pause_draw_stamp = context.draw_stamp;
#endif
          }
          else {
            pause_status = 0;
            paused = 0;
            pause_obj = 0;
          }
        }
      }
      else if (pause_obj) { /* pause screen object exists? */
        arg = 0;
        GoolSendEvent(0, pause_obj, 0xC00, 1, &arg); /* send resume/kill? event to pause screen object */
        pause_obj = 0;
        pause_status = -1;
#ifndef PSX
        ticks_elapsed = GetTicksElapsed();
#endif
        ticks_elapsed = pause_stamp;
#ifdef PSX
        context.c2_p->draw_stamp = pause_draw_stamp;
#else
        context.draw_stamp = pause_draw_stamp;
#endif
      }
    }
    else
      pause_status = 0;
    if (crash && crash_eid != EID_NONE) /* crash exists and there is a pbak entry to play? */
      PbakPlay(&crash_eid);
    if (next_lid == -1 && lid != LID_TITLE
      && (game_state == GAME_STATE_GAMEOVER
       || game_state == GAME_STATE_CONTINUE
       || game_state == 0x400))
      next_lid = LID_TITLE;
    if (next_lid != -1) {
      GoolSendToColliders(0, GOOL_EVENT_LEVEL_END, 0, 0, 0);
      if (next_lid == -2) {
        lid = savestate.lid;
        bonus_return = 1; /* i.e. loading nsf and there is a savestate to load */
      }
      else {
        lid = next_lid;
        bonus_return = 0;
      }
      ns.draw_skip_counter = 2;
      NSKill(&ns);
      paused = 0;
      if (lid == LID_TITLE) {
        respawn_count = 0;
        death_count = 0;
        cortex_count = 0;
        brio_count = 0;
        tawna_count = 0;
        checkpoint_id = -1;
      }
      NSInit(&ns, lid);
      CoreObjectsCreate();
      if (bonus_return) {
        next_lid = -2;
        LevelSpawnObjects();
        next_lid = -1;
        LevelRestart(&savestate);
      }
    }
#ifdef PSX
    NSUpdate(-1);
#endif
    LevelSpawnObjects();
    if (!paused) {
      header = (zone_header*)cur_zone->items[0];
      if (header->flags & (ZONE_FLAG_DARK2 | ZONE_FLAG_LIGHTNING))
        ShaderParamsUpdate(0);
      /* if (!globals->paused) { ??? */
      if (header->flags & ZONE_FLAG_RIPPLE)
        ShaderParamsUpdateRipple(0);
      /* if (!globals->paused) ???*/
      CamUpdate();
    }
    GfxUpdateMatrices();
#ifdef PSX
    ot = context.c2_p->ot;
#else
    ot = context.ot;
#endif
    header = (zone_header*)cur_zone->items[0];
    if ((cur_display_flags & GOOL_FLAG_DISPLAY_WORLDS) && header->world_count && !wgeom_disabled) {
      if (header->flags & ZONE_FLAG_DARK2)
        GfxTransformWorldsDark2(ot);
      else if ((header->flags & ZONE_FLAG_FOG_LIGHTNING) == ZONE_FLAG_FOG_LIGHTNING)
        GfxTransformWorldsDark(ot);
      else if (header->flags & ZONE_FLAG_FOG)
        GfxTransformWorldsFog(ot);
      else if (header->flags & ZONE_FLAG_RIPPLE)
        GfxTransformWorldsRipple(ot);
      else if (header->flags & ZONE_FLAG_LIGHTNING)
        GfxTransformWorldsLightning(ot);
      else
        GfxTransformWorlds(ot);
    }
#if defined(CFLAGS_DRAW_OCTREES) && !defined(PSX)
    if (pads[0].tapped & 4)
      draw_octrees = !draw_octrees;
    if (draw_octrees)
      SwTransformZoneQuery(&cur_zone_query, ot, GLGetPrimsTail());
#endif
    GoolUpdateObjects(!paused);
#ifndef PSX
    GLClear();
#endif
    if (ns.ldat->lid == LID_TITLE)
      TitleUpdate(ot);
#ifdef PSX
    GpuUpdate();
#else
    GLUpdate();
#endif
  } while (!done);
  cur_display_flags = 0;
#ifdef PSX
  GpuUpdate();
  GpuUpdate();
#endif
  NSKill(&ns);
}
