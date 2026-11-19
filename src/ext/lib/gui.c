#include "gui.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include <limits.h>
#include <float.h>

void *GuiId(void *item) { return item; }

gui_ptype gui_s8 = {
  .size=sizeof(int8_t),
  .is_signed=1,
  .is_float=0,
  .vmin = { .s8 = SCHAR_MIN },
  .vmax = { .s8 = SCHAR_MAX },
  .fmt = "%i"
};
gui_ptype gui_s16 = {
  .size=sizeof(int16_t),
  .is_signed=1,
  .is_float=0,
  .vmin = { .s16 = SHRT_MIN },
  .vmax = { .s16 = SHRT_MAX },
  .fmt = "%i"
};
gui_ptype gui_s32 = {
  .size=sizeof(int32_t),
  .is_signed=1,
  .is_float=0,
  .vmin = { .s32 = INT_MIN },
  .vmax = { .s32 = INT_MAX },
  .fmt = "%i"
};
gui_ptype gui_u8 = {
  .size=sizeof(uint8_t),
  .is_signed=0,
  .is_float=0,
  .vmin = { .u8 = 0 },
  .vmax = { .u8 = UCHAR_MAX },
  .fmt = "%i"
};
gui_ptype gui_u16 = {
  .size=sizeof(uint16_t),
  .is_signed=0,
  .is_float=0,
  .vmin = { .u16 = 0 },
  .vmax = { .u16 = USHRT_MAX },
  .fmt = "%i"
};
gui_ptype gui_u32 = {
  .size=sizeof(uint32_t),
  .is_signed=0,
  .is_float=0,
  .vmin = { .u32 = 0 },
  .vmax = { .u32 = UINT_MAX },
  .fmt = "%i"
};
gui_ptype gui_float = {
  .size=sizeof(float),
  .is_signed=1,
  .is_float=1,
  .vmin = { .f = FLT_MIN },
  .vmax = { .f = FLT_MAX },
  .fmt = "%f"
};
gui_ptype gui_double = {
  .size=sizeof(double),
  .is_signed=1,
  .is_float=1,
  .vmin = { .d = -DBL_MAX },
  .vmax = { .d = DBL_MAX },
  .fmt = "%f"
};

gui_item def_item = { .flags = GUI_FLAGS_VISIBLE };
gui_text def_text = { .maxlen = 128 };
gui_scalar def_scalar = { 0 };
gui_list def_list = { 0 };
gui_tree_sync def_tree_sync = { 0 };

int GuiGetInt(void *data, gui_ptype *ptype) {
  if (ptype->is_float) {
    if (ptype->size == sizeof(double)) 
      return (int)*(double*)data;
    return (int)*(float*)data;
  }
  switch(ptype->size) {
  case 1: return *(int8_t*)data;
  case 2: return *(int16_t*)data;
  case 4: return *(int32_t*)data;
  }
  return 0;
}

void GuiSetInt(void *data, gui_ptype *ptype, int value) {
  if (ptype->is_float) {
    if (ptype->size == sizeof(double)) {
      *(double*)data = (double)value;
      return;
    }
    *(float*)data = (float)value;
    return;
  }
  switch(ptype->size) {
  case 1: *(int8_t*)data = value; break;
  case 2: *(int16_t*)data = value; break;
  case 4: *(int32_t*)data = value; break;
  }
}

ImGuiDataType GuiIgType(gui_ptype *ptype) {
  if (ptype->is_float) {
    if (ptype->size == sizeof(double))
      return ImGuiDataType_Double;
    return ImGuiDataType_Float;
  }
  else if (ptype->is_signed) {
    switch(ptype->size) {
    case 1: return ImGuiDataType_S8;
    case 2: return ImGuiDataType_S16;
    case 4: return ImGuiDataType_S32;
    case 8: return ImGuiDataType_S64;
    }
  }
  else {
    switch(ptype->size) {
    case 1: return ImGuiDataType_U8;
    case 2: return ImGuiDataType_U16;
    case 4: return ImGuiDataType_U32;
    case 8: return ImGuiDataType_U64;
    }
  }
  return -1;
}

int GuiUnparse(void *data, gui_dtype *dtype, char *str) {
  if (dtype->is_prim)
    return sprintf(str, dtype->ptype->fmt, data);
  else if (dtype->unparse)
    return dtype->unparse(data, str);
  strcpy(str, data);
  return 1;
}

int GuiParse(char *str, void *data, gui_dtype *dtype) {
  if (dtype->is_prim)
    return sscanf(str, dtype->ptype->fmt, data);
  else if (dtype->parse)
    return dtype->parse(str, data);
  strcpy(data, str);
  return 1;
}

gui_item *GuiItemAlloc() {
  gui_item *item;

  item = (gui_item*)malloc(sizeof(gui_item));
  return item;
}

void GuiItemFree(gui_item *item, int free_children) {
  gui_item *it, *next;

  if (free_children) {
    for (it=item->child;it;it=next) {
      next = it->next;
      GuiItemFree(it, 1);
    }
  }
  if (item->type == scalar && item->s->fmt)
    free(item->s->fmt);
  if (item->type == tree && item->tr->sync.data) {
    if (item->tr->sync.child_fn && item->tr->sync.cur)
      tree_node_free(item->tr->sync.cur, 1);
    if (item->tr->sync.prev) 
      tree_node_free(item->tr->sync.prev, 1);
  }
  if (item->tip)
    free(item->tip);
  if (item->d)
    free(item->d);
  if (item->label)
    free(item->label);
  free(item);
}

gui_item *GuiItemNew() {
  gui_item *item;

  item = (gui_item*)malloc(sizeof(gui_item));
  *item = def_item;
  item->node.data = item;
  return item;
}

void GuiAddChild(gui_item *item, gui_item *child) {
  tree_add_node((tree_node_t*)item, (tree_node_t*)child);
}

void GuiRemoveChild(gui_item *item, gui_item *child) {
  if (!item->child) { return; }
  tree_remove_node((tree_node_t*)child);
  child->parent = 0;
  child->next = 0;
  child->prev = 0;
}

void GuiInsertChild(gui_item *item, gui_item *prev, gui_item *child) {
  tree_insert_node((tree_node_t*)item, (tree_node_t*)prev,
                   (tree_node_t*)child);
}

void GuiAddLabel(gui_item *item, char *str) {
  if (item->label) { free(item->label); }
  item->label = (gui_text*)malloc(sizeof(gui_text)+def_text.maxlen);
  *item->label = def_text;
  strcpy(item->label->str, str);
}

gui_item *GuiWindowNew(char *label) {
  gui_item *item;

  item = GuiItemNew();
  item->type = window;
  if (label) { GuiAddLabel(item, label); }
  return item;
}

