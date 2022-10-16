#ifndef _GUI_H_
#define _GUI_H_

#include "common.h"
#include "geom.h"
#include "util/tree.h"

#define GUI_FLAGS_RO     1
#define GUI_FLAGS_ABS    2 /* absolute or relative to parent? */
#define GUI_FLAGS_CW     4 /* width calculated from children or parent if 0? */
#define GUI_FLAGS_CH     8 /* height calculated from children or parent if 0? */
#define GUI_FLAGS_DIR    16
#define GUI_FLAGS_CELL   32
#define GUI_FLAGS_NOCLIP 64
#define GUI_FLAGS_FIT    128
#define GUI_FLAGS_SCROLL 256 /* enable scroll bars when content exceeds inner rect size */
#define GUI_FLAGS_FIXED  512 /* fixed-position non-scrolled child */

// state flags
#define GUI_FLAGS_MOUSEDOWN 1
#define GUI_FLAGS_MOUSEOVER 2
#define GUI_FLAGS_KEYDOWN   4
#define GUI_FLAGS_ACTIVE    8
#define GUI_FLAGS_SELECTED  16
#define GUI_FLAGS_EXPANDED  32

/* forward declarations */
typedef struct _gui_item gui_item;

/* hierarchy */
typedef enum {
  none = 0,
  text = 1,
  slider = 2,
//  draw = 3,
  list = 4,
  tree = 5,
  node = 6
} gui_type;

typedef enum {
  chars = 1,
  value = 2
} gui_text_type;

#define GUI_COMMON \
struct { \
  int flags; \
  int state; \
  int prev_state; \
  rect2 rect; \
  rect2 crect; \
  rect2 win; \
  int invalid; \
  gui_style style; \
  gui_styles styles; \
  union { \
    gui_handlers; \
    gui_handlers handlers; \
  }; \
  int asc; \
  gui_style *astyles[16]; \
  gui_style cstyle; \
};

/* styles/borders */
typedef struct {
  rect2;
  rgba8;
} gui_rect;

#define GuiSetBorderSize(i,x,s) \
i->style.b[x].bt = s; \
i->style.b[x].bb = s; \
i->style.b[x].bl = s; \
i->style.b[x].br = s;
#define GuiSetBorderColor(i,x,c) \
i->style.b[x].ct = c; \
i->style.b[x].cb = c; \
i->style.b[x].cl = c; \
i->style.b[x].cr = c; \

typedef struct {
  rgba8 ct,cl,cr,cb;    /* color */
  int32_t bt,bl,br,bb; /* size/'padding' */
} gui_border;

typedef dim2 (*gui_cs_t)(gui_item*,dim2);
typedef rgba8 (*gui_rgbfn_t)(gui_item*);

#define GUI_STYLE_FLAGS_VISIBLE 1
#define GUI_STYLE_FLAGS_CS      2
#define GUI_STYLE_FLAGS_AL      4
#define GUI_STYLE_FLAGS_FG      8
#define GUI_STYLE_FLAGS_BG      16
#define GUI_STYLE_FLAGS_AC      32
#define GUI_STYLE_FLAGS_TH      64
#define GUI_STYLE_FLAGS_COLORS  56

#define GUI_STYLE_BFLAGS_CT1 1
#define GUI_STYLE_BFLAGS_CL1 2
#define GUI_STYLE_BFLAGS_CR1 4
#define GUI_STYLE_BFLAGS_CB1 8
#define GUI_STYLE_BFLAGS_BT1 16
#define GUI_STYLE_BFLAGS_BL1 32
#define GUI_STYLE_BFLAGS_BR1 64
#define GUI_STYLE_BFLAGS_BB1 128
#define GUI_STYLE_BFLAGS_C1  15
#define GUI_STYLE_BFLAGS_B1  240
#define GUI_STYLE_BFLAGS_1   255

#define GUI_STYLE_BFLAGS_ALL -1
#define GUI_STYLE_FLAGS_ALL  92 /* does not include visible */

typedef struct {
  int flags;      /* determines which fields are used */
  int bflags;     /* determines which border fields are used */
  int visible;    /* visibility */
  int32_t th;     /* text height */
  dim2 cs;        /* content size */
  gui_cs_t fcs;   /* content size (function-based) */
  vec2 al;        /* content align */
  dim2 lcs;       /* (list) default cell size */
  dim2 mc;        /* (list) max number of cells to fit to the visible region of content in the inner rect */
  rgba8 fg,bg,ac; /* colors */
  gui_rgbfn_t ffg,fbg,fac; /* colors (function-based) */
  int bc;         /* border count */
  union {
    gui_border;
    gui_border b1;
    gui_border b[4];
  };
} gui_style;

/* input handling */
#define KEY_UP        0xD2
#define KEY_DOWN      0xD1
#define KEY_LEFT      0xD0
#define KEY_RIGHT     0xCF
#define KEY_BACKSPACE 0x08
#define KEY_TAB       0x09
#define KEY_RETURN    0x0D
#define KEY_ESCAPE    0x1B
#define KEY_DELETE    0x7F
#define KEY_CAPSLOCK  0xB9
#define KEY_F1        0xBA
#define KEY_F2        0xBB
#define KEY_F3        0xBC
#define KEY_F4        0xBD
#define KEYISCHAR(k) ((k>=0x20)&&(k<=0x7E))

