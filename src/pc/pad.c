#include <ctype.h>
#include <SDL2/SDL.h>
#include "pc/pad.h"

extern uint8_t keys[512]; /* indexed by SDL scancode */

typedef struct {
  SDL_Scancode code;
  uint32_t bits;
} sw_pad_mapping;

const sw_pad_mapping pad_default_mappings[16] = {
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

/* current bindings; set to the defaults on first use */
sw_pad_mapping pad_mappings[64];
int pad_mapping_count;
static int pad_mappings_set = 0;

/* button names used in controls files */
const struct {
  const char *name;
  uint32_t bits;
} pad_button_names[16] = {
  { "select",   PAD_SELECT   },
  { "l3",       PAD_L3       },
  { "r3",       PAD_R3       },
  { "start",    PAD_START    },
  { "up",       PAD_UP       },
  { "right",    PAD_RIGHT    },
  { "down",     PAD_DOWN     },
  { "left",     PAD_LEFT     },
  { "l2",       PAD_L2       },
  { "r2",       PAD_R2       },
  { "l1",       PAD_L1       },
  { "r1",       PAD_R1       },
  { "triangle", PAD_TRIANGLE },
  { "circle",   PAD_CIRCLE   },
  { "cross",    PAD_CROSS    },
  { "square",   PAD_SQUARE   },
};

uint32_t SwPadRead(int idx) {
  uint32_t held, bits;
  int i, count, code;

  held = 0;
  if (idx != 0) { return held; } /* pad 1 only for now */
  if (!pad_mappings_set) { PadResetBindings(); }
  count = pad_mapping_count;
  for (i=0;i<count;i++) {
    code = pad_mappings[i].code;
    if (keys[code]) {
      bits = pad_mappings[i].bits;
      held |= bits;
    }
  }
  return (held << 16);
}

/* strip leading and trailing whitespace from str in place */
static char *PadTrim(char *str) {
  char *end;

  while (isspace((unsigned char)*str)) { str++; }
  end = str + strlen(str);
  while (end > str && isspace((unsigned char)end[-1])) { end--; }
  *end = 0;
  return str;
}

/*
 * parse one line of a controls file, in the form "button = key name".
 * key names are SDL scancode names (e.g. "Z", "Left Shift", "Keypad 4").
 * returns 1 and sets bits/code for a binding, 0 for a blank or
 * comment line, and -1 for an invalid line.
 */
int PadParseBindingLine(const char *line, uint32_t *bits, SDL_Scancode *code) {
  char buf[128], *name, *key, *sep;
  SDL_Scancode scancode;
  int i, count;

  snprintf(buf, sizeof(buf), "%s", line);
  name = PadTrim(buf);
  if (*name == 0 || *name == '#') { return 0; }
  sep = strchr(name, '=');
  if (!sep) { return -1; }
  *sep = 0;
  name = PadTrim(name);
  key = PadTrim(sep+1);
  if (*name == 0 || *key == 0) { return -1; }
  scancode = SDL_GetScancodeFromName(key);
  if (scancode == SDL_SCANCODE_UNKNOWN) { return -1; }
  count = sizeof(pad_button_names)/sizeof(pad_button_names[0]);
  for (i=0;i<count;i++) {
    if (SDL_strcasecmp(name, pad_button_names[i].name) == 0) {
      *bits = pad_button_names[i].bits;
      *code = scancode;
      return 1;
    }
  }
  return -1;
}

/* restore the built-in default bindings */
void PadResetBindings() {
  memcpy(pad_mappings, pad_default_mappings, sizeof(pad_default_mappings));
  pad_mapping_count = sizeof(pad_default_mappings)/sizeof(sw_pad_mapping);
  pad_mappings_set = 1;
}

/* remove every key bound to the given button */
static void PadUnbindButton(uint32_t bits) {
  int i, count;

  count = 0;
  for (i=0;i<pad_mapping_count;i++) {
    if (pad_mappings[i].bits != bits)
      pad_mappings[count++] = pad_mappings[i];
  }
  pad_mapping_count = count;
}

/*
 * load key bindings from a controls file. the first line for a button
 * replaces its current keys, and further lines for it add more keys.
 * invalid lines are reported and skipped.
 * returns the number of bindings applied, or -1 if the file can't be opened.
 */
int PadLoadBindings(const char *path) {
  FILE *file;
  char line[128];
  uint32_t bits, rebound;
  SDL_Scancode code;
  int res, line_num, applied, max;

  file = fopen(path, "rb");
  if (!file) { return -1; }
  if (!pad_mappings_set) { PadResetBindings(); }
  max = sizeof(pad_mappings)/sizeof(sw_pad_mapping);
  rebound = 0;
  applied = 0;
  for (line_num=1;fgets(line, sizeof(line), file);line_num++) {
    res = PadParseBindingLine(line, &bits, &code);
    if (res == -1) {
      fprintf(stderr, "%s:%d: invalid binding, expected \"button = key name\"\n",
        path, line_num);
      continue;
    }
    if (res == 0) { continue; }
    if (!(rebound & bits)) {
      PadUnbindButton(bits);
      rebound |= bits;
    }
    if (pad_mapping_count == max) {
      fprintf(stderr, "%s:%d: too many bindings, skipping\n", path, line_num);
      continue;
    }
    pad_mappings[pad_mapping_count].code = code;
    pad_mappings[pad_mapping_count].bits = bits;
    pad_mapping_count++;
    applied++;
  }
  fclose(file);
  return applied;
}
