#include "gpu.h"
#include "globals.h"
#include "ns.h"
#include "gfx.h"

int rcnt_stopped;       /* 8005642C */
gfx_context_db context; /* 80058400 */

extern ns_struct ns;
extern entry *cur_zone;
extern int paused;
extern uint32_t ticks_elapsed;
extern int draw_count;
extern rgb8 vram_fill_color, vram_fill_color_next;

int GpuFillDisplay() {
  BLK_FILL bf;

  TermPrim(&bf);
  SetBlockFill(&bf);
  bf.r = 0; bf.g = 0; bf.b = 0;
  bf.x = context.c2_p->draw.clip.x; 
  bf.y = 0;
  bf.w = 512; bf.h = 240;
  DrawPrim(&bf);
  return DrawSync(0);
}

int GpuInit() {
  gfx_context *gc;
  BLK_FILL bf;
  DRAWENV env;
  int i;

  draw_count = 0;
  ResetGraph(0);
  SetGraphDebug(0);
  SetDefDrawEnv(&env, 0, 0, 1024, 512);
  env.dfe = 1;
  PutDrawEnv(&env);
  TermPrim(&bf);
  SetBlockFill(&bf);
  bf.r = 0; bf.g = 0; bf.b = 0;
  bf.x = 0; bf.y = 0;
  bf.w = 1023; bf.h = 511;
  DrawPrim(&bf);
  gc = &context.c1;
  context.count = 2;
  for (i=0;i<context.count;i++,gc++) {
    gc->idx = i;
    gc->draw_stamp = 0;
    SetDefDispEnv(&gc->disp, i << 9, 0, 512, 240);
    SetDefDrawEnv(&gc->draw, i << 9, 12, 512, 216);
    gc->draw.ofs[0] = 256;
    gc->draw.ofs[1] = 120;
    gc->draw.dtd = 1;
    if (i == 0) {
      context.cur = gc;
      context.c1_p = gc;
    }
    if (i == 1)
      context.c2_p = gc;
    gc->ticks_per_frame = 34; /* initial default ticks/frame value; future values will be calculated */
    gc->draw_stamp = 0;
  }
  PutDispEnv((int)&context.c1_p->disp);
  PutDrawEnv((int)&context.c2_p->draw);
  return SetDispMask(1);
}

int GpuKill() {
  gfx_context *gc; // $s1
  int i;

  DrawSync(0);
  gc = &context.c1;
  for (i=0;i<context.count;i++,gc++)
    free(gc->prims_head);
  if (cur_lid == LID_TITLE)
    CardKill();
  return SUCCESS;
}

int GpuSetupPrims() {
  gfx_context *gc;
  void *primmem;
  size_t primmem_size;
  int i;

  switch (cur_lid) {
  case LID_TITLE: 
  case LID_BONUSTAWNA1: 
  case LID_BONUSBRIO: 
  case LID_LEVELEND:
  case LID_BONUSTAWNA2:
  case LID_BONUSCORTEX:
    sub_8003CE20();
  }
  gc = &context.c1;
  for (i=0;i<context.count;i++,gc++) {
    if (cur_lid == LID_TITLE)
      primmem_size = 0x1E000;
    else if (cur_lid == LID_INTRO)
      primmem_size = 0x1CC00;
    else
      primmem_size = 0x16000;
    primmem = malloc(primmem_size);
    if (primmem == 0)
      exit(-15);
    gc->prims_head = primmem;
    GpuResetPrims(gc);
  }
  return SUCCESS;
}

void GpuResetPrims(gfx_context *gc) {
  gc->prims_tail = gc->prims_head;
  RGpuResetOT(gc->ot, 2048);
}

void **GpuGetPrimsTail() {
  return &(context.cur->prims_tail);
}

void GpuSwapBuffers() {
  context.c1_p = context.c2_p;
  context.c2_p = context.cur;
  context.cur = context.c1_p;
  VSync(0);
  PutDispEnv(&context.c1_p->disp);
  PutDrawEnv(&context.c2_p->draw);
}

void GpuDrawRect(int x, int y, int w, int h, int r, int g, int b) {
  BLK_FILL *bf;

  bf = (BLK_FILL*)context.cur->prims_tail;
  context.cur->prims_tail += sizeof(BLK_FILL);
  SetBlockFill(bf);
  bf->r = r; bf->g = g; bf->b = b;
  bf->x = context.cur->draw.clip.x + x;
  bf->y = context.cur->draw.clip.y + y;
  bf->w = w; bf->h = h;
  AddPrim(context.cur->ot, bf);
}

