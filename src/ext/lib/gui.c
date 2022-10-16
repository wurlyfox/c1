/**
 * Minimal gui lib (c1 extension).
 *
 * requires
 * - code to render filled rectangles
 * - code to return the bounding rectangle for a rendered text string
 * - code to return the bounding rectangle for a character of a rendered text string
 * - code to render a text string
 * - code to provide mouse and keyboard states
 *
 * See gui.md for additional details.
 */
#include "./pc/gfx/pcgfx.h"
#include "gui.h"
#include <stdlib.h>
#include <string.h>

#define GuiFindChild(i,c,cond) \
for (c=i->child;c;c=c->next) { \
  if (cond) { break; } \
}

/* function for configuring callbacks */
gui_callbacks _gui_callbacks;
void GuiConfig(gui_callbacks *callbacks) {
  _gui_callbacks = *callbacks;
}

/* misc callbacks */
void *GuiId(void *item) { return item; }

/* handler prototypes */
/* text */
void GuiTextKeydown(gui_item *item, gui_input *input);
void GuiTextKeyheld(gui_item *item, gui_input *input);
void GuiTextActivate(gui_item *item, gui_input *input);
void GuiTextDeactivate(gui_item *item, gui_input *input);
/* slider */
void GuiSliderMousedown(gui_item *item, gui_input *input);
void GuiSliderMouseup(gui_item *item, gui_input *input);
void GuiSliderMousedrag(gui_item *item, gui_input *input);
/* scroll */
void GuiScrollLBtnHeld(gui_item *item, gui_input *input);
void GuiScrollRBtnHeld(gui_item *item, gui_input *input);
/* list */
void GuiListKeydown(gui_item *item, gui_input *input);
void GuiListKeyheld(gui_item *item, gui_input *input);
void GuiListActivate(gui_item *item, gui_input *input);
void GuiListDeactivate(gui_item *item, gui_input *input);
/* litem */
void GuiLItemSelect(gui_item *item, gui_input *input);
void GuiLItemDeselect(gui_item *item, gui_input *input);
void GuiLItemActivate(gui_item *item, gui_input *input);
/* text node */
void GuiTextNodeMousedown(gui_item *item, gui_input *input);
void GuiTextNodeSelect(gui_item *item, gui_input *input);
void GuiTextNodeDeselect(gui_item *item, gui_input *input);
void GuiTextNodeExpand(gui_item *item, gui_input *input);
void GuiTextNodeCollapse(gui_item *item, gui_input *input);
/* text tree */
void GuiTextTreeUpdate(gui_item *item, gui_input *input);
void GuiTextTreeKeyheld(gui_item *item, gui_input *input);
void GuiTextTreeKeydown(gui_item *item, gui_input *input);

/* text data binding callbacks */
int TextCopyL(char *str, void *val) {
  strcpy(val, str);
  return 1;
}

int TextCopyR(void *val, char *str) {
  strcpy(str, val);
  return 1;
}

int TextParseInt(char *str, void *val) {
  *((uint32_t*)val) = atoi(str);
  return 1;
}

int TextUnparseInt(void *val, char *str) {
  int res;
  res = sprintf(str, "%d", *((uint32_t*)val));
  return res;
}

int TextParseInt16(char *str, void *val) {
  *((uint16_t*)val) = atoi(str);
  return 1;
}

int TextUnparseInt16(void *val, char *str) {
  int res;
  res = sprintf(str, "%d", *((uint16_t*)val));
  return res;
}

int TextParseInt8(char *str, void *val) {
  *((uint8_t*)val) = atoi(str);
  return 1;
}

int TextUnparseInt8(void *val, char *str) {
  int res;
  res = sprintf(str, "%d", *((uint8_t*)val));
  return res;
}

const rgba8 white = { 255, 255, 255, 255 };
const rgba8 black = {   0,   0,   0, 255 };
const rgba8 grey  = { 128, 128, 128, 255 };
const rgba8 lgrey = { 190, 190, 190, 255 };
const rgba8 dgrey = {  64,  64,  64, 255 };
const rgba8 trans = lgrey;

const gui_border border_trans = {
  .bt=    0,.bl=    0,.br=    0,.bb=    0,
  .ct=trans,.cl=trans,.cr=trans,.cb=trans
};
const gui_border border_black = {
  .bt=    2,.bl=    2,.br=    2,.bb=    2,
  .ct=black,.cl=black,.cr=black,.cb=black
};
const gui_border border_outset = {
  .bt=    2,.bl=    2,.br=    2,.bb=    2,
  .ct=white,.cl=white,.cr=dgrey,.cb=dgrey
};
const gui_border border_inset = {
  .bt=    2,.bl=    2,.br=    2,.bb=    2,
  .ct=dgrey,.cl=dgrey,.cr=white,.cb=white
};

const gui_style style_def = {
  .flags = GUI_STYLE_FLAGS_ALL,
  .bflags = GUI_STYLE_BFLAGS_ALL,
  .visible = 1,
  .al = { 0,0 },        /* center */
  .fg = grey,
  .bg = white,
  .ac = white,
  .bc = 3,
  .b = { border_trans, border_black, border_trans } /* black border, trans padding */
};

const gui_style style_selected = {
  .flags = GUI_STYLE_FLAGS_AL | GUI_STYLE_FLAGS_COLORS,
  .bflags = 0, // (65535 << 8),
  .visible = 1,
  .al = { -1,-1 },        /* center */
  .fg = white,
  .bg = black,
  .ac = white,
  .bc = 3,
  .b = { border_trans, border_trans, border_trans }
};

const gui_style style_active = {
  .flags = GUI_STYLE_FLAGS_AL | GUI_STYLE_FLAGS_COLORS,
  .bflags = 0,// (65535 << 8),
  .visible = 1,
  .al = { 0,0 },        /* center */
  .fg = white,
  .bg = dgrey,
  .ac = white,
  .bc = 3,
  .b = { border_trans, border_black, border_trans }
};

const gui_style style_outset = {
  .flags = GUI_STYLE_FLAGS_AL | GUI_STYLE_FLAGS_COLORS,
  .bflags = (65535 << 8),
  .visible = 1,
  .al = { 0,0 },        /* center */
  .fg = black,
  .bg = lgrey,
  .ac = white,
  .bc = 3,
  .b = { border_trans, border_outset, border_trans }
};

const gui_style style_inset = {
  .flags = GUI_STYLE_FLAGS_AL | GUI_STYLE_FLAGS_COLORS,
  .bflags = (65535 << 8),
  .visible = 1,
  .al = { 0,0 },        /* center */
  .fg = black,
  .bg = lgrey,
  .ac = white,
  .bc = 3,
  .b = { border_trans, border_inset, border_trans }
};

const gui_styles styles_def = {
  .active = (gui_style* const)&style_active,
  .selected = (gui_style* const)&style_selected
};
const gui_handlers handlers_def = { 0 };

/**
 * replace properties of a destination style
 * with the overriding properties from a source style
 *
 * \param src gui_style* - souce style
 * \param dst gui_style* - destination style
 */
void GuiStyleUpdate(gui_style *src, gui_style *dst) {
  int i,bc,bflags;

  if (src->flags & GUI_STYLE_FLAGS_VISIBLE) {
    dst->visible = src->visible;
  }
  if (src->flags & GUI_STYLE_FLAGS_TH) {
    dst->th = src->th;
  }
  if (src->flags & GUI_STYLE_FLAGS_CS) {
    dst->cs = src->cs;
    dst->fcs = src->fcs;
  }
  if (src->flags & GUI_STYLE_FLAGS_AL) {
    dst->al = src->al;
  }
  if (src->flags & GUI_STYLE_FLAGS_FG) {
    dst->fg = src->fg;
    dst->ffg = src->ffg;
  }
  if (src->flags & GUI_STYLE_FLAGS_BG) {
    dst->bg = src->bg;
    dst->fbg = src->fbg;
  }
  if (src->flags & GUI_STYLE_FLAGS_AC) {
    dst->ac = src->ac;
    dst->fac = src->fac;
  }
  bflags = src->bflags;
  bc=0;
  for (i=0;i<src->bc;i++) {
    if (!(bflags & 0xFF)) {
      bflags >>= 8;
      continue;
    }
    if (bflags & GUI_STYLE_BFLAGS_CT1)
      dst->b[i].ct = src->b[i].ct;
    if (bflags & GUI_STYLE_BFLAGS_CL1)
      dst->b[i].cl = src->b[i].cl;
    if (bflags & GUI_STYLE_BFLAGS_CR1)
      dst->b[i].cr = src->b[i].cr;
    if (bflags & GUI_STYLE_BFLAGS_CB1)
      dst->b[i].cb = src->b[i].cb;
    if (bflags & GUI_STYLE_BFLAGS_BT1)
      dst->b[i].bt = src->b[i].bt;
    if (bflags & GUI_STYLE_BFLAGS_BL1)
      dst->b[i].bl = src->b[i].bl;
    if (bflags & GUI_STYLE_BFLAGS_BR1)
      dst->b[i].br = src->b[i].br;
    if (bflags & GUI_STYLE_BFLAGS_BB1)
      dst->b[i].bb = src->b[i].bb;
    bflags >>= 8;
    bc = i+1;
  }
  dst->bc = max(bc,dst->bc);
}

/**
 * update an item's calculated style
 * (replace all properties in the base style with
 *  overriding properties from all applied styles)
 *
 * \param item gui_item* - item
 */
void GuiCalcStyle(gui_item *item) {
  gui_style *style;
  int i;

  item->cstyle = item->style;
  for (i=0;i<item->asc;i++) {
    style = item->astyles[i];
    GuiStyleUpdate(style, &item->cstyle);
  }
}

/**
 * apply a style to an item
 *
 * \param item gui_item* - item
 * \param style gui_style* - style
 */
void GuiApplyStyle(gui_item *item, gui_style *style) {
  int i;

  for (i=0;i<item->asc;i++) {
    if (item->astyles[i] == style) { return; }
  }
  item->astyles[item->asc++] = style;
  GuiCalcStyle(item);
}

/**
 * undo previous application of a style to an item
 *
 * \param item gui_item* - item
 * \param style gui_style* - style (to unapply)
 */
void GuiUnapplyStyle(gui_item *item, gui_style *style) {
  int i;

  for (i=0;i<item->asc;i++) {
    if (item->astyles[i] == style) { break; }
  }
  if (i==item->asc) { return; }
  item->astyles[i] = 0;
  for (;i<item->asc-1;i++) {
    item->astyles[i] = item->astyles[i+1];
  }
  --item->asc;
  GuiCalcStyle(item);
}

/**
 * (callback) calculate full content size for a text item
 *
 * \param item gui_item* - text item
 * \param size dim2 - unused
 * \return dim2 - content size
 */
dim2 GuiTextCS(gui_item *item, dim2 size) {
  gui_text *text;
  bound2 extents;
  dim2 cs;
  int flags;

  text = item->t;
  //if (text->invalid) {
  if (text->unparse)
    text->unparse(text->data, text->str);
    // else we have a label and the str is embedded directly
    // text->invalid = 0;
  //}
  size.w = 0;
  size.h = item->cstyle.th;
  extents = (*_gui_callbacks.text_extents)(item->t->str, &size, -1);
  cs.w = (extents.p2.x - extents.p1.x);
  cs.h = (extents.p2.y - extents.p1.y);
 // item->win.h = cs.h;
  return cs;
}