gui_item *GuiGroupNew() {
  gui_item *item;

  item = GuiItemNew();
  item->type = group;
  return item;
}

gui_item *GuiTextNew(char *str, char *label) {
  gui_item *item;

  item = GuiItemNew();
  item->type = text;
  item->t = (gui_text*)malloc(sizeof(gui_text)+def_text.maxlen+4);
  *item->t = def_text;
  if (str) { snprintf(item->t->str, def_text.maxlen, "%s", str); }
  if (label) { GuiAddLabel(item, label); }
  return item;
}

gui_item *GuiButtonNew(char *str, gui_handler_t click) {
  gui_item *item;

  item = GuiItemNew();
  item->type = button;
  item->b = (gui_button*)malloc(sizeof(gui_button)+def_text.maxlen+4);
  item->b->click = click;
  if (str) { strcpy(item->b->str, str); }
  return item;
}

gui_item *GuiScalarNew(int type, void *data, gui_ptype *ptype, char *label) {
  gui_item *item;

  item = GuiItemNew();
  item->type = scalar;
  item->s = (gui_scalar*)malloc(sizeof(gui_scalar));
  *item->s = def_scalar;
  item->s->type = type;
  item->s->data = data;
  item->s->ptype = ptype;
  if (label) { GuiAddLabel(item, label); }
  return item;
}

gui_item *GuiScalarVecNew(int type, void *data, gui_ptype *ptype, int count, char *fmt, char *label) {
  gui_item *item;

  item = GuiScalarNew(type, data, ptype, label);
  item->s->count = count;
  if (fmt) { item->s->fmt = strdup(fmt); }
  return item;
}

gui_item *GuiListNew(int type, char *label) {
  gui_item *item;

  item = GuiItemNew();
  item->type = list;
  item->l = (gui_list*)malloc(sizeof(gui_list));
  *item->l = def_list;
  item->l->type = type;
  item->l->selected = 0;
  if (label) { GuiAddLabel(item, label); }
  return item;
}

gui_item *GuiTabNew(int type, char *label) {
  gui_item *item;

  item = GuiItemNew();
  item->type = tab;
  item->tb = (gui_tab*)malloc(sizeof(gui_tab));
  item->tb->type = type;
  item->tb->selected = 0;
  item->tb->data = 0;
  item->tb->ptype = 0;
  if (label) { GuiAddLabel(item, label); }
  return item;
}

gui_item *GuiComboNew(char *label, void *data, gui_ptype *ptype, const char **options, int count) {
  gui_item *item, *child;
  int i;

  item = GuiListNew(1, label);
  for (i=0;i<count;i++) {
    child = GuiTextNew((char*)options[i], 0);
    child->flags |= GUI_FLAGS_READONLY;
    GuiAddChild(item, child);
  }
  item->l->data = data;
  item->l->ptype = ptype;
  GuiListSync(item, 3);
  return item;
}

gui_item *GuiNodeNew(void *data, gui_data_unparse_t unparse) {
  gui_item *item;

  item = GuiItemNew();
  item->type = node;
  item->label = (gui_text*)malloc(sizeof(gui_text)+def_text.maxlen+4);
  *item->label = def_text;
  item->label->data = data;
  item->label->dtype.unparse = unparse;
  item->flags |= GUI_FLAGS_READONLY;
  return item;
}

gui_data_unparse_t _node_unparse;
static void *GuiTreeCur(gui_tree_sync *sync) {
  void *data, *cur;
  void **(*child_fn)(void*);

  data = sync->data;
  child_fn = sync->child_fn;
  cur = data;
  if (data && child_fn)
    cur = tree_map_from(data, child_fn, GuiId);
  return cur;
}

gui_item *GuiTreeNew(gui_handler_t select, gui_tree_sync *sync) {
  gui_item *root;
  gui_tree *tr;
  gui_node_map_t map;
  tree_node_t *cur;

  if (sync) {
    cur = GuiTreeCur(sync);
    /* convert tree representation into initial tree of gui items */
    map = sync->map;
    _node_unparse = sync->unparse;
    root = tree_map_to(cur, (void *(*)(void*, void**))map);
    _node_unparse = 0;
  }
  else
    root = GuiNodeNew(0, 0);
  /* convert the root item from node type to tree type
     as the root gui_item must be tree type for this to constitute a tree
     allocate a gui_tree for the item to store information about the tree */
  root->type = tree;
  if (root->tr)
    tr = root->tr = realloc(root->tr, sizeof(gui_tree));
  else
    tr = root->tr = malloc(sizeof(gui_tree));
  tr->sync = def_tree_sync;
  if (sync) {
    tr->sync = *sync;
    tr->sync.cur = cur;
    tr->sync.prev = cur ? tree_copy(cur) : 0;
  }
  tr->select = select;
  tr->selected = 0;
  tr->sel = 0;
  tr->column_count = 0;
  return root;
}

gui_item *GuiColorNew(void *data, void (*unparse)(void*, float[3]), void (*parse)(float[3], void*)) {
  gui_item *item;

  item = GuiItemNew();
  item->type = color;
  item->c = (gui_color*)malloc(sizeof(gui_color));
  item->c->data = data;
  item->c->unparse = unparse;
  item->c->parse = parse;
  return item;
}

gui_item *GuiSeparatorNew() {
  gui_item *item;

  item = GuiItemNew();
  item->type = separator;
  return item;
}

gui_item *GuiHeaderNew(char *label, int open) {
  gui_item *item;

  item = GuiItemNew();
  item->type = header;
  if (open) { item->flags |= GUI_FLAGS_OPEN; }
  if (label) { GuiAddLabel(item, label); }
  return item;
}

gui_item *GuiToggleNew(int type, void *data, char *label) {
  gui_item *item;

  item = GuiItemNew();
  item->type = toggle;
  item->tg = (gui_toggle*)malloc(sizeof(gui_toggle));
  item->tg->type = type;
  item->tg->data = data;
  if (label) { GuiAddLabel(item, label); }
  return item;
}

gui_item *GuiImageNew(float size, gui_image_get_t get) {
  gui_item *item;

  item = GuiItemNew();
  item->type = image;
  item->im = (gui_image*)calloc(1, sizeof(gui_image));
  item->im->size = size;
  item->im->get = get;
  return item;
}

gui_item *GuiGridNew(int column_count, int row_count) {
  gui_item *item;

  item = GuiItemNew();
  item->type = grid;
  item->g = (gui_grid*)malloc(sizeof(gui_grid));
  item->g->column_count = column_count;
  item->g->row_count = row_count;
  item->g->header_weight = 0.0f;
  return item;
}

void GuiSetTip(gui_item *item, char *tip) {
  if (item->tip) 
    free(item->tip);
  item->tip = tip ? strdup(tip) : 0;
}

