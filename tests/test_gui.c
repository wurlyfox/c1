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
