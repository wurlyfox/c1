/**
 * Gui implementation (for realtime in-game debugging).
 *
 * See section in /doc/files.md for more info.
 */
#include "ext/gui.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "ext/ext.h"
#include "ext/tex.h"
#include "ext/ns.h"

#include "ext/lib/refl.h"

#include "ns.h"
#include "gool.h"
#include "globals.h"
#include "../gfx.h"
#include <limits.h>

extern ns_struct ns;
extern gool_handle handles[8];
extern gool_object *objects;
extern gool_object *player;
extern entry *cur_zone;
extern zone_path *cur_path;

extern refl_type zone_path_type;

extern int32_t cam_offset_x, cam_offset_y, cam_offset_z;
extern int32_t cam_zoom, cam_speed;
extern int32_t dcam_accel, dcam_angvel, dcam_angvel2, dcam_rot_y1, dcam_rot_y2, dcam_trans_z;
extern int cam_offset_dir_x, cam_offset_dir_z;
extern ang cam_rot_before, cam_rot_after;
extern vec cam_rot_xz_dir;
extern int32_t cam_rot_xz, cur_progress;
extern mat16 mn_cam_rot, ms_cam_rot, ms_cam_rot2, mn_rot, ms_rot;
extern uint32_t screen_proj;

extern const quad28_t uv_map[600];

extern int pad_lock;
extern void GoolObjectPrintDebug(gool_object*, FILE*);

typedef struct {
  int vertex, edge;
  int32_t pos[3][3];
  float col[3][3];
  int flip_winding, xflip, yflip, winding;
  int tex_sel, cand_sel;
  int inherit_tex, inherit_col;
} wg_view_t;

typedef struct {
  gui_item *root;
  /* zone view */
  gui_item *zone_tree;
  entry *prev_zone;
  int zone_tree_invalid;
  /* gool view */
  gui_item *object_view, *object_tree;
  gool_object *sel_obj;
  /* world geometry view */
  wg_view_t wg_view;
  int wg_icon_count;
  /* camera view */
  int gui_cam_mode;
  gui_item *path_view;
  zone_path *path_shown;
  /* misc */
  int gui_lock;
  gui_font *title_font;
  ImGuiStyle default_style;
} gui_context_t;

gui_context_t gui_ctxt = { 0 };

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
  gool_object *obj;

  obj = (gool_object*)data;
  return GoolObjectName(obj, str);
}

/**
 * disassemble an object's bytecode
 */
static int GuiObjCodeUnparse(void *data, char *str) {
  gool_object *obj;

  if (data == 0) {
    sprintf(str, "no object selected");
    return 1;
  }
  obj = (gool_object*)data;
  if (obj->handle.type != 1) {
    sprintf(str, "non-handle object selected");
    return 1;
  }
  GoolObjectSummaryDebug(obj, str);
  return 1;
}

static gui_item *GuiNodeContent(gui_item *item) {
  gui_item *it;

  for (it=item->child;it;it=it->next) {
    if (it->flags & GUI_FLAGS_NODE_CONTENT) { break; }
  }
  return it;
}

static int GuiEIDParse(char *str, void *data) {
  *(eid_t*)data = NSStringToEID(str);
  return 1;
}

static int GuiEIDUnparse(void *data, char *str) {
  entry *en;
  char *res;
  uint32_t val;
  
  val = *(uint32_t*)data;
  if (val&1) {
    res = NSEIDToString(*(eid_t*)data);
    strcpy(str, res);
  }
  else if (val) {
    en = *((entry**)data);
    res = NSEIDToString(en->eid);
    sprintf(str, "%s [loaded]", res);
  }
  else {
    strcpy(str, "zero");
  }
  return 1;
}

gui_dtype gui_eid = {
  .is_prim = 0,
  .parse = (gui_data_parse_t)GuiEIDParse,
  .unparse = (gui_data_unparse_t)GuiEIDUnparse
};

/** structure field => gui item mappers **/
static gui_item *GuiReflScalar(refl_value *val) {
  gui_item *item;
  gui_ptype *ptype;
  char *typename;

  typename = val->field->type->name;
  if (strcmp(typename, "eid_t") == 0) {
    item = GuiTextNew(0, 0);
    item->flags |= GUI_FLAGS_READONLY;
    item->t->data = val->res;
    item->t->dtype = gui_eid;
    return item;
  }
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
  else { ptype = &gui_u8; } /* default to uint8_t */
  item = GuiScalarNew(1, val->res, ptype, 0);
  return item;
}