/**
 * (callback) calculate full content size for a list item
 *
 * \param item gui_item* - list item
 * \param size dim2 - unused
 * \return dim2 - content size
 */
dim2 GuiListCS(gui_item *item, dim2 size) {
  gui_item *it;

  size.w = 0;
  size.h = 0;
  for (it=item->child;it;it=it->next) {
    if (!(it->flags & GUI_FLAGS_CELL)) { continue; }
    if (item->flags & GUI_FLAGS_DIR) {
      size.h += it->crect.h;
      size.w = it->crect.w;
    }
    else {
      size.w += it->crect.w;
      size.h = it->crect.h;
    }
  }
  return size;
}

gui_item item_def = {
  .node = { 0 },
  .type = none,
  .flags = 0,
  .state = 0, .prev_state = 0,
  .rect = { 0,0,0,0 },
  .crect = { 0,0,0,0 },
  .invalid = 1,
  .style = style_def,
  .styles = styles_def,
  .handlers = handlers_def,
  .asc = 0,
  .astyles = { 0 },
  .cstyle = style_def,
  .d = 0
};

gui_list list_def = {
  .selected = 0
};

gui_text label_def = { 0 };
gui_text text_def = {
  .str = 0,
  .data = 0,
  .invalid = 1,
  .unparse = 0,
  .parse = 0,
  .maxlen = 64
};

// gui_draw draw_def = { 0 };
gui_slider slider_def = {
  .value = 0,
  .step = 1,
  .vmin = -0x80000000,
  .vmax = 0x7FFFFFFF,
  .ssize = 10
};

dim2 dim_def[5] = {
  { .w = 0, .h = 0 },
  { .w = 100, .h = 30 },
  { .w = 16, .h = 16 },
  { .w = 0, .h = 0 },
  { .w = 200, .h = 50 }
};

/**
 * allocate a gui item
 *
 * \return - item
 */
gui_item *GuiItemAlloc() {
  gui_item *item;

  item = (gui_item*)malloc(sizeof(gui_item));
  return item;
}

/**
 * free a previously allocated gui item
 *
 * \param item gui_item* - item
 */
void GuiItemFree(gui_item *item) {
  gui_item *it, *next;

  if (!item) { return; }
  if (item->d) { free(item->d); }
  for (it=item->child;it;it=next) {
    next=it->next;
    GuiItemFree(it);
  }
  free(item);
  item->d=0;
}

/**
 * create a new gui item
 *
 * \return item gui_item* - item
 */
gui_item *GuiItemNew() {
  gui_item *item;

  item = GuiItemAlloc();
  *item = item_def;
  item->self = item;
  return item;
}

int GuiItemIdx(gui_item *item) {
  tree_node_t *node, *parent;

  node = (tree_node_t*)item;
  parent = (tree_node_t*)item->parent;
  return tree_child_node_index(parent, node);
}

int GuiItemPath(gui_item *item, int *path) {
  gui_item *it;
  int i, len;

  if (!item) { return 0; }
  it=item;
  len=1;
  path[0] = 0;
  while (it=it->parent) { ++len; }
  for (it=item,i=0;it->parent;it=it->parent,i++) {
    path[len-i-1] = GuiItemIdx(it);
  }
  return len;
}

char path_str_buf[256];
char *GuiItemPathStr(gui_item *item) {
  char *c;
  int path[256];
  int i, len;

  len=GuiItemPath(item, path);
  c=path_str_buf;
  *(c++) = '(';
  for (i=0;i<len;i++) {
    c+=sprintf(c,"%i,",path[i]);
  }
  --c;
  *(c++) = ')';
  *(c++) = 0;
  return path_str_buf;
}

/**
 * add a child gui item to a [parent] gui item
 *
 * \param item gui_item* - item to add to
 * \param child gui_item* - [child] item to add
 */
void GuiAddChild(gui_item *item, gui_item *child) {
  tree_add_node((tree_node_t*)item, (tree_node_t*)child);
}

/**
 * remove a child gui item from a [parent] gui item
 *
 * \param item gui_item* - item to remove from
 * \param child gui_item* - [child] item to remove
 */
void GuiRemoveChild(gui_item *item, gui_item *child) {
  if (!item->child) { return; }
  tree_remove_node((tree_node_t*)child);
  child->parent = 0;
  child->next = 0;
}

/**
 * insert a child gui item into a [parent] gui item
 * after a previous child gui item
 *
 * \param item gui_item* - item to add to
 * \param prev gui_item* - [child] item to add after (or 0 if front insertion)
 * \param child gui_item* - [child] item to add
 */
void GuiInsertChild(gui_item *item, gui_item *prev, gui_item *child) {
  tree_insert_node((tree_node_t*)item, (tree_node_t*)prev,
                   (tree_node_t*)child);
}

/**
 * create a list
 *
 * \return gui_item* - list
 */
gui_item *GuiListNew() {
  gui_item *item;

  item = GuiItemNew();
  item->type = list;
  item->l = (gui_list*)malloc(sizeof(gui_list));
  *item->l = list_def;
  item->flags = 0;
  //item->style.fcs = GuiListCS;
  item->handlers.keyheld = GuiListKeyheld;
  item->handlers.keydown = GuiListKeydown;
  item->handlers.activate = GuiListActivate;
  item->handlers.deactivate = GuiListDeactivate;
  return item;
}

/**
 * create an editable text
 *
 * \param data void* - data or text string to bind
 * \param type gui_text_type - type of data (value or characters)
 * \return gui_item* - editable text
 */
gui_item *GuiTextNew(void *data, gui_text_type type) {
  gui_item *item;

  item = GuiItemNew();
  item->type = text;
  item->t = (gui_text*)malloc(sizeof(gui_text)+text_def.maxlen);
  *item->t = text_def;
  if (type == chars) {
    item->t->data = 0;
    strcpy(item->t->str, data);
  }
  else if (type == value)
    item->t->data = data;
  item->flags |= GUI_FLAGS_SCROLL;
  item->style.al.x = -1;
  item->style.al.y = -1; /* left-top aligned */
  item->style.th = 20; /* default text height */
  item->style.fcs = GuiTextCS; /* content size callback */
  item->handlers.keydown = GuiTextKeydown;
  item->handlers.keyheld = GuiTextKeyheld;
  item->handlers.activate = GuiTextActivate;
  item->handlers.deactivate = GuiTextDeactivate;
  return item;
}

/**
 * create a read-only text label
 *
 * \param str char* - text string for label
 * \return gui_item* - label
 */
gui_item *GuiLabelNew(char *str) {
  gui_item *item;

  item = GuiTextNew(str, chars);
  item->flags |= GUI_FLAGS_RO;
  item->flags &= ~GUI_FLAGS_SCROLL;
  item->style.fg = black;
  item->style.bg = lgrey;
  item->style.al.x = 0;
  item->style.al.y = 0; /* left-top aligned */
  item->style.bc = 1;   /* no outer border */
  item->handlers.keydown = 0;
  item->handlers.keyheld = 0;
  item->handlers.activate = 0;
  item->handlers.deactivate = 0;
  return item;
}

/**
 * create a button
 *
 * \param text char* - text string for button
 * \param onclick gui_handler_t - onclick handler
 * \return gui_item* - button
 */
gui_item *GuiButtonNew(char *text, gui_handler_t onclick) {
  gui_item *item;

  if (text)
    item = GuiLabelNew(text);
  else
    item = GuiItemNew();
  GuiStyleUpdate((gui_style* const)&style_outset, &item->style);
  item->styles.mousedown = (gui_style* const)&style_inset;
  item->handlers.mousedown = onclick;
  return item;
}

/*
gui_item *GuiDrawNew(int poly_count) {
  gui_item *item;

  item = GuiItemNew();
  item->flags |= GUI_FLAGS_RO;
  item->type = draw;
  item->d = (gui_draw*)malloc(sizeof(gui_draw)+(sizeof(poly3i)*poly_count));
  *item->d = draw_def;
  return item;
}
*/

/**
 * create a slider control
 *
 * \param value void* - pointer to value to bind
 * \return gui_item* - slider [control]
 */
gui_item *GuiSliderNew(void *value) {
  gui_item *item;

  item = GuiItemNew();
  item->type = slider;
  item->s = (gui_slider*)malloc(sizeof(gui_slider));
  *item->s = slider_def;
  item->s->value = value;
  item->style.cs.w = 0;
  item->style.cs.h = 0;
  item->handlers.mousedown = GuiSliderMousedown;
  item->handlers.mouseup = GuiSliderMouseup;
  item->handlers.mousedrag = GuiSliderMousedrag;
  return item;
}

/* compound items (items with children) */

/**
 * create a labeled item
 *
 * \param text char* - text string for label
 * \param child gui_item* - item described by label
 * \return gui_item* - labeled item
 */
gui_item *GuiLItemNew(char *text, gui_item *child) {
  gui_item *item, *label;

  item = GuiListNew();
  item->flags |= (GUI_FLAGS_RO);
  item->handlers.keydown = 0;
  item->handlers.activate = GuiLItemActivate;
  item->handlers.select = GuiLItemSelect;
  item->handlers.deselect = GuiLItemDeselect;
  label = GuiLabelNew(text);
  label->flags |= GUI_FLAGS_CELL;
  child->flags |= GUI_FLAGS_CELL;
  GuiAddChild(item, label);
  GuiAddChild(item, child);
  return item;
}

/**
 * create a scrollbar [control]
 *
 * \param value int32_t* - pointer to integer value to bind
 * \param vmin int32_t - minimum value for control
 * \param vmax int32_t - maximum value for control
 * \param flags int - flags; set/unset GUI_FLAGS_DIR controls scrollbar direction
 * \return gui_item* - scrollbar
 */
gui_item *GuiScrollNew(int32_t *value, int32_t vmin, int32_t vmax, int flags) {
  gui_item *item, *it;

  item = GuiListNew();
  item->flags |= GUI_FLAGS_RO; /* for lists this denotes 'inaccessible' for now; in future this should make all children RO */
  if (flags & GUI_FLAGS_DIR)
    item->flags |= GUI_FLAGS_DIR;
  item->style.bc = 0;
  it = GuiButtonNew("<", 0);
  it->flags |= GUI_FLAGS_CELL;
  if (flags & GUI_FLAGS_DIR)
    it->rect.h = -1;
  else
    it->rect.w = -1;
  it->handlers.mouseheld = GuiScrollLBtnHeld;
  GuiAddChild(item, it);
  it = GuiSliderNew(value);
  it->flags |= GUI_FLAGS_CELL;
  if (flags & GUI_FLAGS_DIR) {
    it->flags |= GUI_FLAGS_DIR;
    it->rect.h = -10;
  }
  else
    it->rect.w = -10;
  it->s->vmin = vmin;
  it->s->vmax = vmax;
  GuiAddChild(item, it);
  it = GuiButtonNew(">", 0);
  it->flags |= GUI_FLAGS_CELL;
  if (flags & GUI_FLAGS_DIR)
    it->rect.h = -1;
  else
    it->rect.w = -1;
  it->handlers.mouseheld = GuiScrollRBtnHeld;
  GuiAddChild(item, it);
  return item;
}

