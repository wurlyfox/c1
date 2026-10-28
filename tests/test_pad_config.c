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

UTEST(pad_config, repeated_lines_add_keys) {
  PadResetBindings();
  write_config("cross = V\ncross = B\n");
  ASSERT_EQ(2, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)PAD_CROSS, held_for(SDL_SCANCODE_V));
  ASSERT_EQ((uint32_t)PAD_CROSS, held_for(SDL_SCANCODE_B));
  ASSERT_EQ(0u, held_for(SDL_SCANCODE_Z));
}

UTEST(pad_config, one_key_can_press_several_buttons) {
  PadResetBindings();
  write_config("cross = V\nsquare = V\n");
  ASSERT_EQ(2, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)(PAD_CROSS | PAD_SQUARE), held_for(SDL_SCANCODE_V));
}

UTEST(pad_config, reloading_replaces_previous_file) {
  PadResetBindings();
  write_config("cross = V\ncross = B\n");
  PadLoadConfig(CONFIG_PATH);
  write_config("cross = N\n");
  ASSERT_EQ(1, PadLoadConfig(CONFIG_PATH));
  ASSERT_EQ((uint32_t)PAD_CROSS, held_for(SDL_SCANCODE_N));
  ASSERT_EQ(0u, held_for(SDL_SCANCODE_V));
  ASSERT_EQ(0u, held_for(SDL_SCANCODE_B));
}

/* the example file documents the built-in defaults, so it must match them */
UTEST(pad_config, example_controls_file_matches_defaults) {
  uint32_t defaults[512];
  int i;

  PadResetBindings();
  for (i=0;i<512;i++)
    defaults[i] = held_for((SDL_Scancode)i);
  /* run from the project root */
  ASSERT_EQ(16, PadLoadConfig("controls.example.cfg"));
  for (i=0;i<512;i++)
    EXPECT_EQ(defaults[i], held_for((SDL_Scancode)i));
}

/*
 * controls.cfg is the player's own (untracked) layout. any layout is fine,
 * so this only checks for typos, and passes if there is no controls.cfg.
 */
UTEST(pad_config, local_controls_file_is_valid) {
  FILE *file;
  char line[128];
  uint32_t bits;
  SDL_Scancode code;
  int line_num;

  file = fopen("controls.cfg", "rb");
  if (!file) { return; }
  for (line_num=1;fgets(line, sizeof(line), file);line_num++) {
    if (PadParseBinding(line, &bits, &code) == -1)
      fprintf(stderr, "controls.cfg:%d: invalid binding: %s", line_num, line);
    EXPECT_NE(-1, PadParseBinding(line, &bits, &code));
  }
  fclose(file);
}
