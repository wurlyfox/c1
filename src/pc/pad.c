#include <SDL2/SDL.h>
#include "pc/pad.h"

extern uint8_t keys[512];

typedef struct {
  SDL_Keycode code;
  uint32_t bits;
} sw_pad_mapping;

const sw_pad_mapping pad_mappings[16] = {
  { .code = SDLK_SPACE,      .bits = PAD_SELECT   },
  { .code = SDLK_1,          .bits = PAD_L3       },
  { .code = SDLK_3,          .bits = PAD_R3       },
  { .code = SDLK_RETURN,     .bits = PAD_START    },
  { .code = SDLK_UP+0x80,    .bits = PAD_UP       },
  { .code = SDLK_RIGHT+0x80, .bits = PAD_RIGHT    },
  { .code = SDLK_DOWN+0x80,  .bits = PAD_DOWN     },
  { .code = SDLK_LEFT+0x80,  .bits = PAD_LEFT     },
  { .code = SDLK_q,          .bits = PAD_L2       },
  { .code = SDLK_e,          .bits = PAD_R2       },
  { .code = SDLK_a,          .bits = PAD_L1       },
  { .code = SDLK_d,          .bits = PAD_R1       },
  { .code = SDLK_s,          .bits = PAD_TRIANGLE },
  { .code = SDLK_c,          .bits = PAD_CIRCLE   },
  { .code = SDLK_z,          .bits = PAD_CROSS    },
  { .code = SDLK_x,          .bits = PAD_SQUARE   },
};

uint32_t SwPadRead(int idx) {
  uint32_t held, bits;
  int i, count, code;

  held = 0;
  if (idx != 0) { return held; } /* pad 1 only for now */
  count = sizeof(pad_mappings)/sizeof(sw_pad_mapping);
  for (i=0;i<count;i++) {
    code = pad_mappings[i].code;
    if (keys[code & 0xFF]) {
      bits = pad_mappings[i].bits;
      held |= bits;
    }
  }
  return (held << 16);
}
