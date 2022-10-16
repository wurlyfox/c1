#ifndef _CARD_H_
#define _CARD_H_

#include "common.h"
#include <LIBAPI.H>

#define CARD_FLAG_PENDING_IO   1
#define CARD_FLAG_ERROR        2
#define CARD_FLAG_CHECK_NEEDED 4
#define CARD_FLAG_CHECKING     8
#define CARD_FLAG_NEW_DEVICE   0x10
#define CARD_FLAG_6            0x20

typedef struct {
  int is_949;
  int image_count;
  int entry_idx; /* directory entry index */
  char name[64];
} card_part;

typedef struct {
  int state;
  int flags;
  int part_count;
  int idx;
  int unk_bound;
  int unk_count;
  int unk6;
  int unk7;
  int entry_count;
  int entry_idx;
  card_part parts[15];
  DIRENTRY entries[16];  /* 0x49C, 0x127 */
} card_struct;

typedef struct {
  char header[4];
  char name[32];
  char unk[60];
  uint16_t clut[16];
} card_header;

typedef uint16_t card_image[8*8];

typedef struct {
  uint32_t progress;
  int _level_count;
  int _init_life_count;
  uint32_t dword_8006190C;
  int _mono;
  uint32_t _sfx_vol;
  uint32_t _mus_vol;
  uint32_t _item_pool1;
  uint32_t _item_pool2;
  uint32_t unk;
  uint32_t unused[21];
  uint32_t checksum;
} card_globals;

typedef struct {
  uint32_t *fd;
  int unk;
  char *name;
  card_header header;
  card_image images[3];
  card_globals globals;
} card_file;

typedef union {
  card_header header;
  card_image image;
  card_globals globals;
} card_block;

#endif /* _CARD_H_ */