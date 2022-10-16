#include "card.h"
#include <LIBMCRD.H>

/* .sdata */
card_struct *card        =  0; /* 80056564; gp[0x5A] */
int card_new_device      =  1; /* 80056568; gp[0x5B] */
uint32_t prev_card_flags = -1; /* 8005656C; gp[0x5C] */
int prev_card_state      = -1; /* 80056570; gp[0x5D] */
/* .sbss */
uint32_t save_id;              /* 8005668C; gp[0xA4] */
int file_is_new;               /* 80056690; gp[0xA5] */
long edesc_s_end_io;           /* 800566C4; gp[0xB2] */
long edesc_s_error;            /* 800566C8; gp[0xB3] */
long edesc_s_timeout;          /* 800566CC; gp[0xB4] */
long edesc_s_newdev;           /* 800566D0; gp[0xB5] */
long edesc_h_end_io;           /* 800566EC; gp[0xBC] */
long edesc_h_error;            /* 800566F0; gp[0xBD] */
long edesc_h_timeout;          /* 800566F8; gp[0xBF] */
long edesc_h_newdev;           /* 800566FC; gp[0xC0] */
/* .bss */
char card_filename[24];        /* 8005703C */

/* inline functions that should exist but don't */
inline int CardOpen(card_file *file, char *filename, int flags, void (*on_error)(card_file*)) {
  int i;
  uint32_t fd;

  for (i=0;i<3;i++) {
    fd = open(filename, flags);
    if (fd >= 0) {
      file->fd = fd;
      return 1;
    }
  }
  on_error(file);
  return 0;
}

inline int CardRead(card_file *file, uint8_t *buf, int count, void (*on_error)(card_file*)) {
  int i, ii, res;

  for (i=0;i<count;i++) {
    for (ii=0;ii<3;ii++) {
      res = read(file->fd, buf+(i*128), 128);
      if (res == 128)
        break;
    }
    if (ii >= 3) {
      on_error(file);
      return 0;
    }
  }
  return 1;
}

inline int CardWrite(card_file *file, uint8_t *buf, int count, void (*on_error)(card_file*)) {
  int i, ii, res;

  for (i=0;i<count;i++) {
    for (ii=0;ii<3;ii++) {
      res = write(file->fd, buf+(i*128), 128);
      if (res == 1)
        break;
    }
    if (ii >= 3) {
      on_error(file);
      return 0;
    }
  }
  return 1;
}

inline int CardErase(card_file *file, void (*on_error)(card_file*)) {
  int i, ii, res;

  VSync(20);
  for (i=0;i<3;i++) {
    res = erase(file->name);
    VSync(20);
    if (res == 1)
      break;
  }
  if (i>=3) {
    on_error(file);
    return 0;
  }
  return 1;
}

int CardUpdate() {
  uint32_t flags;
  int state;

  flags = CardGetFlags();
  if (flags != prev_card_flags) {
    prev_card_flags = flags;
    // nullsub_3();
  }
  state = CardGetState();
  if (state != prev_card_state) {
    prev_card_state = state;
    // nullsub_4();
  }
  switch (state) {
  case 0: /* poll events, possibly respond by testing card */
    CardPollEvents();
    break;
  case 1: /* sort all 949 (crash) files to the start and begin reading (state 5) */
    CardLoad();
    break;
  case 2: /* read gool globals from file */
    CardPartReadGlobals();
    break;
  case 3: /* format card */
    CardFormat();
    break;
  case 4: /* test read */
    CardCheck();
    break;
  case 5: /* read file image blocks */
    CardPartReadImages();
    break;
  case 6:
  case 7:
  case 8: /* write file */
    CardPartWrite(state);
    break;
  default:
    break;
  }
  card_flags_ro = sub_8003CB88();
  card_part_count_ro = card->part_count;
}

/* state 0 */
/* on soft endio event keep in state 0 if flag 3 is set */
/* on soft error event set flag 2, and keep in state 0 */
/* on new device soft event, test writing to the card; is successful then clear flags 2& 3, set flags 4 & 6, and set state to 4 */
/* on soft timeout keep in state 0, set flag 3 and clear flag 4 */
void CardPollEvents() {
  if (card->unk6) {
    card->unk6 = 0;
    sub_80051424();
  }
  res = CardPollSoftEvent();
  if (res == -1) { /* no event? */
    return;
  }
  else if (res == 0) { /* soft end i/o event? */
    if (!(CardGetFlags() & CARD_FLAG_CHECK_NEEDED)) { /* check not needed? */
      CardSetState(0);
      return;
    }
  }
  else if (res == 1) { /* soft error event? */
    CardSetState(0); /* set state 0, clear flag 1 */
    CardSetFlags(CARD_FLAG_ERROR);
    return;
  }
  if (res == 0 || res == 3) { /* soft end i/o and flag 3 set, or new device? */
    CardResetHardEvent(); /* restore hard event state to EvStACTIVE if event occurred */
    sub_80051494(0); /* new_card(0), card_write(0, 0x3F, 0): test writing to block 0x3F of the new card */
    if (CardWaitHardEvent() == 0) { /* i/o ended successfully and not interrupted by any other events? */
      CardClearFlags(CARD_FLAG_ERROR | CARD_FLAG_CHECK_NEEDED); /* clear flags 2 and 3 */
      CardSetFlags(CARD_FLAG_CHECKING | CARD_FLAG_NEW_DEVICE | CARD_FLAG_6); /* set flags 4, 5, and 6 */
      CardSetState(4); /* goto state 4 (test read) */
      sub_8003C3E4(150); /* set max fail count? */
      sub_8003CA04(0); /* clear number of parts, fail counter, and zero partinfos */
      return;
    }
  }
  /* soft end i/o and flag 3 set or new device and above i/o not finished yet and interrupted by another event
     or timeout */
  CardSetState(0);
  CardSetFlags(CARD_FLAG_CHECK_NEEDED); /* check was interrupted; need to redo it */
  CardClearFlags(CARD_FLAG_CHECKING); /* stopped checking at the moment */
  sub_8003CA04(0); /* clear number of parts, fail counter, and zero some array of 14 words? */
}

