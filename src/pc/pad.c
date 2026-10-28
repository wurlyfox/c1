#include <SDL2/SDL.h>
#include "pc/pad.h"

extern uint8_t keys[512]; /* indexed by SDL scancode */

typedef struct {
  SDL_Scancode code;
  uint32_t bits;
} sw_pad_mapping;

const sw_pad_mapping pad_mappings[16] = {
  { .code = SDL_SCANCODE_SPACE,  .bits = PAD_SELECT   },
  { .code = SDL_SCANCODE_1,      .bits = PAD_L3       },
  { .code = SDL_SCANCODE_3,      .bits = PAD_R3       },
  { .code = SDL_SCANCODE_RETURN, .bits = PAD_START    },
  { .code = SDL_SCANCODE_UP,     .bits = PAD_UP       },
  { .code = SDL_SCANCODE_RIGHT,  .bits = PAD_RIGHT    },
  { .code = SDL_SCANCODE_DOWN,   .bits = PAD_DOWN     },
  { .code = SDL_SCANCODE_LEFT,   .bits = PAD_LEFT     },
  { .code = SDL_SCANCODE_Q,      .bits = PAD_L2       },
  { .code = SDL_SCANCODE_E,      .bits = PAD_R2       },
  { .code = SDL_SCANCODE_A,      .bits = PAD_L1       },
  { .code = SDL_SCANCODE_D,      .bits = PAD_R1       },
  { .code = SDL_SCANCODE_S,      .bits = PAD_TRIANGLE },
  { .code = SDL_SCANCODE_C,      .bits = PAD_CIRCLE   },
  { .code = SDL_SCANCODE_Z,      .bits = PAD_CROSS    },
  { .code = SDL_SCANCODE_X,      .bits = PAD_SQUARE   },
};

uint32_t SwPadRead(int idx) {
  uint32_t held, bits;
  int i, count, code;

  held = 0;
  if (idx != 0) { return held; } /* pad 1 only for now */
  count = sizeof(pad_mappings)/sizeof(sw_pad_mapping);
  for (i=0;i<count;i++) {
    code = pad_mappings[i].code;
    if (keys[code]) {
      bits = pad_mappings[i].bits;
      held |= bits;
    }
  }
  return (held << 16);
}
