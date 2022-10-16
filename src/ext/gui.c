/**
 * Gui implementation (for realtime in-game debugging).
 * Mostly reflection-mapped custom views.
 *
 * See section in /doc/files.md for more info.
 */
#include "./ext/gui.h"
#include "./ext/lib/refl.h"
#include "./ext/lib/gui.h"
#include "./util/tree.h"

#include "ns.h"
#include "gool.h"

extern ns_struct ns;
extern gool_handle handles[8];

extern rgba8 black;
extern rgba8 white;
extern gui_border border_trans;

void GuiItemReplace(gui_item *item, gui_item *repl) {
  gui_item *parent, *prev;

  if (!item->parent) { return; }
  parent = item->parent;
  prev = item->prev;
  GuiRemoveChild(parent, item);
  GuiInsertChild(parent, prev, repl);
  GuiItemFree(item);
}

/** custom controls **/

/** child extractor functions **/

gool_handle handles_root = { .type = 3 }; // dummy object

/**
 * extract the children from a gool object, in array form
 */
static void **GuiObjChildren(void *_obj) {
  gool_object *obj, *it, **children;
  int i;

  obj = (gool_object*)_obj;
  if (obj == (void*)&handles_root) {
    children = (gool_object**)malloc(sizeof(gool_object*)*9);
    for (i=0;i<8;i++) {
      children[i] = (gool_object*)&handles[i];
    }
    children[i] = 0;
    return (void**)children;
  }
  for (it=GoolObjectGetChildren(obj),i=0;it;it=it->sibling,i++);
  children = (gool_object**)malloc(sizeof(gool_object*)*(i+1));
  for (it=GoolObjectGetChildren(obj),i=0;it;it=it->sibling,i++) {
    children[i] = it;
  }
  children[i] = 0;
  return (void**)children;
}

/** unparser functions **/

/**
 * format a gool object pointer as a string
 */
static int GuiUnparseObjPtr(void *data, char *str) {
  zone_entity *entity;
  gool_object *obj;
  char *eid_str;
  int id, type;

  obj = (gool_object*)data;
  if (obj == 0) {
    sprintf(str, "none");
    return 1;
  }
  if ((int)data < 0x400) {
    sprintf(str, "non-object pointer");
    return 1;
  }
  if (obj->handle.type == 2) {
    sprintf(str, "handle %i", (gool_handle*)obj - handles);
    return 1;
  }
  if (obj->handle.type == 3) {
    sprintf(str, "root");
    return 1;
  }
  entity = obj->entity;
  if (entity) {
    id = entity->id;
    type = entity->type;
    eid_str = NSEIDToString(((entry*)ns.ldat->exec_map[type])->eid);
    sprintf(str, "%i (%s)", id, eid_str);
  }
  else {
    sprintf(str, "no_entity @%08x", obj);
  }
  return 1;
}

/**
 * disassemble an object's bytecode
 */
#define GOOL_DEBUG
extern void GoolObjectPrintDebug(gool_object*, FILE*);
static int GuiObjCodeUnparse(void *data, char *str) {
  gool_object *obj;
  FILE *stream;
  size_t size;
  char *code;

  if (data == 0) {
    sprintf(str, "no object selected");
    return 1;
  }
  obj = (gool_object*)data;
  if (obj->handle.type != 1) {
    sprintf(str, "non-handle object selected");
    return 1;
  }
  stream = open_memstream(&code, &size);
  GoolObjectPrintDebug(obj, stream);
  fclose(stream);
  memcpy(str, code, size);
  str[size] = 0;
  return 1;
}

/** structure field => gui item mappers **/
static gui_item *GuiReflText(refl_value *val) {
  char *typename;
  gui_item *item;

  typename = val->field->type->name;
  item = GuiTextNew(val->res, value);
  if (strcmp(typename, "uint32_t") == 0
   || strcmp(typename, "int32_t") == 0
   || strcmp(typename, "int") == 0) {
    item->t->parse = TextParseInt;
    item->t->unparse = TextUnparseInt;
  }
  else if (strcmp(typename, "uint16_t") == 0
        || strcmp(typename, "int16_t") == 0) {
    item->t->parse = TextParseInt16;
    item->t->unparse = TextUnparseInt16;
  }
  else if (strcmp(typename, "uint8_t") == 0
        || strcmp(typename, "int8_t") == 0) {
    item->t->parse = TextParseInt8;
    item->t->unparse = TextUnparseInt8;
  }
  GuiSetBorderSize(item, 1, 1);
  GuiSetBorderColor(item, 1, black);
  GuiSetBorderColor(item, 2, white);
  item->flags &= ~GUI_FLAGS_SCROLL;
  item->style.bc=3;
  item->style.fg=black;
  item->style.th=13; /* make the box sufficiently tall */
  return item;
}