int CardCheck() {
  card->unk6 = 0;
  if (CardTestRead(0) == 1) { /* can read with no error? i.e. check completed successfully? */
    CardClearFlags(CARD_FLAG_ERROR | CARD_FLAG_CHECKING); /* no errors, done checking */
    CardSetState(0); /* set state to 0 */
    sub_8003C9B8();  /* tests flags bit 1, 3, 4; if none set then change to state 1 and return 0; else return 1 */
    return 1;
  }
  else if (sub_8003C3FC()) { /* else error occured; increment error count */
    sub_8003CA04(0);   /* reset part count, unk count, and partinfos */
    /* no complete failure yet; the check will be re-done, so it does not need to be re-requested  */
    CardClearFlags(CARD_FLAG_ERROR | CARD_FLAG_CHECK_NEEDED);
    CardSetFlags(CARD_FLAG_CHECKING); /* check will be redone, so status is still 'checking' */
    CardSetState(0); /* return to state 0 */
    return 1;
  }
  return 0;            /* else, max number of errors have occured, so return error */
}

int CardLoad() {
  if (card->unk6) {
    card->unk6 = 0;
    CardResetSoftEvent();
    card_load();
  }
  res = CardPollSoftEvent();
  if (res < 0 || res > 3) { return 0; } /* return if invalid event */
  if (res == 0) { /* soft end i/o event? */
    // v5 = CardFindFile("bu00:", *(_DWORD *)(v3 + 360) + 1180);
    /* count the number of files beginning with the dir entry name */
    count = CardCountFiles(prev_card_state, &card->entries[0]);
    /* sort the files into a contiguous group */
    sub_8003C7A8(&card->entries[0], count);
    if (count > 0 || ++struct32[7] > 10) {
      CardClearFlags(CARD_FLAG_ERROR);
      sub_8003CA50(); /* copy EMPTY string 15x into array of null terminated strings */
      CardSetState(5); /* set state to 5 */
      sub_8003C3E4(32); /* set upper bound to 32 */
      card->part_count = 0;
      card->entry_count = count; /* set the file/direntry count */
      card->entry_idx = 0;
      return 1;
    }
  }
  /* here if end/io and filecount == 0 OR (error or timeout and error counter < max error count) */'
  if (res == 0 || ((res == 1 || res == 2) && !sub_8003C3FC())) {
    CardSetState(1); /* remain in state 1 (will try again at next update) and return 0 */
    return 0;
  }
  else {
    sub_8003CA04(CARD_FLAG_CHECK_NEEDED); /* reset part, unk count, and reset flags to 4 */
    CardSetFlags(CARD_FLAG_ERROR); /* and also set bit 2 */
    CardSetState(0); /* set state to 0 and return 1 */
    return 1;
  }
}


int sub_8003ADB8(card_file *file) {
  if (sub_8003C3FC()) { /* increment fail count if number of fails reaches threshold then set flags 2 and 3, state 0 */
    card->part_count = 0;
    CardSetFlags(CARD_FLAG_ERROR | CARD_FLAG_CHECK_NEEDED);
    CardSetState(0);
  }
  return CardClose(file->fd);
}

int *sub_8003AE08(card_file *file) {
  if (sub_8003C3FC()) { /* increment fail count? if number of fails reaches threshold then set flag 2, state 0 */
    CardSetFlags(CARD_FLAG_ERROR);
    CardSetState(0);
  }
  CardClose(file->fd);
  if (!a1[1][0]) { return; }
  VSync(60);
  for (i=0;i<9;i++) {
    res = erase(file->name);
    VSync(20);
    if (res == 1 && i >= 3) { break; }
  }
  if (i == 9)
    a1[1][0] = 0;
}

int sub_8003AED8(card_struct *card_struct) {
  if (sub_8003C3FC()) {
    CardSetFlags(CARD_FLAG_ERROR);
    CardSetState(0);
  }
  return CardClose(card_struct);
}

