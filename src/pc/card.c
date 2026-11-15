/*
 * virtual memory card for the pc/web builds
 *
 * The game's save and load screens are GOOL code (CardC). They drive the
 * card through CardControl (GOOL memcard-op) and watch a few globals:
 * card_flags_ro, card_part_count_ro and card_partinfos. On the PlayStation
 * this is backed by src/psx/card.c, a state machine over the BIOS memory
 * card routines.
 *
 * This file provides the same states, flags and results for a card that is
 * always inserted and formatted, and keeps the saved games in a small file
 * instead. A card holds up to 15 saves; each one is the block of globals the
 * original writes (card_globals).
 */
#include <stdio.h>
#include <string.h>
#include "pc/card.h"
#include "globals.h"
#include "level.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define CARD_FILENAME "save/card.dat"
#else
#define CARD_FILENAME "c1card.dat"
#endif

#define CARD_FLAG_PENDING_IO   1
#define CARD_FLAG_ERROR        2
#define CARD_FLAG_CHECK_NEEDED 4
#define CARD_FLAG_CHECKING     8
#define CARD_FLAG_NEW_DEVICE   0x10
#define CARD_FLAG_6            0x20
/* flags that stop a read or write from being started */
#define CARD_FLAGS_BUSY (CARD_FLAG_PENDING_IO|CARD_FLAG_CHECK_NEEDED|CARD_FLAG_CHECKING)

#define CARD_SAVE_COUNT 15
#define CARD_MAGIC      0x31433143 /* 'C1C1' */

/* same layout as the save block of the original (src/psx/card.h) */
typedef struct {
  uint32_t progress;
  int32_t _level_count;
  int32_t _init_life_count;
  uint32_t _dword_8006190C;
  int32_t _mono;
  uint32_t _sfx_vol;
  uint32_t _mus_vol;
  uint32_t _item_pool1;
  uint32_t _item_pool2;
  uint32_t unk;
  uint32_t unused[21];
  uint32_t checksum;
} card_globals;

typedef struct {
  uint32_t percent; /* shown in the name of the save: "Crash (12%)" */
  card_globals globals;
} card_save;

typedef struct {
  uint32_t magic;
  uint32_t count;
  card_save saves[CARD_SAVE_COUNT];
} card_file;

typedef struct {
  int state;
  int flags;
  int part_count; /* saves listed so far by the directory read */
  int idx;        /* slot of the pending read or write */
  int entry_idx;
  int loaded;     /* card file has been read from disk */
} card_struct;

static card_struct card = { .flags = CARD_FLAG_CHECK_NEEDED };
static card_file file = { 0 };

static void CardFileLoad() {
  FILE *f;
  card_file tmp;

  if (card.loaded) { return; }
  card.loaded = 1;
  memset(&file, 0, sizeof(file));
  file.magic = CARD_MAGIC;
  f = fopen(CARD_FILENAME, "rb");
  if (!f) { return; }
  if (fread(&tmp, 1, sizeof(tmp), f) == sizeof(tmp)
   && tmp.magic == CARD_MAGIC && tmp.count <= CARD_SAVE_COUNT)
    file = tmp;
  fclose(f);
}

static int CardFileStore() {
  FILE *f;
  int ok;

  f = fopen(CARD_FILENAME, "wb");
  if (!f) {
    printf("Error: could not write the saved games to %s\n", CARD_FILENAME);
    return 0;
  }
  ok = fwrite(&file, 1, sizeof(file), f) == sizeof(file);
  fclose(f);
#ifdef __EMSCRIPTEN__
  /* the page copies the file to the browser's persistent storage */
  EM_ASM({ if (Module.c1SaveChanged) { Module.c1SaveChanged(); } });
#endif
  return ok;
}

