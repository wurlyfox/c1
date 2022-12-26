#include "gui.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include <stdlib.h>
#include <string.h>
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

gui_item def_item = { .flags = GUI_FLAGS_VISIBLE };
gui_text def_text = { .maxlen = 128 };
gui_scalar def_scalar = { 0 };
gui_list def_list = { 0 };
gui_tree_sync def_tree_sync = { 0 };

ImGuiDataType GuiIgType(gui_ptype *ptype) {
  if (ptype->is_float)
    return ImGuiDataType_Float;
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
  int len;

  item = GuiItemNew();
  item->type = text;
  item->t = (gui_text*)malloc(sizeof(gui_text)+def_text.maxlen+4);
  *item->t = def_text;
  if (str) { strcpy(item->t->str, str); }
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

gui_item *GuiGridNew(int column_count, int row_count) {
  gui_item *item;

  item = GuiItemNew();
  item->type = grid;
  item->g = (gui_grid*)malloc(sizeof(gui_grid));
  item->g->column_count = column_count;
  item->g->row_count = row_count;
  return item;
}

void *GuiNodeData(gui_item *item) { /* default _node_unmap */
  return item->label->data;
}

gui_item *GuiNodeMap(void *data, void **c_mapped) {
  gui_item *item, *it;
  int i;

  item = GuiNodeNew(data, _node_unparse);
  if (!c_mapped) { return item; }
  for (i=0;it=c_mapped[i];i++) {
    GuiAddChild(item, it);
  }
  return item;
}

gui_item *GuiTreeNode(gui_item *tree, void *data) {
  gui_item *item;
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

  tree = item->tr;
  sync = &tree->sync;
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
  _node_unparse = 0;
  sync->prev = tree_copy(sync->cur);
}

const ImVec2 def_size = { 0, 0 };

static void GuiWindow(gui_item *item) {
  gui_item *it;
  char *name;

  name = item->label ? item->label->str : "";
  if (!item->parent)
    igBegin(name, 0, 0);
  else
    igBeginChild_ID((ImGuiID)item, def_size, 0, 0);
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
  char *label;

  GuiTextUpdate(item->t);
  label = item->label ? item->label->str : "";
  if (item->flags & GUI_FLAGS_READONLY) {
    if (!label)
      igText(item->t->str);
    else
      igLabelText(label, item->t->str);
  }
  else if (!(item->flags & GUI_FLAGS_MULTILINE))
    igInputText(label, item->t->str, item->t->maxlen, 0, 0, 0);
  else
    igInputTextMultiline(label, item->t->str, item->t->maxlen, def_size, 0, 0, 0);
}

static void GuiButton(gui_item *item) {
  gui_button *button;

  button = item->b;
  if (igButton(button->str, def_size)) {
    button->click(item);
  }
}

static void GuiScalar(gui_item *item) {
  ImGuiDataType data_type;
  gui_scalar *scalar;
  void *p_data, *p_step, *p_min, *p_max;
  char *label, *fmt;

  scalar = item->s;
  label = item->label ? item->label->str : "";
  p_data = scalar->data;
  p_step = &scalar->step;
  fmt = scalar->ptype->fmt;
  p_min = (void*)&scalar->vmin;
  p_max = (void*)&scalar->vmax;
  if (*(uint32_t*)p_min == *(uint32_t*)p_max) {
    p_min = (void*)&scalar->ptype->vmin;
    p_max = (void*)&scalar->ptype->vmax;
  }
  data_type = GuiIgType(scalar->ptype);
  if (scalar->type == 0) /* input */
    igInputScalar(label, data_type, p_data, p_step, 0, fmt, 0);
  else if (scalar->type == 1) /* drag */
    igDragScalar(label, data_type, p_data, 1.0, p_min, p_max, fmt, 0);
  else if (scalar->type == 2) /* slider */
    igSliderScalar(label, data_type, p_data, p_min, p_max,  fmt, 0);
}

static void GuiList(gui_item *item) {
  gui_list *list;
  gui_item *it;
  char *label;
  int res;

  list = item->l;
  label = item->label ? item->label->str : 0;
  if (list->type == 0) /* list box */
    res = igBeginListBox(label, def_size);
  else if (list->type == 1) /* combo box */
    res = igBeginCombo(label, 0, 0);
  for (it=item->child;it;it=it->next)
    GuiItem(it);
  if (!res) { return; }
  if (list->type == 0)
    igEndListBox();
  else if (list->type == 1)
    igEndCombo();
}

ImGuiTableFlags table_flags =
  ImGuiTableFlags_BordersOuter |
  ImGuiTableFlags_Resizable;

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
    tbl = igBeginTableEx(0, (ImGuiID)item, tree->column_count+1,
                         table_flags, def_size, 0.0);
    if (!tbl) { return; }
  }
  GuiNode(item);
  if (tbl)
    igEndTable();
}

static void GuiColor(gui_item *item) {
  gui_color *color;
  char *label;
  int i;

  color = item->c;
  label = item->label ? item->label->str : "";
  if (color->unparse)
    color->unparse(color->data, color->value);
  else {
    for (i=0;i<3;i++)
      color->value[i] = ((float*)color->data)[i];
  }
  igColorEdit3(label, color->value, 0);
  if (color->parse)
    color->parse(color->value, color->data);
  else {
    for (i=0;i<3;i++)
      ((float*)color->data)[i] = color->value[i];
  }
}

static void GuiGrid(gui_item *item) {
  ImVec2 rgn;
  ImGuiStyle *style;
  gui_item *it;
  gui_grid *grid;
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
  tbl = igBeginTableEx(0, (ImGuiID)item, cc,
                       table_flags, def_size, 0.0);
  igGetContentRegionAvail(&rgn);
  style = igGetStyle();
  rgn.y -= style->ItemSpacing.y*rc;
  rgn.x = 0; rgn.y /= rc;
  if (tbl) {
    it=item->child;
    for (ri=0;ri<rc;ri++) {
      igTableNextRow(0, 0.0);
      for (ci=0;ci<cc;ci++) {
        igTableSetColumnIndex(ci);
        if (item->flags & GUI_FLAGS_CELL_CW)
          igBeginChild_ID((ImGuiID)((int)it+1), rgn, 0, 0);
        GuiItem(it);
        if (item->flags & GUI_FLAGS_CELL_CW)
          igEndChild();
        it=it->next;
      }
    }
    igEndTable();
  }
}

void GuiItem(gui_item *item) {
  if (!(item->flags & GUI_FLAGS_VISIBLE)) { return; }
  if (item->flags & GUI_FLAGS_SAMELINE)
    igSameLine(0.0, 0.0);
  if (item->parent && item->parent->type == list) {
    char id[16];
    sprintf(id, "##%i", (uint32_t)item);
    if (igSelectable_Bool(id, item == item->parent->l->selected, 0, def_size))
      item->parent->l->selected = item;
    igSameLine(0.0, 0.0);
  }
  if (item->label)
    GuiTextUpdate(item->label);
  igPushID_Int((int)item);
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
  }
  igPopID();
}

void GuiIgInit() {
}

void GuiIgDraw(gui_item *item) {
  igNewFrame();
  GuiItem(item);
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
  return igIsKeyPressed(key_code, 0);
}