/*
typedef {
  char buf1[64];
  char bufs2[3][128]     // 0x58
  char buf3[64];         // 0x1D8
  char unused[32];
  char buf4[32];         // 0x238
  card_file file;        // 0x258
  int fd;                // 0x268
  int unk;
  char buf5[128];        // 0x270
} stack_sub_8003AF24
*/
int CardPartReadImages() {
  char filename[64];
  card_file file;        // 0x258
  uint32_t fd;                // 0x268
  int unk;
  char part_str[128];        // 0x270

  fd = -1;
  file.fd = &fd;
  if (card->entry_idx >= card->entry_count) { /* invalid entry index? */
    CardClearFlags(CARD_FLAG_ERROR); /* would make more sense if this were CardSetFlags... */
    return CardSetState(0);
  }
  card_part_count = card->part_count;
  entry_idx = card->entry_idx;
  entry = &card->entries[entry_idx];
  part = &card->parts[card_part_count];
  /* get the current dir entry/file name */
  sprintf(filename, "%s%s", "bu00:", entry->name);
  /* open the file */
  if (!CardOpen(&file, filename, 2, sub_8003ADB8))
    return 1;
  /* read the first block (header) of the file */
  if (!CardRead(&file, &file->header, 1, sub_8003ADB8))
    return 1;
  part->is_949 = CardEntryNameIs949(entry->name);
  /* header with valid image count? */
  if (file->header.header[2] == 0x11
    || file->header.header[2] == 0x12
    || file->header.header[2] == 0x13) {
    part->image_count = file->header.header[2] - 16; /* subtract 16 to get the image count */
    damaged = 0;
  }
  else { /* card must be damaged */
    part->image_count = 1;
    damaged = 1;
  }
  /* set name of corresponding part for the block */
  if (damaged) {
    strcpy(part->name, "Damaged slot", 12);
  }
  else {
    part->name[0] = 0;
    if (file->header.name[0] >= 128) { /* is the name in unicode? */
      sub_8003B5E0(file->header); /* convert it to ascii */
    }
    file->header.name[31] = 0; /* ensure that name is null-terminated before copying */
    strcpy(&part->name, file->header.name)
  }
  /* also convert to lowercase */
  sub_8003C970(&part->name);
  /* get number of parts from the name in the header */
  if (damaged)
    part_count=1;
  else
    part_count=part->name[3];
  part->entry_idx = entry_idx;
  if (!damaged) {
    if (!CardRead(&file, (uint8_t*)file->images, part->image_count, sub_8003ADB8))
      return 1;
  }
  /* create an array of null termed strings 'PART %d of part_count', in card_str */
  for (i=0;i<part_count;i++) {
    sprintf(part_str, "PART %d of %d", i+1, part_count);
    sub_8001A754(card_str, (part_count+i)*256, i ? part_str : &part->name, 40, 1);
  }
  if (!damaged) {
    /* copy the (up to, the same, 3) images for each part into their corresponding vram locations */
    for (i=0;i<part_count;i++) {
      for (ii=0;ii<3;ii++) {
        idx = (((card->part_count+i)*3)+ii)<<8;
        image = ii<part->image_count?images[ii]:images[0];
        sub_8001A850(1, card_icon, idx, count ? buf4: 0, image);
      }
    }
  }
  /* set image count in bits 2 and 3 of the parts partinfo */
  card_partinfos[card->part_count] = 0 | (part->image_count<<1);
  if (part->is_949 && !damaged) { /* is part of a scus949 save and not damaged? */
    for (i=0;i<30;i++) { /* try up to 30 times to read the save block */
      res = read(fd, (uint8_t*)file->globals, 128);
      if (res == 128)
        break;
    }
    if (i==30) { /* failed? */
      ptr2[10] = 0;
      card_partinfos[card->part_count] |= 0x10; /* set bit 5 of the partinfo */
    }
    else /* succeeded */
      card_partinfos[card->part_count] |= (8 | (buf[0] << 5)); /* set bit 4 and shift saved progress bits up to bit 6 and beyond */
  }
  /* set bit 18 if the card is not damaged */
  if (!damaged)
    card_partinfos[card->part_count] |= 0x20000;
  /* finally set bit 1, for this part has finished */
  card_partinfos[card->part_count] |= 1;
  CardClose(&fd); /* and close the file */
  /* multiple parts? (and not dmamaged?) */
  if (!damaged && part_count > 1) {
    for (i=0;i<part_count-1;i++) { /* re-use the same partinfo for each part */
      card->parts[card->part_count+i+1] = card->parts[card->part_count];
      card_partinfos[card->part_count+i+1] = card_partinfos[card->part_count];
    }
  }
  card->entry_idx += 1;
  card->part_count += part_count;
}