static gui_item *GuiReflGroup(gui_item **children, int flags) {
  gui_item *item, *it, *content;
  int i, once;

  if (!children) { return (gui_item*)-1; }
  item = GuiGroupNew();
  once = 0;
  for (i=0;(it=children[i]);i++) {
    /* skip children that we chose to not map */
    if ((int)it==-1) { continue; }
    content = GuiNodeContent(it);
    if (!content) { continue; }
    // content->label = it->label;
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
  for (i=0;(it=children[i]);i++) {
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
  float orig;
  int i;

  color = out;
  for (i=0;i<3;i++) {
    orig = (float)color->c[i] / 255;
    if (memcmp(&in[i], &orig, sizeof(orig)) != 0)
      color->c[i] = (uint8_t)(in[i] * 255);
  }
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
  float orig;
  int i;

  color = out;
  for (i=0;i<3;i++) {
    orig = (float)color->c[i] / 65535;
    if (memcmp(&in[i], &orig, sizeof(orig)) != 0)
      color->c[i] = (uint16_t)(in[i] * 65535);
  }
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
  double value;
  float orig;
  int i;

  color = out;
  for (i=0;i<3;i++) {
    orig = (float)color->c[i] / 0xFFFFFFFF;
    if (memcmp(&in[i], &orig, sizeof(orig)) != 0) {
      value = limit(in[i], 0.0f, 1.0f);
      color->c[i] = (uint32_t)(value * UINT32_MAX);
    }
  }
}

static gui_item *GuiReflRgb8(gui_item **children) {
  gui_item *item, *content;
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
  gui_item *item, *content;
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
  gui_item *item, *content;
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
  gui_item *item;

  item = GuiTextNew(0, 0);
  item->flags |= GUI_FLAGS_READONLY;
  item->t->data = *(void**)val->res;
  item->t->dtype.unparse = GuiUnparseObjPtr;
  return item;
}

static gui_item *GuiMapField(refl_value *val, refl_path *path, void **mapped) {
  gui_item *node, *item, **children;
  refl_value pval;
  char *typename;
  int count, label;

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

static gui_item *GuiMapObjField(refl_value *val, refl_path *path, void **mapped) {
  const char *rnames[9] = { "data", "regs", "memory",
    "colors_i", "vectors_v", "vectors_a", "l", "c", "a" };
  int i;

  for (i=0;i<9;i++) { /* skip rejected fields */
    if (strcmp(val->field->name, rnames[i]) == 0)
      return (gui_item*)-1;
  }
  return GuiMapField(val, path, mapped);
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
  tree->flags |= GUI_FLAGS_HSCROLL;
  if (obj==0) { return tree; }
  gool_object_type = ReflGetType(0, "gool_object");
  items = (gui_item**)ReflMap(obj, gool_object_type, (refl_map_t)GuiMapObjField);
  for (i=0;(it=items[i]);i++) {
    if ((int)it == -1) { continue; }
    GuiAddChild(tree, it);
  }
  free(items);
  return tree;
}

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
  view->flags |= GUI_FLAGS_HSCROLL;
  text = GuiObjCodeText(obj);
  buttons = GuiGridNew(3,3);
  buttons->flags |= GUI_FLAGS_HSCROLL;
  item = GuiButtonNew("Break", GuiObjBreak);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Resume", GuiObjContinue);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Step", GuiObjStep);
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Break T", GuiObjBreakTrans);
  GuiSetTip(item, "Break trans thread");
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Resume T", GuiObjContinueTrans);
  GuiSetTip(item, "Continue trans thread");
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Step T", GuiObjStepTrans);
  GuiSetTip(item, "Step trans thread");
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Break C", GuiObjBreakCode);
  GuiSetTip(item, "Break code thread");
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Resume C", GuiObjContinueCode);
  GuiSetTip(item, "Continue code thread");
  GuiAddChild(buttons, item);
  item = GuiButtonNew("Step C", GuiObjStepCode);
  GuiSetTip(item, "Step code thread");
  GuiAddChild(buttons, item);
  GuiAddChild(view, text);
  GuiAddChild(view, buttons);
  return view;
}

/** specific views **/
static gui_item* GuiObjView(void*);

static void GuiSelectObject(gool_object *obj) {
  gui_item *node, *new;

  if (!gui_ctxt.object_tree || !gui_ctxt.object_view) 
    return;
  node = obj ? GuiTreeNode(gui_ctxt.object_tree, obj) : 0;
  gui_ctxt.object_tree->tr->selected = node;
  gui_ctxt.object_tree->tr->sel = obj;
  new = GuiObjView(obj);
  GuiItemReplace(gui_ctxt.object_view, new);
  gui_ctxt.object_view = new;
  gui_ctxt.sel_obj = obj;
}

static void GuiObjSelect(gui_item *item) {
  gui_tree *tree;
  gool_handle *handle;

  tree = item->tr;
  handle = tree->sel;
  if (handle && handle->type == 1) {
    ExtSelectObject(&ext_ctxt, tree->sel);
    GuiSelectObject(tree->sel);
  }
}

static gui_item *GuiObjTreeView(void *obj) {
  gui_item *view, *tree;

  view = GuiWindowNew("object tree view");
  view->flags |= GUI_FLAGS_HSCROLL;
  tree = GuiObjTree(obj, GuiObjSelect);
  gui_ctxt.object_tree = tree;
  GuiAddChild(view, tree);
  return view;
}

static gui_item *GuiObjView(void *obj) {
  gui_item *view, *top, *bot;
  gui_item *grid;

  view = GuiGroupNew();
  GuiAddLabel(view, "object view");
  grid = GuiGridNew(1, 0);
  grid->flags |= GUI_FLAGS_CELL_CW;
  top = GuiObjPropTree(obj);
  GuiAddChild(grid, top);
  bot = GuiObjCodeView(obj);
  GuiAddChild(grid, bot);
  GuiAddChild(view, grid);
  return view;
}

static void GuiFullscreenSync(gui_item *item, int flags) {
  if (flags & GUI_SYNC_INTERNAL)
    ExtSetFullscreen(&ext_ctxt, ext_config.fullscreen);
}

static gui_item *GuiDisplayConfigView() {
  gui_item *view, *grid, *item, *list;
  static const char *fps_modes[] = {
    "30 FPS (PS1 Native)",
    "60 FPS",
    "120 FPS",
    "Uncapped"
  };
  static const char *aspect_modes[] = {
    "4:3 Original",
    "16:9 Widescreen"
  };

  view = GuiGroupNew();
  GuiAddLabel(view, "Display Settings");
  grid = GuiGroupNew();
  
  list = GuiComboNew("Frame Rate", &ext_config.fps_mode, 
    &gui_s32, fps_modes, 4);
  list->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, list);

  list = GuiComboNew("Aspect Ratio", &ext_config.aspect_mode, 
    &gui_s32, aspect_modes, 2);
  list->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, list);

  item = GuiToggleNew(0, &ext_config.fullscreen, "Fullscreen (F11)");
  item->sync = GuiFullscreenSync;
  GuiAddChild(grid, item);

  item = GuiToggleNew(0, &ext_config.use_title_font, 
    "Use Title Screen GUI Font");
  GuiAddChild(grid, item);

  GuiAddChild(grid, GuiSeparatorNew());
  item = GuiTextNew("Polygon Raster Mode:", 0);
  item->flags |= GUI_FLAGS_READONLY;
  GuiAddChild(grid, item);

  item = GuiToggleNew(0, &ext_config.enable_hwpersp, 
    "Enable Native Perspective Transform");
  GuiAddChild(grid, item);
  item = GuiToggleNew(0, &ext_config.enable_textures, 
    "Enable Textures");
  GuiAddChild(grid, item);
  item = GuiToggleNew(0, &ext_config.enable_gouraud, 
    "Enable Gouraud Shading");
  GuiAddChild(grid, item);
  item = GuiToggleNew(0, &ext_config.enable_wireframe, 
    "Enable Wireframe");
  GuiAddChild(grid, item);
  
  GuiAddChild(grid, GuiSeparatorNew());
  item = GuiTextNew("Debug Overlays:", 0);
  item->flags |= GUI_FLAGS_READONLY;
  GuiAddChild(grid, item);

  item = GuiToggleNew(0, &ext_config.show_fps, 
    "Show FPS Counter");
  GuiAddChild(grid, item);
  item = GuiToggleNew(0, &ext_config.show_octrees, 
    "Show Octree Volumes");
  GuiAddChild(grid, item);
  item = GuiToggleNew(0, &ext_config.show_objbounds, 
    "Show Object Bounding Boxes");
  GuiAddChild(grid, item);
  item = GuiToggleNew(0, &ext_config.show_wallmap, 
    "Show Wall Collision Bitmap");
  GuiAddChild(grid, item);

  GuiAddChild(view, grid);
  return view;
}

static gui_item *GuiDebugView() {
  gui_item *root;
  gui_item *grid;
  gui_item *tree_view, *obj_view;
  gui_item *group, *item;

  root = GuiWindowNew("debug view");
  group = GuiGroupNew();
  item = GuiToggleNew(0, &ext_ctxt.options.object_mouse_control, 
    "Enable object mouse control");
  GuiSetTip(item, 
    "Highlight objects under the mouse, select them, "
    "and drag them onto world surfaces.");
  GuiAddChild(group, item);
  GuiAddChild(root, group);
  grid = GuiGridNew(0, 1);
  grid->g->header_weight = 0.65f;
  tree_view = GuiObjTreeView(&handles_root);
  obj_view = GuiObjView(0);
  gui_ctxt.object_view = obj_view;
  GuiAddChild(grid, tree_view);
  GuiAddChild(grid, obj_view);
  GuiAddChild(root, grid);
  return root;
}

static gui_item *GuiMapZoneField(refl_value *val, refl_path *path, void **mapped) {
  const char *rnames[7] = { "nodes", "points", "path_points", "pad", "l", "c", "a" };
  int i;

  for (i=0;i<7;i++) { /* skip rejected fields */
    if (strcmp(val->field->name, rnames[i]) == 0)
      return (gui_item*)-1;
  }
  return GuiMapField(val, path, mapped);
}

static gui_item *GuiZoneTree(void *obj) {
  refl_type *zone_type;
  gui_item *tree, **items, *it;
  int i;

  tree = GuiTreeNew(0, 0);
  GuiAddLabel(tree, "Zone");
  tree->tr->column_count = 1;
  if (obj==0) { return tree; }
  zone_type = ReflGetType(0, "zone");
  items = (gui_item**)ReflMap(obj, zone_type, (refl_map_t)GuiMapZoneField);
  for (i=0;(it=items[i]);i++) {
    if ((int)it == -1) { continue; }
    GuiAddChild(tree, it);
  }
  free(items);
  return tree;
}

static void GuiLevelViewUpdate(gui_item *view) {
  gui_item *old_tree;

  if (!gui_ctxt.zone_tree_invalid) { 
    return; 
  }
  old_tree = gui_ctxt.zone_tree;
  gui_ctxt.zone_tree = GuiZoneTree(cur_zone);
  GuiItemReplace(old_tree, gui_ctxt.zone_tree);
  gui_ctxt.zone_tree_invalid = 0;
}

static gui_item *GuiLevelView() {
  gui_item *view, *grid;
  gui_item *item;

  view = GuiGroupNew();
  view->update = GuiLevelViewUpdate;
  GuiAddLabel(view, "Level and Spatial");
  grid = GuiGridNew(1, 0);

  item = GuiZoneTree(0);
  gui_ctxt.zone_tree = item;
  GuiAddChild(grid, item);
  GuiAddChild(view, grid);
  return view;
}

/** World geometry view update callbacks **/
#define WG_FLAGS_EDIT   1 /* shown only in edit mode */
#define WG_FLAGS_POLY   2 /* disabled without a selected polygon */
#define WG_FLAGS_MANUAL 4 /* disabled unless edit mode and a new polygon is selected */
#define WG_FLAGS_VERT   8 /* disabled without a selected vertex */

static void GuiWgGate(gui_item *item) {
  ext_options_t *options; 
  ext_polyref_t ref;
  ext_poly_t poly;
  int flags, valid, vertex, enabled;

  options = &ext_ctxt.options;
  flags = (int)(intptr_t)item->user;
  valid = ExtGetSelectedPoly(&ext_ctxt, &ref, &poly);
  vertex = ExtGetSelectedVertex(&ext_ctxt);
  enabled = 1;
  if ((flags & WG_FLAGS_EDIT) && !options->enable_edit)
    enabled = 0;
  if ((flags & WG_FLAGS_POLY) && !valid)
    enabled = 0;
  if ((flags & WG_FLAGS_MANUAL) && !(options->enable_edit && valid && ref.manual))
    enabled = 0;
  if ((flags & WG_FLAGS_VERT) && (!valid || vertex < 0 || vertex >= 3))
    enabled = 0;
  GuiSetEnabled(item, enabled);
}