void GuiSetEnabled(gui_item *item, int enabled) {
  if (enabled) item->flags &= ~GUI_FLAGS_DISABLED;
  else item->flags |= GUI_FLAGS_DISABLED;
}

static void GuiItemLabel(gui_item *item, char *id) {
  ImVec2 avail, label_size;
  float x, lw, x0;

  x = 0.0f;
  if (item->label && item->label->str[0] != '#') {
    if (item->flags & GUI_FLAGS_RIGHTALIGN) {
      x = igGetCursorPosX();
      igSetCursorPosX(igGetStyle()->WindowPadding.x);
    }
    igAlignTextToFramePadding();
    if (item->flags & GUI_FLAGS_FORM) {
      x0 = igGetCursorPosX();
      igGetContentRegionAvail(&avail);
      igCalcTextSize(&label_size, item->label->str, 0, 1, -1.0f);
      lw = limit(avail.x * 0.26f, 110.0f, 190.0f);
      lw = limit(lw, label_size.x+12.0f, avail.x*0.65f);
      igText(item->label->str);
      igSameLine(x0 + lw, 0.0f);
      if (!(item->next && (item->next->flags & GUI_FLAGS_SAMELINE)))
        igSetNextItemWidth(-1.0f);
    }
    else {
      igText(item->label->str);
      igSameLine(x, 6.0f);
    }
  }
  strcpy(id, "##selection");
}

static void GuiItemRightAlign(gui_item *item) {
  ImGuiStyle *style;
  ImVec2 avail, size;
  float w, x, m;

  style = igGetStyle();
  igGetContentRegionAvail(&avail);
  switch (item->type) {
  case text:
    igCalcTextSize(&size, item->t->str, 0, 0, -1.0f);
    w = size.x;
    break;
  case button:
    /* measure button text + default frame padding */
    igCalcTextSize(&size, item->b->str, 0, 0, -1.0f);
    w = size.x + style->FramePadding.x * 2.0f;
    break;
  case toggle:
    if (item->tg->type == 1) {
      igCalcTextSize(&size, item->label ? item->label->str : "", 0, 0, -1.0f);
      w = size.x + style->FramePadding.x * 2.0f;
    }
    else {
      /* box size + padding + label text width */
      w = igGetFrameHeight();
      if (item->label && item->label->str[0] != '#') {
        igCalcTextSize(&size, item->label->str, 0, 0, -1.0f);
        w += size.x + style->ItemInnerSpacing.x;
      }
    }
    break;
  case list:
  case scalar:
  case color:
  default:
    w = igCalcItemWidth();
    break;
  }
  m = max(style->ItemSpacing.x, 0.0f);
  x = igGetCursorPosX() + avail.x - w - 2*m;
  if (x > igGetCursorPosX()) {
    igSetCursorPosX(x);
  }
}

void GuiListSync(gui_item *item, int flags) {
  gui_list *list;
  gui_item *it;
  int i, cur_idx;

  list = item->l;
  if (!list->data) { return; }
  if (flags & 1) {
    cur_idx = GuiGetInt(list->data, list->ptype);
    list->selected = 0;
    for_each_child_indexed(item, it, i) {
      if (i == cur_idx) {
        list->selected = it;
        break;
      }
    }
  }
  if (flags & 2 && list->selected) {
    cur_idx = -1;
    for_each_child_indexed(item, it, i) {
      if (list->selected == it) {
        cur_idx = i;
        break;
      }
    }
    GuiSetInt(list->data, list->ptype, cur_idx);
  }
}

void *GuiNodeData(gui_item *item) { /* default _node_unmap */
  return item->label->data;
}

gui_item *GuiNodeMap(void *data, void **c_mapped) {
  gui_item *item, *it;
  int i;

  item = GuiNodeNew(data, _node_unparse);
  if (!c_mapped) { return item; }
  for (i=0;(it=c_mapped[i]);i++) {
    GuiAddChild(item, it);
  }
  return item;
}

gui_item *GuiTreeNode(gui_item *tree, void *data) {
  gui_tree *tr;
  gui_tree_sync *sync;
  tree_node_t *n;

  if (data == 0) { return (gui_item*)0; }
  tr = tree->tr;
  sync = &tr->sync;
  if (!sync->unmap) { return (gui_item*)0; }
  tree_first_node(&tree->node, n, (sync->unmap((gui_item*)n)==data));
  if (n == 0) { return (gui_item*)0; }
  return (gui_item*)n;
}

gui_item *GuiNodeTree(gui_item *node) {
  gui_item *it;

  for (it=node;it;it=it->parent) {
    if (it->type==tree) { break; }
  }
  return it;
}

/**
 * sync changes for a data bound tree
 */
static void GuiTreeSync(gui_item *item) {
  gui_tree *tree;
  gui_tree_sync *sync;
  list_t *deltas;
  tree_delta_t *delta;
  gui_item *nitem, *child, *parent, *prev;
  gui_item *l, *r;
  void *sel;

  tree = item->tr;
  sel = tree->sel;
  sync = &tree->sync;
  if (sync->child_fn && sync->cur)
    tree_node_free(sync->cur, 1);
  sync->cur = GuiTreeCur(sync);
  if (!sync->prev)
    sync->prev = tree_copy(sync->cur);
  /* get the list of changes between the previous and current tree layout */
  deltas = tree_changes(sync->prev, sync->cur);
  /* add/remove/move/swap the corresponding gui items for each change */
  _node_unparse = sync->unparse;
  list_for_each(deltas, delta) {
    switch (delta->op) {
    case 1: // add
      nitem = sync->map(delta->value, 0);
      parent = GuiTreeNode(item, delta->parent);
      prev = GuiTreeNode(item, delta->prev);
      GuiInsertChild(parent, prev, nitem);
      break;
    case 2: // remove
      nitem = GuiTreeNode(item, delta->value);
      if (!nitem) { break; }
      if (tree->selected == nitem) {
        tree->selected = 0;
        tree->sel = 0;
      }
      parent = nitem->parent;
      for_each_child(nitem, child) {
        GuiRemoveChild(child->parent, child);
        GuiAddChild(parent, child);
      }
      GuiRemoveChild(nitem->parent, nitem);
      GuiItemFree(nitem, 0);
      break;
    case 3: // move
      nitem = GuiTreeNode(item, delta->value);
      if (!nitem) { break; }
      GuiRemoveChild(nitem->parent, nitem);
      parent = GuiTreeNode(item, delta->parent);
      prev = GuiTreeNode(item, delta->prev);
      GuiInsertChild(parent, prev, nitem);
      break;
    case 4: // swap
      l = GuiTreeNode(item, delta->v1);
      r = GuiTreeNode(item, delta->v2);
      if (l==0 || r==0) continue;
      tree_swap_nodes(&l->node, &r->node);
      break;
    case 5:
      nitem = sync->map(item->label->data, 0);
      item->label->data = delta->value;
      GuiAddChild(item, nitem);
      break;
    }
  }
  if (!tree->selected && sel) {
    tree->selected = GuiTreeNode(item, sel);
    if (tree->selected)
      tree->sel = sel;
  }
  _node_unparse = 0;
  list_free(deltas, 3);
  tree_node_free(sync->prev, 1);
  sync->prev = tree_copy(sync->cur);
}