int sub_8003B5E0(char *str) {
  char *src, *dst;
  char c1, c2
  int i;

  src = str;
  dst = str;
  for (i=0;i<32;i++) {
    c1 = src[0];
    c2 = src[1];
    if (c1 <= 128)
      ptr--;
    else if (c1 == 0xA0) {
      a2 = 32;
      ptr--;
    }
    else if (c1 == 0x81) {
      if (byte_80053890[0] != 0xFF) {
        while (byte_80053890[++v1] != c2 && byte_80053890[++v1] != 0xFF);
        if (v1 & 1)
          a2 = byte_80053890[v1-1];
        else
          a2 = 0x2E;
      }
      else
        a2 = 0x2E;
    }
    else if (c1 == 0x82) {
      if (c2 - 0x60 >= 0x1A) {
        if (c2 - 0x4F >= 0xA)
          a2 = c2 - 0x1F;
        if (((c2 + 0x7F) & 0xFF) < 0x1A) {
          a2 = c2 - 0x20;
          v0 = a2 & 0xFF;
        }
      }
    }
    else if ((c1 + 0x5F) & 0xFF < 0x3F) {
      ptr--;
      a2 = 0x3F;
    }
    else
      a2 = 0x3F;
    }
    if ((a2 & 0xFF) && ((a2 - 0x20) & 0xFF) > 0x60)
      a2 = 0x20;
    *(ptr2++) = a2;
    if (a2 == 0) { return; }
    ptr += 2;
  }
}

void sub_8003B74C(char *str, int count) {
  char buf[0x50], *src;
  uint16_t *out;
  int done;

  done = 0;
  strcpy(buf, str);
  src = buf;
  dst = (uint16_t*)str;
  for (i=0;i<count;i++) {
    c = *(src++);
    if (done)
      *(dst++) = 0x8140;
    else if (c >= 'a' && c <= 'z')
      *(dst++) = c + 0x8220;
    else if (c >= 'A' && c <= 'Z')
      *(dst++) = c + 0x821F;
    else if (c >= '0' && c <= '9')
      *(dst++) = c + 0x821F;
    else {
      *(dst) = 0x8140;
      for (ii=0;byte_80053890[ii*2]!=0xFF;ii++) {
        if (byte_80053890[ii*2] == c) {
          *(dst++) = byte_80053890[(ii*2)+1] | 0x8100;
          break;
        }
      }
      if (c==0) {
        *(dst++) = 0;
        done = 1;
      }
    }
  }
  for (i=0;i<count;i++) {
    b[0] = ((uint8_t*)out)[0];
    b[1] = ((uint8_t*)out)[1];
    ((uint8_t*)out)[1] = b[0];
    ((uint8_t*)out)[0] = b[1];
    out++;
  }
}

