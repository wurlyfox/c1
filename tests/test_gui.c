#include "utest.h"
#include "ext/lib/gui.h"

UTEST(gui, add_child_links_parent_and_child) {
  gui_item *parent, *child;

  parent = GuiGroupNew();
  child = GuiGroupNew();
  GuiAddChild(parent, child);
  ASSERT_TRUE(parent->child == child);
  ASSERT_TRUE(child->parent == parent);
  GuiItemFree(parent, 1);
}

UTEST(gui, remove_node_moves_only_child_to_parent) {
  gui_item *parent, *item, *child;

  parent = GuiGroupNew();
  item = GuiGroupNew();
  child = GuiGroupNew();
  GuiAddChild(parent, item);
  GuiAddChild(item, child);
  GuiRemoveNode(item);
  ASSERT_TRUE(parent->child == child);
  ASSERT_TRUE(child->parent == parent);
  ASSERT_TRUE(child->next == 0);
  GuiItemFree(parent, 1);
}

UTEST(gui, remove_node_without_children) {
  gui_item *parent, *first, *second;

  parent = GuiGroupNew();
  first = GuiGroupNew();
  second = GuiGroupNew();
  GuiAddChild(parent, first);
  GuiAddChild(parent, second);
  GuiRemoveNode(first);
  ASSERT_TRUE(parent->child == second);
  ASSERT_TRUE(second->prev == 0);
  ASSERT_TRUE(second->next == 0);
  GuiItemFree(parent, 1);
}