static gui_item *GuiReflList(gui_item **children, int flags) {
  gui_item *item, *it;
  int i;

  if (!children) { return (gui_item*)-1; }
  item = GuiListNew();
  item->flags = flags | GUI_FLAGS_SCROLL;
  item->style.fcs = GuiListCS;
  GuiSetBorderSize(item, 1, 1);
  for (i=0;it=children[i];i++) {
    /* skip children that we chose to not map */
    if ((int)it==-1) { continue; }
    it->flags |= GUI_FLAGS_CELL;
    GuiSetBorderSize(it, 0, 1);
    GuiAddChild(item, it);
  }
  return item;
}

static gui_item *GuiReflVec(gui_item **children) {
  gui_item *item;

  item = GuiReflList(children, 0); /* horizontal list with x/y/z */
  return item;
}

static gui_item *GuiReflVec16(gui_item **children) {
  gui_item *item;

  item = GuiReflList(children, 0); /* horizontal list with x/y/z */
  return item;
}

static gui_item *GuiReflAng(gui_item **children) {
  gui_item *item;

  item = GuiReflList(children, 0); /* horizontal list with y/z/x */
  return item;
}

static rgba8 GuiBoxColor(gui_item *item) {
  gui_item *r_text, *g_text, *b_text;
  rgba8 color;

  r_text = item->parent->child->child->next;
  g_text = r_text->parent->next->child->next;
  b_text = g_text->parent->next->child->next;
  color.r = *((uint8_t*)r_text->t->data) * 257;
  color.g = *((uint8_t*)g_text->t->data) * 257;
  color.b = *((uint8_t*)b_text->t->data) * 257;
  color.a = 255;
  return color;
}

static rgba8 GuiBoxColor16(gui_item *item) {
  gui_item *r_text, *g_text, *b_text;
  rgba8 color;

  r_text = item->parent->child->child->next;
  g_text = r_text->parent->next->child->next;
  b_text = g_text->parent->next->child->next;
  color.r = *((uint16_t*)r_text->t->data);
  color.g = *((uint16_t*)g_text->t->data);
  color.b = *((uint16_t*)b_text->t->data);
  color.a = 255;
  return color;
}

static gui_item *GuiReflRgb8(gui_item **children) {
  gui_item *item, *it;

  item = GuiReflList(children, 0); /* horizontal list with r/g/b */
  item->flags |= GUI_FLAGS_DIR | GUI_FLAGS_CH;
  item->style.fcs = GuiListCS;
  it = GuiItemNew();
  it->style.fbg = GuiBoxColor;
  it->style.bc = 2;
  GuiSetBorderSize(it,0,1);
  GuiSetBorderSize(it,1,1);
  GuiSetBorderColor(it,0,black);
  it->rect.w = 20;
  it->rect.h = 20;
  it->flags |= GUI_FLAGS_CELL;
  GuiAddChild(item, it);
  return item;
}

static gui_item *GuiReflRgb16(gui_item **children) {
  gui_item *item, *it;

  item = GuiReflList(children, 0); /* horizontal list with r/g/b */
  item->flags |= GUI_FLAGS_DIR | GUI_FLAGS_CH;
  item->style.fcs = GuiListCS;
  it = GuiItemNew();
  it->type == none;
  it->style.fbg = GuiBoxColor16;
  it->style.bc = 2;
  GuiSetBorderSize(it,0,1);
  GuiSetBorderSize(it,1,1);
  GuiSetBorderColor(it,0,black);
  it->rect.w = 20;
  it->rect.h = 20;
  it->flags |= GUI_FLAGS_CELL;
  GuiAddChild(item, it);
  return item;
}

