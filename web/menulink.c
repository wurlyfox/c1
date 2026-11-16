/*
 * web build: an extra entry in the game's own title menu that opens a link
 *
 * The title menu is GOOL code (GamOC, state 8: START, LOAD GAME, PASSWORD,
 * OPTIONS). It already has a hidden fifth entry left by the developers,
 * "TEST SAVE SYSTEM", which is created when global 68 is set as the menu
 * starts. This file turns that entry into the link:
 *
 * - it sets the global just until the menu has started (the global also
 *   enables debug shortcuts elsewhere, so it is cleared again right after);
 * - it replaces the entry's text, in the loaded copy of the menu's strings;
 * - when the entry is chosen, it takes the button press away from the game
 *   (which would open the developers' save test) and asks the page to open
 *   the link instead.
 *
 * Nothing in the game files is changed.
 */
#include <string.h>
#include <emscripten.h>
#include "common.h"
#include "globals.h"
#include "ns.h"
#include "pad.h"
#include "gool.h"
#include "title.h"

#define GLOBAL_MENU_EXTRA   68   /* nonzero: the menu gets its fifth entry */
#define MENU_STATE          8    /* state of GamOC that runs the title menu */
#define MENU_REG_SELECTION  0x26 /* register a0: index of the selected entry */
#define MENU_EXTRA_INDEX    4
#define TITLE_STATE_MENU    5
#define PAD_CONFIRM         (0x40|0x800) /* cross, start */

extern ns_struct ns;
extern pad pads[2];
extern gool_handle handles[8];
extern title_struct *title;

static const char original_text[] = "TEST SAVE SYSTEM";
static gool_object *menu_obj;
static eid_t gamoc_eid;

static int FindMenu(gool_object *obj, int arg) {
  if (obj->global && obj->global->eid == gamoc_eid && obj->state == MENU_STATE)
    menu_obj = obj;
  return 0;
}

/* writes the label over the developers' text; same length, padded with
   spaces, so the strings that follow stay where they are */
static void ReplaceText(entry *exec) {
  char label[sizeof(original_text)];
  uint8_t *p, *end;
  int i, c, len, done;

  len = sizeof(original_text)-1;
  done = 0;
  for (i=0;i<len;i++) {
    c = done ? 0 : EM_ASM_INT({ return ((Module.c1MenuLabel || "").toUpperCase().charCodeAt($0)) || 0; }, i);
    if (c == 0) { done = 1; }
    label[i] = done ? ' ' : (char)c;
  }
  for (i=0;i<exec->item_count;i++) {
    p = exec->items[i];
    end = exec->items[i+1];
    for (;p+len<=end;p++) {
      if (memcmp(p, original_text, len) == 0) {
        memcpy(p, label, len);
        return;
      }
    }
  }
}

/* state of the title sequence (TITLE_STATE_MENU = main menu), -1 outside it */
int WebTitleState() {
  if (!ns.ldat || ns.ldat->lid != LID_TITLE || !title) { return -1; }
  return title->state;
}

/* called once per frame from the main loop, before the game objects run:
   makes the menu start with its fifth entry and gives it its text */
void WebMenuLinkUpdate() {
  uint32_t *globals_w;
  int i, enabled;

  globals_w = (uint32_t*)&globals;
  if (ns.ldat->lid != LID_TITLE || !title || title->state != TITLE_STATE_MENU) {
    globals_w[GLOBAL_MENU_EXTRA] = 0;
    menu_obj = 0;
    return;
  }
  enabled = EM_ASM_INT({ return Module.c1MenuLabel && Module.c1OpenMenuLink ? 1 : 0; });
  if (!enabled) { return; }
  if (!gamoc_eid) { gamoc_eid = NSStringToEID("GamOC"); }
  menu_obj = 0;
  for (i=0;i<8;i++)
    GoolObjectHandleTraverseTreePostorder((gool_object*)&handles[i], FindMenu, 0);
  if (!menu_obj) { /* the menu has not started yet: make it start with 5 entries */
    globals_w[GLOBAL_MENU_EXTRA] = 1;
    return;
  }
  globals_w[GLOBAL_MENU_EXTRA] = 0;
  ReplaceText(menu_obj->global);
}

/* called from GoolObjectUpdate right after the pad is read, which happens
   just before the menu object itself runs: when the extra entry is chosen,
   the press is taken away from the game and the page opens the link */
void WebMenuLinkPad() {
  if (!menu_obj || ns.ldat->lid != LID_TITLE || !title || title->state != TITLE_STATE_MENU
   || menu_obj->state != MENU_STATE || !menu_obj->global || menu_obj->global->eid != gamoc_eid)
    return;
  if (menu_obj->regs[MENU_REG_SELECTION] == MENU_EXTRA_INDEX && (pads[0].tapped & PAD_CONFIRM)) {
    pads[0].tapped &= ~PAD_CONFIRM;
    EM_ASM({ Module.c1OpenMenuLink(); });
  }
}
