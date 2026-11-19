#ifndef _GUI_H_
#define _GUI_H_

#include "common.h"
#include "geom.h"
#include "util/tree.h"

/* hierarchy */
#define GUI_FLAGS_READONLY     1
#define GUI_FLAGS_MULTILINE    2
#define GUI_FLAGS_SAMELINE     4
#define GUI_FLAGS_VISIBLE      8
#define GUI_FLAGS_NODE_CONTENT 16
#define GUI_FLAGS_CELL_CW      32
#define GUI_FLAGS_RIGHTALIGN   64
#define GUI_FLAGS_DISABLED     128
#define GUI_FLAGS_OPEN         256 
#define GUI_FLAGS_FORM         512
#define GUI_FLAGS_BUTTON       1024 
#define GUI_FLAGS_HSCROLL      2048 
#define GUI_FLAGS_STATUSBAR    4096 

typedef struct _gui_item gui_item;

typedef enum {
  none = 0,
  window = 1,
  group = 2,
  text = 3,
  button = 4,
  scalar = 6,
  list = 7,
  tab = 8,
  tree = 9,
  node = 10,
  color = 11,
  grid = 12,
  separator = 13,
  header = 14,
  toggle = 15,
  image = 16,
} gui_type;

typedef union {
  int8_t s8; uint8_t u8;
  int16_t s16; uint16_t u16;
  int32_t s32; uint32_t u32;
  int64_t s64; uint64_t u64;
  float f;
  double d;
} gui_value;

typedef struct {
  size_t size;
  int is_signed:1;
  int is_float:1;
  gui_value vmin, vmax;
  char fmt[4];
} gui_ptype;

#define GUI_SYNC_INTERNAL 1
#define GUI_SYNC_EXTERNAL 2

typedef void (*gui_handler_t)(gui_item *item);
typedef void (*gui_sync_t)(gui_item *item, int flags);
typedef int (*gui_data_unparse_t)(void*, char*);
typedef int (*gui_data_parse_t)(char*, void*);
typedef gui_item *(*gui_node_map_t)(void*, void**);
typedef void *(*gui_node_unmap_t)(gui_item *item);

typedef struct {
  int is_prim;
  union {
    gui_ptype *ptype;
    struct {
      gui_data_unparse_t unparse;
      gui_data_parse_t parse;
    };
  };
} gui_dtype;

typedef struct {
  size_t maxlen;
  int cidx;
  void *data;
  gui_dtype dtype;
  char str[];
} gui_text;

typedef struct {
  gui_handler_t click;
  char str[];
} gui_button;

typedef struct {
  int type;
  void *data;
  gui_ptype *ptype;
  float step;
  gui_value vmin, vmax;
  int count;
  char *fmt;
} gui_scalar;

typedef int (*gui_image_get_t)(gui_item *item, int idx, 
  float uv0[2], float uv1[2], char *tip);

typedef struct {
  gui_image_get_t get;
  int index;
  float size;
} gui_image;

typedef struct {
  int type; 
  void *data;
} gui_toggle;

typedef struct {
  int type; 
  float height; 
  int clicked; 
  gui_item *selected;
  void *data;
  gui_ptype *ptype;
} gui_list;

typedef gui_list gui_tab;

typedef struct {
  /* internal data tree */
  tree_node_t *cur, *prev;
  /* bound (external) data tree */
  void *data;
  void **(*child_fn)(void*);
  /* mapping */
  gui_data_unparse_t unparse;
  gui_node_map_t map;
  gui_node_unmap_t unmap;
} gui_tree_sync;

typedef struct {
  /* state */
  gui_item *selected;
  void *sel;
  /* handlers */
  gui_handler_t select;
  /* treeview only */
  int column_count;
  int tbl;
  /* synced data */
  gui_tree_sync sync;
} gui_tree;

typedef struct {
  void *data;
  void (*unparse)(void*, float[3]);
  void (*parse)(float[3], void*);
  float value[3];
} gui_color;

typedef struct {
  int column_count;
  int row_count;
  float header_weight;
} gui_grid;

#define GUI_GLYPH_SIZE 16
#define GUI_GLYPH_COUNT 95