static gui_item *GuiReflMat16(gui_item **children) {
  gui_item *item;

  item = GuiReflList(children, 0);
  item->flags |= GUI_FLAGS_DIR | GUI_FLAGS_CH; /* vertical list with each vector list item */
  item->child->child->next->flags |= GUI_FLAGS_DIR | GUI_FLAGS_CH;
  return item;
}

static gui_item *GuiReflBound(gui_item **children) {
  gui_item *item;

  item = GuiReflList(children, 0);
  item->flags |= GUI_FLAGS_DIR | GUI_FLAGS_CH; /* vertical list with each vector list item */
  return item;
}

static gui_item *GuiReflObjPtr(refl_value *val) {
  /* create a read-only text with the entity id and gool entry name */
  gool_object *obj;
  gui_item *item;

  item = GuiTextNew(*((void**)val->res), value);
  item->flags |= GUI_FLAGS_RO;
  item->t->unparse = GuiUnparseObjPtr;
  GuiSetBorderSize(item, 1, 1);
  GuiSetBorderColor(item, 1, black);
  GuiSetBorderColor(item, 2, white);
  item->flags &= ~GUI_FLAGS_SCROLL;
  item->style.bc=3;
  item->style.fg=black;
  item->style.th=13; /* make the box sufficiently tall */
  return item;
}

static gui_item *GuiMapObjField(refl_value *val, refl_path *path, void **mapped) {
  char *typename;
  int label, count;
  gui_item *item, *litem, **children;
  refl_value pval;

    /* for now we will skip fields of these names as they refer to blobs
     and we dont yet have a working hex edit control */
  if (path->len > 1) {
    pval = path->values[path->len-2];
    count = ReflGetCount(pval.res, val->field);
  }
  if (strcmp(val->field->name, "data") == 0) { return (gui_item*)-1; }
  if (strcmp(val->field->name, "regs") == 0) { return (gui_item*)-1; }
  if (strcmp(val->field->name, "colors_i") == 0) { return (gui_item*)-1; }
  label = 1; /* all items will have a label unless specified otherwise */
  /* by default we will skip pointer valued fields
     these will eventually be implemented as collapsible sublists
     that will end at cycles
     these should otherwise be type-specific condensed 'previews' of the
     pointed objects */
  children = (gui_item**)mapped;
  typename = val->field->type->name;
  /* mappers for field type */
  if (val->field->flags & REFL_FLAGS_POINTER) {
    if (strcmp(typename, "gool_object") == 0)
      item = GuiReflObjPtr(val);
    else
      return (gui_item*)-1;
  }
  else if (!children)
    item = GuiReflText(val);
  else if (strcmp(typename, "vec") == 0)
    item = GuiReflVec(children);
  else if (strcmp(typename, "vec16") == 0)
    item = GuiReflVec16(children);
  else if (strcmp(typename, "ang") == 0)
    item = GuiReflAng(children);
  else if (strcmp(typename, "mat16") == 0)
    item = GuiReflMat16(children);
  else if (strcmp(typename, "rgb8") == 0)
    item = GuiReflRgb8(children);
  else if (strcmp(typename, "rgb16") == 0)
    item = GuiReflRgb16(children);
  else if (strcmp(typename, "bound") == 0)
    item = GuiReflBound(children);
  else { /* default case when there are children */
    /* vertical list which spans the height of its children */
    item = GuiReflList(children, GUI_FLAGS_DIR | GUI_FLAGS_CH);
  }
  if (label) {
    /* create a 2 cell/column horizontal 'list' with
       the label in the left and item in the right */
    litem = GuiLItemNew(val->field->name, item);
    /* set label alignment to top of cell */
    litem->child->style.al.x = -1;
    litem->child->style.al.y = -1;
    /* make the text height sufficiently tall; */
    litem->child->style.th = 10;
    litem->child->style.al.y = 0;
    litem->child->rect.h = 15;
    litem->child->rect.w = -1;
    litem->child->next->rect.w = -6;
    /* force the height that of the tallest:
       either the label text or the item */
    litem->flags |= GUI_FLAGS_CH;
    GuiSetBorderSize(litem, 1, 0);
    return litem;
  }
  return item;
}