/*

state = 6: choose a new save id
           set file_is_new to 0
           if no current part or part is a crash part go to state 7
           else erase the corresponding file for the part, and if successful set state to 7, then return

if the current part index is larger than the number of parts loaded then:
  it will be necessary to load a new part? so get the name for the corresponding dir entry;
  if it is already a crash (949) entry, then increment the minor save id of the entry name
  and use that as the part name; else, start a new bu00:BASCUS-9490000MMMMMM (where M is the
  save id) and use that as the part name (in place of the corresponding dir entry?);
  if state 7 then go to state 8
  open the file with that part name; if that fails, then return error
  create a new header block with the 'SC\0x13\0x01' header
  set block name to 'Crash (<percent_complete>%)' and convert it to unicode (??)
  copy image for icon 45 into image 0 and 1, also clut into block clut,
  and image for icon 46 into image 2
  write the header block, 3 image blocks, and a save block
  close the file and clear flag 2
  tests flags bit 1, 3, 4; if none set then change to state 1
  clear flag 5
  copy filename to 8005703C
else if state 7, then just open the part at that index; that is-open the filename
as the dir entry name and return if failure, else close the file and goto state 8,
setting file_is_new to 1
*/
int CardPartWrite(int state) {
  uint32_t fd;
  char filename[64];
  card_part *part;
  card_file file;

  /* get the current part */
  if (card->part_count <= 0 || card->idx >= card->part_count)
    part = 0;
  else
    part = &card->parts[card->idx];
  /* choose a new save id if state 6 */
  if (state == 6) {
    save_id = randb(999999);
    file_is_new = 0;
  }
  /* set up the file struct */
  fd = -1;
  file.fd = &fd;
  file.is_new = file_is_new;
  file.name = filename;
  /* if no current part or part is a crash part */
  if (!part || part->is949) {
    if (state == 6) { /* state 6? */
      CardSetState(7); /* change to state 7 */
      state = 7;
    }
  }
  /* else */
  else {
    sprintf(filename, "%s%s", "bu00:", card->entries[part->entry_idx]->name);
    if (state == 6) {
      if (!CardErase(&file, sub_8003AE08))
        return 1;
      else {
        CardSetState(7);
        return 0;
      }
    }
  }
  if (part && part->is949) {
    /* get directly from the dir entry name */
    sprintf(filename, "%s%s", "bu00:", card->entries[part->entry_idx]->name);
  }
  else {
    /* chars 12 and 13 of entry name are the minor save id */
    idx = card->entry_count-1;
    entry = &card->entries[idx];
    if (idx >= 0 && CardEntryNameIs949(entry->name))
      num = (((entry->name[12]-'0')*10)+(entry->name[13]-'0')) + 1; /* increment the numeric value of chars 12 and 13 */
    else
      num = 0; /* start minor save id off at 0 */
    num = min(num, 100);
    sprintf(filename, "%s%s%02d%06d", "bu00:", "BASCUS-94900", num, save_id); /* bu00:BASCUS-94900mmMMMMMM; mm = minor save id, MMMMMM = major save id */
  }
  if (part && part->is949) {
    if (state == 7)
      CardSetState(8);
    if (!CardOpen(&file, filename, 2, sub_8003AE08))
      return 1;
    for (i=127;i>=0;i--)
      ((uint8_t*)file.header[i] = 0;
    file.header.header[0] = 'S';
    file.header.header[1] = 'C';
    file.header.header[2] = 0x13;
    file.header.header[3] = 1;
    sprintf(file.header.name, "Crash (%d%%)", percent_complete);
    /* convert the block name to unicode (??) */
    sub_8003B74C(file.header.name, 0x20);
    /* copy image for icon 45 into image 0 and 1, also clut into block clut,
       and image for icon 46 into image 2 */
    sub_8001A850(2, card_icon, 0x2D00, file.header.clut, images[0]);
    sub_8001A850(2, card_icon, 0x2D00,                0, images[1]);
    sub_8001A850(2, card_icon, 0x2E00,                0, images[2]);
    if (!CardWrite(file, (uint8_t*)&file.header, 1, sub_8003AE08))
      return 1;
    if (!CardWrite(file, (uint8_t*)&file.images, 3, sub_8003AE08))
      return 1;
    sub_8003BE40(&file.globals);
    for (i=0;i<5;i++) {
      if (!CardWrite(file, (uint8_t*)&file.globals, 1, sub_8003AE08))
        return 1;
    }
    CardClose(&fd);
    CardClearFlags(CARD_FLAG_ERROR);
    CardSetState(0);
    sub_8003C9B8(0);
    strcpy(dword_8005703C, filename+5); /* skip the "bu00:" */
    return CardClearFlags(0x10);
  }
  else if (state == 7) { /* part had to be replaced */
    VSync(20);
    if (!CardOpen(&file, filename, 0x10200, sub_8003AE08))
      return 1;
    printf("file descriptor is %d\n", &fd);
    CardClose(&fd);
    file_is_new = 1;
    return CardSetState(8);
  }
}

void CardSaveGlobals(card_globals *saved) {
  int i;

  for (i=0;i<sizeof(card_globals)/sizeof(uint32_t);i++)
    ((uint32_t*)saved)[i] = 0;
  saved->progress = (key_count << 10) | (gem_count << 5) | level_count;
  saved->_level_count = level_count;
  saved->_init_life_count = init_life_count;
  saved->dword_8006190C = dword_8006190C;
  saved->_mono = mono;
  saved->_sfx_vol = sfx_vol;
  saved->_mus_vol = mus_vol;
  saved->_item_pool1 = item_pool1;
  saved->_item_pool2 = item_pool2;
  saved->unk = 0;
  saved->checksum = sub_8003BFFC(save, 128);
}

// unreferenced
int CardLoadGlobals(card_globals *saved) {
  uint32_t checksum;

  checksum = saved->checksum;
  saved->checksum = 0;
  if (checksum != sub_8003BFFC(globals, 128))
    return 0;
  LevelResetGlobals(1);
  level_count = saved->_level_count;
  init_life_count = saved->_init_life_count;
  dword_8006190C = saved->dword_8006190C;
  mono = saved->_mono;
  sfx_vol = saved->_sfx_vol;
  mus_vol = saved->_mus_vol;
  item_pool1 = saved->_item_pool1;
  item_pool2 = saved->_item_pool2;
  levels_unlocked = saved->_level_count;
  cur_map_level = saved->_level_count;
  return 1;
}

unsigned uint32_t sub_8003BFFC(char *buf, int len) {
  uint32_t checksum;

  checksum = 0x12345678;
  for (i=0;i<len;i++) {
    checksum = checksum + buf[i];
    checksum = checksum | (checksum << 3) | (checksum >> 29);
  }
  return checksum;
}

/*
if current part index is larger than number of parts loaded, or the part that index is a non-949 part
then set flag 2, state to 0, and return
else read past all blocks for the part until the save block (if any read fails then return)
read the save block and load all saved globals into memory
if loading globals fails then increment fail count; if the fail count is above the threshold then
go to state 0 and set flag 2
else loading globals succeeded so set state to 0, close the file, and return
*/

int CardPartReadGlobals() {
  uint32_t fd;
  char filename[64];
  card_part *part;
  card_file file;

  if (card->part_count <= 0 || card->idx >= card->part_count)
    part = 0;
  else
    part = &card->parts[card->idx];
  if (!part || !part->is949) {
    CardSetFlags(CARD_FLAG_ERROR);
    CardSetState(0);
    return 0;
  }
  fd = -1;
  file.fd = &fd;
  file.unk = file_is_new;
  file.name = filename;
  sprintf(filename, "%s%s", "bu00:", card->entries[part->entry_idx]->name);
  if (!CardOpen(&file, filename, 2, sub_8003AED8))
    return 1
  if (!CardRead(&file, (uint8_t*)&file.header, 1, sub_8003AED8))
    return 1;
  if (!CardRead(&file, (uint8_t*)&file.images, 3, sub_8003AED8))
    return 1;
  for (i=0;i<5;i++) { /* try 5 times to read the save block and load globals from it */
    if (!CardRead(&file, (uint8_t*)&file.globals, 1, sub_8003AED8))
      return 1;
    if (CardLoadGlobals(&file.globals))
      break;
  }
  if (i == 5) { /* loading globals failed? */
    if (sub_8003C3FC()) { /* increment fail count; if maxed out then */
      CardSetState(0); /* go to state 0 */
      CardSetFlags(CARD_FLAG_ERROR); /* set flag 2 */
    }
  }
  else
    CardSetState(0); /* go to state 0 */
  CardClose(&fd); /* close the file */
  return 1;
}

int CardFormat() {
  if (format("bu00:") == 1) { /* format was successful? */
    CardClearFlags(CARD_FLAG_ERROR); /* clear flag 2 */
    CardSetFlags(CARD_FLAG_CHECK_NEEDED); /* set flag 3 */
  }
  else if (sub_8003C3FC()) { /* else increment error count; below threshold? */
    CardSetFlags(CARD_FLAG_ERROR); /* set flag 2*/
    CardSetState(0); /* set to state 0 */
  }
}

int sub_8003C3E4(int a1) {
  card->count_unk = 0;
  card->bound_unk = a1;
  return 0;
}

int sub_8003C3FC() {
  return ++card->count_unk >= card->bound_unk;
}

// int sub_8003C4_20() { /* CardPollSoftEvent */

int CardPollSoftEvent() {
  randb(999); /* ??? */
  if (TestEvent(edesc_s_end_io)) /* soft end i/o event? */
    return 0;
  if (TestEvent(edesc_s_error)) /* soft error event? */
    return 1;
  if (TestEvent(edesc_s_timeout)) /* soft timeout event? */
    return 2;
  if (TestEvent(edesc_s_newdev)) { /* soft new device event? */
    CardSetFlags(CARD_FLAG_NEW_DEVICE);
    return 3;
  }
  return -1;
}

// unreferenced
int CardWaitSoftEvent() {
  int res;

  while ((res = CardPollSoftEvent()) == -1);
  return res;
}

void CardResetSoftEvent() {
  TestEvent(edesc_s_end_io); /* soft end i/o event? */
  TestEvent(edesc_s_error); /* soft error event? */
  TestEvent(edesc_s_timeout); /* soft timeout event? */
  TestEvent(edesc_s_newdev); /* soft new device event? */
}

// unreferenced
inline int CardPollHardEvent() {
  randb(999);
  if (TestEvent(edesc_h_end_io) == 1) /* hard end i/o event? */
    return 0;
  if (TestEvent(edesc_h_error) == 1) /* hard error event? */
    return 1;
  if (TestEvent(edesc_h_timeout) == 1) /* hard timeout event? */
    return 2;
  if (TestEvent(edesc_h_newdev) == 1) /* hard new device event? */
    return 3;
  return -1;
}

int CardWaitHardEvent() {
  int res;

  while ((res = CardPollHardEvent()) == -1);
  return res;
}

void CardResetHardEvent() {
  TestEvent(edesc_h_end_io); /* hard end i/o event? */
  TestEvent(edesc_h_error); /* hard error event? */
  TestEvent(edesc_h_timeout); /* hard timeout event? */
  TestEvent(edesc_h_newdev); /* hard new device event? */
}

int CardPartNameIs949(int a1) {
  return strncmp("BASCUS-94900", a1, 12) == 0;
}

int CardCountFiles(char *name, DIRENTRY *entry) {
  int i;
  char buf[0x80];

  strcpy(buf, name);
  strcat(buf, "*");
  if (firstfile2(buf, entry) == entry) {
    for (i=0;;i++) {
      if (nextfile(buf, ++entry) != entry) { break; }
    }
  }
  return min(i, 15);
}

/* sort parts with the same BASCUS-94900 header into a contiguous group */
void sub_8003C7A8(card_part *parts, int part_count) {
  card_part *part_a, *part_b;
  char buf[20];
  int i, res, res2, res3, flag;

  for (i=0;i<part_count;i++) {
    part_a = &parts[i];
    res = strncmp("BASCUS-94900", part_a->name, 12) == 0;
    for (ii=1;ii<part_count;ii++) {
      part_b = &parts[ii];
      res2 = strncmp("BASCUS-94900", part_b->name, 12) == 0;
      res3 = strcmp(part_a->name, part_b->name);
      flag = 0;
      if (res)
        flag = !res2 || res3; /* res && (!res2 || res3)   second does not have bascus header or does not match */
      else
        flag = !res2 && res3; /* !res && (!res2 && res3)  both do not have bascus header and do not match */
      if (flag) {
        RMemcpy(part_a->name, buf, 20);
        RMemcpy(part_b->name, part_a->name, 20);
        RMemcpy(buf, part_b->name, 20);
        i--;
        break;
      }
    }
  }
}


void CardClose(uint32_t *fd) {
  if (*fd >= 0)
    close(*fd);
  *fd = -1;
}

// unreferenced
void sub_8003C944(int ticks) {
  uint32_t stamp;

  stamp = ticks_elapsed;
  while (ticks_elapsed - stamp < ticks);
}

/* toLower */
void sub_8003C970(char *str) {
  for (c=*str;c;c=*(++str))
    if (c - 'a' < 26)
      *str = c - 32;
  }
}