const ImVec2 def_size = { 0, 0 };

static void GuiWindow(gui_item *item) {
  gui_item *it, *footer;
  ImVec2 avail;
  float height, footer_height;
  char *name;
  int visible;

  name = item->label ? item->label->str : "";
  if (!item->parent)
    visible = igBegin(name, 0, 0);
  else
    visible = igBeginChild_Str("##window", def_size, 0,
      (item->flags & GUI_FLAGS_HSCROLL) ? ImGuiWindowFlags_HorizontalScrollbar : 0);
  if (visible && (item->flags & GUI_FLAGS_STATUSBAR) && item->child) {
    for (footer=item->child;footer->next;footer=footer->next);
    igGetContentRegionAvail(&avail);
    footer_height = igGetTextLineHeight();
    height = avail.y - footer_height - 2.0f * igGetStyle()->ItemSpacing.y - 1.0f;
    if (height < 1.0f) height = 1.0f;
    if (igBeginChild_Str("##content", (ImVec2){0, height}, 0, 0)) {
      for (it=item->child;it!=footer;it=it->next)
        GuiItem(it);
    }
    igEndChild();
    igSeparator();
    if (igBeginChild_Str("##status", (ImVec2){0, footer_height}, 0,
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
      GuiItem(footer);
    igEndChild();
  }
  else if (visible)
    for (it=item->child;it;it=it->next)
      GuiItem(it);
  if (!item->parent)
    igEnd();
  else
    igEndChild();
}

static void GuiGroup(gui_item *item) {
  int line_sizes[128] = { 0 };
  gui_item *it;
  int i, j, count;

  count = 0;
  for (it=item->child;it;it=it->next) {
    if (it!=item->child && !(it->flags & GUI_FLAGS_SAMELINE))
      ++count;
    ++line_sizes[count];
  }
  ++count;
  igBeginGroup();
  it = item->child;
  for (i=0;i<count;i++) {
    if (line_sizes[i]<=0) { continue; }
    if (line_sizes[i]==1) {
      GuiItem(it);
      it=it->next;
      continue;
    }
    igPushMultiItemsWidths(line_sizes[i], igCalcItemWidth());
    for (j=0;j<line_sizes[i];j++) {
      GuiItem(it);
      it = it->next;
      igPopItemWidth();
    }
  }
  igEndGroup();
}

static void GuiTextUpdate(gui_text *text) {
  if (text->data) {
    GuiUnparse(text->data, &text->dtype, text->str);
  }
}

static void GuiText(gui_item *item) {
  char id[32];

  GuiTextUpdate(item->t);
  GuiItemLabel(item, id);
  if (item->flags & GUI_FLAGS_READONLY) {
    if (item->label && item->label->str[0] != '#')
      igAlignTextToFramePadding();
    igTextUnformatted(item->t->str, 0);
  }
  else if (!(item->flags & GUI_FLAGS_MULTILINE)) {
    if (igInputText(id, item->t->str, item->t->maxlen, 0, 0, 0)) {
      if (item->t->data) 
        GuiParse(item->t->str, item->t->data, &item->t->dtype);
      if (item->sync) 
        item->sync(item, GUI_SYNC_INTERNAL);
    }
  }
  else if (igInputTextMultiline(id, item->t->str, item->t->maxlen, def_size, 0, 0, 0)) {
    if (item->t->data) 
      GuiParse(item->t->str, item->t->data, &item->t->dtype);
    if (item->sync) 
      item->sync(item, GUI_SYNC_INTERNAL);
  }
}

static void GuiButton(gui_item *item) {
  gui_button *button;

  button = item->b;
  if (igButton(button->str, def_size)) {
    if (button->click) 
      button->click(item);
  }
}

static void GuiToggle(gui_item *item) {
  ImGuiStyle *style;
  ImVec4 col, hover_col;
  gui_toggle *toggle;
  char *label;
  int on, value;

  style = igGetStyle();
  toggle = item->tg;
  label = item->label ? item->label->str : "";
  on = toggle->data && *(int*)toggle->data != 0;
  if (toggle->type == 1) { /* toggle button */
    if (on) {
      col = style->Colors[ImGuiCol_HeaderActive];
      hover_col = style->Colors[ImGuiCol_HeaderHovered];
      igPushStyleColor_Vec4(ImGuiCol_Button, col);
      igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, hover_col);
    }
    if (igButton(label, def_size)) {
      if (toggle->data) 
        *(int*)toggle->data = !on;
      if (item->sync) 
        item->sync(item, GUI_SYNC_INTERNAL);
    }
    if (on) 
      igPopStyleColor(2);
  }
  else { /* checkbox */
    value = on;
    if (igCheckbox(label, (_Bool*)&value)) {
      if (toggle->data) 
        *(int*)toggle->data = value;
      if (item->sync) 
        item->sync(item, GUI_SYNC_INTERNAL);
    }
  }
}

static void GuiImage(gui_item *item) {
  const ImVec4 bgsel = {0.1f,0.6f,0.1f,1};
  const ImVec4 bgnorm = {0.12f,0.12f,0.12f,1};
  const ImVec4 tint = {1,1,1,1};
  const ImVec4 border = {0.4f,0.4f,0.4f,1};
  ImVec4 bg;
  ImVec2 size, uv0, uv1;
  gui_image *image;
  gui_list *pl;
  float fuv0[2], fuv1[2];
  char tip[128];
  int texid, click;

  image = item->im;
  fuv0[0] = fuv0[1] = 0;
  fuv1[0] = fuv1[1] = 0;
  tip[0] = 0;
  texid = image->get ? image->get(item, image->index, fuv0, fuv1, tip) : -1;
  size = (ImVec2){image->size, image->size};
  uv0 = (ImVec2){fuv0[0], fuv0[1]};
  uv1 = (ImVec2){fuv1[0], fuv1[1]};
  if (item->flags & GUI_FLAGS_BUTTON) {
    pl = item->parent && item->parent->type == list ? item->parent->l : 0;
    bg = (pl && pl->selected == item) ? bgsel : bgnorm;
    if (texid >= 0)
      click = igImageButton("##im", 
        (ImTextureID)texid, size, uv0, uv1, bg, tint);
    else
      click = igButton("?", size);
    if (click) {
      if (pl) { 
        pl->selected = item; 
        pl->clicked = 1; 
      }
      else if (item->sync) 
        item->sync(item, GUI_SYNC_INTERNAL);
    }
  }
  else if (texid >= 0)
    igImage((ImTextureID)texid, size, uv0, uv1,
            tint, border);
  else
    igDummy(size);
  if (tip[0] && igIsItemHovered(0))
    igSetTooltip("%s", tip);
}

static void GuiHeader(gui_item *item) {
  gui_item *it;
  char *label;

  label = item->label ? item->label->str : "";
  if (item->flags & GUI_FLAGS_OPEN)
    igSetNextItemOpen(1, ImGuiCond_Once);
  if (igCollapsingHeader_TreeNodeFlags(label, 0)) {
    for (it=item->child;it;it=it->next)
      GuiItem(it);
  }
}

static void GuiScalar(gui_item *item) {
  ImGuiDataType data_type;
  gui_scalar *scalar;
  void *p_data, *p_step, *p_min, *p_max;
  double dstep;
  char id[32];
  char *fmt;
  int n, res;

  scalar = item->s;
  GuiItemLabel(item, id);
  p_data = scalar->data;
  p_step = &scalar->step;
  if (scalar->ptype->is_float && scalar->ptype->size == sizeof(double)) {
    dstep = scalar->step;
    p_step = &dstep;
  }
  fmt = scalar->fmt ? scalar->fmt : scalar->ptype->fmt;
  p_min = (void*)&scalar->vmin;
  p_max = (void*)&scalar->vmax;
  if (memcmp(p_min, p_max, scalar->ptype->size) == 0) {
    p_min = (void*)&scalar->ptype->vmin;
    p_max = (void*)&scalar->ptype->vmax;
  }
  data_type = GuiIgType(scalar->ptype);
  n = scalar->count > 1 ? scalar->count : 1;
  res = 0;
  if (scalar->type == 0) /* input */
    res = igInputScalarN(id, data_type, p_data, n, p_step, 0, fmt, 0);
  else if (scalar->type == 1) /* drag */
    res = igDragScalarN(id, data_type, p_data, n, scalar->step > 0.0f ? scalar->step : 1.0f, p_min, p_max, fmt, 0);
  else if (scalar->type == 2) /* slider */
    res = igSliderScalarN(id, data_type, p_data, n, p_min, p_max, fmt, 0);
  if (res && item->sync) 
    item->sync(item, GUI_SYNC_INTERNAL);
}

static void GuiList(gui_item *item) {
  ImGuiStyle *style;
  ImVec2 pos, last;
  gui_list *list;
  gui_item *it, *sel, *prev;
  float right, w;
  char id[32], *preview;
  int res;

  style = igGetStyle();
  list = item->l;
  GuiItemLabel(item, id);
  res = 0;
  GuiListSync(item, 1);
  preview = "";
  if (list->type == 1 && list->selected) {
    sel = list->selected;
    if (sel->label)
      preview = sel->label->str;
    else
      preview = sel->t->str;
  }
  prev = list->selected;
  if (list->type == 0) /* list box */
    res = igBeginListBox(id, def_size);
  else if (list->type == 1) /* combo box */
    res = igBeginCombo(id, preview, 0);
  else if (list->type == 2)
    res = 1; /* radio group, laid out in a row */
  else if (list->type == 3) { /* grid of selectable items */
    /* children must set list->clicked if they are clicked */
    list->clicked = 0;
    if (!igBeginChild_Str(id, (ImVec2){0, list->height}, ImGuiChildFlags_Border, 0)) {
      igEndChild();
      return;
    }
    igGetContentRegionAvail(&last);
    igGetCursorScreenPos(&pos);
    right = pos.x + last.x;
    res = 1;
  }
  if (!res) { return; }
  for_each_child(item, it) {
    if (list->type == 2 && it != item->child)
      igSameLine(0.0f, 10.0f); 
    else if (list->type == 3 && it != item->child) {
      igGetItemRectMax(&last);
      w = it->im->size + style->FramePadding.x * 2.0f;
      if (last.x + style->ItemSpacing.x + w <= right)
        igSameLine(0.0f, -1.0f);
    }
    GuiItem(it);
  }
  if (list->type == 0)
    igEndListBox();
  else if (list->type == 1)
    igEndCombo();
  else if (list->type == 2) {}
  else if (list->type == 3)
    igEndChild();
  GuiListSync(item, 2);
  if ((list->selected != prev || list->clicked) && item->sync) 
    item->sync(item, GUI_SYNC_INTERNAL);
}

static void GuiTab(gui_item *item) {
  ImVec2 avail, label_size;
  gui_tab *tab;
  gui_item *it;
  char *label, id[32];
  float w, side_w, page_w;
  int is_sel;

  tab = item->tb;
  if (!tab->selected) 
    tab->selected = item->child;
  GuiItemLabel(item, id);
  if (tab->type == 0) { /* tab bar */
    if (!igBeginTabBar(id, ImGuiTabBarFlags_FittingPolicyScroll)) { return; }
    for (it=item->child;it;it=it->next) {
      label = it->label ? it->label->str : "";
      if (igBeginTabItem(label, 0, 0)) {
        tab->selected = it;
        GuiItem(it);
        igEndTabItem();
      }
    }
    igEndTabBar();
  }
  else if (tab->type == 1) {
    igGetContentRegionAvail(&avail);
    side_w = 140.0f;
    for_each_child(item, it) {
      label = it->label ? it->label->str : "";
      igCalcTextSize(&label_size, label, 0, 1, -1.0f);
      w = label_size.x + 2.0f * igGetStyle()->FramePadding.x + 4.0f;
      if (w > side_w)
        side_w = w;
    }
    side_w = limit(side_w, 1.0f, avail.x * 0.45f);
    page_w = avail.x - side_w - igGetStyle()->ItemSpacing.x;
    if (page_w < 1.0f) page_w = 1.0f;
    igBeginChild_Str(id, (ImVec2){side_w, avail.y}, 0,
      ImGuiWindowFlags_HorizontalScrollbar);
    for_each_child(item, it) {
      label = it->label ? it->label->str : "";
      is_sel = it == tab->selected;
      if (igSelectable_Bool(label, is_sel, 0, def_size)) {
        tab->selected = it;
      }
    }
    igEndChild();
    igSameLine(0.0f, -1.0f);
    id[strlen(id)-1] += 1;
    igBeginChild_Str(id, (ImVec2){page_w, avail.y}, 0, 0);
    if (tab->selected)
      GuiItem(tab->selected);
    igEndChild();
  }
}

ImGuiTableFlags table_flags =
  ImGuiTableFlags_BordersOuter |
  ImGuiTableFlags_Resizable |
  ImGuiTableFlags_NoSavedSettings;

ImGuiTreeNodeFlags node_flags =
  ImGuiTreeNodeFlags_DefaultOpen |
  ImGuiTreeNodeFlags_OpenOnArrow |
  ImGuiTreeNodeFlags_OpenOnDoubleClick |
  ImGuiTreeNodeFlags_SpanAvailWidth |
  ImGuiTreeNodeFlags_SpanFullWidth;

static void GuiNode(gui_item *item) {
  ImGuiTreeNodeFlags flags;
  gui_item *tree, *it;
  int i, open;

  flags = node_flags;
  tree = GuiNodeTree(item);
  if (tree->tr->selected == item)
    flags |= ImGuiTreeNodeFlags_Selected;
  for (it=item->child;it;it=it->next) {
    if (!(it->flags & GUI_FLAGS_NODE_CONTENT)) { break; }
  }
  if (!item->child || !it)
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  if (tree->tr->column_count) {
    igTableNextRow(0, 0.0);
    igTableSetColumnIndex(0);
    igAlignTextToFramePadding();
  }
  open = igTreeNodeEx_Str(item->label->str, flags);
  if (igIsItemClicked(0)) {
    tree->tr->selected = item;
    if (tree->tr->sync.unmap)
      tree->tr->sel = tree->tr->sync.unmap(item);
    if (tree->tr->select)
      tree->tr->select(tree);
  }
  i=0;
  for (it=item->child;it;it=it->next) {
    if (!(it->flags & GUI_FLAGS_NODE_CONTENT)) { continue; }
    igTableSetColumnIndex(++i);
    GuiItem(it);
  }
  if (!open) { return; }
  igTableSetColumnIndex(0);
  for (it=item->child;it;it=it->next) {
    if (it->flags & GUI_FLAGS_NODE_CONTENT) { continue; }
    GuiItem(it);
  }
  if (!(flags & ImGuiTreeNodeFlags_Leaf))
    igTreePop();
}

static void GuiTree(gui_item *item) {
  ImGuiTableFlags flags;
  gui_tree *tree;
  int tbl;

  tree = item->tr;
  if (!tree->selected) {
    tree->selected = item;
    if (tree->sync.unmap)
      tree->sel = tree->sync.unmap(tree->selected);
    if (tree->select)
      tree->select(item);
  }
  if (tree->sync.data)
    GuiTreeSync(item);
  tbl = 0;
  if (tree->column_count) {
    flags = table_flags;
    if (item->flags & GUI_FLAGS_HSCROLL)
      flags |= ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit;
    tbl = igBeginTable("##tree", tree->column_count+1,
                       flags, def_size,
                       (item->flags & GUI_FLAGS_HSCROLL) ? 620.0f : 0.0f);
    if (!tbl) { return; }
    if (item->flags & GUI_FLAGS_HSCROLL) {
      igTableSetupColumn("##field", ImGuiTableColumnFlags_WidthFixed, 220.0f, 0);
      igTableSetupColumn("##value", ImGuiTableColumnFlags_WidthFixed, 400.0f, 0);
    }
  }
  igPushStyleVar_Float(ImGuiStyleVar_IndentSpacing, 9.0f);
  GuiNode(item);
  igPopStyleVar(1);
  if (tbl)
    igEndTable();
}

static void GuiColor(gui_item *item) {
  gui_color *color;
  char id[32];
  int i, res;

  color = item->c;
  GuiItemLabel(item, id);
  if (color->unparse)
    color->unparse(color->data, color->value);
  else {
    for (i=0;i<3;i++)
      color->value[i] = ((float*)color->data)[i];
  }
  res = igColorEdit3(id, color->value, 0);
  if (res) {
    if (color->parse)
      color->parse(color->value, color->data);
    else {
      for (i=0;i<3;i++)
        ((float*)color->data)[i] = color->value[i];
    }
  }
  if (res && item->sync) 
    item->sync(item, GUI_SYNC_INTERNAL);
}

static void GuiGrid(gui_item *item) {
  ImGuiStyle *style;
  ImGuiTableFlags flags;
  ImVec2 rgn, table_size;
  gui_item *it;
  gui_grid *grid;
  float inner_width, frame_height, cw, ch;
  int rc, cc, ri, ci;
  int tbl;

  grid = item->g;
  rc = grid->row_count;
  cc = grid->column_count;
  if (cc==0) {
    if (rc!=1) { return; }
    for (it=item->child;it;it=it->next,cc++);
  }
  else if (rc==0) {
    if (cc!=1) { return; }
    for (it=item->child;it;it=it->next,rc++);
  }
  style = igGetStyle();
  flags = table_flags;
  table_size = def_size;
  inner_width = 0.0f;
  if ((item->flags & GUI_FLAGS_HSCROLL) && cc > 1) {
    flags |= ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit;
    frame_height = igGetFrameHeight();
    cw = 130.0f;
    ch = frame_height + (2.0f * style->CellPadding.y);
    inner_width = cc * cw;
    table_size.y = (rc * ch) + style->ScrollbarSize + 2.0f;
  }
  tbl = igBeginTable("##grid", cc, flags, table_size, inner_width);
  igGetContentRegionAvail(&rgn);
  rgn.y -= style->ItemSpacing.y*rc;
  rgn.x = 0; rgn.y /= rc;
  if (tbl) {
    if (inner_width > 0.0f) {
      for (ci=0;ci<cc;ci++)
        igTableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, cw, 0);
    }
    else if (cc == 2 
     && grid->header_weight > 0.0f && grid->header_weight < 2.0f) {
      igTableSetupColumn("##first", ImGuiTableColumnFlags_WidthStretch,
        grid->header_weight, 0);
      igTableSetupColumn("##second", ImGuiTableColumnFlags_WidthStretch,
        2.0f - grid->header_weight, 0);
    }
    it=item->child;
    for (ri=0;ri<rc;ri++) {
      igTableNextRow(0, 0.0);
      for (ci=0;ci<cc;ci++) {
        igTableSetColumnIndex(ci);
        if (item->flags & GUI_FLAGS_CELL_CW) {
          igPushID_Int(ri * cc + ci);
          igBeginChild_Str("##cell", rgn, 0, 0);
        }
        GuiItem(it);
        if (item->flags & GUI_FLAGS_CELL_CW) {
          igEndChild();
          igPopID();
        }
        it=it->next;
      }
    }
    igEndTable();
  }
}

