#ifndef _F_GOOL_H_
#define _F_GOOL_H_

#include "common.h"
#include "geom.h"
#include "ns.h"

typedef struct {
  uint32_t type;
  uint32_t category;
  uint32_t unk_0x8;
  uint32_t init_sp;
  uint32_t subtype_map_idx;
  uint32_t unk_0x14;
} gool_header;

typedef union {
  uint16_t event_map[0];
  uint16_t subtype_map[0];
} gool_state_maps;

typedef struct {
  uint32_t flags;
  uint32_t status_c;
  uint16_t extern_idx;
  uint16_t pc_event;
  uint16_t pc_trans;
  uint16_t pc_code;
} gool_state;

/* anim types */
#define GOOL_ANIM_TYPE_VTX    1
#define GOOL_ANIM_TYPE_SPRITE 2
#define GOOL_ANIM_TYPE_FONT   3
#define GOOL_ANIM_TYPE_TEXT   4
#define GOOL_ANIM_TYPE_FRAG   5

/* TODO: defined constants for category, header type, */
#define gool_anim_header struct { \
  uint8_t type; \
  uint8_t unused; \
  uint8_t length; \
  uint8_t unused_2; \
}

typedef struct {
  gool_anim_header;
  eid_t eid;
} gool_vertex_anim;

typedef struct {
  gool_anim_header;
  eid_t tpage;
  texinfo2 texinfos[];
} gool_sprite_anim;

typedef union {
  struct {
    texinfo2 texinfo;
    uint16_t width;
    uint16_t height;
  };
  uint32_t has_texture;
} gool_glyph;

typedef struct {
  texinfo2 texinfo;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
} gool_frag;

typedef struct {
  gool_anim_header;
  eid_t tpage;
  gool_glyph glyphs[63];
  union {
    gool_frag backdrop;
    int has_backdrop;
  };
} gool_font;

typedef struct {
  gool_anim_header;
  uint32_t unk_0x4;
  uint32_t glyphs_offset;
  char strings[];
} gool_text;

typedef struct {
  gool_anim_header;
  eid_t tpage;
  uint32_t frag_count; /* per frame */
  gool_frag frags[];
} gool_frag_anim;

typedef union {
  struct {
    gool_anim_header;
    eid_t eid;
    uint8_t data[];
  };
  gool_vertex_anim v;
  gool_sprite_anim s;
  gool_font f;
  gool_text t;
  gool_frag_anim r;
} gool_anim;

typedef union { /* used by zdat.h */
  struct {
    union {
      struct {
        matn16 light_mat;
        rgb16 color;
      };
      uint16_t l[12];       /* direct access to component values */
    };
    union {
      struct {
        matn16 color_mat;
        rgb16 intensity;
      };
      rgb16 vert_colors[4]; /* used for gouraud shaded font chars */
      uint16_t c[12];       /* direct access to component values */
    };
  };
  uint16_t a[24];
} gool_colors;

#endif /* _F_GOOL_H_ */
