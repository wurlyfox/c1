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

UTEST(gui, remove_node_moves_every_child_to_parent) {
  gui_item *parent, *to_remove, *child1, *child2, *child3;

  parent = GuiGroupNew();
  to_remove = GuiGroupNew();
  child1 = GuiGroupNew();
  child2 = GuiGroupNew();
  child3 = GuiGroupNew();
  GuiAddChild(parent, to_remove);
  GuiAddChild(to_remove, child1);
  GuiAddChild(to_remove, child2);
  GuiAddChild(to_remove, child3);
  GuiRemoveNode(to_remove);
  /* the children are now the parent's children, in their original order */
  ASSERT_TRUE(parent->child == child1);
  ASSERT_TRUE(child1->next == child2);
  ASSERT_TRUE(child2->next == child3);
  ASSERT_TRUE(child3->next == 0);
  ASSERT_TRUE(child1->parent == parent);
  ASSERT_TRUE(child2->parent == parent);
  ASSERT_TRUE(child3->parent == parent);
  GuiItemFree(parent, 1);
}