static void GuiStatusText(gui_item *item) {
  snprintf(item->t->str, item->t->maxlen, "%s", 
    ExtGetStatus(&ext_ctxt));
}

static void GuiWgPolygonText(gui_item *item) {
  ext_polyref_t ref;
  ext_poly_t poly;

  if (!ExtGetSelectedPoly(&ext_ctxt, &ref, &poly)) {
    item->t->str[0] = 0;
    return;
  }
  snprintf(item->t->str, item->t->maxlen,
           ref.manual ? "New polygon #%d" : "World polygon %d", ref.poly_idx);
}

static void GuiWgWorldText(gui_item *item) {
  ext_polyref_t ref;
  ext_poly_t poly;
  char name[64];

  if (!ExtGetSelectedPoly(&ext_ctxt, &ref, &poly)) {
    item->t->str[0] = 0;
    return;
  }
  if (ExtWorldName(poly.world_eid, name))
    snprintf(item->t->str, item->t->maxlen, "%s", name);
  else
    snprintf(item->t->str, item->t->maxlen, "(not in current zone)");
}

static void GuiWgDescribeTexture(const texinfo *tex, char *out, size_t size) {
  int w, h;

  UvGetDimensions(tex->uv_idx, &w, &h);
  snprintf(out, size, "%dx%d seg %d off (%d,%d) mode %d",
           w, h, (int)tex->segment, (int)tex->offs_x,
           (int)tex->offs_y, (int)tex->color_mode);
}

static void GuiWgTextureText(gui_item *item) {
  ext_polyref_t ref;
  ext_poly_t poly;

  if (!ExtGetSelectedPoly(&ext_ctxt, &ref, &poly))
    snprintf(item->t->str, item->t->maxlen, "-");
  else if (!poly.has_tinfo)
    snprintf(item->t->str, item->t->maxlen, "none");
  else
    GuiWgDescribeTexture(&poly.tinfo, item->t->str, item->t->maxlen);
}

static void GuiWgUvText(gui_item *item) {
  const quad28_t *uvs;
  texinfo tex;

  if (!ExtGetSelectedTexture(&ext_ctxt, &tex)) {
    item->t->str[0] = 0;
    return;
  }
  uvs = &uv_map[tex.uv_idx];
  snprintf(item->t->str, item->t->maxlen, 
           "uv (%d,%d) (%d,%d) (%d,%d)",
           uvs[0][0].x, uvs[0][0].y, uvs[0][1].x, 
           uvs[0][1].y, uvs[0][2].x, uvs[0][2].y);
}

static void GuiWgZoneStatsText(gui_item *item) {
  const ext_stats_t *stats;
  
  stats = ExtGetStats(&ext_ctxt);
  if (ExtZoneStatsValid(&ext_ctxt))
    snprintf(item->t->str, item->t->maxlen, 
             "avg %.1f%%  min %.1f%%",
             stats->zone_avg_coverage * 100.0, 
             stats->zone_min_coverage * 100.0);
  else
    snprintf(item->t->str, item->t->maxlen, "not computed");
}

static void GuiWgLevelStatsText(gui_item *item) {
  const ext_stats_t *stats;
  
  stats = ExtGetStats(&ext_ctxt);
  if (stats->level_samples > 0)
    snprintf(item->t->str, item->t->maxlen, 
             "avg %.1f%%  min %.1f%%",
             stats->level_avg_coverage * 100.0, 
             stats->level_min_coverage * 100.0);
  else
    snprintf(item->t->str, item->t->maxlen, "not computed");
}

static void GuiWgLevelSamplesText(gui_item *item) {
  const ext_stats_t *stats;
  
  stats = ExtGetStats(&ext_ctxt);
  if (stats->level_samples > 0)
    snprintf(item->t->str, item->t->maxlen, 
             "%d zones, %d samples",
             stats->level_zones, stats->level_samples);
  else
    item->t->str[0] = 0;
}

static void GuiWgCandidatesText(gui_item *item) {
  const ext_polyref_t *cand;
  int count, sel;

  count = ExtGetCandidateCount(&ext_ctxt);
  sel = ExtGetSelectedCandidate(&ext_ctxt);
  cand = ExtGetCandidate(&ext_ctxt, sel);
  if (cand)
    snprintf(item->t->str, item->t->maxlen, 
             "candidate %d/%d: poly %d",
             sel + 1, count, cand->poly_idx);
  else if (!ext_ctxt.options.enable_candidates)
    snprintf(item->t->str, item->t->maxlen, "candidates disabled");
  else
    snprintf(item->t->str, item->t->maxlen, 
             "%d candidates at current camera", count);
}

static void GuiWgVertexRow(gui_item *item) {
  ext_polyref_t ref;
  ext_poly_t poly;
  int vertex, idx, visible;

  idx = (int)item->user;
  vertex = ExtGetSelectedVertex(&ext_ctxt);
  if (ExtGetSelectedPoly(&ext_ctxt, &ref, &poly)
   && (vertex < 0 || vertex == idx))
    visible = 1;
  else
    visible = 0;
  if (visible) item->flags |= GUI_FLAGS_VISIBLE; 
  else item->flags &= ~GUI_FLAGS_VISIBLE;
}

static void GuiWgVertexPosEdit(gui_item *item) {
  ext_polyref_t ref;
  ext_poly_t poly;
  int idx, enabled;
  
  idx = (int)item->user;
  enabled = ext_ctxt.options.enable_edit 
         && ExtGetSelectedPoly(&ext_ctxt, &ref, &poly)
         && ref.manual && !poly.vertex_locked[idx];
  GuiSetEnabled(item, enabled);
}

static void GuiWgVertexColEdit(gui_item *item) {
  ext_polyref_t ref;
  ext_poly_t poly;
  int enabled;

  enabled = ext_ctxt.options.enable_edit 
         && ExtGetSelectedPoly(&ext_ctxt, &ref, &poly)
         && ref.manual;
  GuiSetEnabled(item, enabled);
}

static void GuiWgPosSync(gui_item *item, int flags) {
  vec pos;
  int idx;

  idx = (int)item->user;
  if (flags & GUI_SYNC_INTERNAL) {
    pos = (vec){ .x = gui_ctxt.wg_view.pos[idx][0], .y = gui_ctxt.wg_view.pos[idx][1], .z = gui_ctxt.wg_view.pos[idx][2] };
    ExtSetVertexPos(&ext_ctxt, idx, &pos);
  }
  if (flags & GUI_SYNC_EXTERNAL) {
    if (ExtGetVertexPos(&ext_ctxt, idx, &pos)) {
      gui_ctxt.wg_view.pos[idx][0] = pos.x;
      gui_ctxt.wg_view.pos[idx][1] = pos.y;
      gui_ctxt.wg_view.pos[idx][2] = pos.z;
    } 
  }
}

static void GuiWgColSync(gui_item *item, int flags) {
  rgba color;
  uint32_t c;
  float f;
  int i, idx;

  idx = (int)item->user;
  if (flags & GUI_SYNC_INTERNAL) {
    for (i=0;i<3;i++) {
      f = gui_ctxt.wg_view.col[idx][i];
      c = (uint32_t)(limit(f, 0, 1) * 255.0f + 0.5f);
      color.c[i] = c * 0x01010101u;
    }
    color.a = ~0u;
    ExtSetVertexColor(&ext_ctxt, idx, &color);
  }
  if (flags & GUI_SYNC_EXTERNAL) {
    if (ExtGetVertexColor(&ext_ctxt, idx, &color)) {
      for (i=0;i<3;i++)
        gui_ctxt.wg_view.col[idx][i] = (float)(color.c[i] >> 24) / 255.0f;
    }
  }
}

static void GuiWgVertexSel(gui_item *item, int flags) {
  if (flags & GUI_SYNC_INTERNAL) 
    ExtSelectVertex(&ext_ctxt, gui_ctxt.wg_view.vertex - 1);
  if (flags & GUI_SYNC_EXTERNAL)
    gui_ctxt.wg_view.vertex = ExtGetSelectedVertex(&ext_ctxt) + 1;
}