static uint32_t CardChecksum(card_globals *saved) {
  uint32_t checksum;
  uint8_t *buf;
  int i;

  buf = (uint8_t*)saved;
  checksum = 0x12345678;
  for (i=0;i<(int)sizeof(card_globals);i++) {
    checksum = checksum + buf[i];
    checksum = checksum | (checksum << 3) | (checksum >> 29);
  }
  return checksum;
}

static void CardSaveGlobals(card_globals *saved) {
  memset(saved, 0, sizeof(card_globals));
  saved->progress = (key_count << 10) | (gem_count << 5) | level_count;
  saved->_level_count = level_count;
  saved->_init_life_count = init_life_count;
  saved->_dword_8006190C = dword_8006190C;
  saved->_mono = mono;
  saved->_sfx_vol = sfx_vol;
  saved->_mus_vol = mus_vol;
  saved->_item_pool1 = item_pool1;
  saved->_item_pool2 = item_pool2;
  saved->unk = 0;
  saved->checksum = CardChecksum(saved);
}

static int CardLoadGlobals(card_globals *saved) {
  card_globals tmp;

  tmp = *saved;
  tmp.checksum = 0;
  if (saved->checksum != CardChecksum(&tmp))
    return 0;
  LevelResetGlobals(1);
  level_count = saved->_level_count;
  init_life_count = saved->_init_life_count;
  dword_8006190C = saved->_dword_8006190C;
  mono = saved->_mono;
  sfx_vol = saved->_sfx_vol;
  mus_vol = saved->_mus_vol;
  item_pool1 = saved->_item_pool1;
  item_pool2 = saved->_item_pool2;
  levels_unlocked = saved->_level_count;
  cur_map_level = saved->_level_count;
  return 1;
}

static void CardSetState(int state) {
  card.state = state;
  if (state == 0)
    card.flags &= ~CARD_FLAG_PENDING_IO;
  else
    card.flags |= CARD_FLAG_PENDING_IO;
}

static void CardResetParts() {
  int i;

  card.part_count = 0;
  for (i=0;i<CARD_SAVE_COUNT;i++)
    card_partinfos[i] = 0;
}

/* start reading the directory unless the card is busy or needs a check */
static void CardRequestLoad() {
  if (!(card.flags & (CARD_FLAGS_BUSY)))
    CardSetState(1);
}

/* The original also writes the name of each slot ("EMPTY", "CRASH (12%)")
   into a text buffer (card_str) and its icon into video memory (card_icon).
   Neither exists in the pc port: its text and icon helpers are PlayStation
   only (see sub_8001A754 and sub_8001A850 in gfx.c). The screens still show
   which slots are used and their progress from card_partinfos. */
static void CardSetPartName(int idx, char *name) {
  (void)idx;
  (void)name;
}

/* lists one save: its name and its part info, as the directory read of the
   original does for each file on the card */
static void CardReadPart() {
  card_save *save;
  char name[64];
  uint32_t info;

  save = &file.saves[card.entry_idx];
  sprintf(name, "CRASH (%d%%)", (int)save->percent);
  CardSetPartName(card.part_count, name);
  info = 3 << 1;                        /* image count */
  info |= 8 | (save->globals.progress << 5); /* crash save, with its progress */
  info |= 0x20000;                      /* not damaged */
  info |= 1;                            /* done */
  card_partinfos[card.part_count] = info;
  card.entry_idx++;
  card.part_count++;
}

static void CardWritePart() {
  card_save *save;

  if (card.idx < (int)file.count)       /* overwrite an existing save */
    save = &file.saves[card.idx];
  else if (file.count < CARD_SAVE_COUNT) /* or add one */
    save = &file.saves[file.count++];
  else
    save = 0;
  if (save) {
    save->percent = percent_complete;
    CardSaveGlobals(&save->globals);
  }
  if (!save || !CardFileStore()) {
    card.flags |= CARD_FLAG_ERROR;
    CardSetState(0);
    return;
  }
  card.flags &= ~CARD_FLAG_ERROR;
  CardSetState(0);
  CardRequestLoad();                    /* the directory changed */
  card.flags &= ~CARD_FLAG_NEW_DEVICE;
}