/**
 * create a [read-only] text node
 *
 * each text node item is a vertical list of 1 to 2 items
 * - the first item in the list is a text item which is the textual representation
 *   of the node item (determined by unparse)
 * - the second item in the list, if present, is a list of all corresponding
 *   text node items for child nodes (which are rendered below the node item's text)
 *
 * when this item is selected, the tree's 'selected' property is set to the
 * data bound to the first list item
 *
 * note that the GuiTextNodeAdd/Insert and GuiTextNodeRemove functions should be used
 * instead of GuiAddChild/GuiRemoveChild to insert/remove a node to/from a text tree
 *
 * \param data void* - data to bind to text item
 * \param unparse gui_data_unparse_t - function to convert bound data to displayable text
 */
gui_item *GuiTextNodeNew(void *data, gui_data_unparse_t unparse) {
  gui_item *item, *text, *sublist;
  // gui_item *label;

  item = GuiItemNew();
  item->type = node;
  item->n = tree_node_new();
  item->n->data = (void*)item;
  item->flags |= GUI_FLAGS_DIR | GUI_FLAGS_CELL | GUI_FLAGS_SCROLL | GUI_FLAGS_CH; /* vertical list */
  item->state |= GUI_FLAGS_EXPANDED;
  item->style.b[0] = border_trans;
  item->style.b[1] = border_trans;
  item->style.b[2] = border_trans;
  item->style.b[1].bl = 10;
  item->style.fcs = GuiListCS;
  item->handlers.mousedown = GuiTextNodeMousedown;
  item->handlers.select = GuiTextNodeSelect;
  item->handlers.deselect = GuiTextNodeDeselect;
  item->handlers.expand = GuiTextNodeExpand;
  item->handlers.collapse = GuiTextNodeCollapse;
  // label = (*map)(data);
  // label->flags |= GUI_FLAGS_CELL;
  // GuiAddChild(list, label);
  text = GuiTextNew(0, value);
  text->flags |= GUI_FLAGS_RO | GUI_FLAGS_CELL;
  text->flags &= ~GUI_FLAGS_SCROLL;
  text->style.fg = black;
  text->style.b[0] = border_trans;
  text->style.b[1] = border_trans;
  text->style.b[1].bl=10;
  text->style.b[2] = border_trans;
  text->t->data = data;
  text->t->unparse = unparse;
  text->t->parse = 0;
  text->style.th = 10;
  text->rect.h = 10;
  text->handlers.keydown = 0;
  text->handlers.keyheld = 0;
  text->handlers.activate = 0;
  text->handlers.deactivate = 0;
  GuiAddChild(item, text);
  sublist = GuiListNew();
  sublist->flags |= GUI_FLAGS_DIR | GUI_FLAGS_CELL | GUI_FLAGS_CH;
  sublist->style.b[0] = border_trans;
  sublist->style.b[1] = border_trans;
  sublist->style.b[2] = border_trans;
  sublist->style.fcs = GuiListCS;
  sublist->style.visible = 1;
  sublist->handlers.keydown = 0;
  sublist->handlers.keyheld = 0;
  sublist->handlers.activate = 0;
  sublist->handlers.deactivate = 0;
  GuiAddChild(item, sublist);
  return item;
}

/**
 * (callback) map a generic data node to a corresponding text node
 *
 * the _text_node_unparse global must be set prior to calling any
 * function which uses this callback; it will determine how the
 * data bound to the text node is unparsed
 *
 * \param data void* - pointer to node of generic data
 * \param c_mapped void** - pointer to zero terminated array of
 *                          mapped children
 * \return gui_item* - text node
 */
gui_data_unparse_t _text_node_unparse;
static gui_item *GuiTextNodeMap(void *data, void **c_mapped) {
  gui_item *node;
  int i;

  node = GuiTextNodeNew(data, _text_node_unparse);
  if (c_mapped) {
    for (i=0;c_mapped[i];i++) {
      GuiTextNodeAdd(node, c_mapped[i]);
    }
  }
  return node;
}

/**
 * get the generic data bound to [the text item of] a text node
 *
 * \param item gui_item* - text node
 * \return void* - pointer to [node of] generic data
 */
static inline void *GuiTextNodeUnmap(gui_item *item) {
  return item->child->t->data;
}

/**
 * create a text tree
 *
 * bind 'data' to a tree of text items;
 * said tree is reached by recursively deconstructing data with 'child_fn'
 * and converting each node to text via 'unparse' and then to text items,
 * at each update
 *
 * tree changes are tracked so that only the necessary operations are performed
 * per update to add/remove/move/swap text items
 *
 * \param void* data - data to bind
 * \param char*(*)(void*) unparse - function to convert descendant object to
 *                                  [text] string
 * \param void**(*)(void*) child_fn - function to extract child objects from
 *                                    data and descendants
 * \param gui_handler_t select - select handler to use for each node
 * \returns gui_item* - text tree
 */
gui_item *GuiTextTreeNew(void *data, gui_data_unparse_t unparse,
                         void **(*child_fn)(void*), gui_handler_t select) {
  tree_node_t *cur, *n;
  gui_tree *tr;
  gui_item *root;

  cur = data;
  if (data && child_fn) { /* data is not already tree_node_t? */
    /* deconstruct the singular, all-encompassing, generic 'data' object
       into a tree of separate data objects */
    cur = tree_map_from(data, child_fn, GuiId);
  }
  /* convert the tree representation into the initial tree of gui text items */
  _text_node_unparse = unparse;
  root = tree_map_to(cur, (void *(*)(void*, void**))GuiTextNodeMap);
  _text_node_unparse = 0;
  /* convert the root item from node type to tree type
     as the root gui_item must be tree type for this to constitute a tree
     reallocate the item's gui_node data as a gui_tree to store information about the tree
     (we can still access the gui_node data thru the union, as gui_tree begins with the gui_node data) */
  root->type = tree;
  tr = root->tr = realloc(root->tr, sizeof(gui_tree));
  for_each_child_node(root->n, n) { n->parent = root->n; }
  /* store pointer to the bound data and initial tree representation
     of that data, as well as the unparse and child_fn callbacks supplied */
  tr->data = data;
  tr->unparse = unparse;
  tr->child_fn = child_fn;
  tr->select = select;
  tr->cur = cur;
  tr->prev = cur ? tree_copy(cur) : 0;
  tr->selected = 0;
  tr->sel = 0;
  root->handlers.update = GuiTextTreeUpdate;
  root->handlers.keydown = GuiTextTreeKeydown;
  root->handlers.keyheld = GuiTextTreeKeyheld;
  GuiSetBorderSize(root,1,0);
  return root;
}

/**
 * calculate the sums of sizes of borders 0 to c
 * for given item and style
 *
 * \param item gui_item* - item
 * \param style gui_style* - style (with borders)
 * \param c int - border index upper bound, or -1 for all borders
 * \return gui_border - border size (sum)
 */
static gui_border GuiBorderSum(gui_item *item, gui_style *style, int c) {
  gui_border b;
  int i;

  if (c==-1) { c = style->bc; }
  else { c = min(c+1,style->bc); }
  b = border_trans;
  for (i=0;i<c;i++) {
    b.bt+=style->b[i].bt;
    b.bl+=style->b[i].bl;
    b.br+=style->b[i].br;
    b.bb+=style->b[i].bb;
  }
  return b;
}

/**
 * calculate location, dimensions, and color for one of an item's
 * visible constituent rects
 *
 * \param item gui_item* - item
 * \param type int - 0 = outer rect, 1 = inner rect, 2 = top border,
 *                   3 = left border, 4 = right border, 5 = bot border
 * \param idx int - border index (for type 2-5 only)
 * \param rect gui_rect - output rect (with loc, dim, and color)
 * \return int - success code
 */
int GuiCommonRect(gui_item *item, int type, int idx, gui_rect *rect) {
  gui_style *style;
  gui_border border;
  rect2 item_rect;
  int res;

  if (!item)
    return 0;
  res = 1;
  if (item->crect.w == 0 || item->crect.h == 0)
    res = 0;
  style = &item->cstyle;
  rect->rect2 = item->crect;
  if (type == 0) { /* outer rect (ac color) */
    rect->rgba8 = style->ac;
  }
  else if (type == 1) { /* inner rect (bg color) */
    border = GuiBorderSum(item, style, -1);
    rect->x += border.bl;
    rect->y += border.bt;
    rect->w -= (border.bl + border.br);
    rect->h -= (border.bt + border.bb);
    if (style->fbg)
      rect->rgba8 = (*style->fbg)(item);
    else
      rect->rgba8 = style->bg;
  }
  else if (type == 2) { /* top border (ct color) */
    if (idx > 0) {
      border = GuiBorderSum(item, style, idx-1);
      rect->x += border.bl;
      rect->y += border.bt;
      rect->w -= (border.bl + border.br);
    }
    rect->h = style->b[idx].bt;
    rect->rgba8 = style->b[idx].ct;
  }
  else if (type == 3) { /* left border (cl color) */
    if (idx > 0) {
      border = GuiBorderSum(item, style, idx-1);
      rect->x += border.bl;
      rect->y += border.bt;
      rect->h -= (border.bt + border.bb);
    }
    rect->w = style->b[idx].bl;
    rect->rgba8 = style->b[idx].cl;
  }
  else if (type == 4) { /* right border (cr color) */
    rect->x = item->crect.x + item->crect.w;
    if (idx > 0) {
      border = GuiBorderSum(item, style, idx-1);
      rect->x -= border.br;
      rect->y += border.bt;
      rect->h -= (border.bt + border.bb);
    }
    rect->x -= style->b[idx].br;
    rect->w = style->b[idx].br;
    rect->rgba8 = style->b[idx].cr;
  }
  else if (type == 5) { /* bottom border (cb color) */
    rect->y = item->crect.y + item->crect.h;
    if (idx > 0) {
      border = GuiBorderSum(item, style, idx-1);
      rect->x += border.bl;
      rect->y -= border.bb;
      rect->w -= (border.bl + border.br);
    }
    rect->y -= style->b[idx].bb;
    rect->h = style->b[idx].bb;
    rect->rgba8 = style->b[idx].cb;
  }
  rect->w = limit(((int)rect->w),0,0x7FFFFFFF);
  rect->h = limit(((int)rect->h),0,0x7FFFFFFF);
  /*if (rect->x >= item->crect.x && rect->x <= item->crect.x + (int32_t)item->crect.w &&
      rect->y >= item->crect.y && rect->y <= item->crect.y + (int32_t)item->crect.h &&
      rect->x + (int32_t)rect->w >= item->crect.x && rect->x + (int32_t)rect->w <= item->crect.x + (int32_t)item->crect.w &&
      rect->y + (int32_t)rect->h >= item->crect.x && rect->y + (int32_t)rect->h <= item->crect.y + (int32_t)item->crect.h)
    return 1;
  return 0;*/
  return res;
}