static void GuiWgEdgeSel(gui_item *item, int flags) {
  if (flags & GUI_SYNC_INTERNAL) 
    ExtSelectEdge(&ext_ctxt, gui_ctxt.wg_view.edge);
  if (flags & GUI_SYNC_EXTERNAL) 
    gui_ctxt.wg_view.edge = ExtGetSelectedEdge(&ext_ctxt);
}

static void GuiWgWinding(gui_item *item, int flags) {
  texinfo tex;
  if (flags & GUI_SYNC_INTERNAL) 
    ExtSetFlipWinding(&ext_ctxt, gui_ctxt.wg_view.flip_winding);
  if (flags & GUI_SYNC_EXTERNAL) {
    gui_ctxt.wg_view.flip_winding = 0;
    if (ExtGetSelectedTexture(&ext_ctxt, &tex))
      UvGetFlipWinding(tex.uv_idx, &gui_ctxt.wg_view.flip_winding);
  }
}

static void GuiWgWindParts(gui_item *item, int flags) {
  texinfo tex;
  int xflip, yflip, winding;
  
  xflip = 0; 
  yflip = 0;
  winding = 0;
  if (!ExtGetSelectedTexture(&ext_ctxt, &tex)) {
    if (flags & GUI_SYNC_EXTERNAL) {
      if (item->type == list) 
        gui_ctxt.wg_view.winding = 0;
      else if (item->tg->data == &gui_ctxt.wg_view.xflip) 
        gui_ctxt.wg_view.xflip = 0;
      else 
        gui_ctxt.wg_view.yflip = 0;
    }
    return;
  }
  UvGetFlip(tex.uv_idx, &xflip, &yflip);
  UvGetWinding(tex.uv_idx, &winding);
  if (flags & GUI_SYNC_INTERNAL) {
    if (item->type == list) 
      winding = gui_ctxt.wg_view.winding;
    else if (item->tg->data == &gui_ctxt.wg_view.xflip) 
      xflip = gui_ctxt.wg_view.xflip;
    else 
      yflip = gui_ctxt.wg_view.yflip;
    ExtSetWindingParts(&ext_ctxt, xflip, yflip, winding);
  }
  if (flags & GUI_SYNC_EXTERNAL) {
    if (item->type == list) 
      gui_ctxt.wg_view.winding = winding;
    else if (item->tg->data == &gui_ctxt.wg_view.xflip) 
      gui_ctxt.wg_view.xflip = xflip;
    else 
      gui_ctxt.wg_view.yflip = yflip;
  }
}

static void GuiWgExtrude(gui_item *item) { 
  ExtExtrudeSelected(&ext_ctxt); 
}

static void GuiWgDelete(gui_item *item) { 
  ExtDeleteSelected(&ext_ctxt); 
}

static void GuiWgInheritTex(gui_item *item, int flags) {
  if (flags & GUI_SYNC_INTERNAL) 
    ExtInheritTexToggle(&ext_ctxt);
  if (flags & GUI_SYNC_EXTERNAL)
    gui_ctxt.wg_view.inherit_tex = ExtGetState(&ext_ctxt)->inherit_target >= 0;
}

static void GuiWgInheritCol(gui_item *item, int flags) {
  if (flags & GUI_SYNC_INTERNAL) 
    ExtInheritColorToggle(&ext_ctxt);
  if (flags & GUI_SYNC_EXTERNAL)
    gui_ctxt.wg_view.inherit_col = ExtGetState(&ext_ctxt)->inherit_color_target >= 0;
}

static void GuiWgSave(gui_item *item) { 
  ExtSaveJson(&ext_ctxt); 
}

static void GuiWgLoad(gui_item *item) { 
  ExtLoadJson(&ext_ctxt); 
}

static void GuiWgZoneStats(gui_item *item) { 
  ExtRecomputeStats(&ext_ctxt, 0); 
}

static void GuiWgLevelStats(gui_item *item) { 
  ExtRecomputeStats(&ext_ctxt, 1); 
}

static void GuiWgTexSel(gui_item *item, int flags) {
  texinfo tex;

  if (flags & GUI_SYNC_INTERNAL) 
    ExtApplyTexture(&ext_ctxt, gui_ctxt.wg_view.tex_sel);
  if (flags & GUI_SYNC_EXTERNAL)
    gui_ctxt.wg_view.tex_sel = ExtGetSelectedTexture(&ext_ctxt, &tex)
                             ? ExtGetTextureIndex(&ext_ctxt, &tex) : -1;
}

static void GuiWgCandSel(gui_item *item, int flags) {
  if (flags & GUI_SYNC_INTERNAL) 
    ExtSelectCandidate(&ext_ctxt, gui_ctxt.wg_view.cand_sel);
  if (flags & GUI_SYNC_EXTERNAL) 
    gui_ctxt.wg_view.cand_sel = ExtGetSelectedCandidate(&ext_ctxt);
}

static void GuiWgCandPrev(gui_item *item) { 
  int sel, count, idx;

  sel = ExtGetSelectedCandidate(&ext_ctxt);
  count = ExtGetCandidateCount(&ext_ctxt);
  idx = sel <= 0 ? count - 1 : sel - 1; 
  ExtSelectCandidate(&ext_ctxt, idx);
}

static void GuiWgCandNext(gui_item *item) { 
  int sel, count, idx;

  sel = ExtGetSelectedCandidate(&ext_ctxt);
  count = ExtGetCandidateCount(&ext_ctxt);
  idx = sel + 1 >= count ? 0 : sel + 1;
  ExtSelectCandidate(&ext_ctxt, idx);
}

static int GuiWgIcon(const texinfo *tex, float *uv0, float *uv1) {
  fbound ext;
  int res;

  res = TextureBlockLoad(tex, &ext);
  if (res < 0)
    return res;
  uv0[0] = ext.p1.x; uv0[1] = ext.p1.y;
  uv1[0] = ext.p2.x; uv1[1] = ext.p2.y;
  return res;
}

static int GuiWgIconCur(gui_item *item, int idx, float *uv0, float *uv1, char *tip) {
  texinfo tex;

  if (!ExtGetSelectedTexture(&ext_ctxt, &tex)) 
    return -1;
  GuiWgDescribeTexture(&tex, tip, 128);
  return GuiWgIcon(&tex, uv0, uv1);
}

static int GuiWgIconSub(gui_item *item, int idx, float *uv0, float *uv1, char *tip) {
  const texinfo *tex;
  
  tex = ExtGetTexture(&ext_ctxt, idx);
  if (!tex) 
    return -1;
  GuiWgDescribeTexture(tex, tip, 128);
  return GuiWgIcon(tex, uv0, uv1);
}

static void GuiWgIconGridSync(gui_item *list) {
  gui_item *it, *next, *img;
  int i, count;
  
  GuiWgGate(list);
  count = ExtGetTextureCount(&ext_ctxt);
  if (gui_ctxt.wg_icon_count == count) 
    return;
  gui_ctxt.wg_icon_count = count;
  list->l->selected = 0;
  for (it=list->child;it;it=next) {
    next = it->next;
    GuiRemoveChild(list, it);
    GuiItemFree(it, 1);
  }
  for (i=0;i<count;i++) {
    img = GuiImageNew(48.0f, GuiWgIconSub);
    img->im->index = i;
    img->flags |= GUI_FLAGS_BUTTON;
    GuiAddChild(list, img);
  }
}

static void GuiWgCandRange(gui_item *item) {
  int count;
  
  count = ExtGetCandidateCount(&ext_ctxt);
  item->s->vmin.s32 = 0;
  item->s->vmax.s32 = count > 1 ? count - 1 : 1;
  GuiSetEnabled(item, count > 0);
}

static void GuiWgCandBtn(gui_item *item) {
  int count;
 
  count = ExtGetCandidateCount(&ext_ctxt);
  GuiSetEnabled(item, count > 0);
}

/** World geometry view GUI control helpers **/
static gui_item *GuiWgText(gui_item *parent, char *label, gui_handler_t update) {
  gui_item *item;
  
  item = GuiTextNew("", label);
  item->flags |= GUI_FLAGS_READONLY | GUI_FLAGS_FORM;
  item->update = update;
  GuiAddChild(parent, item);
  return item;
}

static gui_item *GuiGated(gui_item *item, int mode, char *tip) {
  item->update = GuiWgGate;
  item->user = (void*)(intptr_t)mode;
  if (tip) GuiSetTip(item, tip);
  return item;
}