/* advances the card by one step; called once per frame */
void CardUpdate() {
  int i;

  CardFileLoad();
  switch (card.state) {
  case 0: /* idle; a check was requested: the card is there and answers */
    if (card.flags & CARD_FLAG_CHECK_NEEDED) {
      card.flags &= ~(CARD_FLAG_ERROR | CARD_FLAG_CHECK_NEEDED);
      card.flags |= CARD_FLAG_CHECKING | CARD_FLAG_NEW_DEVICE | CARD_FLAG_6;
      CardSetState(4);
      CardResetParts();
    }
    break;
  case 4: /* check the format: always formatted */
    card.flags &= ~(CARD_FLAG_ERROR | CARD_FLAG_CHECKING);
    CardSetState(0);
    CardRequestLoad();
    break;
  case 1: /* start reading the directory */
    card.flags &= ~CARD_FLAG_ERROR;
    for (i=0;i<CARD_SAVE_COUNT;i++)
      CardSetPartName(i, "EMPTY");
    CardSetState(5);
    CardResetParts();
    card.entry_idx = 0;
    break;
  case 5: /* read the directory, one save per step */
    if (card.entry_idx >= (int)file.count) {
      card.flags &= ~CARD_FLAG_ERROR;
      CardSetState(0);
    }
    else
      CardReadPart();
    break;
  case 2: /* load a saved game */
    if (card.idx >= card.part_count || !CardLoadGlobals(&file.saves[card.idx].globals))
      card.flags |= CARD_FLAG_ERROR;
    CardSetState(0);
    break;
  case 3: /* format: erase all saves */
    file.count = 0;
    memset(file.saves, 0, sizeof(file.saves));
    CardFileStore();
    card.flags &= ~CARD_FLAG_ERROR;
    card.flags |= CARD_FLAG_CHECK_NEEDED;
    CardSetState(0);
    break;
  case 6:
  case 7:
  case 8: /* save the game */
    CardWritePart();
    break;
  default:
    CardSetState(0);
    break;
  }
  card_flags_ro = card.flags;
  card_part_count_ro = card.part_count;
}

/* start a read (state 2) or a write (state 6) of the slot */
static int CardStart(int state, int part_idx) {
  if (card.flags & CARD_FLAGS_BUSY) {
    card.flags |= CARD_FLAG_ERROR;
    card_flags_ro = card.flags;
    return 1;
  }
  card.idx = part_idx;
  CardSetState(state);
  card_flags_ro = card.flags;
  return 0;
}

/* GOOL memcard-op; the cases follow CardControl in src/psx/card.c */
int CardControl(int op, int part_idx) {
  switch (op) {
  case 2: /* acknowledge the 'card changed' flag */
    card.flags &= ~CARD_FLAG_6;
    return 0;
  case 3: /* save into the slot */
  case 6: /* save into the slot last used: there is none to remember here */
    return CardStart(6, part_idx);
  case 4: /* load the slot */
    return CardStart(2, part_idx);
  case 5: /* format the card */
    if (card.state != 0) {
      card.flags |= CARD_FLAG_ERROR;
      card_flags_ro = card.flags;
      return 1;
    }
    card.idx = part_idx;
    CardSetState(3);
    card_flags_ro = card.flags;
    return 0;
  case 7: /* is the card of the last save still inserted? 1 = cannot tell,
             which sends the game through the full slot selection */
    return 1;
  case 8: /* 1 = no card answers */
    return 0;
  case 9: /* forget the last save name */
    return 0;
  case 10: /* request a new check of the card */
    CardSetState(0);
    CardResetParts();
    card.flags = (card.flags & CARD_FLAG_NEW_DEVICE) | CARD_FLAG_CHECK_NEEDED;
    card_flags_ro = card.flags;
    return 0;
  default:
    return 1;
  }
}