void GuiItem(gui_item *item) {
  gui_list *plist;
  gui_item *sibling;
  int disabled, idx;

  if (item->update) 
    item->update(item);
  if (item->sync) 
    item->sync(item, GUI_SYNC_EXTERNAL);
  if (!(item->flags & GUI_FLAGS_VISIBLE)) { return; }
  for (idx=0,sibling=item->prev;sibling;sibling=sibling->prev)
    idx++;
  igPushID_Int(idx);
  if (item->flags & GUI_FLAGS_SAMELINE)
    igSameLine(0.0, -1.0f);
  if (item->flags & GUI_FLAGS_RIGHTALIGN)
    GuiItemRightAlign(item);
  if (item->parent && item->parent->type == list && item->parent->l->type != 3) {
    char id[16];
    plist = item->parent->l;
    strcpy(id, "##value");
    if (plist->type == 2) {
      if (igRadioButton_Bool(id, item == plist->selected))
        plist->selected = item;
    }
    else {
      if (igSelectable_Bool(id, item == plist->selected, 0, def_size))
        plist->selected = item;
    }
    igSameLine(0.0, 0.0);
  }
  if (item->label)
    GuiTextUpdate(item->label);
  disabled = !!(item->flags & GUI_FLAGS_DISABLED);
  if (disabled) 
    igBeginDisabled(1);
  switch (item->type) {
  case none:
    break;
  case window:
    GuiWindow(item);
    break;
  case group:
    GuiGroup(item);
    break;
  case text:
    GuiText(item);
    break;
  case scalar:
    GuiScalar(item);
    break;
  case list:
    GuiList(item);
    break;
  case tab:
    GuiTab(item);
    break;
  case node:
    GuiNode(item);
    break;
  case tree:
    GuiTree(item);
    break;
  case color:
    GuiColor(item);
    break;
  case grid:
    GuiGrid(item);
    break;
  case button:
    GuiButton(item);
    break;
  case separator:
    igSeparator();
    break;
  case header:
    GuiHeader(item);
    break;
  case toggle:
    GuiToggle(item);
    break;
  case image:
    GuiImage(item);
    break;
  }
  if (disabled) 
    igEndDisabled();
  if (item->tip && item->type != header 
   && igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    igSetTooltip("%s", item->tip);
  igPopID();
}

struct gui_font {
  gui_glyph glyphs[GUI_GLYPH_COUNT];
  int rects[GUI_GLYPH_COUNT];
  ImFont *imfont;
  ImFontAtlas *atlas;
  struct gui_font *next;
};

static gui_font *gui_fonts;

/**
 * Synthesize a glyph of size and color from an 
 * ImFontGlyph and atlas. 
 * 
 * For use when a target font lacks certain glyphs that
 * need to be filled in.
 */
static void GuiFontSynthesizeGlyph(gui_glyph *dst, const ImFontGlyph *glyph,
  const uint8_t *atlas_pixels, int atlas_width, int atlas_height,
  float size, rgba color) {
  uint8_t mask[GUI_GLYPH_SIZE][GUI_GLYPH_SIZE] = {0};
  uint8_t *d;
  int x, y, dx, dy, sx, sy, idx, alpha, edge;
  float nx, ny, gx, gy, width, height;

  if (!glyph->Visible || glyph->AdvanceX <= 0 || size <= 0)
    return;
  width = glyph->X1 - glyph->X0;
  height = glyph->Y1 - glyph->Y0;
  if (width <= 0 || height <= 0) 
    return;
  for (y=0;y<GUI_GLYPH_SIZE;y++) {
    for (x=0;x<GUI_GLYPH_SIZE;x++) {
      /* get normalized coords for the cell center */
      nx = (x + 0.5f) / GUI_GLYPH_SIZE;
      ny = (y + 0.5f) / GUI_GLYPH_SIZE;
      /* transform with respect to the glyph bounding box */
      gx = ((nx * glyph->AdvanceX) - glyph->X0) / width;
      gy = ((ny * size) - glyph->Y0) / height;
      if (gx < 0 || gx >= 1 || gy < 0 || gy >= 1) continue;
      /* get location of the corresponding source pixel in the atlas */
      sx = (int)((glyph->U0 + gx * (glyph->U1 - glyph->U0)) * atlas_width);
      sy = (int)((glyph->V0 + gy * (glyph->V1 - glyph->V0)) * atlas_height);
      /* get the alpha value from that pixel as the mask */
      if (sx >= 0 && sx < atlas_width && sy >= 0 && sy < atlas_height) {
        idx = (sy * atlas_width) + sx;
        mask[y][x] = atlas_pixels[idx*4+3];
      }
    }
  }
  /* map masks to pixels and apply edge fading */
  d = (uint8_t*)dst->pixels;
  for (y=0;y<GUI_GLYPH_SIZE;y++) {
    for (x=0;x<GUI_GLYPH_SIZE;x++) {
      alpha = mask[y][x];
      edge = 0;
      /* get the maximum alpha value in the 3x3 neighborhood 
         (at edges this will be the inner alpha value) */
      for (dy=-1;dy<=1;dy++)
        for (dx=-1;dx<=1;dx++)
          if (y+dy >= 0 && y+dy < GUI_GLYPH_SIZE
           && x+dx >= 0 && x+dx < GUI_GLYPH_SIZE
           && mask[y+dy][x+dx] > edge)
            edge = mask[y+dy][x+dx];
      /* apply color to the destination pixel 
         (alpha will be half the inner value at edges 
          for an effective blend) */
      *(d++) = alpha ? color.r : 20;
      *(d++) = alpha ? color.g : 20;
      *(d++) = alpha ? color.b : 20;
      *(d++) = max(alpha, edge/2);
    }
  } 
  dst->advance = GUI_GLYPH_SIZE;
  dst->present = 1;
}

gui_font *GuiFontNew(const gui_glyph *glyphs, const gui_font_options *options) {
  ImFontAtlas *atlas;
  ImFontConfig *config;
  ImFont *base;
  ImFontGlyph *glyph;
  ImFontAtlasCustomRect *rect;
  gui_font *font, *it;
  uint32_t *pixels, *s, *d;
  uint8_t *atlas_pixels;
  int c, x, y, idx, width, height;

  /* allocate font */
  font = malloc(sizeof(gui_font));
  if (!font) {
    fprintf(stderr, "Unable to create GUI font: allocation failed\n");
    return 0;
  }
  memcpy(font->glyphs, glyphs, sizeof(font->glyphs));
  /* get an atlas for this font */
  atlas = igGetIO()->Fonts;
  if (atlas->Fonts.Size == 0 && !ImFontAtlas_AddFontDefault(atlas, 0)) {
    fprintf(stderr, "Unable to create GUI font: default font allocation failed\n");
    free(font);
    return 0;
  }
  font->atlas = atlas;
  if (options->synthesize) {
    /* get the pixel data for the atlas and font data */
    ImFontAtlas_GetTexDataAsRGBA32(atlas, &atlas_pixels, &width, &height, 0);
    base = atlas->Fonts.Data[0];
    if (!atlas_pixels || !base || base->FontSize <= 0) {
      fprintf(stderr, "Unable to create GUI font: default font unavailable\n");
      free(font);
      return 0;
    }
    /* fill in missing glyphs */
    for (c='!';c<='~';c++) {
      if (font->glyphs[c-' '].present) continue;
      /* get the imgui glyph for this char */
      glyph = (ImFontGlyph*)ImFont_FindGlyphNoFallback(base, c);
      /* synthesize the missing glyph */
      GuiFontSynthesizeGlyph(&font->glyphs[c-' '], glyph, atlas_pixels,
        width, height, base->FontSize, options->synth_color);
    }
  }
  /* create the imgui font */
  config = ImFontConfig_ImFontConfig();
  if (!config) {
    fprintf(stderr, "Unable to create GUI font: config allocation failed\n");
    free(font);
    return 0;
  }
  config->SizePixels = GUI_GLYPH_SIZE;
  font->imfont = ImFontAtlas_AddFontDefault(atlas, config);
  ImFontConfig_destroy(config);
  if (!font->imfont) {
    fprintf(stderr, "Unable to create GUI font: font allocation failed\n");
    free(font);
    return 0;
  }
  /* register imgui glyphs for each character */
  for (c=' ';c<='~';c++) {
    idx = c - ' ';
    font->rects[idx] = -1;
    if (!font->glyphs[idx].present) continue;
    font->rects[idx] = ImFontAtlas_AddCustomRectFontGlyph(atlas, font->imfont, c,
      GUI_GLYPH_SIZE, GUI_GLYPH_SIZE,
      font->glyphs[idx].advance, (ImVec2){0, 0});
  }
  /* rebuild the font atlas, blank for now */
  if (!ImFontAtlas_Build(atlas)) {
    fprintf(stderr, "Unable to create GUI font: atlas build failed\n");
    free(font);
    return 0;
  }
  /* get a pointer to the atlas pixel data */
  ImFontAtlas_GetTexDataAsRGBA32(atlas, &atlas_pixels, &width, &height, 0);
  pixels = (uint32_t*)atlas_pixels;
  /* add font to the main list of fonts
     before copying glyph pixel data */
  font->imfont->Scale = options->scale;
  font->next = gui_fonts;
  gui_fonts = font;
  /* rebuilding the atlas clears pixel data for any other fonts 
     previously added to that atlas (since the glyph rects have 
     to be reallocated); so [re]copy the pixel data for each font
     associated with this atlas */
  for (it=gui_fonts;it;it=it->next) {
    if (it->atlas != atlas) continue;
    for (c=' ';c<='~';c++) {
      idx = c - ' ';
      if (it->rects[idx] < 0) continue;
      rect = ImFontAtlas_GetCustomRectByIndex(atlas, it->rects[idx]);
      s = it->glyphs[idx].pixels;
      d = pixels + (rect->Y * width + rect->X);
      for (y=0;y<GUI_GLYPH_SIZE;y++) {
        for (x=0;x<GUI_GLYPH_SIZE;x++)
          d[x] = *(s++);
        d += width;
      }
      idx = it->imfont->IndexLookup.Data[c];
      glyph = &it->imfont->Glyphs.Data[idx];
      glyph->Colored = 1;
    }
  }
  atlas->TexPixelsUseColors = 1;
  return font;
}

void GuiFontApply(gui_font *font) {
  igGetIO()->FontDefault = font ? font->imfont : 0;
}

void GuiFontClear(void) {
  gui_font *it, **link;
  ImFontAtlas *atlas;
  
  atlas = igGetIO()->Fonts;
  GuiFontApply(0);
  for (link=&gui_fonts;*link;) {
    it = *link;
    if (it->atlas != atlas) {
      link = &it->next;
      continue;
    }
    *link = it->next;
    free(it);
  }
}

void GuiIgInit() {
  ImGuiStyle *style;
  style = igGetStyle();
  style->Colors[ImGuiCol_WindowBg].w = 0.94f; 
  style->Colors[ImGuiCol_ChildBg].w  = 0.40f;
  style->WindowRounding    = 4.0f;
  style->FrameRounding     = 3.0f;
  style->PopupRounding     = 3.0f;
  style->ScrollbarRounding = 4.0f;
  style->GrabRounding      = 3.0f;
  style->TabRounding       = 3.0f;
  style->WindowPadding     = (ImVec2){ 10.0f, 10.0f };
  style->FramePadding      = (ImVec2){ 6.0f, 3.5f };
  style->ItemSpacing       = (ImVec2){ 8.0f, 6.0f };
  style->ItemInnerSpacing  = (ImVec2){ 6.0f, 4.0f };
  style->IndentSpacing     = 10.0f;
  style->WindowBorderSize  = 1.0f;
  style->FrameBorderSize   = 1.0f;
  style->Colors[ImGuiCol_Border] = (ImVec4){ 0.18f, 0.35f, 0.55f, 0.70f };
}

ImGuiWindowFlags fps_overlay_flags = 
   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
 | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
 | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

void GuiIgFpsOverlay(void) {
  const ImVec2 pos = { 8.0f, 8.0f };
  const ImVec2 pivot = { 0.0f, 0.0f };
  const float alpha = 0.55f;
  ImGuiIO *io;
  float ms;
  
  io = igGetIO();
  ms = io->Framerate > 0.0f ? 1000.0f / io->Framerate : 0.0f;
  igSetNextWindowPos(pos, ImGuiCond_Always, pivot);
  igSetNextWindowBgAlpha(alpha);
  if (igBegin("##fps", 0, fps_overlay_flags))
    igText("%.1f FPS (%.2f ms)", io->Framerate, ms);
  igEnd();
}

void (*GuiIgExtraDraw)(void) = 0;

void GuiIgDraw(gui_item *item) {
  igNewFrame();
  GuiItem(item);
  if (GuiIgExtraDraw) GuiIgExtraDraw();
  igRender();
}

void GuiIgNavEnableKeyboard() {
  ImGuiIO *io;

  io = igGetIO();
  io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

void GuiIgNavDisableKeyboard() {
  ImGuiIO *io;

  io = igGetIO();
  io->ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
}

int GuiIgKeyPressed(int key_code) {
  return igIsKeyPressed_Bool(key_code, 0);
}

int GuiIgWantCaptureMouse() {
  return igGetIO()->WantCaptureMouse;
}

int GuiIgWantCaptureKeyboard() {
  return igGetIO()->WantCaptureKeyboard;
}