/** World geometry view implementation **/
static void GuiWgModeSection(gui_item *grid) {
  ext_options_t *options;
  gui_item *sec, *item, *row;

  options = &ext_ctxt.options;

  sec = GuiHeaderNew("Mode and overlay", 1);
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, sec);

  item = GuiToggleNew(0, &options->enable_pick, 
    "Enable picking and overlay");
  GuiSetTip(item, "Click polygons/vertices in the view to select them");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);
  
  item = GuiToggleNew(0, &options->show_normals, "Normals");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);
  
  item = GuiToggleNew(0, &options->show_vertices, "Vertices");
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);

  item = GuiToggleNew(0, &options->enable_hover, 
    "Highlight polygon under mouse");
  GuiSetTip(item, "Cyan preview of the polygon a click would select");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiToggleNew(0, &options->enable_edit, "Edit mode");
  GuiSetTip(item, 
    "Enables extrude/inherit/delete and editing of new polygons");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);
}

static void GuiWgVertexEdit(gui_item *sec, int idx) {
  wg_view_t *view;
  gui_item *item, *row;
  char name[16];

  view = &gui_ctxt.wg_view;

  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  row->update = GuiWgVertexRow;
  row->user = (void*)idx;
  
  sprintf(name, "v%d pos", idx);
  item = GuiScalarVecNew(1, view->pos[idx], &gui_s32, 3, "%d", name);
  item->s->vmin.s32 = -0x7FFFFFF; 
  item->s->vmax.s32 = 0x7FFFFFF; 
  item->s->step = 4.0f;
  item->sync = GuiWgPosSync;
  item->user = (void*)idx;
  item->update = GuiWgVertexPosEdit;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);

  sprintf(name, "v%d color", idx);
  item = GuiColorNew(view->col[idx], 0, 0);
  GuiAddLabel(item, name);
  item->sync = GuiWgColSync;
  item->user = (void*)idx;
  item->update = GuiWgVertexColEdit;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);

  GuiAddChild(sec, row);
}

static void GuiWgSelectionSection(gui_item *grid) {
  static const char *edges[] = { "v0 - v1", "v1 - v2", "v2 - v0" };
  static const char *verts[] = { "whole polygon", "v0", "v1", "v2" };
  static const char *perms[] = { "012", "021", "102", "120", "201", "210" };
  wg_view_t *view;
  gui_item *sec, *item, *row, *opt;
  int i;

  view = &gui_ctxt.wg_view;

  sec = GuiHeaderNew("Selected polygon", 1);
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, sec);

  GuiWgText(sec, "Polygon", GuiWgPolygonText);
  GuiWgText(sec, "World", GuiWgWorldText);
  
  item = GuiComboNew("Vertex", &view->vertex, &gui_s32, verts, 4);
  item->flags |= GUI_FLAGS_FORM;
  item->sync = GuiWgVertexSel;
  item->update = GuiWgGate; 
  item->user = (void*)WG_FLAGS_POLY;
  GuiAddChild(sec, item);
  
  item = GuiComboNew("Edge", &view->edge, &gui_s32, edges, 3);
  GuiGated(item, WG_FLAGS_EDIT | WG_FLAGS_POLY, "Edge used by Extrude");
  item->flags |= GUI_FLAGS_FORM;
  item->sync = GuiWgEdgeSel;
  GuiAddChild(sec, item);

  item = GuiImageNew(64.0f, GuiWgIconCur);
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  GuiWgText(sec, "Texture", GuiWgTextureText);
  
  item = GuiScalarNew(1, &view->flip_winding, &gui_s32, "FlipWinding");
  GuiGated(item, WG_FLAGS_MANUAL, 
    "Flip + winding index (0-23) of the UV map");
  item->s->vmin.s32 = 0; 
  item->s->vmax.s32 = 23; 
  item->s->step = 0.2f;
  item->sync = GuiWgWinding;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);
  
  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);

  item = GuiToggleNew(0, &view->xflip, "Flip X");
  GuiGated(item, WG_FLAGS_MANUAL, 0);
  item->sync = GuiWgWindParts;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);

  item = GuiToggleNew(0, &view->yflip, "Flip Y");
  GuiGated(item, WG_FLAGS_MANUAL, 0);
  item->sync = GuiWgWindParts;
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);
  
  item = GuiListNew(2, "Winding");
  GuiGated(item, WG_FLAGS_MANUAL, 
    "Vertex-to-corner permutation of the UV triangle");
  item->flags |= GUI_FLAGS_FORM;
  item->l->data = &view->winding;
  item->l->ptype = &gui_s32;
  item->sync = GuiWgWindParts;
  for (i=0;i<6;i++) {
    opt = GuiTextNew((char*)perms[i], 0);
    opt->flags |= GUI_FLAGS_READONLY;
    GuiAddChild(item, opt);
  }
  GuiListSync(item, 1);
  GuiAddChild(sec, item);

  GuiWgText(sec, "UVs", GuiWgUvText);

  for (i=0;i<3;i++) {
    GuiWgVertexEdit(sec, i);
  }
}

static void GuiWgEditSection(gui_item *grid) {
  ext_options_t *options;
  wg_view_t *view;
  gui_item *sec, *item, *row;

  options = &ext_ctxt.options;
  view = &gui_ctxt.wg_view;

  sec = GuiHeaderNew("Edit", 1);
  sec->update = GuiWgGate; 
  sec->user = (void*)WG_FLAGS_EDIT;
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, sec);

  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);

  item = GuiButtonNew("Extrude", GuiWgExtrude);
  GuiGated(item, WG_FLAGS_POLY, 
    "Auto-extrude a new polygon from the selected edge");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);

  item = GuiToggleNew(1, &view->inherit_tex, "Inherit");
  GuiSetTip(item, 
    "Click another polygon to copy its subtexture and winding");
  item->sync = GuiWgInheritTex;
  item->update = GuiWgGate;
  item->user = (void*)WG_FLAGS_MANUAL;
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);

  item = GuiToggleNew(1, &view->inherit_col, "Inherit color");
  GuiSetTip(item, 
    "Click a vertex to copy its color onto the selected vertex");
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  item->sync = GuiWgInheritCol;
  item->update = GuiWgGate;
  item->user = (void*)(WG_FLAGS_MANUAL | WG_FLAGS_VERT);
  GuiAddChild(row, item);
  
  item = GuiButtonNew("Delete", GuiWgDelete);
  GuiGated(item, WG_FLAGS_MANUAL, "Delete the selected new polygon");
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);
  
  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);
  
  item = GuiToggleNew(0, &options->enable_snap, "Snap");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);
  
  item = GuiToggleNew(0, &options->enable_snap_color, "Snap inherits color");
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);
  
  item = GuiTextNew(0, "JSON file");
  item->t->data = ext_ctxt.json_path;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);

  item = GuiButtonNew("Load", GuiWgLoad);
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);

  item = GuiButtonNew("Save", GuiWgSave);
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);

  item = GuiListNew(3, 0);
  GuiGated(item, WG_FLAGS_MANUAL, 
    "Textures used in this zone; click to apply to the selected new polygon");
  item->l->height = 220.0f;
  item->l->data = &view->tex_sel;
  item->l->ptype = &gui_s32;
  item->sync = GuiWgTexSel;
  item->update = GuiWgIconGridSync;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);  
}

static void GuiWgCoverageSection(gui_item *grid) {
  ext_options_t *options;
  wg_view_t *view;
  gui_item *sec, *item, *row;

  options = &ext_ctxt.options;
  view = &gui_ctxt.wg_view;
  sec = GuiHeaderNew("Widescreen coverage", 0);
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, sec);

  GuiWgText(sec, "Zone", GuiWgZoneStatsText);
  GuiWgText(sec, "Level", GuiWgLevelStatsText);
  GuiWgText(sec, "", GuiWgLevelSamplesText);
  
  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);
  
  item = GuiButtonNew("Recompute zone", GuiWgZoneStats);
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);
  
  item = GuiButtonNew("Recompute level", GuiWgLevelStats);
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);
  
  item = GuiToggleNew(0, &options->enable_auto_edge, 
    "Auto-select edge bordering uncovered space");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiToggleNew(0, &options->enable_candidates, 
    "Enable candidates");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);
  
  GuiWgText(sec, "Candidates", GuiWgCandidatesText);
  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);

  item = GuiButtonNew("<", GuiWgCandPrev);
  item->update = GuiWgCandBtn;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);

  item = GuiButtonNew(">", GuiWgCandNext);
  item->update = GuiWgCandBtn;
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);

  item = GuiScalarNew(2, &view->cand_sel, &gui_s32, "##cand");
  item->sync = GuiWgCandSel;
  item->update = GuiWgCandRange;
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);
}