/**
* rect calculation
*
* - if the item is a list, we make sure to first calculate rects for all child cells
* 1) absolute positioned: if GUI_FLAGS_ABS is set,
*                         then crect.loc is set to rect.loc
* 2) else, relative positioned, to the parent inner rect:
*   a) parent inner rect is calculated
*   b) location of parent inner rect is adjusted by the scroll amount 'parent->win.loc'
*   c) rect loc and dim are determined:
*     1) if item is a cell (GUI_FLAGS_CELL), then we do the steps for cell rect calculation,
*        relative to the parent inner rect (used in step vii)
*     2) else the item is a noncell
*        a) the item location is simply the item rect location plus the parent
*           inner rect location
*        b) for item dimensions, there are several cases. see non-cell rect dimensions:
*           1) the dimensions are dependent on the parent inner rect dimensions
*           2) the dimensions are dependent on the sum of child outer rect dimensions
*           3) the dimensions are directly specified in the item rect
*
*
* rect dimensions
*
* 2) dimensions are dependent on sum of child outer rect dimensions
*  - in this case all child rects must be calculated before the rect dimensions themselves
*    can be calculated
*    thus, the rect calculation flow is different for these and perhaps some grandchildren
*    and etc.
*    in particular
*
* cell rect calculation
*
* i) the number of sibling cells, including the cell itself, is determined
* ii) the number of sibling cells, including the cell itself, which have specified
*     a [proportional] height or width value, and the number that have not,
*     are determined
* iii) the sum of the sibling proportional width or height values is computed
* iv) if the parent is horizontal (no GUI_FLAGS_DIR) then the width is divided
*     proportionally amongst cells that have specified a width value and cells
*     that have not; else if the parent is vertical then the height is divided
* v) the portion of width/height which is assigned to the cells with specified
*    width/height value is divided according to those width/height proportions;
*    the portion of width/height assigned to the other cells is divided equally
*    amongst those cells
* vi) if the calculated parent s
* vi) if the calculated parent style specifies a 'max cells per visible frame'
*     count value, then the number of sibling cells determined is limited to
*     this value for the calculations; effectively this causes n cells at most to fit
*     evenly inside the parent inner rect, rather than exactly the number of
*     child cells, and the rest to overflow
*     * this is case 1 where the total width or heights of child cells (i.e.
*       content) can be made to exceed the parent inner rect size
* vii) if any cell specifies that its width or height is to be the sum of child
*      widths or heights, then that sum is chosen over the divided width/height;
*      * this is case 2 where total width or heights of child cells can be made
*        to exceed parent inner rect size
* viii) iteration starts with the first sibling cell, at the position of the parent
*       inner rect, and increasingly adds width (for horizontal parent) or height
*       (for vertical parent) of cells, which were computed according to the
*       proportions in step v, until the current item (cell) is reached, to get
*       the location
* ix)  the height and width are the ones computed for the cell in step v
*/

static int GuiCalcRects(gui_item *item);

/**
 * this is the replacement for GuiCalcRects when flags are GUI_FLAGS_CW or GUI_FLAGS_CH
 *
 * the behavior is somewhat like GuiCalcCellRect being called for all child items/cells
 */
static int32_t GuiCalcChDimSum(gui_item *item, int flags) {
  gui_item *it;
  gui_border border;
  dim2 dim;
  int cur, dir, s;

  dir = !!(item->flags & GUI_FLAGS_DIR);
  cur = 0;
  for (it=item->child;it;it=it->next) {
    if (!(it->flags & GUI_FLAGS_CELL)) { continue; }
    if (it->cstyle.visible == -1) { continue; }
    if (flags & GUI_FLAGS_CW) {
      if (it->flags & GUI_FLAGS_CW)
        s = GuiCalcChDimSum(it, GUI_FLAGS_CW);
      else {
        s = it->rect.w;
      }
    }
    else if (flags & GUI_FLAGS_CH) {
      if (it->flags & GUI_FLAGS_CH)
        s = GuiCalcChDimSum(it, GUI_FLAGS_CH);
      else {
        s = it->rect.h;
      }
    }
    if (((flags & GUI_FLAGS_CW) && !dir) ||
        ((flags & GUI_FLAGS_CH) && dir))
      cur += s;
    else
      cur = max(cur, s);
  }
  border = GuiBorderSum(item, &item->cstyle, -1);
  if (flags & GUI_FLAGS_CH) {
    cur += border.bt + border.bb;
  }
  else if (flags & GUI_FLAGS_CW) {
    cur += border.bl + border.br;
  }
  return cur;
}


/* rect calc rules:

- a) if the item is a cell (GUI_FLAGS_CELL), then it is only necessary to recursively call for each child
     item; the item's rect will have already been calculated in the call for the parent item

- 1) calculate rect location
  - if GUI_FLAGS_ABS then it is directly that of the item
  - otherwise it is the item location relative to the inner rect of the parent
- 2) calculate rect dimensions
  - if the item width is 0
    - if GUI_FLAGS_CW then the rect width is the sum of calculated widths of all child item (outer) rects,
      if those rects can be calculated
    - else the rect width is the width of the parent rect, if it is not dependent on calculated child rects;
      else return 0
  - else the rect width is directly that of the item
  - if the item height is 0
    - if GUI_FLAGS_CH then the rect height is the sum of calculated heights of all child item (outer) rects,
      if those rects can be calculated
    - else the rect height is the height of the parent rect, if it is not dependent on calculated child rects;
      else return 0
  - else the rect height is directly that of the item
 */


/**
 * calculate the rect for a normal (non-cell) item
 *
 *  1) calculates absolute location for GUI_FLAGS_ABS
 *     calculates relative location to parent inner rect for
 *     GUI_FLAGS_ABS not set
 *  2) calculates dimensions (width and/or height) as
 *    a) the sum of children dimensions if GUI_FLAGS_CW/CH
 *    b) the content dimensions if GUI_FLAGS_FIT
 *    c) the constant item dimensions if present (or there is
 *       no parent)
 *    d) otherwise, the parent dimensions
 *
 * \param item gui_item* - item
 * \param prect gui_rect* - parent inner rect, adjusted to window
 * \return int - success code
 */
static int GuiCalcRect(gui_item *item, gui_rect *prect) {
  rect2 prev;
  dim2 cs;

  prev = item->crect;
  if (item->cstyle.fcs)
    cs = (*item->cstyle.fcs)(item, item->cstyle.cs);
  else
    cs = item->cstyle.cs;
  if (item->flags & GUI_FLAGS_ABS) {
    item->crect.x = item->rect.x;
    item->crect.y = item->rect.y;
  }
  else {
    item->crect.x = prect->x + item->rect.x;
    item->crect.y = prect->y + item->rect.y;
  }
  item->crect.w = 0;
  if (item->flags & GUI_FLAGS_CW) {}
  else if (item->flags & GUI_FLAGS_FIT)
    item->crect.w = cs.w;
  else if (!item->parent || item->rect.w)
    item->crect.w = item->rect.w;
  else
    item->crect.w = prect->w;
  item->crect.h = 0;
  if (item->flags & GUI_FLAGS_CH) {}
  else if (item->flags & GUI_FLAGS_FIT)
    item->crect.h = cs.h;
  else if (!item->parent || item->rect.h)
    item->crect.h = item->rect.h;
  else
    item->crect.h = prect->h;
  if (item->flags & GUI_FLAGS_CW)
    item->crect.w = GuiCalcChDimSum(item, GUI_FLAGS_CW);
  if (item->flags & GUI_FLAGS_CH)
    item->crect.h = GuiCalcChDimSum(item, GUI_FLAGS_CH);
  return 1;
}

/**
 * calculate the rect for a cell item
 *
 * 1) the number of sibling cells, including the cell itself, is determined
 * 2) the number of sibling cells, including the cell itself, which have specified
 *    a [proportional] height or width value, and the number that have not,
 *    are determined
 * 3) if the parent is horizontal then the width is divided proportionally
 *    amongst cells that have specified a width value and cells that have not;
 *    else if the parent is vertical then the height is divided
 *    amongst cells that have specified a height value and those that have not
 * 4) the portion of width/height which is assigned to the cells with specified
 *    width/height value is divided according to those width/height proportions;
 *    the portion of width/height assigned to the other cells is divided equally
 *    amongst those cells
 * 5) if the calculated parent style specifies a 'max cells per visible frame'
 *    count value, then the number of sibling cells determined is limited to
 *    this value for the calculations; effectively this causes n cells at most to fit
 *    evenly inside the parent inner rect, rather than exactly the number of
 *    child cells, and the rest to overflow
 * 6) if any cell specifies that its width or height is to be the sum of child
 *    widths or heights, then that sum is chosen over the divided width/height
 * 7) iteration starts with the first sibling cell, at the position of the parent
 *    inner rect, and increasingly adds width (for horizontal parent) or height
 *    (for vertical parent) of cells, which were computed according to the
 *    proportions in step 5, until the current item (cell) is reached, to get
 *    the location
 */
static int GuiCalcCellRect(gui_item *item, gui_rect *prect) {
  gui_item *it;
  rect2 prev;
  int c1, c2;
  int sum, s1, s2, s;
  int wrem, hrem;
  int cur, dir;
  char path[64];
  dim2 dim;

  prev = item->crect;
  sum = 0; s = 0; c1 = 0; c2 = 0;
  dir = !!(item->parent->flags & GUI_FLAGS_DIR);
  for (it=item->parent->child;it;it=it->next) {
    if (!(it->flags & GUI_FLAGS_CELL)) { continue; }
    if (it->cstyle.visible == -1) { continue; }
    if (!dir) {
      if ((int)it->rect.w > 0) { continue; }
      if (it->rect.w == 0)
        ++c1;
      else
        sum += -(int)it->rect.w;
    }
    else {
      if ((int)it->rect.h > 0) { continue; }
      if (it->rect.h == 0)
        ++c1;
      else
        sum += -(int)it->rect.h;
    }
    ++c2;
  }
  c2=c2?c2:1;
  if (!dir) {
    if (item->parent->cstyle.mc.w)
      c2 = min(c2,item->parent->cstyle.mc.w);
    s1 = (prect->w * c1)/c2;
    s2 = (prect->w * (c2-c1))/c2;
  }
  else {
    if (item->parent->cstyle.mc.h)
      c2 = min(c2,item->parent->cstyle.mc.h);
    s1 = (prect->h * c1)/c2;
    s2 = (prect->h * (c2-c1))/c2;
  }
  if (!dir) cur = prect->x;
  else      cur = prect->y;
  for (it=item->parent->child;it;it=it->next) {
    if (!(it->flags & GUI_FLAGS_CELL)) { continue; }
    if (it->cstyle.visible == -1) { continue; }
    if (!dir) {
      if (it->flags & GUI_FLAGS_CW)
        s = GuiCalcChDimSum(it, GUI_FLAGS_CW);
      else if ((int)it->rect.w > 0)
        s = it->rect.w;
      else if (it->rect.w == 0)
        s = s1 / c1;
      else
        s = (s2*-(int)it->rect.w)/sum;
      /*
      else if (item->parent->cstyle.lcs.w)
        s = item->parent->cstyle.lcs.w;
      */
    }
    else {
      if (it->flags & GUI_FLAGS_CH)
        s = GuiCalcChDimSum(it, GUI_FLAGS_CH);
      else if ((int)it->rect.h > 0)
        s = it->rect.h;
      else if (it->rect.h == 0)
        s = s1/c1;
      else
        s = (s2*-(int)it->rect.h)/sum;
      /*
      else if (item->parent->cstyle.lcs.h)
        s = item->parent->cstyle.lcs.h;
      */
    }
    if (it!=item) {
      cur += s;
      continue;
    }
    if (!dir) {
      item->crect.w = s;
      item->crect.h = prect->h;
      item->crect.x = cur;
      item->crect.y = prect->y;
    }
    else {
      item->crect.w = prect->w;
      item->crect.h = s;
      item->crect.x = prect->x;
      item->crect.y = cur;
    }
    return 1;
  }
}

