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
extern gool_object *objects;
extern gool_object *player;

void GuiItemReplace(gui_item *item, gui_item *repl) {
  gui_item *parent, *prev;

  if (!item->parent) { return; }
  parent = item->parent;
  prev = item->prev;
  GuiRemoveChild(parent, item);
  GuiInsertChild(parent, prev, repl);
  GuiItemFree(item, 1);
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
  entry *exec;
  char *eid_str;
  int id, type, subtype;

  obj = (gool_object*)data;
  if (obj == 0) {
    sprintf(str, "none");
    return 1;
  }
  if (!(((int)data >= (int)objects &&
         (int)data <= (int)(objects+(GOOL_OBJECT_COUNT-1))) 
      ||((int)data == (int)player)
      ||((int)data >= (int)&handles[0] &&
         (int)data <= (int)&handles[7])
      ||((int)data == (int)&handles_root))) {
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
  if ((int)entity >= (int)ns.pagemem 
   && (int)entity <= (int)(ns.pagemem+ns.physical_page_count)) {
    id = entity->id;
    type = entity->type;
    eid_str = NSEIDToString(((entry*)ns.ldat->exec_map[type])->eid);
    sprintf(str, "%i (%s)", id, eid_str);
  }
  else if ((exec = obj->external) || (exec = obj->global)) {
    eid_str = NSEIDToString(exec->eid);
    subtype = obj->subtype;
    sprintf(str, "%s (subtype %i)", eid_str, subtype);
  }
  else {
    sprintf(str, "unknown @%08x", (uint32_t)obj);
  }
  return 1;
}

#ifdef CFLAGS_GOOL_DEBUG
/**
 * disassemble an object's bytecode
 */
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
#endif

static gui_item *GuiNodeContent(gui_item *item) {
  gui_item *it;

  for (it=item->child;it;it=it->next) {
    if (it->flags & GUI_FLAGS_NODE_CONTENT) { break; }
  }
  return it;
}

/** structure field => gui item mappers **/
static gui_item *GuiReflScalar(refl_value *val) {
  gui_item *item;
  gui_ptype *ptype;
  char *typename;

  typename = val->field->type->name;
  if (strcmp(typename, "uint32_t") == 0)
    ptype = &gui_u32;
  else if (strcmp(typename, "int32_t") == 0
   || strcmp(typename, "int") == 0)
    ptype = &gui_s32;
  else if (strcmp(typename, "uint16_t") == 0)
    ptype = &gui_u16;
  else if (strcmp(typename, "int16_t") == 0)
    ptype = &gui_s16;
  else if (strcmp(typename, "uint8_t") == 0)
    ptype = &gui_u8;
  else if (strcmp(typename, "int8_t") == 0)
    ptype = &gui_s8;
  item = GuiScalarNew(0, val->res, ptype, 0);
  return item;
}

static gui_item *GuiReflGroup(gui_item **children, int flags) {
  gui_item *item, *it, *content, *label;
  int i, once;

  if (!children) { return (gui_item*)-1; }
  item = GuiGroupNew();
  once = 0;
  for (i=0;it=children[i];i++) {
    /* skip children that we chose to not map */
    if ((int)it==-1) { continue; }
    content = GuiNodeContent(it);
    content->label = it->label;
    it->label = 0;
    content->flags |= it->flags;
    GuiItemFree(it, 0);
    content->flags &= ~GUI_FLAGS_NODE_CONTENT;
    if (once && (flags & 1)) { content->flags |= GUI_FLAGS_SAMELINE; }
    if (!once) { once=1; }
    GuiAddChild(item, content);
  }
  return item;
}

static gui_item *GuiReflNode(gui_item **children) {
  gui_item *item, *it;
  int i;

  item = GuiNodeNew(0, 0);
  for (i=0;it=children[i];i++) {
    if ((int)it==-1) { continue; }
    GuiAddChild(item, it);
  }
  return item;
}

static gui_item *GuiReflVec(gui_item **children) {
  gui_item *item;

  item = GuiReflGroup(children, 1); /* horizontal group with x/y/z */
  return item;
}

static gui_item *GuiReflVec16(gui_item **children) {
  gui_item *item;

  item = GuiReflGroup(children, 1); /* horizontal group with x/y/z */
  return item;
}

static gui_item *GuiReflAng(gui_item **children) {
  gui_item *item;

  item = GuiReflGroup(children, 1); /* horizontal group with y/z/x */
  return item;
}

static void Rgb8ToFloats(void *in, float out[3]) {
  rgb8 *color;

  color = in;
  out[0] = (float)color->r / 255;
  out[1] = (float)color->g / 255;
  out[2] = (float)color->b / 255;
}

static void FloatsToRgb8(float in[3], void *out) {
  rgb8 *color;

  color = out;
  color->r = (uint8_t)(in[0] * 255);
  color->g = (uint8_t)(in[1] * 255);
  color->b = (uint8_t)(in[2] * 255);
}

static void Rgb16ToFloats(void *in, float out[3]) {
  rgb16 *color;

  color = in;
  out[0] = (float)color->r / 65535;
  out[1] = (float)color->g / 65535;
  out[2] = (float)color->b / 65535;
}

static void FloatsToRgb16(float in[3], void *out) {
  rgb16 *color;

  color = out;
  color->r = (uint16_t)(in[0] * 65535);
  color->g = (uint16_t)(in[1] * 65535);
  color->b = (uint16_t)(in[2] * 65535);
}

static void RgbToFloats(void *in, float out[3]) {
  rgb *color;

  color = in;
  out[0] = (float)color->r / 0xFFFFFFFF;
  out[1] = (float)color->g / 0xFFFFFFFF;
  out[2] = (float)color->b / 0xFFFFFFFF;
}

static void FloatsToRgb(float in[3], void *out) {
  rgb *color;

  color = out;
  color->r = (uint32_t)(in[0] * 0xFFFFFFFF);
  color->g = (uint32_t)(in[1] * 0XFFFFFFFF);
  color->b = (uint32_t)(in[2] * 0xFFFFFFFF);
}

static gui_item *GuiReflRgb8(gui_item **children) {
  gui_item *item, *it, *content;
  void *data;
  int i;

  content = GuiNodeContent(children[0]);
  data = content->s->data;
  for (i=0;children[i];i++) {
    GuiItemFree(children[i], 1);
  }
  item = GuiColorNew(data, Rgb8ToFloats, FloatsToRgb8);
  return item;
}

static gui_item *GuiReflRgb16(gui_item **children) {
  gui_item *item, *it, *content;
  void *data;
  int i;

  content = GuiNodeContent(children[0]);
  data = content->s->data;
  for (i=0;children[i];i++) {
    GuiItemFree(children[i], 1);
  }
  item = GuiColorNew(data, Rgb16ToFloats, FloatsToRgb16);
  return item;
}

static gui_item *GuiReflRgb(gui_item **children) {
  gui_item *item, *it, *content;
  void *data;
  int i;

  content = GuiNodeContent(children[0]);
  data = content->s->data;
  for (i=0;children[i];i++) {
    GuiItemFree(children[i], 1);
  }
  item = GuiColorNew(data, RgbToFloats, FloatsToRgb);
  return item;
}

static gui_item *GuiReflMat16(gui_item **children) {
  gui_item *item, *it, *cmp, *next, *group;
  gui_item *rows[4];
  int i;

  group = children[0]->child;
  for (i=0,it=group->child;it;it=next,i++) {
    next = it->next;
    GuiRemoveChild(group, it);
    for (cmp=it->child;cmp;cmp=cmp->next) {
      if (!cmp->prev) { continue; }
      cmp->flags |= GUI_FLAGS_SAMELINE;
    }
    rows[i] = GuiNodeNew(0,0);
    GuiAddChild(rows[i], it);
    it->flags |= GUI_FLAGS_NODE_CONTENT;
  }
  rows[i] = 0;
  item = GuiReflGroup(rows, 0);
  GuiItemFree(children[0], 1);
  GuiItemFree(children[1], 1);
  return item;
}

static gui_item *GuiReflBound(gui_item **children) {
  gui_item *item;

  item = GuiReflGroup(children, 0);
  return item;
}

static gui_item *GuiReflObjPtr(refl_value *val) {
  gool_object *obj;
  gui_item *item;

  item = GuiTextNew(0, 0);
  item->flags |= GUI_FLAGS_READONLY;
  item->t->data = *(void**)val->res;
  item->t->dtype.unparse = GuiUnparseObjPtr;
  return item;
}

static gui_item *GuiMapObjField(refl_value *val, refl_path *path, void **mapped) {
  const char *rnames[9] = { "data", "regs", "memory",
    "colors_i", "vectors_v", "vectors_a", "l", "c", "a" };
  gui_item *node, *item, **children;
  refl_value pval;
  char *name, *typename;
  int i, count, label;

  for (i=0;i<9;i++) { /* skip rejected fields */
    if (strcmp(val->field->name, rnames[i]) == 0)
      return (gui_item*)-1;
  }
  label = 1; /* all items will have a label unless specified otherwise */
  children = (gui_item**)mapped;
  typename = val->field->type->name;
  /* mappers for field type */
  if (path->len > 1 && (pval = path->values[path->len-2],
    count = ReflGetCount(pval.res, val->field)) > 1 &&
    path->name[strlen(path->name)-1] != ']') { /* field is array type? */
    if (!children) { return (gui_item*)-1; }
    item = GuiReflGroup(children, 0); /* vertically group mapped elements */
  }
  else if (val->field->flags & REFL_FLAGS_POINTER) {
    if (strcmp(typename, "gool_object") == 0)
      item = GuiReflObjPtr(val);
    else
      return (gui_item*)-1;
  }
  else if (!children)
    item = GuiReflScalar(val);
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
  else if (strcmp(typename, "rgb") == 0)
    item = GuiReflRgb(children);
  else if (strcmp(typename, "bound") == 0)
    item = GuiReflBound(children);
  else {
    node = GuiReflNode(children);
    if (label)
      GuiAddLabel(node, val->field->name);
    return node;
  }
  node = GuiNodeNew(0,0);
  item->flags |= GUI_FLAGS_NODE_CONTENT;
  GuiAddChild(node, item);
  if (label)
    GuiAddLabel(node, val->field->name);
  return node;
}

static gui_item *GuiObjTree(void *obj, gui_handler_t select) {
  gui_item *tree;
  gui_tree_sync sync;

  sync.data = obj;
  sync.child_fn = GuiObjChildren;
  sync.unparse = GuiUnparseObjPtr;
  sync.map = GuiNodeMap;
  sync.unmap = GuiNodeData;
  tree = GuiTreeNew(select, &sync);
  return tree;
}

static gui_item *GuiObjPropTree(void *obj) {
  refl_type *gool_object_type;
  gui_item *tree, **items, *it;
  char label[64];
  int i;

  tree = GuiTreeNew(0, 0);
  GuiUnparseObjPtr(obj, label);
  GuiAddLabel(tree, label);
  tree->tr->column_count = 1;
  if (obj==0) { return tree; }
  gool_object_type = ReflGetType(0, "gool_object");
  items = (gui_item**)ReflMap(obj, gool_object_type, (refl_map_t)GuiMapObjField);
  for (i=0;it=items[i];i++) {
    if ((int)it == -1) { continue; }
    GuiAddChild(tree, it);
  }
  free(items);
  return tree;
}

#ifdef CFLAGS_GOOL_DEBUG
static gui_item *GuiObjCodeText(void *obj) {
  gui_item *item;

  item = GuiTextNew(0, 0);
  item->t = (gui_text*)realloc(item->t, sizeof(gui_text)+0x2000);
  item->t->maxlen = 0x2000;
  item->t->data = obj;
  item->t->dtype.unparse = GuiObjCodeUnparse;
  item->flags |= GUI_FLAGS_READONLY | GUI_FLAGS_MULTILINE;
  return item;
}

static void GuiObjBreak(gui_item *item) {
  gool_object *obj;


  obj = item->parent->prev->t->data;
  GoolObjectPause(obj, 0, 0);
}

static void GuiObjContinue(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectResume(obj, 0);
}

static void GuiObjStep(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectStep(obj, 0);
}


static void GuiObjBreakTrans(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectPause(obj, 1, 0);
}

static void GuiObjContinueTrans(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectResume(obj, 1);
}

static void GuiObjStepTrans(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectStep(obj, 1);
}

static void GuiObjBreakCode(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectPause(obj, 2, 0);
}

static void GuiObjContinueCode(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectResume(obj, 2);
}

static void GuiObjStepCode(gui_item *item) {
  gool_object *obj;

  obj = item->parent->prev->t->data;
  GoolObjectStep(obj, 2);
}

static gui_item *GuiObjCodeView(void *obj) {
  gui_item *view, *text, *buttons, *item;

  view = GuiWindowNew("code view");
  text = GuiObjCodeText(obj);
  buttons = GuiGridNew(3,3);
  item = GuiButtonNew("Break", GuiObjBreak);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Continue", GuiObjContinue);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Step", GuiObjStep);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Break Trans", GuiObjBreakTrans);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Continue Trans", GuiObjContinueTrans);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Step Trans", GuiObjStepTrans);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Break Code", GuiObjBreakCode);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Continue Code", GuiObjContinueCode);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Step Code", GuiObjStepCode);
  GuiAddChild(buttons, item);
  GuiAddChild(view, text);
  GuiAddChild(view, buttons);
  return view;
}
#endif