/* tests flags bit 1, 3, 4; if none set then change to state 1 and return 0; else return 1 */
int sub_8003C9B8() {
  if (!(CardGetFlags() & 0xD)) {
    CardSetState(1);
    card->unk_count = 0;
    card->unk_bound = 64;
    card->unk7 = 0;
    return 0;
  }
  return 1;
}

int sub_8003CA04(uint32_t flags) {
  int i;

  card->part_count = 0;
  card->unk_count = 0;
  for (i=0;i<14;i++)
    card_partinfos[i] = 0;
  CardSetFlags(flags);
  return 0;
}

/* copy the string EMPTY 15x delimited by null terms, into the card_str buf */
void sub_8003CA50() {
  int i;

  for (i=0;i<15;i++) {
    // sub_8001A754(card_str, i << 8, "EMPTY", 40);
    sub_8001A754(card_str, i << 8, gp[0x62], 40);
  }
}

int CardSetFlags(uint32_t flags) {
  card->flags |= flags;
  card_flags_ro = card->flags;
  return 0;
}

int CardClearFlags(uint32_t flags) {
  card->flags &= ~flags;
  return 0;
}

/*
   sets state
   clears bit 1 if state 0, else sets
   sets word 7 to 1 if state 0, 1, or 4
*/
int CardSetState(int state) {
  card->state = state;
  if (state == 0 || state == 1 || state == 4)
    card->unk6 = 1;
  if (state == 0)
    card->flags &= ~CARD_FLAG_PENDING_IO;
  else {
    card->flags |= CARD_FLAG_PENDING_IO;
    card_flags_ro = card->flags;
  }
  return 0;
}