static int GuiCalcRects(gui_item *item) {
  gui_item *it;
  gui_rect rect;
  dim2 cs;
  int flag, flags, res;

  //if (!item->invalid) { return 1; }
  flags = item->flags;
  if (item->parent) {
    /* if any child item causes total dimensions of items
       to fall below the parent inner crect size, then padding will try to compensate
       making the inner crect smaller, and this will cause total dimensions to
       fall even lower in a cascading effect.
    */
    res = GuiCommonRect(item->parent, 1, 0, &rect);
    if (!(flags & GUI_FLAGS_FIXED)) {
      rect.x -= item->parent->win.x;
      rect.y -= item->parent->win.y;
    }
    if (flags & GUI_FLAGS_CELL)
      GuiCalcCellRect(item, &rect);
    else
      GuiCalcRect(item, &rect);
    if (!res) {
      item->invalid = 1;
      return 0;
    }
  }
  else
    GuiCalcRect(item, 0);
  //if (!item->invalid) { return 1; }
  if (item->type == list) {
    for (it=item->child;it;it=it->next) {
      GuiCalcRects(it);
    }
  }
  else if (item->type == text && item->child) {
    GuiCalcRects(item->child);
  }
  /*
  flags = item->flags;
  if (item->parent && !(flags & GUI_FLAGS_FIXED)) {
    rect.x -= item->parent->win.x;
    rect.y -= item->parent->win.y;
  }
  if (flags & GUI_FLAGS_CELL)
    GuiCalcCellRect(item, &rect, 0);
  else
    GuiCalcRect(item, &rect, 0);
  */
  /*
  if (flags & GUI_FLAGS_CLIP) {
    if (item->crect.x < rect.x) { item->crect.x = rect.x; }
    if (item->crect.x + (int32_t)item->crect.w > rect.x + (int32_t)rect.w)
      item->crect.w = rect.w;
    if (item->crect.y < rect.y) { item->crect.y = rect.y; }
    if (item->crect.x + (int32_t)item->crect.h > rect.y + (int32_t)rect.h)
      item->crect.h = rect.h;
  }
  */
  item->invalid = 0;
  return 1;
}

#define GuiHScroll(item) \
({ gui_item *it; \
   GuiFindChild(item, it, \
    it->type == list \
 && it->child && it->child->next \
 && it->child->next->type == slider \
 && !(it->flags & GUI_FLAGS_DIR)); \
   it; })

#define GuiVScroll(item) \
({ gui_item *it; \
   GuiFindChild(item, it, \
    it->type == list \
 && it->child && it->child->next \
 && it->child->next->type == slider \
 && (it->flags & GUI_FLAGS_DIR)); \
   it; })

static int GuiCalcScroll(gui_item *item, int flags) {
  gui_item *it;
  gui_rect rect;
  gui_style *style;
  gui_border *b, border;
  dim2 dim, cs;
  int horiz, vert;

  style = &item->cstyle;
  if (style->fcs) /* content size is a function of the item? */
    cs = style->fcs(item, style->cs);
  else
    cs = style->cs;
  b = &style->b[style->bc-1]; /* last border is padding */
  horiz = !!(flags & 1);
  vert = !!(flags & 2);
  dim = item->crect.dim;
  border = GuiBorderSum(item, style, -1);
  if (horiz || vert) {
    /* content is too large
       so clip the content window */
    GuiCommonRect(item, 1, 0, &rect);
    item->win.w = min(cs.w, rect.w);
    item->win.h = min(cs.h, rect.h);
    /* create a scrollbar when
       there are at least 16 pixels of bottom/right padding in which to fit it
       or the content window plus padding has at least 24 pixels such that
       16 - padding can be stolen to leave >= 8 for viewing */
    horiz = horiz && (b->bb >= 16 || (b->bb+item->win.h >= 24));
    vert = vert && (b->br >= 16 || (b->br+item->win.w >= 24));
    /* if there are not at least 16 pixels of padding
       then we have to steal from the content window */
    if (horiz && b->bb < 16 && b->bb+item->win.h >= 24)
      item->win.h -= (16-b->bb);
    if (vert && b->br < 16 && b->br+item->win.w >= 24)
      item->win.w -= (16-b->br);
    /* calculate the adjusted content window */
    if (horiz) {
      GuiFindChild(item, it,
        it->type == list
       && it->child && it->child->next
       && it->child->next->type == slider
       && !(it->flags & GUI_FLAGS_DIR));
      if (!it) {
        if (item->type == list || item->type == tree) {
          if (item->cstyle.bc != 4) {
            item->style.b[3] = item->style.b[item->style.bc-1];
            item->style.bc = 4;
          }
          item->style.b[2].bt = 0;
          item->style.b[2].bb = 16;
          item->style.b[2].bl = 0;
          item->style.b[2].br = 0;
          b = &item->cstyle.b[3];
        }
        it = GuiScrollNew(&item->win.x, 0, cs.w-item->win.w, 0);
        it->flags |= GUI_FLAGS_FIXED; /* do not scroll the scrollbar itself */
        it->rect.w = rect.w - (horiz&&vert?16:0);
        it->rect.h = 16;
        it->rect.x = 0;
        it->rect.y = rect.h+b->bb-16;
        GuiAddChild(item, it);
      }
      else {
        it->child->next->s->vmin = 0;
        it->child->next->s->vmax = cs.w-item->win.w;
      }
    }
    if (vert) {
      GuiFindChild(item, it,
        it->type == list
       && it->child && it->child->next
       && it->child->next->type == slider
       && (it->flags & GUI_FLAGS_DIR));
      if (!it) {
        if (item->type == list || item->type == tree) {
          if (item->style.bc != 4) {
            item->style.b[3] = item->style.b[item->style.bc-1];
            item->style.bc = 4;
          }
          item->style.b[2].bt = 0;
          if (!horiz) { item->style.b[2].bb = 0; }
          item->style.b[2].bl = 0;
          item->style.b[2].br = 16;
          b = &item->style.b[3];
        }
        it = GuiScrollNew(&item->win.y, 0, cs.h-item->win.h, GUI_FLAGS_DIR);
        it->flags |= GUI_FLAGS_FIXED;
        it->rect.w = 16;
        it->rect.h = rect.h - (horiz&&vert?16:0);
        it->rect.x = rect.w+b->br-16;
        it->rect.y = 0;
        GuiAddChild(item, it);
      }
      else {
        it->child->next->s->vmin = 0;
        it->child->next->s->vmax = cs.h-item->win.h;
      }
    }
  }
  else {
    /* ensure that child scrollbars do not exist */
     GuiFindChild(item, it,
        it->type == list
       && it->child && it->child->next
       && it->child->next->type == slider
       && !(it->flags & GUI_FLAGS_DIR));
    if (it) {
      GuiRemoveChild(item, it);
      GuiItemFree(it);
      item->win.x = 0;
    }
    it = 0;
    GuiFindChild(item, it,
        it->type == list
       && it->child && it->child->next
       && it->child->next->type == slider
       && (it->flags & GUI_FLAGS_DIR));
    if (it) {
      GuiRemoveChild(item, it);
      GuiItemFree(it);
      item->win.y = 0;
    }
  }
}

/*
  typically border size is constant and does not depend on anything else
  the inner 'content' rect loc/dim for an item depends on border sizes
  and child item sizes
  however sometimes the width and/or height of the inner rect will depend
  on content
  in which case the padding border must change to align content according
  to the al property
*/
static int GuiCalcPad(gui_item *item) {
  gui_item *it, *scroll;
  gui_border *b, border;
  gui_style *style;
  dim2 dim, cs;
  int horiz, vert;

  style = &item->cstyle;
  if (style->fcs) /* content size is a function of the item? */
    cs = style->fcs(item, style->cs);
  else
    cs = style->cs;
  border = GuiBorderSum(item, style, -1);
  dim = item->crect.dim;
  dim.w -= (border.bl + border.br);
  dim.h -= (border.bt + border.bb);
  item->win.dim.w = cs.w ? min(cs.w,dim.w) : dim.w;
  item->win.dim.h = cs.h ? min(cs.h,dim.h) : dim.h;
  if (style->bc == 0) { /* if borderless then nothing to fit to */
    return 1;
  }
  if (cs.w == 0 && cs.h == 0) {
    /* default to no padding adjustments when no content size specified */
    return 1;
  }
  dim = item->crect.dim;
  if (style->bc > 1) {
    border = GuiBorderSum(item, style, style->bc-2);
    dim.w -= (border.br + border.bl);
    dim.h -= (border.bt + border.bb);
  }
  b = &style->b[style->bc-1]; /* last border is padding */
  if (style->al.x == -1) {
    b->bl = 0;
    b->br = ((int32_t)dim.w - (int32_t)cs.w);
  }
  else if (style->al.x == 0) {
    b->bl = (((int32_t)dim.w - (int32_t)cs.w) / 2)
          + (((int32_t)dim.w - (int32_t)cs.w) % 2);
    b->br = ((int32_t)dim.w - (int32_t)cs.w) / 2;
  }
  else if (style->al.x == 1) {
    b->bl = ((int32_t)dim.w - (int32_t)cs.w);
    b->br = 0;
  }
  if (style->al.y == -1) {
    b->bt = 0;
    b->bb = ((int32_t)dim.h - (int32_t)cs.h);
  }
  else if (style->al.y == 0) {
    b->bt = (((int32_t)dim.h - (int32_t)cs.h) / 2)
          + (((int32_t)dim.h - (int32_t)cs.h) % 2);
    b->bb = ((int32_t)dim.h - (int32_t)cs.h) / 2;
  }
  else if (style->al.y == 1) {
    b->bt = ((int32_t)dim.h - (int32_t)cs.h);
    b->bb = 0;
  }
  /* at this point the padding size will be negative
     if content is too large to fit, i.e. goes past margin/border
  */
  horiz = (b->bl < 0 || b->br < 0) && dim.h >= 20;
  vert = (b->bt < 0 || b->bb < 0) && dim.w >= 20;
  b->bt = max(0,b->bt);
  b->bl = max(0,b->bl);
  b->br = max(0,b->br);
  b->bb = max(0,b->bb);
  if (item->flags & GUI_FLAGS_SCROLL)
    GuiCalcScroll(item, horiz | (vert<<1));
  return 1;
}

int GuiClipRect(gui_item *item, gui_rect *rect) {
  gui_rect prect;
  bound2 bound, pbound;

  while (item->parent && (item->flags & GUI_FLAGS_NOCLIP)) { item = item->parent; }
  if (!item->parent) { return 1; }
  GuiCommonRect(item->parent, 1, 0, &prect);
  Rect2ToBound2(bound, (*rect));
  Rect2ToBound2(pbound, prect);
  if (bound.p1.x < pbound.p1.x && bound.p2.x < pbound.p1.x) { return 0; }
  if (bound.p1.x > pbound.p2.x && bound.p2.x > pbound.p2.x) { return 0; }
  if (bound.p1.y < pbound.p1.y && bound.p2.y < pbound.p1.y) { return 0; }
  if (bound.p1.y > pbound.p2.y && bound.p2.y > pbound.p2.y) { return 0; }
  bound.p1.x = max(bound.p1.x, pbound.p1.x);
  bound.p2.x = min(bound.p2.x, pbound.p2.x);
  bound.p1.y = max(bound.p1.y, pbound.p1.y);
  bound.p2.y = min(bound.p2.y, pbound.p2.y);
  Bound2ToRect2((*rect), bound);
  return 1;
}

