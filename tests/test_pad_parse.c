#include "utest.h"
#include "pc/pad.h"

/* parse line, returning the result and storing what was parsed */
static int parse(const char *line, uint32_t *bits, SDL_Scancode *code) {
  *bits = 0;
  *code = SDL_SCANCODE_UNKNOWN;
  return PadParseBindingLine(line, bits, code);
}

UTEST(pad_parse, letter_binding) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(1, parse("cross = Z", &bits, &code));
  ASSERT_EQ((uint32_t)PAD_CROSS, bits);
  ASSERT_EQ(SDL_SCANCODE_Z, code);
}

UTEST(pad_parse, every_button_name) {
  static const struct { const char *name; uint32_t bits; } buttons[] = {
    { "select", PAD_SELECT }, { "l3", PAD_L3 }, { "r3", PAD_R3 },
    { "start", PAD_START }, { "up", PAD_UP }, { "right", PAD_RIGHT },
    { "down", PAD_DOWN }, { "left", PAD_LEFT }, { "l2", PAD_L2 },
    { "r2", PAD_R2 }, { "l1", PAD_L1 }, { "r1", PAD_R1 },
    { "triangle", PAD_TRIANGLE }, { "circle", PAD_CIRCLE },
    { "cross", PAD_CROSS }, { "square", PAD_SQUARE },
  };
  char line[64];
  uint32_t bits; SDL_Scancode code;
  int i;

  for (i=0;i<sizeof(buttons)/sizeof(buttons[0]);i++) {
    snprintf(line, sizeof(line), "%s = A", buttons[i].name);
    EXPECT_EQ(1, parse(line, &bits, &code));
    EXPECT_EQ(buttons[i].bits, bits);
  }
}

UTEST(pad_parse, ignores_surrounding_whitespace) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(1, parse("  triangle=S \t\r\n", &bits, &code));
  ASSERT_EQ((uint32_t)PAD_TRIANGLE, bits);
  ASSERT_EQ(SDL_SCANCODE_S, code);
}

UTEST(pad_parse, names_are_case_insensitive) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(1, parse("Circle = c", &bits, &code));
  ASSERT_EQ((uint32_t)PAD_CIRCLE, bits);
  ASSERT_EQ(SDL_SCANCODE_C, code);
}

UTEST(pad_parse, multi_word_key_name) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(1, parse("l1 = Left Shift", &bits, &code));
  ASSERT_EQ((uint32_t)PAD_L1, bits);
  ASSERT_EQ(SDL_SCANCODE_LSHIFT, code);
}

UTEST(pad_parse, special_key_names) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(1, parse("up = Up", &bits, &code));
  ASSERT_EQ(SDL_SCANCODE_UP, code);
  ASSERT_EQ(1, parse("start = Return", &bits, &code));
  ASSERT_EQ(SDL_SCANCODE_RETURN, code);
  ASSERT_EQ(1, parse("square = Keypad 4", &bits, &code));
  ASSERT_EQ(SDL_SCANCODE_KP_4, code);
}

UTEST(pad_parse, equals_key) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(1, parse("r2 = =", &bits, &code));
  ASSERT_EQ((uint32_t)PAD_R2, bits);
  ASSERT_EQ(SDL_SCANCODE_EQUALS, code);
}

UTEST(pad_parse, blank_and_comment_lines_are_skipped) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(0, parse("", &bits, &code));
  ASSERT_EQ(0, parse("   \r\n", &bits, &code));
  ASSERT_EQ(0, parse("# cross = Z", &bits, &code));
  ASSERT_EQ(0, parse("  # indented comment", &bits, &code));
}

UTEST(pad_parse, unknown_button_is_invalid) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(-1, parse("jump = Z", &bits, &code));
}

UTEST(pad_parse, unknown_key_is_invalid) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(-1, parse("cross = Banana", &bits, &code));
}

UTEST(pad_parse, missing_parts_are_invalid) {
  uint32_t bits; SDL_Scancode code;
  ASSERT_EQ(-1, parse("cross Z", &bits, &code));
  ASSERT_EQ(-1, parse("cross =", &bits, &code));
  ASSERT_EQ(-1, parse("= Z", &bits, &code));
}