/** specific views **/
static gui_item* GuiObjView(void*);
static void GuiObjSelect(gui_item *item) {
  gui_item *root, *old, *new;
  gui_tree *tree;
  gool_handle *handle;

  tree = item->tr;
  old = item->parent->next;
  handle = tree->sel;
  if (handle->type == 1) {
    new = GuiObjView(tree->sel);
    GuiItemReplace(old, new);
  }
}

static gui_item *GuiObjTreeView(void *obj) {
  gui_item *view, *tree;

  view = GuiWindowNew("object tree view");
  tree = GuiObjTree(obj, GuiObjSelect);
  GuiAddChild(view, tree);
  return view;
}

static gui_item *GuiObjView(void *obj) {
  gui_item *view, *top, *bot;
  gui_item *grid;
  char str[0x1000];

  view = GuiWindowNew("object view");
  grid = GuiGridNew(1, 0);
  grid->flags |= GUI_FLAGS_CELL_CW;
  top = GuiObjPropTree(obj);
  GuiAddChild(grid, top);
#ifdef CFLAGS_GOOL_DEBUG
  bot = GuiObjCodeView(obj);
  GuiAddChild(grid, bot);
#endif
  GuiAddChild(view, grid);
  return view;
}

static gui_item *GuiDebugView() {
  gui_item *root, *text;
  gui_item *grid;
  gui_item *tree_view, *obj_view;
  gui_item *tmp;
  refl_value val;
  void *obj;

  root = GuiWindowNew("debug view");
  grid = GuiGridNew(0, 1);
  tree_view = GuiObjTreeView(&handles_root);
  obj_view = GuiObjView(0);
  GuiAddChild(grid, tree_view);
  GuiAddChild(grid, obj_view);
  GuiAddChild(root, grid);
  return root;
}

/** main **/
gui_item *root;
int gui_lock = 0;
extern int pad_lock;

void GuiInit() {
  pad_lock = 0;
  GuiIgInit();
  ReflInit(types);
  root = GuiDebugView();
}

void GuiUpdate() {
  if (GuiIgKeyPressed(526)) {
    root->flags = (~root->flags & GUI_FLAGS_VISIBLE);
  }
  if (GuiIgKeyPressed(512) && (root->flags & GUI_FLAGS_VISIBLE)) {
    gui_lock = !gui_lock;
    if (!gui_lock) {
      pad_lock = 3;
      GuiIgNavEnableKeyboard();
    }
    else {
      pad_lock = 0;
      GuiIgNavDisableKeyboard();
    }
  }
}

void GuiDraw() {
  GuiIgDraw(root);
}