static void GuiCommonDraw(gui_item *item) {
  gui_rect rect;
  poly4i *prim, *tail, *next;
  int i,ii;

  for (i=1;i<6;i++) {
    for (ii=0;ii<((i>=2)?item->style.bc:1);ii++) {
      GuiCommonRect(item, i, ii, &rect);
      if (!GuiClipRect(item, &rect))
        continue;
      (*_gui_callbacks.draw_rect)(&rect);
    }
  }
}

static void GuiTextDraw(gui_item *item) {
  gui_text *txt;
  gui_rect rect, clip;
  rgba8 fg;

  txt = item->t;
  // if (txt->invalid) {
    if (txt->unparse)
      txt->unparse(txt->data, txt->str);
    // else we have a label and the str is embedded directly
    GuiCalcPad(item);
    txt->invalid = 0;
  // }
  if (!item->parent || item->parent->type != text)
    GuiCommonDraw(item);
  GuiCommonRect(item, 1, 0, &rect);
  clip = rect;
  if (!GuiClipRect(item, &clip)) { return; }
  rect.x -= item->win.x;
  rect.y -= item->win.y;
  if (item->cstyle.ffg)
    fg = (*item->cstyle.ffg)(item);
  else
    fg = item->cstyle.fg;
  (*_gui_callbacks.draw_text)(txt->str, &rect.rect2, &clip.rect2, &fg);
  // GLPrint(txt->str, rect.rect2, 1, (rgb8*)&fg, &clip.rect2);
}

static void GuiSliderDraw(gui_item *item) {
  gui_slider *slider;
  gui_border *b, border;
  gui_style *style;
  dim2 dim;
  int32_t pm, px;
  int value, len;

  /* manually calculate padding based on vmin, vmax, value, and ssize */
  style = &item->cstyle;
  slider = item->s;
  value = *((int*)slider->value);
  dim = item->crect.dim;
  if (style->bc > 1) {
    border = GuiBorderSum(item, style, style->bc-2);
    dim.w -= (border.br + border.bl);
    dim.h -= (border.bt + border.bb);
  }
  if ((int)dim.w <= 0 || (int)dim.h <= 0) { return; } /* cannot draw */
  b = &style->b[style->bc-1]; /* last border is padding */
  len = slider->vmax - slider->vmin;
  pm = value - slider->vmin;
  px = slider->vmax - value;
  if (item->flags & GUI_FLAGS_DIR) { /* vertical slider? */
    b->bt = ((int32_t)dim.h * pm * (len-slider->ssize))/(len*len);
    b->bb = ((int32_t)dim.h * px * (len-slider->ssize))/(len*len);
  }
  else {
    b->bl = ((int32_t)dim.w * pm * (len-slider->ssize))/(len*len);
    b->br = ((int32_t)dim.w * px * (len-slider->ssize))/(len*len);
  }
  GuiCommonDraw(item);
}

static void GuiListDraw(gui_item *item) {
  gui_list *list;
  gui_item *it;

  GuiCommonDraw(item);
}

void GuiItemDraw(gui_item *item) {
  gui_item *it;
  gui_rect rect;
  char *path_str;

  GuiCalcStyle(item);
  if (!(item->style.visible == 1)) { return; }
  GuiCalcRects(item);
  GuiCalcPad(item);
  /* trivial reject */
  GuiCommonRect(item, 1, 0, &rect);
  if (!GuiClipRect(item, &rect)) { return; }
  //path_str = GuiItemPathStr(item);
  //printf("drawing item %s\n", path_str);
  //printf("crect: (%i, %i) - (%i, %i)\n", item->crect.x, item->crect.y, item->crect.w, item->crect.h);
  switch (item->type) {
  case none:
    GuiCommonDraw(item);
    break;
  case text:
    GuiTextDraw(item);
    break;
  case slider:
    GuiSliderDraw(item);
    break;
  case list:
  case node:
  case tree:
    GuiListDraw(item);
    break;
  }
  for (it=item->child;it;it=it->next) {
    GuiItemDraw(it);
  }
}

void GuiItemInvalidate(gui_item *item) {
  gui_item *it;

  item->invalid = 1;
  for (it=item->child;it;it=it->next) {
    GuiItemInvalidate(it);
  }
}

void GuiCommonMousedown(gui_item *item, gui_input *input) {
  if (item->type == list && item->child
   && item->child->next && item->child->next->type == slider)
    return;
  if (item->type == text && !(item->flags & GUI_FLAGS_RO)) {
    item->state |= GUI_FLAGS_ACTIVE;
  }
}

void GuiCommonSelectR(gui_item *item, gui_item *root) {
  gui_item *it;
  if ((root!=item) && (root->state & GUI_FLAGS_SELECTED))
    root->state &= ~GUI_FLAGS_SELECTED;
  for (it=root->child;it;it=it->next)
    GuiCommonSelectR(item, it);
}

void GuiCommonSelect(gui_item *item, gui_input *input) {
  gui_item *it;
  bound2 bound, pbound;

  it = item;
  Rect2ToBound2(bound, item->crect);
  while (it->parent) {
    if (!(it->parent->flags & GUI_FLAGS_SCROLL)) { return; }
    Rect2ToBound2(pbound, it->parent->crect);
    if (bound.p1.x < pbound.p1.x) {
      it->parent->win.x -= (pbound.p1.x - bound.p1.x);
    }
    else if (bound.p2.x > pbound.p2.x) {
      it->parent->win.x += (bound.p2.x - pbound.p2.x);
    }
    if (bound.p1.y < pbound.p1.y) {
      it->parent->win.y -= (pbound.p1.y - bound.p1.y);
    }
    if (bound.p2.y > pbound.p2.y) {
      it->parent->win.y += (bound.p2.y - pbound.p2.y);
    }
    it = it->parent;
  }
  if (item->parent && item->parent->type == list) {
    gui_item *root;
    for (root=item;root->parent;root=root->parent);
    GuiCommonSelectR(item, root);
  }
}

void GuiCommonActivateR(gui_item *item, gui_item *root) {
  gui_item *it;
  if ((root!=item) && (root->state & GUI_FLAGS_ACTIVE))
    root->state &= ~GUI_FLAGS_ACTIVE;
  for (it=root->child;it;it=it->next)
    GuiCommonActivateR(item, it);
}

void GuiCommonActivate(gui_item *item, gui_input *input) {
  gui_item *root;
  for (root=item;root->parent;root=root->parent);
  GuiCommonActivateR(item, root);
}


gui_input input_prev;
uint8_t keys_time[512] = { 0 };

int cursor_time = 0;
const int cursor_rate = 10;

#define GUI_ON(f) (!(item->prev_state & f) && (item->state & f))
#define GUI_OFF(f) ((item->prev_state & f) && !(item->state & f))
void GuiCommonHandlers(gui_item *item, gui_input *input) {
  /* note that if a handler changes state it will not
     be detected until the next frame! */
  if (GUI_ON(GUI_FLAGS_MOUSEDOWN)) { /* instant of left click? */
    /* if (item->styles.mouseup)
      GuiUnapplyStyle(item, item->styles.mouseup) */
    if (item->styles.mousedown)
      GuiApplyStyle(item, item->styles.mousedown);
    GuiCommonMousedown(item, input);
    if (item->handlers.mousedown)
      (*item->handlers.mousedown)(item, input);
  }
  else if (GUI_OFF(GUI_FLAGS_MOUSEDOWN)) { /* instant of release? */
    if (item->styles.mousedown)
      GuiUnapplyStyle(item, item->styles.mousedown);
    /* if (item->styles.mouseup)
      GuiApplyStyle(item, item->styles.mouseup) */
    if (item->handlers.mouseup)
      (*item->handlers.mouseup)(item, input);
  }
  if (item->state & GUI_FLAGS_MOUSEDOWN) {
    if (item->handlers.mouseheld)
      (*item->handlers.mouseheld)(item, input);
  }
  if (GUI_ON(GUI_FLAGS_MOUSEOVER)) { /* instant of mouseover? */
    /* if (item->styles.mouseleave)
      GuiUnapplyStyle(item, item->styles.mouseleave) */
    if (item->styles.mouseover)
      GuiApplyStyle(item, item->styles.mouseover);
    if (item->handlers.mouseover)
      (*item->handlers.mouseover)(item, input);
  }
  else if (GUI_OFF(GUI_FLAGS_MOUSEOVER)) { /* instant of mouseleave? */
    if (item->styles.mouseover)
      GuiUnapplyStyle(item, item->styles.mouseover);
    /* if (item->styles.mouseleave)
      GuiApplyStyle(item, item->styles.mouseleave) */
    if (item->handlers.mouseleave)
      (*item->handlers.mouseleave)(item, input);
  }
  /* for now there is no state flag for mouse motion (such would turn on
     and off every other frame, unless some sort of threshold were used)
     we simply trigger the mouse drag handler when item has the mouse down
     state (i.e. mouse is held after an initial click within item crect)
     and there is a change in mouse location */
  int move = (input->mouse.x != input_prev.mouse.x || input->mouse.y != input_prev.mouse.y);
  if (move && (item->state & GUI_FLAGS_MOUSEDOWN)) {
    if (item->handlers.mousedrag)
      (*item->handlers.mousedrag)(item, input);
  }
  if (GUI_ON(GUI_FLAGS_SELECTED)) {
    if (item->styles.selected)
      GuiApplyStyle(item, item->styles.selected);
    GuiCommonSelect(item, input);
    if (item->handlers.select)
      (*item->handlers.select)(item, input);
  }
  else if (GUI_OFF(GUI_FLAGS_SELECTED)) {
    if (item->styles.active)
      GuiUnapplyStyle(item, item->styles.selected);
    if (item->handlers.deselect)
      (*item->handlers.deselect)(item, input);
  }
  if (GUI_ON(GUI_FLAGS_ACTIVE)) {
    if (item->styles.active)
      GuiApplyStyle(item, item->styles.active);
    GuiCommonActivate(item, input);
    if (item->handlers.activate)
      (*item->handlers.activate)(item, input);
  }
  else if (GUI_OFF(GUI_FLAGS_ACTIVE)) {
    if (item->styles.active)
      GuiUnapplyStyle(item, item->styles.active);
    if (item->handlers.deactivate)
      (*item->handlers.deactivate)(item, input);
  }
  if (GUI_ON(GUI_FLAGS_EXPANDED)) {
    if (item->styles.expanded)
      GuiApplyStyle(item, item->styles.expanded);
    if (item->handlers.expand)
      (*item->handlers.expand)(item, input);
  }
  else if (GUI_OFF(GUI_FLAGS_EXPANDED)) {
    if (item->styles.expanded)
      GuiUnapplyStyle(item, item->styles.expanded);
    if (item->handlers.collapse)
      (*item->handlers.collapse)(item, input);
  }
}

#define GuiPointInRect(p,r) \
((p)->x >= (r)->x && (p)->x <= (r)->x + (int32_t)(r)->w && \
 (p)->y >= (r)->y && (p)->y <= (r)->y + (int32_t)(r)->h)