typedef struct {
  uint8_t key;
  int key_time;
  int click;   /* bitfield */
  int click_time;
  /* user must populate these fields */
  uint8_t keys[512];
  vec2 mouse;
  rect2 window;
  rect2 screen;
} gui_input;

typedef void (*gui_handler_t)(gui_item*,gui_input*);

typedef struct {
  /* mouse click */
  gui_handler_t mousedown;
  gui_handler_t mouseup;
  gui_handler_t mouseheld;
  /* mouse hover */
  gui_handler_t mouseover;
  gui_handler_t mouseleave;
  /* mouse combination */
  gui_handler_t mousedrag;
  /* key press */
  union { gui_handler_t keydown, keypress; };
  union { gui_handler_t keyup, keyrelease; };
  gui_handler_t keyheld;
  /* item select */
  gui_handler_t select;
  gui_handler_t deselect;
  /* item expand */
  gui_handler_t expand;
  gui_handler_t collapse;
  /* item activate */
  gui_handler_t activate;
  gui_handler_t deactivate;
  /* item timeline */
  gui_handler_t update;
} gui_handlers;

/*
   select / expand / activate    state ind. model (mouse clicks)
   select -> activate            2 state model (list keyboard nav type 1)
   select -> expand -> activate  3 state model (list keyboard nav type 2)

   list item selected = child label highlighted
   list item expanded = child list visible
   list item collapsed = child list not displayed
   list item activated = (one shot) child list (or item) activated
*/

typedef struct {
  gui_style *mousedown;
  gui_style *mouseover;
  gui_style *keydown;
  gui_style *selected;
  /* gui_style *unselected */
  gui_style *collapsed;
  gui_style *expanded;
  gui_style *active;
  /* gui style *inactive */
} gui_styles;

/* specific items (substructs) */
typedef int (*gui_data_unparse_t)(void*, char*);
typedef int (*gui_data_parse_t)(char*, void*);

typedef struct {
  void *data;
  size_t maxlen;
  int invalid;
  gui_data_unparse_t unparse;
  gui_data_parse_t parse;
  int cidx;
  char str[];
} gui_text;

typedef struct {
  struct _gui_item *selected;
  int mode;
} gui_list;

typedef struct {
  int32_t *value;
  int32_t step;
  int32_t vmin, vmax;
  int32_t ssize;
} gui_slider;

/*
typedef struct {
  int poly_count;
  poly3i polys[];
} gui_draw;
*/

typedef tree_node_t gui_node;

typedef struct {
  /* root of tree of mapped root gui_items */
  tree_node_t n;
  /* state */
  gui_item *selected;
  void *sel;
  /* internal data tree */
  tree_node_t *cur, *prev;
  /* bound (external) data tree */
  void *data;
  void **(*child_fn)(void*);
  /* mapping */
  gui_data_unparse_t unparse;
  // gui_node_map_t map;
  // gui_node_unmap_t unmap;
  /* handlers */
  gui_handler_t select;
} gui_tree;

/* generic item struct */
typedef struct _gui_item {
  union {
    struct { TREE_NODE(gui_item); };
    tree_node_t node;
  };
  gui_type type;
  GUI_COMMON;
  union {
    gui_list *l;
    gui_text *t;
    gui_slider *s;
//    gui_draw *d;
    gui_node *n;
    gui_tree *tr;
    void *d;
  };
  struct _gui_item *rep;
  void *userdata;
} gui_item;

/* configuration */
typedef struct {
  void (*draw_rect)(gui_rect*);
  void (*draw_text)(char*,rect2*,rect2*,rgba8*);
  bound2 (*text_extents)(char*,dim2*,int);
  bound2 (*char_extents)(char*,dim2*,int);
} gui_callbacks;

/* gui init */
extern void GuiConfig(gui_callbacks *callbacks);

// temp
extern dim2 GuiListCS(gui_item *item, dim2 size);

/* text binding callbacks */
extern int TextCopyL(char *str, void *val);
extern int TextCopyR(void *val, char *str);
extern int TextParseInt(char *str, void *val);
extern int TextUnparseInt(void *val, char *str);
extern int TextParseInt16(char *str, void *val);
extern int TextUnparseInt16(void *val, char *str);
extern int TextParseInt8(char *str, void *val);
extern int TextUnparseInt8(void *val, char *str);

/* gui functions */
extern gui_item *GuiItemAlloc();
extern void GuiItemFree(gui_item *item);
extern gui_item *GuiItemNew();
extern gui_item *GuiTextNew(void *data, gui_text_type type);
extern gui_item *GuiListNew();
extern gui_item *GuiLItemNew(char *text, gui_item *child);
extern gui_item *GuiTextNodeNew(void *data, gui_data_unparse_t unparse);
extern gui_item *GuiTextTreeNew(void *data, gui_data_unparse_t unparse,
                                void **(*child_fn)(void*), gui_handler_t select);
extern void GuiAddChild(gui_item *item, gui_item *child);
extern void GuiRemoveChild(gui_item *item, gui_item *child);
extern void GuiInsertChild(gui_item *item, gui_item *prev, gui_item *child);
extern void GuiItemDraw(gui_item *item);
extern void GuiItemUpdate(gui_item *item, gui_input *input);
extern void GuiItemInvalidate(gui_item *item);

/* exported helper functions */
extern void GuiTextNodeAdd(gui_item *parent, gui_item *node);
extern void GuiTextNodeInsert(gui_item *parent, gui_item *prev, gui_item *node);
extern void GuiTextNodeRemove(gui_item *node);

#endif /* _GUI_H_ */
