#include <LIBAPI.H>
#include <LIBGTE.H>
#include <LIBGS.H>

#include "ns.h"
#include "pad.h"
#include "psx/r3000a.h"
#include "psx/gpu.h"
#include "psx/card.h"
#include "psx/cdr.h"

/* .sbss */
long rcnt_event;
/* .scratch */
scratch __attribute__((section("scratch"))) scratch;

extern ns_struct ns;
extern int use_cd;
extern eid_t insts[8];
extern page_struct texture_pages[16];

int init() {
  int i;
  for (i=0;i<1024;i++)
    ((uint8_t*)scratch)[i] = 0; /* clear scratch memory */
  GsSetWorkBase(0); /* extended graphics lib is not used */
  InitGeom(); /* initialize gte  */
  if (use_cd) { /* use_cd? */
    ResetCallback();
    CdInit(); /* initialize cdrom */
  }
  PadInit(2); /* initialize 2 joypads */
  CardInit(); /* initialize memcard */
  EnterCriticalSection(); /* disable interrupts */
  StopRCnt(0xF2000002); /* stop root counter if active */
  rcnt_event = OpenEvent(0xF2000002,2, 0x1000, RIncTickCount); /* set root counter handler */
  EnableEvent(rcnt_event); /* enable handler for root counter interrupt */
  SetRCnt(0xF2000002, 0x1000, 0x1000); /* set parameters for root counter */
  StartRCnt(0xF2000002); /* start root counter */
  ExitCriticalSection(); /* re-enable interrupts */
  GpuInit(); /* initialize gpu */
  sranda2(); /* seed random generator */
  for (i=0;i<3;i++)
    insts[i] = EID_NONE;
  for (i=0;i<16;i++)
    texture_pages[i].eid = EID_NONE;
  ns.draw_skip_counter = 0;
  if (use_cd) { /* use_cd? */
    CdReadFileSys(); /* read filesystem */
  }
  return SUCCESS;
}

int kill() {
  CdControl(8, 0, 0); /* stop cdrom */
  StopCallback();
  CardKill(); /* kill memcard */
  // sub_8003E490(); /* wraps StopPAD2() */
  StopPAD();
  ResetGraph(3);
  EnterCriticalSection(); /* disable interrupts */
  StopRCnt(0xF2000002); /* stop root counter */
  DisableEvent(rcnt_event); /* disable event handler for root counter interrupt */
  CloseEvent(rcnt_event); /* close event handler */
  ExitCriticalSection(); /* re-enable interrupts */
  return SUCCESS;
}
