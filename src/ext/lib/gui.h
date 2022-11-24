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

typedef struct _gui_item gui_item;

typedef enum {
  none = 0,
  window = 1,
  group = 2,
  text = 3,
  button = 4,
  scalar = 5,
  list = 6,
  tree = 7,
  node = 8,
  color = 9,
  grid = 10,
} gui_type;

typedef union {
  int8_t s8; uint8_t u8;
  int16_t s16; uint16_t u16;
  int32_t s32; uint32_t u32;
  int64_t s64; uint64_t u64;
  float f;
} gui_value;

typedef struct {
  size_t size;
  int is_signed:1;
  int is_float:1;
  gui_value vmin, vmax;
  char fmt[4];
} gui_ptype;

typedef void (*gui_handler_t)(gui_item *item);
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
} gui_scalar;

typedef struct {
  int type;
  gui_item *selected;
} gui_list;

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
} gui_grid;

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
    gui_tree *tr;
    gui_color *c;
    gui_grid *g;
    void *d;
  };
} gui_item;

extern gui_ptype gui_s8;
extern gui_ptype gui_u8;
extern gui_ptype gui_s16;
extern gui_ptype gui_u16;
extern gui_ptype gui_s32;
extern gui_ptype gui_u32;
extern gui_ptype gui_float;

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
extern gui_item *GuiScalarNew(int type, void *data, gui_ptype *ptype, char *label);
extern gui_item *GuiListNew(int type, char *label);
extern gui_item *GuiNodeNew(void *data, gui_data_unparse_t unparse);
extern gui_item *GuiTreeNew(gui_handler_t select, gui_tree_sync *sync);
extern gui_item *GuiColorNew(void *data, void (*unparse)(void*, float[3]), void (*parse)(float[3], void*));
extern gui_item *GuiGridNew(int column_count, int row_count);

extern void GuiItem(gui_item *item);
extern void GuiIgInit();
extern void GuiIgDraw(gui_item *item);
extern void GuiIgNavEnableKeyboard();
extern void GuiIgNavDisableKeyboard();
extern int GuiIgKeyPressed(int key_code);

#endif /* _GUI2_H_ */