void GuiCommonUpdate(gui_item *item, gui_input *input) {
  int32_t x, y;
  uint32_t state;
  int i, move, over;
  int kp,kr,kd;

  if (!item->parent) { /* assumes one update per frame of the root item */
    ++cursor_time;
    if (cursor_time > ((cursor_rate*2)-1))
      cursor_time = 0;
    if (input->click & GUI_FLAGS_MOUSEDOWN)
      ++input->click_time;
    else
      input->click_time = 0;
  }
  state = item->prev_state;
  over = GuiPointInRect(&input->mouse, &item->crect);
  if (over) {
    if (!(input_prev.click & GUI_FLAGS_MOUSEDOWN)
     && (input->click & GUI_FLAGS_MOUSEDOWN)) /* left click? */
      item->state |= GUI_FLAGS_MOUSEDOWN;
    if (!(state & GUI_FLAGS_MOUSEOVER)) /* mouse not already over? */
      item->state |= GUI_FLAGS_MOUSEOVER;
  }
  else
    item->state &= ~GUI_FLAGS_MOUSEOVER; /* mouse not over */
  if ((state & GUI_FLAGS_MOUSEDOWN)
     && !(input->click & GUI_FLAGS_MOUSEDOWN))
    item->state &= ~GUI_FLAGS_MOUSEDOWN; /* mouse not clicking */
  /* check for key presses/releases */
  kp=0;kr=0;kd=0;
  for (i=0;i<512;i++) {
    if (!input_prev.keys[i] && input->keys[i]) {
      kp=1;
      if (!item->parent) { keys_time[i] = 0; }
      if (item->handlers.keypress) {
        input->key = i;
        input->key_time = 0;
        (*item->handlers.keypress)(item, input);
      }
    }
    else if (input_prev.keys[i] && !input->keys[i]) {
      kr=1;
      if (item->handlers.keyrelease) {
        input->key = i;
        input->key_time = keys_time[i];  /* -1?? */
        (*item->handlers.keyrelease)(item, input);
      }
    }
    if (input->keys[i]) {
      kd=1;
      if (!item->parent) { keys_time[i]++; }
      if (item->handlers.keyheld) {
        input->key = i;
        input->key_time = keys_time[i]-1;
        (*item->handlers.keyheld)(item, input);
      }
    }
  }
  if (kd) { item->state |= GUI_FLAGS_KEYDOWN; }
  else    { item->state &= ~GUI_FLAGS_KEYDOWN; }
  /* signal handlers / style applications for all other state changes */
  GuiCommonHandlers(item, input);
  item->prev_state = item->state;
}

void GuiTextUpdate(gui_item *item, gui_input *input) {
  gui_item *cursor;
  gui_rect rect;
  dim2 size;
  bound2 extents;
  int len;
  char tmp;

  if (item->flags & GUI_FLAGS_RO) { return; } /* no cursor for RO text */
  if (!(cursor = item->child)) {
    cursor = GuiLabelNew("_");
    cursor->flags |= GUI_FLAGS_FIT | GUI_FLAGS_NOCLIP;
    cursor->style.th = item->style.th;
    cursor->style.fg = black;
    cursor->style.bg = white;
    GuiAddChild(item, cursor);
    item->t->cidx = strlen(item->t->str);
  }
  if (item->state & GUI_FLAGS_SELECTED) {
    GuiCommonRect(item, 1, 0, &rect);
    len = strlen(item->t->str);
    /*if (item->t->cidx == 0) {
      extents.p1.x = 0; extents.p1.y = 0;
      cursor->t->str[0] = item->t->str[item->t->cidx];
    }
    else {*/
      // size.w = 0; size.h = cursor->style.th;
      size.w = 0; size.h = rect.h;
      extents = (*_gui_callbacks.char_extents)(item->t->str, &size, item->t->cidx);
      // extents = (*_gui_callbacks.text_extents)(item->t->str, &rect.dim, item->t->cidx);
      // extents = GLTextExtents(item->t->str, rect.dim, 1, 0, item->t->cidx);
      //rect.w = (extents.p2.x - extents.p1.x);
      if (item->t->cidx != len)
        cursor->t->str[0] = item->t->str[item->t->cidx];
      else
        cursor->t->str[0] = '_';
      if (cursor->t->str[0] == '\n')
        cursor->t->str[0] = ' ';
    //}
    cursor->rect.x = extents.p1.x;
    cursor->rect.y = extents.p1.y;
    cursor->rect.w = extents.p2.x - extents.p1.x;
    cursor->rect.h = extents.p2.y - extents.p1.y;
    /* blink cursor */
    if (cursor_time > (cursor_rate-1))
      cursor->style.visible = 0;
    else
      cursor->style.visible = 1;
  }
  else
    cursor->style.visible = 0;
}

void GuiSliderUpdate(gui_item *item, gui_input *input) {
  gui_slider *s;
  int32_t value;

  s = item->s;
  value = *s->value;
  value = limit(value, s->vmin, s->vmax);
  if (value != *s->value)
    *s->value = value;
}

void GuiListUpdate(gui_item *item, gui_input *input) {
  if (!item->l->selected && item->child) {
    if (!item->parent)
      item->child->state |= GUI_FLAGS_SELECTED;
    item->l->selected = item->child;
  }
}

void GuiItemUpdate(gui_item *item, gui_input *input) {
  gui_item *it;
  int32_t x,y;
  int i;

  /* adjust mouse x and y first */
  if (!item->parent) {
    x = (input->mouse.x*input->screen.w)/input->window.w + input->screen.x;
    y = (input->mouse.y*input->screen.h)/input->window.h + input->screen.y;
    input->mouse.x = x;
    input->mouse.y = y;
  }
  switch (item->type) {
  case text:
    GuiTextUpdate(item, input);
    break;
  case slider:
    GuiSliderUpdate(item, input);
    break;
  case list:
    GuiListUpdate(item, input);
    break;
  }
  GuiCommonUpdate(item, input);
  if (item->handlers.update)
    (*item->handlers.update)(item, input);
  /* update all children */
  for (it=item->child;it;it=it->next)
    GuiItemUpdate(it, input);
  if (!item->parent) {
    input_prev = *input;
  }
}

/* handlers */
/* text */
void GuiTextKeyheld(gui_item *item, gui_input *input) {
  cursor_time = 0;
  if (input->key_time > 10)
    GuiTextKeydown(item, input);
}

void GuiTextKeydown(gui_item *item, gui_input *input) {
  gui_item *cursor;
  char *str, prev[64];
  int i,len,cidx;

  if ((item->flags & GUI_FLAGS_RO) || !(item->state & GUI_FLAGS_SELECTED))
    return;
  if (!(cursor = item->child))
    return;
  str = item->t->str;
  cidx = item->t->cidx;
  strcpy(prev, str);
  len = strlen(str);
  if (KEYISCHAR(input->key) || input->key==KEY_RETURN) {
    for (i=len;i>=cidx;i--)
      str[i]=str[i-1];
    str[cidx]=input->key;
    if (str[cidx]==KEY_RETURN) { str[cidx]='\n'; }
    if (cidx==len) { str[cidx+1]=0; }
    ++item->t->cidx;
  }
  else {
    switch (input->key) {
    case KEY_BACKSPACE:
      if (cidx==0) { break; }
      str[cidx-1] = 0;
      for (i=cidx-1;i<len;i++)
        str[i]=str[i+1];
      /* fall thru */
    case KEY_LEFT:
      cidx = --item->t->cidx;
      break;
    case KEY_RIGHT:
      cidx = ++item->t->cidx;
      break;
    case KEY_DELETE:
      if (cidx==len) { break; }
      str[cidx] = 0;
      for (i=cidx;i<len;i++)
        str[i]=str[i+1];
      break;
    case KEY_ESCAPE:
      if (item->parent && item->parent->type == list
        && !item->parent->handlers.keydown) { /* parent does not process keydown */
        item->state &= ~GUI_FLAGS_ACTIVE;
        item->parent->state |= GUI_FLAGS_ACTIVE;
      }
      break;
    }
  }
  len = strlen(str);
  item->t->cidx = limit(item->t->cidx,0,len);
  if (item->t->data && item->t->parse) {
    if ((*item->t->parse)(str, item->t->data) < 0)
      strcpy(str, prev); /* undo changes if parse fails */
  }
}

void GuiTextActivate(gui_item *item, gui_input *input) {
  item->state |= GUI_FLAGS_SELECTED;
}

void GuiTextDeactivate(gui_item *item, gui_input *input) {
  item->state &= ~GUI_FLAGS_SELECTED;
}

/* list (helpers) */


/* list */
/*
void GuiListUpdate(gui_item *item, gui_input *input) {
  gui_item *it;

  // ensure that the selected item is present in the list
  // it may have been removed, in which case no item in the list should be selected
  for (it=item->child;it;it=it->next) {
    if (it == item->l->selected) { break; }
  }
  item->l->selected = it;
}
*/

void GuiListKeyheld(gui_item *item, gui_input *input) {
  if (input->key_time > 10 && !(input->key_time % 3))
    GuiListKeydown(item, input);
}

void GuiListKeydown(gui_item *item, gui_input *input) {
  gui_item *selected, *next;
  int key_next, key_prev;
  int key_child, key_parent;

  if (!(item->state & GUI_FLAGS_ACTIVE) || (item->flags & GUI_FLAGS_RO))
    return;
  if (item->flags & GUI_FLAGS_DIR) { /* vertical list? */
    key_next = KEY_DOWN;
    key_prev = KEY_UP;
  }
  else {
    key_next = KEY_TAB;
    key_prev = -1;
  }
  key_child = KEY_RETURN;
  key_parent = KEY_ESCAPE;
  selected = item->l->selected;
  /*
  if (item->l->mode == 1) { // 3-state mode?
    if (input->key == key_prev)
      next = GuiItemPrevF(selected, GUI_FLAGS_EXPANDED);
    else if (input->key == key_next)
      next = GuiItemNextF(selected, GUI_FLAGS_EXPANDED);
    else if (input->key == key_child) {
      if (selected->state & GUI_FLAGS_EXPANDED)
        selected->state &= ~GUI_FLAGS_EXPANDED;
      else
        selected->state |= GUI_FLAGS_EXPANDED;
    }
  }
  else */if (input->key == key_next) {
    next = selected->next;
    if (!next) { next = selected->parent->child; }
  }
  else if (input->key == key_prev) {
    next = selected->prev;
    if (!next) { next = selected->parent->tail_child; }
  }
  else if (input->key == key_child) {
    if (selected && selected->type == list) {
      item->state &= ~GUI_FLAGS_ACTIVE;
      selected->state |= GUI_FLAGS_ACTIVE;
    }
  }
  else if (input->key == key_parent) {
    if (item->parent && item->parent->type == list) {
      item->state &= ~GUI_FLAGS_ACTIVE;
      item->parent->state |= GUI_FLAGS_ACTIVE;
    }
  }
  if (next) {
    selected->state &= ~GUI_FLAGS_SELECTED;
    next->state |= GUI_FLAGS_SELECTED;
    if (next->parent && next->parent->type == list) {
      item->state &= ~GUI_FLAGS_ACTIVE;
      next->parent->state |= GUI_FLAGS_ACTIVE;
      next->parent->l->selected = next;
    }
  }
}

void GuiListActivate(gui_item *item, gui_input *input) {
  item->l->selected->state |= GUI_FLAGS_SELECTED;
}

void GuiListDeactivate(gui_item *item, gui_input *input) {
  item->l->selected->state &= ~GUI_FLAGS_SELECTED;
}