static gui_item *GuiObjView(void *obj) {
  refl_type *gool_object_type;
  gui_item *view, **items, *it;
  gui_item *top, *bot;
  int i;

  view = GuiListNew();
  view->flags |= GUI_FLAGS_DIR;
  if (obj) {
    top = GuiListNew();
    top->flags |= GUI_FLAGS_CELL | GUI_FLAGS_DIR | GUI_FLAGS_SCROLL;
    top->style.fcs = GuiListCS;
    gool_object_type = ReflGetType(0, "gool_object");
    items = (gui_item**)ReflMap(obj, gool_object_type, (refl_map_t)GuiMapObjField);
    for (i=0;it=items[i];i++) {
      if ((int)it == -1) { continue; }
      it->flags |= GUI_FLAGS_CELL;
      GuiAddChild(top, it);
    }
    free(items);
  }
  else {
    top = GuiItemNew();
    top->flags |= GUI_FLAGS_CELL;
  }
  GuiAddChild(view, top);
  bot = GuiTextNew(0, value);
  bot->flags |= GUI_FLAGS_CELL | GUI_FLAGS_RO | GUI_FLAGS_SCROLL;
  bot->style.th = 10;
  bot->style.fg = black;
  free(bot->t);
  bot->t = (gui_text*)malloc(sizeof(gui_text)+0x1000);
  bot->t->maxlen = 0x1000;
  bot->t->unparse = GuiObjCodeUnparse;
  bot->t->data = 0;
  GuiAddChild(view, bot);
  return view;
}

static gui_item *GuiObjTree(void *obj, gui_handler_t select) {
  gui_item *tree;

  tree = GuiTextTreeNew(obj, GuiUnparseObjPtr, GuiObjChildren, select);
  return tree;
}

/** specific views **/
static void GuiObjSelect(gui_item *item, gui_input *input) {
  gui_item *root, *old, *new;
  gui_item *cview;
  gui_tree *tree;
  gool_handle *handle;

  tree = item->tr;
  old = item->next;
  handle = tree->sel;
  if (handle->type == 1) {
    new = GuiObjView(tree->sel);
    new->flags |= GUI_FLAGS_CELL;
    new->rect.w = -4;
    GuiItemReplace(old, new);
  }
  cview = item->next->child->next;
  cview->t->data = tree->sel;
}

extern int pad_lock;
static void GuiDebugKeydown(gui_item *item, gui_input *input) {
  if (input->key == KEY_ESCAPE) {
    item->style.visible = !item->style.visible;
    if (item->style.visible)
      pad_lock = 3;
    else
      pad_lock = 0;
  }
}

static gui_item *GuiDebugView() {
  gui_item *root, *text;
  gui_item *tree_view, *obj_view;
  refl_value val;
  void *obj;

  root = GuiListNew();
  root->flags &= ~GUI_FLAGS_DIR;
  root->handlers.keydown = GuiDebugKeydown;
  root->style.visible = 1;
  //tree_view = GuiListNew();
  tree_view = GuiObjTree(&handles_root, GuiObjSelect);
  tree_view->state |= GUI_FLAGS_ACTIVE;
  if (tree_view->child && tree_view->child->child) {
    text = tree_view->child->child;
    obj = text->t->data;
  }
  else
    obj = 0;
  obj_view = GuiObjView(obj);
  tree_view->flags |= GUI_FLAGS_CELL;
  obj_view->flags |= GUI_FLAGS_CELL;
  tree_view->rect.w = -1;
  obj_view->rect.w = -4;
  GuiAddChild(root, tree_view);
  GuiAddChild(root, obj_view);
  return root;
}

/** main **/
gui_item *root;
extern rect2 screen;
void GuiInit(gui_callbacks *callbacks) {
  gui_item *text, *t2;
  gui_input i = { 0 };

  pad_lock = 0;
  GuiConfig(callbacks);
  ReflInit(types);
  root = GuiDebugView();
  root->style.visible = 0;
  root->flags = GUI_FLAGS_ABS;
  root->rect.x = screen.x; root->rect.y = screen.y;
  root->rect.w = screen.w; root->rect.h = screen.h;
  root->style.bc = 3;
  root->style.fcs = GuiListCS;
}

void GuiUpdate(gui_input *input) {
  int i;
  if (!root->style.visible) {
    for (i=0;i<512;i++) {
      if (i == KEY_ESCAPE) { continue; }
      input->keys[i] = 0;
    }
  }
  GuiItemUpdate(root, input);
}

void GuiDraw() {
  GuiItemDraw(root);
}
