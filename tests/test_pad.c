#include <string.h>
#include <SDL2/SDL_scancode.h>
#include "utest.h"
#include "pc/pad.h"

/* key state normally filled in by src/pc/init.c, indexed by SDL scancode */
uint8_t keys[512];

static void press(SDL_Scancode code) {
  keys[code] = 1;
}

UTEST(pad, nothing_held_when_no_keys_pressed) {
  memset(keys, 0, sizeof(keys));
  ASSERT_EQ(0u, SwPadRead(0));
}

UTEST(pad, letter_key_maps_to_button) {
  memset(keys, 0, sizeof(keys));
  press(SDL_SCANCODE_Z);
  ASSERT_EQ((uint32_t)PAD_CROSS << 16, SwPadRead(0));
}

UTEST(pad, arrow_key_maps_to_button) {
  memset(keys, 0, sizeof(keys));
  press(SDL_SCANCODE_UP);
  ASSERT_EQ((uint32_t)PAD_UP << 16, SwPadRead(0));
}

UTEST(pad, held_buttons_combine) {
  memset(keys, 0, sizeof(keys));
  press(SDL_SCANCODE_Z);
  press(SDL_SCANCODE_LEFT);
  press(SDL_SCANCODE_RETURN);
  ASSERT_EQ((uint32_t)(PAD_CROSS | PAD_LEFT | PAD_START) << 16, SwPadRead(0));
}

UTEST(pad, only_player_one_is_read) {
  memset(keys, 0, sizeof(keys));
  press(SDL_SCANCODE_Z);
  ASSERT_EQ(0u, SwPadRead(1));
}