int CardGetState() {
  card = (card_struct*)gp[0x5A];
  return card->state;
}

int CardGetFlags() {
  card = (card_struct*)gp[0x5A];
  return card->flags;
}

void CardInit() {
  EnterCriticalSection();
  edesc_s_end_io = OpenEvent(0xF4000001, 4, 0x2000);
  edesc_s_error = OpenEvent(0xF4000001, 0x8000, 0x2000);
  edesc_s_timeout = OpenEvent(0xF4000001, 0x100, 0x2000);
  edesc_s_newdev = OpenEvent(0xF4000001, 0x2000, 0x2000);
  edesc_h_end_io = OpenEvent(0xF0000011, 4, 0x2000);
  edesc_h_error = OpenEvent(0xF0000011, 0x8000, 0x2000);
  edesc_h_timeout = OpenEvent(0xF0000011, 0x100, 0x2000);
  edesc_h_newdev = OpenEvent(0xF0000011, 0x2000, 0x2000);
  ExitCriticalSection();
  VSync(5);
  InitCARD2(1);
  VSync(5);
  StartCARD2();
  VSync(5);
  bu_init();
  VSync(5);
  EnableEvent(edesc_s_end_io);
  EnableEvent(edesc_s_error);
  EnableEvent(edesc_s_timeout);
  EnableEvent(edesc_s_newdev);
  EnableEvent(edesc_h_end_io);
  EnableEvent(edesc_h_error);
  EnableEvent(edesc_h_timeout);
  EnableEvent(edesc_h_newdev);
}

int CardKill() {
  StopCARD2();
  return 0;
}

int sub_8003CD60(uint32_t flags) {
  sub_8003CA04(0);
  card->state = 0;
  if (card->state == 0) { /* ??? */
    card->unk6 = 1;
    card->flags &= ~CARD_FLAG_PENDING_IO;
  }
  else {
    card->flags |= CARD_FLAG_PENDING_IO;
    card_flags_ro = card->flags;
  }
  card->flags = (card->flags & 0xFFFFFFC0) | flags;
  card_flags_ro = card->flags;
}

int CardAlloc() {
  card = (card_struct*)calloc(1, sizeof(card_struct));
  if (card_new_device) {
    card->flags |= CARD_FLAG_NEW_DEVICE;
    card_flags_ro = card->flags;
    sprintf(&dword_8005703C, "~inv!");
  }
  else{
    card->flags &= ~CARD_FLAG_NEW_DEVICE;
  }
  sub_8003CD60(CARD_FLAG_CHECK_NEEDED);
  return 0;
}

