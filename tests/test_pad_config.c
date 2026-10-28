#include <string.h>
#include "utest.h"
#include "pc/pad.h"

#define CONFIG_PATH "build/test_controls.cfg"

extern uint8_t keys[512]; /* defined in test_pad.c */

/* write contents to the test controls file */
static void write_config(const char *contents) {
  FILE *file;

  file = fopen(CONFIG_PATH, "wb");
  fputs(contents, file);
  fclose(file);
}

/* the buttons held while only the given key is pressed */
static uint32_t held_for(SDL_Scancode code) {
  memset(keys, 0, sizeof(keys));
  keys[code] = 1;
  return SwPadRead(0) >> 16;
}

UTEST(pad_config, missing_file_keeps_defaults) {
  PadResetBindings();
  remove(CONFIG_PATH);
  ASSERT_EQ(-1, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)PAD_CROSS, held_for(SDL_SCANCODE_Z));
}

UTEST(pad_config, binding_replaces_default_key) {
  PadResetBindings();
  write_config("cross = V\n");
  ASSERT_EQ(1, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)PAD_CROSS, held_for(SDL_SCANCODE_V));
  ASSERT_EQ(0u, held_for(SDL_SCANCODE_Z));
}

UTEST(pad_config, unlisted_buttons_keep_defaults) {
  PadResetBindings();
  write_config("cross = V\n");
  ASSERT_EQ(1, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)PAD_SQUARE, held_for(SDL_SCANCODE_X));
  ASSERT_EQ((uint32_t)PAD_UP, held_for(SDL_SCANCODE_UP));
}

UTEST(pad_config, invalid_lines_are_skipped) {
  PadResetBindings();
  write_config("jump = Z\r\ncircle = B\r\ncross = Banana\r\n");
  ASSERT_EQ(1, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)PAD_CIRCLE, held_for(SDL_SCANCODE_B));
  ASSERT_EQ(0u, held_for(SDL_SCANCODE_C));
  ASSERT_EQ((uint32_t)PAD_CROSS, held_for(SDL_SCANCODE_Z));
}

UTEST(pad_config, comments_and_blank_lines_are_ignored) {
  PadResetBindings();
  write_config("# controls\n\n  triangle = Left Shift\n");
  ASSERT_EQ(1, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)PAD_TRIANGLE, held_for(SDL_SCANCODE_LSHIFT));
  ASSERT_EQ(0u, held_for(SDL_SCANCODE_S));
}

UTEST(pad_config, reset_restores_defaults) {
  write_config("cross = V\n");
  PadLoadConfig(CONFIG_PATH);
  PadResetBindings();
  ASSERT_EQ((uint32_t)PAD_CROSS, held_for(SDL_SCANCODE_Z));
  ASSERT_EQ(0u, held_for(SDL_SCANCODE_V));
}