static gui_item *GuiWorldGeomView() {
  gui_item *view, *grid;

  view = GuiGroupNew();

  grid = GuiGridNew(1, 0);
  GuiWgModeSection(grid);
  GuiWgSelectionSection(grid);
  GuiWgEditSection(grid);
  GuiWgCoverageSection(grid);
  GuiAddChild(view, grid);
  
  return view;
}

/** Camera view update callbacks **/
static void GuiCamHw(gui_item *item) {
  GuiSetEnabled(item, ext_config.enable_hwpersp);
}

static void GuiCamOrbitOpt(gui_item *item) {
  if (ext_cam.options.enable_orbit) item->flags |= GUI_FLAGS_VISIBLE;
  else item->flags &= ~GUI_FLAGS_VISIBLE;
  GuiCamHw(item);
}

static void GuiCamModeSync(gui_item *item, int flags) {
  if (flags & GUI_SYNC_INTERNAL) 
    CamExSetMode(gui_ctxt.gui_cam_mode);
  if (flags & GUI_SYNC_EXTERNAL) 
    gui_ctxt.gui_cam_mode = ext_cam.mode;
}

static void GuiCamResetOffsets(gui_item *item) { 
  CamExResetOffsets(); 
}

static void GuiCamResetZoom(gui_item *item) { 
  ext_cam.options.cam_zoom = 1.0f; 
}

static void GuiCamPathSync(gui_item *view) {
  gui_item *it, *next, *tree, **items;
  int i;

  if (cur_path == gui_ctxt.path_shown && view->child) 
    return;
  gui_ctxt.path_shown = cur_path;
  for (it=view->child;it;it=next) {
    next = it->next;
    GuiRemoveChild(view, it);
    GuiItemFree(it, 1);
  }
  tree = GuiTreeNew(0, 0);
  GuiAddLabel(tree, cur_path ? "Current path" : "Current path (none)");
  tree->tr->column_count = 1;
  GuiAddChild(view, tree);
  if (!cur_path) return;
  items = (gui_item**)ReflMap(cur_path, &zone_path_type, 
    (refl_map_t)GuiMapZoneField);
  for (i=0;(it=items[i]);i++) {
    if ((int)it == -1) { continue; }
    GuiAddChild(tree, it);
  }
  free(items);
}

/** Camera view control helpers **/
static gui_item *GuiCamVec(gui_item *parent, char *label, 
                           void *data, gui_ptype *ptype, int count) {
  gui_item *item;

  item = GuiScalarVecNew(0, data, ptype, count, 0, label);
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(parent, item);
  return item;
}

static gui_item *GuiCamMat(gui_item *parent, char *label, mat16 *mat) {
  gui_item *sec;
  char name[32];
  int i;

  sec = GuiHeaderNew(label, 0);
  for (i=0;i<3;i++) {
    sprintf(name, "row %d", i);
    GuiCamVec(sec, name, mat->m[i], &gui_s16, 3);
  }
  GuiCamVec(sec, "t", &mat->t, &gui_s32, 3);
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(parent, sec);
  return sec;
}

/** Camera view implementation **/
static void GuiCamExtendedSection(gui_item *grid) {
  static const char *modes[] = 
    { "Game (rail)", "Chase (behind Crash)", "Free" };
  ext_cam_options_t *options;
  gui_item *sec, *item, *row;

  options = &ext_cam.options;
  sec = GuiHeaderNew("Extended camera (native perspective only)", 1);
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, sec);

  item = GuiToggleNew(0, &ext_config.enable_hwpersp, 
    "Native perspective transform");
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);
  
  item = GuiComboNew("Mode", &gui_ctxt.gui_cam_mode, &gui_s32, modes, 3);
  GuiSetTip(item, "Game: rail camera. "
    "Chase: fixed distance behind Crash. Free: user-controlled");
  item->update = GuiCamHw;
  item->sync = GuiCamModeSync;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);
  
  row = GuiGroupNew();
  row->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, row);

  item = GuiScalarNew(2, &options->cam_zoom, &gui_float, "Zoom");
  GuiSetTip(item, "Frustum scale; >1 zooms out (mouse wheel)");
  item->s->vmin.f = 0.25f; 
  item->s->vmax.f = 4.0f; 
  item->s->fmt = "%.2f";
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(row, item);

  item = GuiButtonNew("1x", GuiCamResetZoom);
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM | GUI_FLAGS_SAMELINE;
  GuiAddChild(row, item);

  item = GuiToggleNew(0, &options->enable_input, 
    "Mouse / keyboard control");
  GuiSetTip(item, "Look: Shift+drag or right-drag. "
                  "Pan: Ctrl+drag or middle-drag. "
                  "Zoom: wheel/scroll. "
                  "Alt+drag: move toward/away from selected polygon. "
                  "I/K/J/L: forward/back/left/right, U/O down/up");
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiScalarVecNew(1, ext_cam.offs_trans.c, &gui_double, 3, 
    "%.0f", "Pos offset");
  item->s->step = 10.0f; 
  item->s->vmin.d = -1e7;
  item->s->vmax.d = 1e7; 
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiScalarVecNew(1, ext_cam.offs_rot.c, &gui_double, 3, 
    "%.1f", "Rot offset");
  item->s->step = 0.5f; 
  item->s->vmin.d = -360.0; 
  item->s->vmax.d = 360.0; 
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiButtonNew("Reset offsets", GuiCamResetOffsets);
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiToggleNew(0, &options->enable_orbit, "Auto-orbit");
  GuiSetTip(item, "Orbits the camera pivot "
    "(Crash in chase mode, point in front of the camera in game mode); "
    "offsets still apply on top");
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiScalarNew(2, &ext_cam.orbit_speed, &gui_double, 
    "Orbit speed (deg/s)");
  item->s->vmin.d = 0.0; 
  item->s->vmax.d = 360.0; 
  item->s->fmt = "%.1f"; 
  item->update = GuiCamOrbitOpt;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiScalarNew(2, &ext_cam.orbit_radius, &gui_double, 
    "Orbit radius");
  item->s->vmin.d = 100.0; 
  item->s->vmax.d = 20000.0; 
  item->s->fmt = "%.0f"; 
  item->update = GuiCamOrbitOpt;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);

  item = GuiScalarNew(2, &ext_cam.orbit_dir, &gui_double, 
    "Orbit direction (deg)");
  GuiSetTip(item, "Direction of travel over the orbit sphere: "
    "0 = left to right (equator), 90 = up along the longitude, "
    "180 = right to left, 270 = down");
  item->s->vmin.d = 0.0; 
  item->s->vmax.d = 360.0; 
  item->s->fmt = "%.0f"; 
  item->update = GuiCamOrbitOpt;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);
 
  // GuiBoundText(sec, "Pose", ext_cam.pose_info[0]);
  // GuiBoundText(sec, "", ext_cam.pose_info[1]);
}

static void GuiCamPathsSection(gui_item *grid) {
  ext_cam_options_t *options;
  gui_item *sec, *item;

  options = &ext_cam.options;
  sec = GuiHeaderNew("Camera paths", 1);
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, sec);

  item = GuiToggleNew(0, &options->show_paths, 
    "Show camera paths in current zone");
  item->update = GuiCamHw;
  item->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, item);
  
  gui_ctxt.path_view = GuiGroupNew();
  gui_ctxt.path_view->update = GuiCamPathSync;
  gui_ctxt.path_view->flags |= GUI_FLAGS_FORM;
  GuiAddChild(sec, gui_ctxt.path_view);
}