/* slider */
gui_item *slider_sel;
void GuiSliderMousedown(gui_item *item, gui_input *input) {
  gui_rect rect;

  GuiCommonRect(item, 1, 0, &rect); /* get the inner rect */
  if (input->mouse.x >= rect.x && input->mouse.x <= rect.x + (int32_t)rect.w
    && input->mouse.y >= rect.y && input->mouse.y <= rect.y + (int32_t)rect.h)
    slider_sel = item;
  else
    slider_sel = 0;
}

void GuiSliderMouseup(gui_item *item, gui_input *input) {
  slider_sel = 0;
}

void GuiSliderMousedrag(gui_item *item, gui_input *input) {
  gui_slider *s;
  gui_border *b;
  gui_rect rect;
  int32_t value;
  int bc;

  /* get inner rect + padding */
  GuiCommonRect(item, 1, 0, &rect);
  bc = item->cstyle.bc;
  b = &item->cstyle.b[bc-1];
  if (item->flags & GUI_FLAGS_DIR) {  /* vertical slider? */
    rect.y -= b->bt;
    rect.h += (b->bt + b->bb);
  }
  else { /* horizontal slider */
    rect.x -= b->bl;
    rect.w += (b->bl + b->br);
  }
  s = item->s;
  /*
  if (item->flags & GUI_FLAGS_DIR) {
    rect.y += (s->ssize/2) + (s->ssize%2);
    rect.h -= s->ssize;
  }
  else {
    rect.x += (s->ssize/2) + (s->ssize%2);
    rect.w -= s->ssize;
  }*/
  /* map mouse location to the slider value
     horizontal should be in the range rect.x to rect.x+rect.w
     vertical should be in the range rect.y to rect.y+rect.h
     they determine the proportion by which to scale (vmax-vmin) */
  if (item->flags & GUI_FLAGS_DIR) {
    value = s->vmin + ((input->mouse.y-rect.y)*(s->vmax-s->vmin))/(int32_t)rect.h;
  }
  else {
    value = s->vmin + ((input->mouse.x-rect.x)*(s->vmax-s->vmin))/(int32_t)rect.w;
  }
  value = limit(value, s->vmin, s->vmax);
  *((int32_t*)s->value) = value;
}

/* scroll */
void GuiScrollLBtnHeld(gui_item *item, gui_input *input) {
  gui_item *slider;
  gui_slider *s;

  slider = item->parent->child->next;
  s = slider->s;
  *((int32_t*)s->value) -= s->step;
}

void GuiScrollRBtnHeld(gui_item *item, gui_input *input) {
  gui_item *slider;
  gui_slider *s;

  slider = item->parent->child->next;
  s = slider->s;
  *((int32_t*)s->value) += s->step;
}

/* litem */
void GuiLItemSelect(gui_item *item, gui_input *input) {
  item->child->state |= GUI_FLAGS_SELECTED;
}
void GuiLItemDeselect(gui_item *item, gui_input *input) {
  item->child->state &= ~GUI_FLAGS_SELECTED;
}

void GuiLItemActivate(gui_item *item, gui_input *input) {
  /* todo: find a better way. this is hacky */
  if (input->key == KEY_RETURN) {
    item->child->next->state |= GUI_FLAGS_ACTIVE;
    item->state &= ~GUI_FLAGS_ACTIVE;
  }
  if (input->key == 1 || input->key == KEY_ESCAPE) { /* ? */
    item->parent->state |= GUI_FLAGS_ACTIVE;
    item->state &= ~GUI_FLAGS_ACTIVE;
  }
}

/* text node (helpers) */
void GuiTextNodeAdd(gui_item *parent, gui_item *node) {
  gui_item *sublist;

  sublist = parent->child->next;
  tree_add_node(parent->n, node->n);
  tree_add_node(&sublist->node, &node->node);
}

void GuiTextNodeInsert(gui_item *parent, gui_item *prev, gui_item *node) {
  gui_item *sublist;

  sublist = parent->child->next;
  tree_insert_node(parent->n, prev ? prev->n : 0, node->n);
  tree_insert_node(&sublist->node, prev ? &prev->node : 0, &node->node);
}

void GuiTextNodeRemove(gui_item *node) {
  if (node->type == tree) { return; }
  tree_remove_node(node->n);
  tree_remove_node(&node->node);
}

gui_item *GuiTextNodeRoot(gui_item *item) {
  while (item) {
    if (item->type == tree) { break; }
    item = item->parent;
  }
  return item;
}

static gui_item *GuiTextNodeExpA(gui_item *item) {
  gui_item *it, *found;

  if (!item) { return 0; }
  it = item;
  found = 0;
  while (it->n->parent) {
    it = it->n->parent->data;
    if (!(it->state & GUI_FLAGS_EXPANDED))
      found = it;
    if (it->type == tree) { break; }
  }
  return found ? found : item;
}

/* text node */
void GuiTextNodeMousedown(gui_item *item, gui_input *input) {
  gui_item *root, *label, *scroll;
  gui_tree *tree;
  rect2 *crect;

  label = item->child;
  crect = &label->crect;
  if (!GuiPointInRect(&input->mouse, crect)) { return; }
  root = GuiTextNodeRoot(item);
  if (scroll = GuiVScroll(root)) {
    crect = &scroll->crect;
    if (GuiPointInRect(&input->mouse, crect)) { return; }
  }
  if (scroll = GuiHScroll(root)) {
    crect = &scroll->crect;
    if (GuiPointInRect(&input->mouse, crect)) { return; }
  }
  tree = root->tr;
  tree->selected->state &= ~GUI_FLAGS_SELECTED;
  item->state |= GUI_FLAGS_SELECTED;
  tree->selected = item;
  tree->sel = GuiTextNodeUnmap(tree->selected);
}

void GuiTextNodeSelect(gui_item *item, gui_input *input) {
  gui_item *root, *node;
  gui_tree *tree;

  /* select the label item */
  item->child->state |= GUI_FLAGS_SELECTED;
  GuiUnapplyStyle(item, item->styles.selected);
  /* update selected item for tree */
  root = GuiTextNodeRoot(item);
  tree = root->tr;
  tree->selected = item;
  /* update selected data for tree */
  tree->sel = GuiTextNodeUnmap(item);
  /* run select callback for tree */
  if (tree->select)
    (*tree->select)(root, input);
}

void GuiTextNodeDeselect(gui_item *item, gui_input *input) {
  item->child->state &= ~GUI_FLAGS_SELECTED;
}

void GuiTextNodeExpand(gui_item *item, gui_input *input) {
  gui_item *sublist;

  sublist = item->child->next;
  sublist->style.visible = 1;
}

void GuiTextNodeCollapse(gui_item *item, gui_input *input) {
  gui_item *sublist;

  sublist = item->child->next;
  sublist->style.visible = -1;
}

/* text tree (helpers) */
gui_item *GuiTextTreeNode(gui_item *tree, void *data) {
  gui_item *item;
  gui_node *n;
  if (data == 0) { return (gui_item*)0; }
  /* note that we can access tree->t->n as tree->n */
  tree_first_node(tree->n, n, (GuiTextNodeUnmap(n->data)==data));
  if (n == 0) { return (gui_item*)0; }
  item = n->data;
  return item;
}

/* text tree */
void GuiTextTreeUpdate(gui_item *item, gui_input *input) {
  tree_node_t *cur;
  gui_tree *tree;
  list_t *deltas;
  tree_delta_t *delta;
  gui_item *nitem, *child, *parent, *prev;
  gui_item *lprev, *rprev, *lnext, *rnext;
  gui_item *l, *r, *psel;

  /* get the tree info from the item */
  tree = item->tr;
  if (!tree->selected) {
    tree->selected = item;
    tree->sel = GuiTextNodeUnmap(tree->selected);
  }
  if (tree->data) {
    if (tree->child_fn) {
      /* extract the current, possibly new, tree layout from the currently bound tree data
         using the 'child_fn' supplied when creating the tree */
      tree->cur = tree_map_from(tree->data, tree->child_fn, GuiId);
    }
    else {
      /* grab the current, possibly new, tree layout that is the currently bound data */
      tree->cur = tree->data;
    }
  }
  if (!tree->prev)
    tree->prev = tree_copy(tree->cur);
  /* get the list of changes between the previous and current tree layout */
  deltas = tree_changes(tree->prev, tree->cur);
  _text_node_unparse = tree->unparse;
  /* add/remove/move/swap the corresponding gui text items for each change */
  list_for_each(deltas, delta) {
    switch (delta->op) {
    case 1: // add
      nitem = GuiTextNodeMap(delta->value, 0);
      parent = GuiTextTreeNode(item, delta->parent);
      prev = GuiTextTreeNode(item, delta->prev);
      GuiTextNodeInsert(parent, prev, nitem);
      break;
    case 2: // remove
      nitem = GuiTextTreeNode(item, delta->value);
      if (!nitem) { break; }
      parent = nitem->n->parent->data;
      for_each_child(nitem->n, child) {
        GuiTextNodeRemove(child);
        GuiTextNodeAdd(parent, child);
      }
      GuiTextNodeRemove(nitem);
      GuiItemFree(nitem);
      break;
    case 3: // move
      nitem = GuiTextTreeNode(item, delta->value);
      if (!nitem) { break; }
      GuiTextNodeRemove(nitem);
      parent = GuiTextTreeNode(item, delta->parent);
      prev = GuiTextTreeNode(item, delta->prev);
      GuiTextNodeInsert(parent, prev, nitem);
      break;
    case 4: // swap
      l = GuiTextTreeNode(item, delta->v1);
      r = GuiTextTreeNode(item, delta->v2);
      tree_swap_nodes(l->n, r->n);
      tree_swap_nodes(&l->node, &r->node);
      break;
    case 5:
      nitem = GuiTextNodeMap(item->child->t->data, 0);
      item->child->t->data = delta->value;
      GuiTextNodeAdd(item, nitem);
      break;
    }
  }
  tree->prev = tree_copy(tree->cur);
  /* clear the selected property if no child item is selected */
  // if (!item)
  //   item = GuiFindItem(item, 0, GuiSelected);
  //   tree->selected = 0;
  _text_node_unparse = 0;
}

void GuiTextTreeKeyheld(gui_item *item, gui_input *input) {
  cursor_time = 0;
  if (input->key_time > 10)
    GuiTextTreeKeydown(item, input);
}

void GuiTextTreeKeydown(gui_item *item, gui_input *input) {
  gui_tree *tree;
  gui_node *node;
  gui_item *next;

  tree = item->tr;
  if (!tree->selected) { return; }
  node = tree->selected->n;
  if (input->key == KEY_DOWN) {
    node = GuiTextNodeExpA(node->data)->n;
    node = tree_node_preorder_cond_next(node,
      ((gui_item*)node->data)->state & GUI_FLAGS_EXPANDED,
      item->n);
  }
  else if (input->key == KEY_UP) {
    node = tree_node_preorder_prev(node, item->n);
    node = GuiTextNodeExpA(node->data)->n;
  }
  else if (input->key == KEY_RIGHT)
    tree->selected->state |= GUI_FLAGS_EXPANDED;
  else if (input->key == KEY_LEFT)
    tree->selected->state &= ~GUI_FLAGS_EXPANDED;
  else if (input->key == KEY_RETURN)
    tree->selected->state |= GUI_FLAGS_ACTIVE;
  else
    return;
  if (!node) { return; }
  next = node->data;
  tree->selected->state &= ~GUI_FLAGS_SELECTED;
  next->state |= GUI_FLAGS_SELECTED;
  tree->selected = next;
  tree->sel = GuiTextNodeUnmap(tree->selected);
}