int CardFree() {
  card_new_device = !!(card->flags & CARD_FLAG_NEW_DEVICE);
  if (!card) {
    free(card);
    card = 0;
  }
  return 0;
}

int CardTestRead(int chan) {
  int res;
  char buf[128]; // [sp+10h] [-80h] BYREF

  bzero(buf, 128);
  CardResetHardEvent(); /* reset event states so that a new event can occur */
  _new_card(); /* disable default generation of a 'new card' event which occurs after card_read */
  _card_read(chan, 0, buf); /* read 128 bytes from block 0 */
  res = CardWaitHardEvent(); /* wait for read to complete */
  if (res != 0) { return 0; } /* return fail if result is not 'io completed' */
  return (buf[0] == 'M' && buf[1] == 'C'); /* else return success if first 2 buf chars are 'M' and 'C' */
}

// unreferenced
int sub_8003D008() {
  card->flags &= ~0x20;
  return 0;
}

// unreferenced
int sub_8003D028(int part_idx) {
  if (card->flags & 0xD) {
    card->flags |= CARD_FLAG_ERROR;
    card_flags_ro = card->flags;
    return 1;
  }
  else {
    card->state = 6;
    card->flags |= CARD_FLAG_PENDING_IO;
    card->idx = part_idx;
    card->unk_bound = 32;
    card->unk_count = 0;
    card_flags_ro = card->flags;
    return 0;
  }
}

int sub_8003D094() {
  int i;

  if (card->flags & 0xD) {
    card->flags |= CARD_FLAG_ERROR;
    card_flags_ro = card->flags;
    return 1;
  }
  for (i=0;i<card->part_count;i++) {
    if (strcmp(dword_8005703C, card->parts[i]->name) == 0)
      break;
  }
  if (i==card->part_count) {
    card->flags |= CARD_FLAG_ERROR;
    card_flags_ro = card->flags;
    return 1;
  }
  else {
    card->state = 6;
    card->flags |= CARD_FLAG_PENDING_IO;
    card->idx = i;
    card->unk_bound = 32;
    card->unk_count = 0;
    card_flags_ro = card->flags;
    return 0;
  }
}

int sub_8003D1B4() {
  // strcpy(dword_8005703C, "~inv!");
  strcpy(dword_8005703C, gp[0x64]);
  return 0;
}

int sub_8003D1F0() {
  if ((card && card->flags & CARD_FLAG_NEW_DEVICE) || card_new_device) {
    // if (strcmp(dword_8005703C, "~inv!"))
    if (strcmp(dword_8005703C, gp[0x64])) {
      CardResetSoftEvent();
      sub_80051424(0);
      res = CardWaitSoftEvent();
      return (res != 0);
    }
  }
  return 1;
}

int sub_8003D308() {
  int res;

  CardResetSoftEvent();
  sub_80051424(0);
  res = CardWaitSoftEvent();
  return (res == 2);
}

// unreferenced
/* just like sub_8003D028 except card->state is set to 2 in the false case */
int sub_8003D3D8(int part_idx) {
  if (card->flags & 0xD) {
    card->flags |= CARD_FLAG_ERROR;
    card_flags_ro = card->flags;
    return 1;
  }
  else {
    card->state = 2;
    card->flags |= CARD_FLAG_PENDING_IO;
    card->idx = part_idx;
    card->unk_bound = 32;
    card->unk_count = 0;
    card_flags_ro = card->flags;
    return 0;
  }
}

int sub_8003D444(int part_idx) {
  if (card->state != 0) {
    card->flags |= CARD_FLAG_ERROR;
    card_flags_ro = card->flags;
    return 1;
  }
  else {
    card->state = 3;
    card->flags |= CARD_FLAG_PENDING_IO;
    card->idx = part_idx;
    card->unk_bound = 3;
    card->unk_count = 0;
    card_flags_ro = card->flags;
    return 0;
  }
}

// unreferenced
int sub_8003D4AC() {
  if (!card) { return 1; }
  sub_8003CD60((card->flags & CARD_FLAG_NEW_DEVICE) | CARD_FLAG_CHECK_NEEDED); /* force check? */
  return 0;
}

int CardControl(int op, int part_idx) {
  switch (op) {
  case 2:
    return sub_8003D008(); /* clear flag 6 */
  case 3:
    return sub_8003D028(part_idx); /* write part with idx */
  case 4:
    return sub_8003D3D8(part_idx); /* read gool globals from part with idx */
  case 5:
    return sub_8003D444(part_idx); /* format card */
  case 6:
    return sub_8003D094(); /* write part with name */
  case 7:
    return sub_8003D1F0(); /* wait for soft event if new device; return 1 if not end i/o event */
  case 8:
    return sub_8003D308(); /* wait for soft event; return 1 if timeout */
  case 9:
    return sub_8003D1B4(); /* copy filename from gool into buf */
  case 10:
    return sub_8003D4AC(); /* force check? */
  default:
    return 1;
  }
}