static void GuiCamVariablesSection(gui_item *grid) {
  gui_item *sec;

  sec = GuiHeaderNew("Game camera variables", 0);
  sec->flags |= GUI_FLAGS_FORM;
  GuiAddChild(grid, sec);

  GuiCamVec(sec, "cam.trans", &cam.trans, &gui_s32, 3);
  GuiCamVec(sec, "cam.rot (y, x, z)", &cam.rot, &gui_s32, 3);
  GuiCamVec(sec, "cam.scale", &cam.scale, &gui_s32, 3);
  GuiCamVec(sec, "cam_prev.trans", &cam_prev.trans, &gui_s32, 3);
  GuiCamVec(sec, "cam_prev.rot", &cam_prev.rot, &gui_s32, 3);
  GuiCamVec(sec, "cam_trans_ro", &cam_trans_ro, &gui_s32, 3);
  GuiCamVec(sec, "cam_rot_ro", &cam_rot_ro, &gui_s32, 3);
  GuiCamVec(sec, "cam_rot_before", &cam_rot_before, &gui_s32, 3);
  GuiCamVec(sec, "cam_rot_after", &cam_rot_after, &gui_s32, 3);
  GuiCamVec(sec, "cam_rot_xz_dir", &cam_rot_xz_dir, &gui_s32, 3);
  GuiCamVec(sec, "cam_rot_xz", &cam_rot_xz, &gui_s32, 1);
  GuiCamVec(sec, "cam_rot_xz_ro", &cam_rot_xz_ro, &gui_s32, 1);
  GuiCamVec(sec, "cur_progress", &cur_progress, &gui_s32, 1);
  GuiCamVec(sec, "cam_offset x/y/z", &cam_offset_x, &gui_s32, 1);
  GuiCamVec(sec, "cam_offset_y", &cam_offset_y, &gui_s32, 1);
  GuiCamVec(sec, "cam_offset_z", &cam_offset_z, &gui_s32, 1);
  GuiCamVec(sec, "cam_offset_dir_x", &cam_offset_dir_x, &gui_s32, 1);
  GuiCamVec(sec, "cam_offset_dir_z", &cam_offset_dir_z, &gui_s32, 1);
  GuiCamVec(sec, "cam_zoom", &cam_zoom, &gui_s32, 1);
  GuiCamVec(sec, "cam_speed", &cam_speed, &gui_s32, 1);
  GuiCamVec(sec, "dcam_accel", &dcam_accel, &gui_s32, 1);
  GuiCamVec(sec, "dcam_angvel", &dcam_angvel, &gui_s32, 1);
  GuiCamVec(sec, "dcam_angvel2", &dcam_angvel2, &gui_s32, 1);
  GuiCamVec(sec, "dcam_rot_y1", &dcam_rot_y1, &gui_s32, 1);
  GuiCamVec(sec, "dcam_rot_y2", &dcam_rot_y2, &gui_s32, 1);
  GuiCamVec(sec, "dcam_trans_z", &dcam_trans_z, &gui_s32, 1);
  GuiCamVec(sec, "dcam_zoom_speed", &dcam_zoom_speed, &gui_s32, 1);
  GuiCamVec(sec, "cam_spin_obj_vert", &cam_spin_obj_vert, &gui_s32, 1);
  GuiCamVec(sec, "island_cam_state", &island_cam_state, &gui_s32, 1);
  GuiCamVec(sec, "island_cam_rot_x", &island_cam_rot_x, &gui_u32, 1);
  GuiCamVec(sec, "i_death_cam", &i_death_cam, &gui_s32, 1);
  GuiCamVec(sec, "screen_shake", &screen_shake, &gui_s32, 1);
  GuiCamVec(sec, "screen_ro", &screen_ro, &gui_s32, 2);
  GuiCamVec(sec, "screen_proj", &screen_proj, &gui_u32, 1);
  GuiCamMat(sec, "mn_cam_rot", &mn_cam_rot);
  GuiCamMat(sec, "ms_cam_rot", &ms_cam_rot);
  GuiCamMat(sec, "ms_cam_rot2", &ms_cam_rot2);
  GuiCamMat(sec, "mn_rot", &mn_rot);
  GuiCamMat(sec, "ms_rot", &ms_rot);
}

static gui_item *GuiCameraView() {
  gui_item *view, *grid;

  view = GuiGroupNew();

  grid = GuiGridNew(1, 0);
  GuiAddChild(view, grid);
  
  GuiCamExtendedSection(grid);
  GuiCamPathsSection(grid);
  GuiCamVariablesSection(grid);
  return view;
}

/** Root view implementation **/
static gui_item *GuiRoot() {
  gui_item *root, *status;
  gui_item *tabs;
  gui_item *disp_view, *debug_view, *level_view;
  gui_item *geom_view, *cam_view;

  root = GuiWindowNew("Diagnostics");
  root->flags |= GUI_FLAGS_STATUSBAR;

  tabs = GuiTabNew(0, "##tabs");
  GuiAddChild(root, tabs);

  disp_view = GuiDisplayConfigView();
  debug_view = GuiDebugView();
  level_view = GuiLevelView();
  geom_view = GuiWorldGeomView();
  cam_view = GuiCameraView();
  
  GuiAddLabel(disp_view, "Display");
  GuiAddLabel(debug_view, "GOOL");
  GuiAddLabel(level_view, "Level");
  GuiAddLabel(geom_view, "Geometry");
  GuiAddLabel(cam_view, "Camera");

  GuiAddChild(tabs, disp_view);
  GuiAddChild(tabs, debug_view);
  GuiAddChild(tabs, level_view);
  GuiAddChild(tabs, geom_view);
  GuiAddChild(tabs, cam_view);

  status = GuiTextNew("", 0);
  status->flags |= GUI_FLAGS_READONLY;
  status->update = GuiStatusText;
  GuiAddChild(root, status);
  return root;
}

/** Styles and Fonts **/
static void GuiIgDefaultStyle() {
  ImGuiStyle *style;

  style = igGetStyle();
  *style = gui_ctxt.default_style;
}

static void GuiIgTitleStyle() {
  ImGuiStyle *style;

  style = igGetStyle();
  *style = gui_ctxt.default_style;
  style->WindowPadding = (ImVec2){ 10, 10 };
  style->FramePadding = (ImVec2){ 7, 4 };
  style->ItemSpacing = (ImVec2){ 8, 6 };
  style->ItemInnerSpacing = (ImVec2){ 7, 4 };
  style->CellPadding = (ImVec2){ 6, 4 };
  style->WindowRounding = 6;
  style->FrameRounding = 4;
  style->TabRounding = 4;
  style->Colors[ImGuiCol_Text] = (ImVec4){ 0.96f, 0.91f, 0.82f, 1.0f };
  style->Colors[ImGuiCol_TextDisabled] = (ImVec4){ 0.54f, 0.56f, 0.58f, 1.0f };
  style->Colors[ImGuiCol_WindowBg] = (ImVec4){ 0.07f, 0.08f, 0.10f, 0.99f };
  style->Colors[ImGuiCol_ChildBg] = (ImVec4){ 0.055f, 0.065f, 0.08f, 1.0f };
  style->Colors[ImGuiCol_PopupBg] = (ImVec4){ 0.10f, 0.11f, 0.13f, 0.99f };
  style->Colors[ImGuiCol_Border] = (ImVec4){ 0.27f, 0.30f, 0.32f, 0.90f };
  style->Colors[ImGuiCol_FrameBg] = (ImVec4){ 0.14f, 0.16f, 0.18f, 1.0f };
  style->Colors[ImGuiCol_FrameBgHovered] = (ImVec4){ 0.20f, 0.25f, 0.27f, 1.0f };
  style->Colors[ImGuiCol_FrameBgActive] = (ImVec4){ 0.24f, 0.31f, 0.31f, 1.0f };
  style->Colors[ImGuiCol_TitleBg] = (ImVec4){ 0.10f, 0.12f, 0.14f, 1.0f };
  style->Colors[ImGuiCol_TitleBgActive] = (ImVec4){ 0.16f, 0.19f, 0.20f, 1.0f };
  style->Colors[ImGuiCol_TitleBgCollapsed] = (ImVec4){ 0.09f, 0.10f, 0.12f, 1.0f };
  style->Colors[ImGuiCol_ScrollbarBg] = (ImVec4){ 0.06f, 0.07f, 0.08f, 1.0f };
  style->Colors[ImGuiCol_ScrollbarGrab] = (ImVec4){ 0.31f, 0.36f, 0.36f, 1.0f };
  style->Colors[ImGuiCol_ScrollbarGrabHovered] = (ImVec4){ 0.39f, 0.48f, 0.46f, 1.0f };
  style->Colors[ImGuiCol_ScrollbarGrabActive] = (ImVec4){ 0.47f, 0.59f, 0.55f, 1.0f };
  style->Colors[ImGuiCol_Header] = (ImVec4){ 0.16f, 0.23f, 0.23f, 1.0f };
  style->Colors[ImGuiCol_HeaderHovered] = (ImVec4){ 0.21f, 0.30f, 0.29f, 1.0f };
  style->Colors[ImGuiCol_HeaderActive] = (ImVec4){ 0.25f, 0.36f, 0.34f, 1.0f };
  style->Colors[ImGuiCol_Button] = (ImVec4){ 0.15f, 0.19f, 0.20f, 1.0f };
  style->Colors[ImGuiCol_ButtonHovered] = (ImVec4){ 0.22f, 0.29f, 0.29f, 1.0f };
  style->Colors[ImGuiCol_ButtonActive] = (ImVec4){ 0.27f, 0.37f, 0.35f, 1.0f };
  style->Colors[ImGuiCol_CheckMark] = (ImVec4){ 0.95f, 0.69f, 0.32f, 1.0f };
  style->Colors[ImGuiCol_SliderGrab] = (ImVec4){ 0.73f, 0.52f, 0.29f, 1.0f };
  style->Colors[ImGuiCol_SliderGrabActive] = (ImVec4){ 0.95f, 0.69f, 0.32f, 1.0f };
  style->Colors[ImGuiCol_Tab] = (ImVec4){ 0.13f, 0.16f, 0.17f, 1.0f };
  style->Colors[ImGuiCol_TabHovered] = (ImVec4){ 0.22f, 0.30f, 0.29f, 1.0f };
  style->Colors[ImGuiCol_TabSelected] = (ImVec4){ 0.19f, 0.25f, 0.25f, 1.0f };
  style->Colors[ImGuiCol_TabSelectedOverline] = (ImVec4){ 0.95f, 0.69f, 0.32f, 1.0f };
  style->Colors[ImGuiCol_TabDimmed] = (ImVec4){ 0.10f, 0.12f, 0.13f, 1.0f };
  style->Colors[ImGuiCol_TabDimmedSelected] = (ImVec4){ 0.15f, 0.19f, 0.19f, 1.0f };
  style->Colors[ImGuiCol_Separator] = (ImVec4){ 0.30f, 0.34f, 0.34f, 0.80f };
  style->Colors[ImGuiCol_SeparatorHovered] = (ImVec4){ 0.46f, 0.57f, 0.53f, 1.0f };
  style->Colors[ImGuiCol_SeparatorActive] = (ImVec4){ 0.62f, 0.72f, 0.64f, 1.0f };
  style->Colors[ImGuiCol_TableHeaderBg] = (ImVec4){ 0.14f, 0.17f, 0.18f, 1.0f };
  style->Colors[ImGuiCol_TableBorderStrong] = (ImVec4){ 0.28f, 0.31f, 0.31f, 1.0f };
  style->Colors[ImGuiCol_TableBorderLight] = (ImVec4){ 0.18f, 0.21f, 0.22f, 1.0f };
  style->Colors[ImGuiCol_TableRowBg] = (ImVec4){ 0, 0, 0, 0 };
  style->Colors[ImGuiCol_TableRowBgAlt] = (ImVec4){ 1, 1, 1, 0.025f };
  style->Colors[ImGuiCol_TextSelectedBg] = (ImVec4){ 0.28f, 0.44f, 0.40f, 0.65f };
  style->Colors[ImGuiCol_NavHighlight] = (ImVec4){ 0.95f, 0.69f, 0.32f, 1.0f };
}