unsigned int GpuDrawOverlay(int brightness) { /* GpuSetBrightness??? */
  POLY_F4 *poly; // $s1
  DR_MODE *dr_mode;
  uint16_t tpage;
  uint8_t adj[16];

  dr_mode = (DR_MODE *)context.cur->prims_tail;
  context.cur->prims_tail += sizeof(DR_MODE);
  poly = (POLY_F4 *)context.cur->prims_tail;
  context.cur->prims_tail += sizeof(POLY_F4);
  adj[0] = 12; adj[1] = 25; adj[2] = 38; adj[3] = 51;
  adj[4] = 64; adj[5] = 77; adj[6] = 91; adj[7] =105;
  adj[8] =120; adj[9] =135; adj[10]=151; adj[11]=167;
  adj[12]=185; adj[13]=203; adj[14]=225; adj[15]=255;
  if (brightness) {
    brightness = adj[brightness >> 4];
    v2->x0 = -256; v2->y0 = -120;
    v2->x1 =  256; v2->y1 = -120;
    v2->x2 = -256; v2->y2 =  120;
    v2->x3 =  256; v2->y3 =  120;
    v2->r0 = brightness;
    v2->g0 = brightness;
    v2->b0 = brightness;
    SetPolyF4(poly);
    SetSemiTrans(poly, 1);
    AddPrim(context.cur->ot, poly);
    tpage = GetTPage(0, 2, 0, 0);
    SetDrawMode(dr_mode, 1, 0, tpage, 0);
    AddPrim(context.cur->ot, dr_mode);
  }
}

int GpuRoundTicks(int ticks) {
  if (ticks < 0)
    return 34;
  if (ticks < 19) 
    return 17; /* 1/2 frame */
  if (ticks < 36) 
    return 34; /* 1 frame */
  if (ticks < 53)
    return 51; /* 1 1/2 frames */
  return ticks;
}

int GpuUpdate() {
  rgb fill;
  int fh, elapsed_since;
  zone_header *header;

  vram_fill_color = vram_fill_color_next; /* fourth/third byte not copied (its an rgb8 = 3byte value) */
  if (!paused && (cur_display_flags & 0x1000))
    draw_count++;
  if (fade_counter != 0) { /* brightness */
    if (fade_counter < -2) {
      fade_counter += fade_step;
      GpuDrawOverlay(fade_counter + 256);
      if (fade_counter == 0 && !(cur_display_flags & 0x200000))
        fade_counter = -2;
    }
    else if (fade_counter < 0) {
      GpuDrawOverlay(256);
      fade_counter = -1;
    }
    else {
      fade_counter -= fade_step;
      GpuDrawOverlay(fade_counter);
    }
  }
  if (cur_display_flags & 0x80000) {
    if (cur_display_flags & 0x2000) {
      header = (zone_header*)cur_zone->items[0];
      fill = vram_fill_color;
      fh = header->vram_fill_height;
      GpuDrawRect(0, 0, 512, fh, fill.r, fill.g, fill.b);
      GpuDrawRect(0, y, 512, 216-fh, fill.r, fill.g, fill.b);
    }
  }
  AudioUpdate(); /* audio sync with video */
  context.c2_p->sync_stamp = ticks_elapsed;
  DrawSync(0);
  if (pbak_state == 0)
    ticks_cur_frame = ticks_elapsed - context.c2_p->draw_stamp; /* time elapsed between current sync and previous draw  */
  else
    ticks_cur_frame = 17; /* default to 17 */
  if (ns->draw_skip_counter == 0) { /* (ns->gpu_disabled? ns->game_paused?) */
    context.c1_p = context.c2_p; /* swap draw/display contexts */
    context.c2_p = context.cur;
    context.cur = context.c1_p;
  }
  VSync(0); /* wait for VBlank */
  elapsed_since = ticks_elapsed - context.c1_p->draw_stamp;
  if (elapsed_since < 25) /* time elapsed since previous draw still short? (less than 3/4 frame) */
    VSync(0); /* wait for an additional VBlank */
  if (cur_display_flags & 0x100000) {
    if (!rcnt_stopped) { /* root counter has not yet been stopped due to memory card i/o? */
      EnterCriticalSection();
      StopRCnt(0xF2000002); /* stop it */
      ExitCriticalSection();
    }
    rcnt_stopped = 1; /* set flag for root counter is stopped */
    CardUpdate(); /* do memory card i/o */
    ticks_elapsed += 34; /* assume 1 frame will have elapsed */
  }
  else {
    if (rcnt_stopped) { /* root counter -is- currently stopped due to memory card i/o? */
      EnterCriticalSection();
      StartRCnt(0xF2000002); /* start it up again */
      ExitCriticalSection();
    }
    rcnt_stopped = 0; /* clear flag for root counter is stopped */
  }
  EnterCriticalSection(); /* ? why is this necessary? how does this differ from previous accesses of frames_elapsed? */
  if (pbak_state == 2)
    ticks_elapsed = cur_pbak_frame->ticks_elapsed; /* set ticks elapsed to the frame value */
  context.c2_p->draw_stamp = ticks_elapsed;
  ExitCriticalSection();
  elapsed_since = context.c2_p->draw_stamp - context.c1_p->draw_stamp; /* time elapsed between draws */
  context.c1_p->ticks_per_frame = GpuRoundTicks(elapsed_since); /* record rounded tick count */
  PutDispEnv(&context.c1_p->draw); /* put disp environment */
  PutDrawEnv(&context.c2_p->disp); /* put draw environment */
  if (ns->draw_skip_counter > 0)
    ns->draw_skip_counter--;
  if (ns->draw_skip_counter == 0)
    DrawOTag(&context.c2_p->ot); /* draw all primitives in the draw buffer */
  /* reset tail of prim list to start of allocated prim mem */
  context.c2_p->prims_tail = context.c1_p->prims_head; 
  RGpuResetOT(&context.c2_p->ot, 2048); /* reset the ot */
  cur_display_flags = next_display_flags;  /* copy display/animate flags */
}