typedef struct {
  uint32_t pixels[GUI_GLYPH_SIZE * GUI_GLYPH_SIZE];
  float advance;
  int present;
} gui_glyph;

typedef struct {
  float scale;
  int synthesize;
  rgba synth_color;
} gui_font_options;

typedef struct gui_font gui_font;

typedef struct _gui_item {
  union {
    struct { TREE_NODE(gui_item); };
    tree_node_t node;
  };
  gui_type type;
  int flags;
  gui_text *label;
  union {
    gui_text *t;
    gui_button *b;
    gui_scalar *s;
    gui_list *l;
    gui_tab *tb;
    gui_tree *tr;
    gui_color *c;
    gui_grid *g;
    gui_toggle *tg;
    gui_image *im;
    void *d;
  };
  gui_handler_t update; /* called every frame before drawing */
  gui_sync_t sync;
  void *user;
  char *tip;
} gui_item;

extern gui_ptype gui_s8;
extern gui_ptype gui_u8;
extern gui_ptype gui_s16;
extern gui_ptype gui_u16;
extern gui_ptype gui_s32;
extern gui_ptype gui_u32;
extern gui_ptype gui_float;
extern gui_ptype gui_double;

extern gui_item *GuiItemAlloc();
extern void GuiItemFree(gui_item *item, int free_children);
extern gui_item *GuiItemNew();

extern void GuiAddChild(gui_item *item, gui_item *child);
extern void GuiRemoveChild(gui_item *item, gui_item *child);
extern void GuiInsertChild(gui_item *item, gui_item *prev, gui_item *child);
extern void GuiAddLabel(gui_item *item, char *str);

extern void *GuiNodeData(gui_item *item);
extern gui_item *GuiNodeMap(void *data, void **c_mapped);
extern gui_item *GuiTreeNode(gui_item *tree, void *data);
extern gui_item *GuiNodeTree(gui_item *node);

extern gui_item *GuiWindowNew(char *label);
extern gui_item *GuiGroupNew();
extern gui_item *GuiTextNew(char *str, char *label);
extern gui_item *GuiButtonNew(char *str, gui_handler_t click);
extern gui_item *GuiToggleNew(int type, void *data, char *label);
extern gui_item *GuiScalarNew(int type, void *data, gui_ptype *ptype, char *label);
extern gui_item *GuiScalarVecNew(int type, void *data, gui_ptype *ptype, int count, char *fmt, char *label);
extern gui_item *GuiSeparatorNew();
extern gui_item *GuiHeaderNew(char *label, int open);
extern gui_item *GuiListNew(int type, char *label);
extern gui_item *GuiComboNew(char *label, void *data, gui_ptype *ptype, const char **options, int count);
extern gui_item *GuiGridNew(int column_count, int row_count);
extern gui_item *GuiTabNew(int type, char *label);
extern gui_item *GuiNodeNew(void *data, gui_data_unparse_t unparse);
extern gui_item *GuiTreeNew(gui_handler_t select, gui_tree_sync *sync);
extern gui_item *GuiColorNew(void *data, void (*unparse)(void*, float[3]), void (*parse)(float[3], void*));
extern gui_item *GuiImageNew(float size, gui_image_get_t get);


extern void GuiListSync(gui_item *item, int flags);

extern void GuiSetTip(gui_item *item, char *tip);
extern void GuiSetEnabled(gui_item *item, int enabled);

extern gui_font *GuiFontNew(const gui_glyph *glyphs, const gui_font_options *options);
extern void GuiFontApply(gui_font *font);
extern void GuiFontClear(void);

extern void GuiItem(gui_item *item);
extern void GuiIgInit();
extern void GuiIgDraw(gui_item *item);
extern void GuiIgFpsOverlay(void);
extern void GuiIgNavEnableKeyboard();
extern void GuiIgNavDisableKeyboard();
extern int GuiIgKeyPressed(int key_code);
extern int GuiIgWantCaptureMouse();
extern int GuiIgWantCaptureKeyboard();

extern void (*GuiIgExtraDraw)(void);

#endif /* _GUI_H_ */