void GuiShadeGlyph(gui_glyph *glyph, texinfo *tinfo,
  uint16_t r1, uint16_t g1, uint16_t b1, 
  uint16_t r2, uint16_t g2, uint16_t b2) {
  rgba top, bot;
  uint8_t *p; 
  uint32_t shade, value;
  int i, x, y, scale;

  top.r = (tinfo->r * r1) >> 8;
  top.g = (tinfo->g * g1) >> 8;
  top.b = (tinfo->b * b1) >> 8;
  bot.r = (tinfo->r * r2) >> 8;
  bot.g = (tinfo->g * g2) >> 8;
  bot.b = (tinfo->b * b2) >> 8;
  scale = 255 * (GUI_GLYPH_SIZE - 1);
  p = (uint8_t*)glyph->pixels;
  for (y=0;y<GUI_GLYPH_SIZE;y++) {
    for (x=0;x<GUI_GLYPH_SIZE;x++,p+=4) {
      for (i=0;i<3;i++) {
        /* domain of shade is
           [0, scale] */
        shade = top.c[i] * (GUI_GLYPH_SIZE - 1 - y)
              + bot.c[i] * y;
        /* domain of p[i] * shade is
           [0, 255 * scale] 
           value is scaled to 2*[0, 255] by dividing by scale
           and addition of scale / 2 rounds to nearest integer */
        value = (p[i]*shade*2 + scale/2) / scale;
        p[i] = value > 255 ? 255 : value;
      }
    }
  }
}

static gui_font *GuiCreateGameFont(lid_t lid, eid_t gool_eid, eid_t font_eid,
  uint16_t r1, uint16_t g1, uint16_t b1, 
  uint16_t r2, uint16_t g2, uint16_t b2, const char *synth_chars) {
  const gui_font_options options = {
    .scale = 0.75f,
    .synthesize = 1,
    .synth_color = {255, 160, 0}
  };
  gui_font *res;
  gui_glyph *glyphs, *dst;
  gool_font font;
  gool_glyph *src;
  tpage font_page;
  texinfo info;
  int c, failed;

  res = 0;
  failed = 0;
  glyphs = 0;
  if (!NSReadFontAssets(lid, gool_eid, font_eid, &font, &font_page)) {
    fprintf(stderr, "GUI font: cannot load font assets\n");
    goto done;
  }
  glyphs = calloc(GUI_GLYPH_COUNT, sizeof(*glyphs));
  if (!glyphs) {
    fprintf(stderr, "GUI font: cannot allocate glyphs\n");
    goto done;
  }
  glyphs[0].present = 1;
  glyphs[0].advance = GUI_GLYPH_SIZE;
  for (c='!';c<='^';c++) {
    src = &font.glyphs[c-' '];
    if (!src->has_texture || (synth_chars && strchr(synth_chars, c)))
      continue;
    dst = &glyphs[c - ' '];
    info.rgninfo = src->texinfo.rgninfo;
    info.colinfo = src->texinfo.colinfo;
    info.tpage = font.tpage;
    if (!TextureBlockCopy(&info, &font_page, GUI_GLYPH_SIZE,
                          GUI_GLYPH_SIZE, (uint8_t*)dst->pixels)) {
      failed++;
      continue;
    }
    GuiShadeGlyph(dst, &info, r1, g1, b1, r2, g2, b2);
    dst->advance = GUI_GLYPH_SIZE;
    dst->present = 1;
  }
  for (c='A';c<='Z';c++) {
    if (!glyphs[c-' '].present) {
      fprintf(stderr, "GUI font: missing required glyph '%c'; using default\n", c);
      goto done;
    }
    glyphs[c-'A'+'a'-' '] = glyphs[c-' '];
  }
  res = GuiFontNew(glyphs, &options);
done:
  free(glyphs);
  return res;
}

static gui_font *GuiCreateTitleFont(void) {
  eid_t gool_eid, font_eid;

  gool_eid = NSStringToEID("GamOC");
  font_eid = NSStringToEID("Fon0T");
  return GuiCreateGameFont(0x19, gool_eid, font_eid,
                           0x1FF, 0x190, 0,
                           0x1FF, 0, 0, "#*+^");
}

/** Main GUI functions **/
static void GuiFpsOverlay(void) {
  if (ext_config.show_fps) 
    GuiIgFpsOverlay();
}

void GuiInit() {
  pad_lock = 0;

  ReflInit(types);
  GuiIgInit();
  gui_ctxt.default_style = *igGetStyle();
  gui_ctxt.title_font = GuiCreateTitleFont();
  gui_ctxt.root = GuiRoot();
  gui_ctxt.sel_obj = 0;
  gui_ctxt.prev_zone = 0;
  gui_ctxt.zone_tree_invalid = 1;
  gui_ctxt.wg_view.tex_sel = -1;
  gui_ctxt.wg_icon_count = -1;
  GuiIgExtraDraw = GuiFpsOverlay;
}

void GuiUpdate() {
  gool_object *sel_obj;
  
  sel_obj = ExtGetSelectedObject(&ext_ctxt);
  if (gui_ctxt.object_tree && gui_ctxt.sel_obj != sel_obj)
    GuiSelectObject(sel_obj);
  if (cur_zone != gui_ctxt.prev_zone) {
    gui_ctxt.prev_zone = cur_zone;
    gui_ctxt.zone_tree_invalid = 1;
  }
  if (GuiIgKeyPressed(526)) {
    gui_ctxt.root->flags ^= GUI_FLAGS_VISIBLE;
  }
  if (GuiIgKeyPressed(512) && (gui_ctxt.root->flags & GUI_FLAGS_VISIBLE)) {
    gui_ctxt.gui_lock = !gui_ctxt.gui_lock;
    if (!gui_ctxt.gui_lock) {
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
  if (ext_config.use_title_font && gui_ctxt.title_font) {
    GuiIgTitleStyle();
    GuiFontApply(gui_ctxt.title_font);
  }
  else {
    GuiIgDefaultStyle();
    GuiFontApply(0);
  }
  GuiIgDraw(gui_ctxt.root);
}
