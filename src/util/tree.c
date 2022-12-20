#include "tree.h"
#include <string.h>
//#include "mmalloc.h"

tree_node_t *tree_node_next_ancestor(tree_node_t *node, tree_node_t *root) {
  while (node=node->parent) {
    if (node == root) { return 0; }
    if (node->next) { return node->next; }
  }
  return 0;
}

tree_node_t *tree_node_next_descendant(tree_node_t *node) {
  while (node->child) {
    node = node->child;
  }
  return node;
}

tree_node_t *tree_node_next_cousin(tree_node_t *node, tree_node_t *root) {
  tree_node_t *it;
  int level, l;

  for (it=node,level=0;it;it=it->parent,++level);
  l = level;
  for (l-=1,it=node->parent;it&&!it->next;it=it->parent,l--);
  if (!it) { return 0; }
  if (it == root) { return 0; }
  it=it->next;
  for (;l!=level;it=it->child,l++);
  return it;
}

tree_node_t *tree_node_prev_ancestor(tree_node_t *node, tree_node_t *root) {
  while (node=node->parent) {
    if (node == root) { return 0; }
    if (node->prev) { return node->prev; }
  }
  return 0;
}

tree_node_t *tree_node_last_descendant(tree_node_t *node) {
  while (node->tail_child) {
    node = node->tail_child;
  }
  return node;
}

/**
 * allocate a tree node
 *
 * \returns tree_node_t* - node
 */
tree_node_t *tree_node_alloc() {
  tree_node_t *node;

  node = (tree_node_t*)malloc(sizeof(tree_node_t));
  return node;
}

/**
 * free a tree node, and optionally its descendants
 *
 * \param node tree_node_t* - node
 * \param free_desc int - flag to free descendants
 */
void tree_node_free(tree_node_t *node, int free_desc) {
  tree_node_t *it, *next;
  if (free_desc) {
    for (it=node->child;it;it=next) {
      next=it->next;
      tree_node_free(it, 1);
    }
  }
  free(node);
}

tree_node_t empty_node = { 0 };

/**
 * create a new tree node
 *
 * \returns tree_node_t* - node
 */
tree_node_t *tree_node_new() {
  tree_node_t *node;

  node = tree_node_alloc();
  *node = empty_node;
  return node;
}

/**
 * add a child node to a parent node
 *
 * \param parent tree_node_t* - parent node
 * \param node tree_node_t* - child node
 */
void tree_add_node(tree_node_t *parent, tree_node_t *node) {
  node->parent = parent;
  if (!parent->tail_child) {
    parent->head_child = node;
    parent->tail_child = node;
  }
  else {
    node->prev = parent->tail_child;
    parent->tail_child->next = node;
    parent->tail_child = node;
  }
}

/**
 * insert a child node after another child node
 *
 * \param parent tree_node_t* - parent node
 * \param prev tree_node_t* - [existing] preceding child node (or 0 if front insertion)
 * \param node tree_node_t* - child node to insert
 */
void tree_insert_node(tree_node_t *parent, tree_node_t *prev, tree_node_t *node) {
  if (prev) {
    node->next = prev->next;
    if (prev->next)
      prev->next->prev = node;
    else
      parent->tail_child = node;
    prev->next = node;
  }
  else {
    node->next = parent->head_child;
    if (parent->head_child)
      parent->head_child->prev = node;
    else
      parent->tail_child = node;
    parent->head_child = node;
  }
  node->parent = parent;
  node->prev = prev;
}

/**
 * remove a node from its tree
 *
 * note that a root node cannot be removed, for it is the defining node
 * [if we had a tree_t this would be possible, but we'd need an extra arg for each call]
 * (on the other hand it -is- possible to -swap- in a new root)
 *
 * \param node tree_node_t* - node to remove
 */
void tree_remove_node(tree_node_t *node) {
  if (!node->parent) { return; }
  if (!node->prev) { node->parent->head_child = node->next; }
  else { node->prev->next = node->next; }
  if (!node->next) { node->parent->tail_child = node->prev; }
  else { node->next->prev = node->prev; }
  node->next = 0; node->prev = 0; node->parent = 0;
}

/**
 * remove a node from its tree and have the parent inherit any grandchildren
 *
 * if the node is the root then its first child is made the root, and the rest of the
 * children are inherited by that child
 *
 * \param node tree_node_t* - node to remove
 * \returns tree_node_t* - [possibly new] root of the tree
 */
tree_node_t *tree_remove_node_inherit(tree_node_t *node) {
  tree_node_t *root, *parent, *it, *next;

  root = node;
  while (root->parent) { root = root->parent; }
  if (node == root) {
    parent = root = node->child;
    tree_remove_node(node->child);
  }
  else {
    parent = node->parent;
    tree_remove_node(node);
  }
  for (it=node->child;it;it=next) {
    next = it->next;
    tree_add_node(parent, it);
  }
  return root;
}


/**
 * test if a node is a descendant of another
 *
 * \param root tree_node_t* - root node
 * \param node tree_node_t* - node to test
 * \returns int - boolean
 */
int tree_has_node(tree_node_t *root, tree_node_t *node) {
  tree_node_t *n;

  tree_first_node_preorder(root, n, node == n);
  return !!n;
}

/**
 * get the index of a child node
 *
 * \param parent tree_node_t* - parent node
 * \param node tree_node_t* - child node
 * \returns - index of child node, or -1 if not found
 */
int tree_child_node_index(tree_node_t *parent, tree_node_t *node) {
  tree_node_t *n;
  int i;

  if (!parent) { return -1; }
  first_child_node_indexed(parent,n,i,node==n);
  if (n == 0) { return -1; }
  return i;
}

/**
 * get the child node at a specific index
 *
 * \param parent tree_node_t* - parent node
 * \param idx int - child node index
 * \returns - child node, or 0 if no parent or index out of range
 */
tree_node_t *tree_child_node_at_index(tree_node_t *parent, int idx) {
  tree_node_t *n;
  int i;

  if (!parent) { return 0; }
  first_child_node_indexed(parent,n,i,idx==i);
  if (!n) { return 0; }
  return n;
}

/**
 * find the descendant node with specific [data] value
 *
 * \param root tree_node_t* - root node
 * \param data void* - data
 * \returns tree_node_t* - node, or 0 if not found
 */
tree_node_t *tree_find_node(tree_node_t *root, void *data) {
  tree_node_t *n;

  tree_first_node(root,n,n->data==data);
  return n;
}

/**
 * get the level of a node
 *
 * \param node tree_node_t* - node
 * \returns int - level
 */
int tree_node_level(tree_node_t *node) {
  tree_node_t *it;
  int level;

  for (level=0,it=node;it;(it=it->parent),++level);
  return level;
}

/**
 * swap a pair of tree nodes
 *
 * the address and value of each node is retained; modifications
 * are to list order only
 *
 * \param l tree_node_t* - 'left' node
 * \param r tree_node_t* - 'right' node
 * \returns tree_node_t* - [possibly new] root of the tree
 */
tree_node_t *tree_swap_nodes(tree_node_t *l, tree_node_t *r) {
  tree_node_t *lparent, *lprev, *lchild, *rparent, *rprev, *rchild;
  tree_node_t *lh_child, *lt_child, *rh_child, *rt_child;
  tree_node_t *it, *root;
  void *data;

  /* this variant is not intended for swapping nodes in separate trees
     as such, if l and r are the respective roots of 2 separate trees
     then the function returns 0 */
  if (!l->parent && !r->parent)
    return 0;
  /*
  for_each_child_node(l, it) {
    it->parent = r;
  }
  for_each_child_node(r, it) {
    it->parent = l;
  }
  */
  lparent = l->parent;
  rparent = r->parent;
  lprev = l->prev;
  rprev = r->prev;
  /*
  lh_child = l->head_child;
  rh_child = r->head_child;
  lt_child = l->tail_child;
  rt_child = r->tail_child;
  l->head_child = rh_child;
  r->head_child = lh_child;
  l->tail_child = rt_child;
  r->tail_child = lt_child;
  */
  tree_remove_node(l);
  tree_remove_node(r);
  if (!lparent) { root = r; }
  if (!rparent) { root = l; }
  if (rprev == l && lparent)
    tree_insert_node(lparent, lprev, r);
  if (rparent)
    tree_insert_node(rparent, (rprev==l)?r:rprev, l);
  if (rprev != l && lparent)
    tree_insert_node(lparent, (lprev==r)?l:lprev, r);
  return root;
}

/**
 * add a child node to a tree by value
 *
 * \param root tree_node_t* - tree root node
 * \param parent void* - [existing] parent node [data] value
 * \param data void* - child node [data] value
 * \returns int - 1 if successful, 0 if parent cannot be found
 */
int tree_add(tree_node_t *root, void *parent, void *data) {
  tree_node_t *n, *p;

  if (parent == 0) { p = root; }
  else { tree_first_node(root, p, p->data == parent); }
  if (p == 0) { return 0; }
  n = tree_node_alloc();
  n->data = data;
  n->next = 0;
  tree_add_node(p, n);
  return 1;
}

/**
 * insert a child node into a tree by value
 *
 * \param root tree_node_t* - tree root node
 * \param prev void* - previous node [data] value
 * \param data void* - node [data] value
 */
void tree_insert(tree_node_t *root, void *prev, void *data) {
  tree_node_t *n, *p;

  tree_first_node(root, p, p->data == data);
  n = tree_node_alloc();
  n->data = data;
  tree_insert_node(root, p, n);
}

/**
 * remove a node from a tree by value (and free the node)
 *
 * \param root tree_node_t* - tree root node
 * \param data void* - node [data] value
 * \returns int - 1 if successful, 0 if value cannot be found or is value of root
 */
int tree_remove(tree_node_t *root, void *data) {
  tree_node_t *n;

  tree_first_node(root, n, n->data == data);
  if (n == 0) { return 0; }
  if (n == root) { return 0; } /* cannot remove the root */
  tree_remove_node(n);
  tree_node_free(n, 1);
  return 1;
}

/**
 * remove a node from a tree by value and have the parent inherit any grandchildren
 *
 * if the node is the root then its first child is made the root, and the rest of the
 * children are inherited by that child
 *
 * \param root tree_node_t* - tree root node
 * \param data void* - node [data] value
 * \returns tree_node_t* - [possibly new] root of the tree, or 0 if value cannot be found
 */
tree_node_t *tree_remove_inherit(tree_node_t *root, void *data) {
  tree_node_t *n, *p, *it, *next;

  tree_first_node(root, n, n->data == data);
  if (n == 0) { return 0; }
  root = tree_remove_node_inherit(n);
  return root;
}

/**
 * test if a tree contains a value
 *
 * \param root tree_node_t* - tree root node
 * \param data void* - node [data] value
 * \returns int - 1 if the value is found, 0 otherwise
 */
int tree_contains(tree_node_t *root, void *data) {
  tree_node_t *n;

  tree_first_node(root, n, n->data == data);
  return !!n;
}

/**
 * get the height of a tree
 *
 * \param root tree_node_t* - tree root node
 * \returns int - height
 */
int tree_height(tree_node_t *root) {
  tree_node_t *it;
  int i;

  for (i=0,it=root;it;it=it->child,i++);
  return i;
}

/**
 * get the index of a child node by value
 *
 * \param parent tree_node_t* - parent node
 * \param data void* - child node [data] value
 * \returns - index of child node, or -1 if no child found with value
 */
int tree_child_index(tree_node_t *parent, void *data) {
  void *d;
  int i;

  if (!parent) { return -1; }
  first_child_indexed(parent,d,i,data==d);
  if (d == 0) { return -1; }
  return i;
}

/**
 * get the value of the child node at a specific index
 *
 * \param parent tree_node_t* - parent node
 * \param int idx - child node index
 * \returns void* - [data] value of child node, or 0 if index out of range or no parent
 */
void *tree_child_at_index(tree_node_t *parent, int idx) {
  void *d;
  int i;

  if (!parent) { return 0; }
  first_child_indexed(parent,d,i,idx==i);
  if (d == 0) { return 0; }
  return d;
}

/**
 * get the number of children for a parent node
 *
 * \param parent tree_node_t* - parent node
 * \returns int - number of children, or -1 if no parent
 */
int tree_child_count(tree_node_t *parent) {
  void *data;
  int i;

  if (!parent) { return -1; }
  for_each_child_indexed(parent,data,i);
  return i;
}

/**
 * copy a tree
 *
 * the destination tree will have the same structure and values
 * as the source tree, but the nodes in the destination tree will be
 * separate objects from the nodes in the source tree
 *
 * \param root tree_node_t* - source tree root node
 * \returns tree_node_t* - destination tree root node
 */
 tree_node_t *tree_copy(tree_node_t *root) {
  tree_node_t *it;
  tree_node_t *droot, *dit;

  droot = tree_node_new();
  droot->data = root->data;
  for_each_child_node(root, it) {
    dit = tree_copy(it);
    tree_add_node(droot, dit);
  }
  return droot;
}

/**
 * flatten a tree to a list (pre-order form)
 *
 * the values of the list nodes will be the same as the values of the tree nodes
 *
 * \param root - tree root node
 * \returns list_t - flattened tree
 */
list_t *tree_flatten(tree_node_t *root) {
  list_t *list;
  void *data;

  list = list_new();
  tree_preorder(root, data) {
    list_append(list, data);
  }
  return list;
}

/**
 * flatten a tree to a list (pre-order form)
 *
 * the values of the list nodes will be the tree nodes themselves
 *
 * \param root - tree root node
 * \returns list_t - flattened tree nodes
 */
list_t *tree_flatten_nodes(tree_node_t *root) {
  list_t *list;
  tree_node_t *it;

  list = list_new();
  tree_preorder_nodes(root, it) {
    list_append(list, it);
  }
  return list;
}


tree_node_t *list_unflatten_nodes_r(tree_node_t *parent, list_node_t **it) {
  tree_node_t *node, *dparent, *dnode;

  dparent = tree_node_new();
  dparent->data = parent->data;
  while (*it && (node=(tree_node_t*)((*it)->data)) && node->parent == parent) {
    *it = (*it)->next;
    dnode = list_unflatten_nodes_r(node, it);
    if (dnode) { tree_add_node(dparent, dnode); }
  }
  return dparent;
}

/**
 * 'unflatten' a tree (in list form) flattened with tree_flatten_nodes
 *
 * tree_flatten_nodes stores the nodes instead of just the values so that this function can
 * be used to recover the original tree structure; any nodes removed from the flattened list
 * will also be removed in the unflattened tree
 *
 * exchanging of position in the flattened list vs. the original tree is unaccounted for as
 * there otherwise could be several possible inverse unflattened trees resulting from corresponding
 * modifications to the original tree; addition of new nodes to the flattened list are unaccounted
 * for the same reason
 */
tree_node_t *list_unflatten_nodes(list_t *list) {
  tree_node_t *root, *droot;
  list_node_t *it;

  if (!list || !list->head || !list->head->data) { return 0; }
  root = (tree_node_t*)list->head->data;
  it = list->head->next;
  droot = list_unflatten_nodes_r(root, &it);
  return droot;
}

/**
 * map [the datas of] all nodes in a tree to a new tree, in pre-order
 *
 * \param root - tree root node
 * \param map - callback to map the data of each node
 * \return - mapped tree root node
 */
tree_node_t *tree_map(tree_node_t *root, void *(*map)(void*)) {
  tree_node_t *mapped, *it, *mc;
  void *data;

  data = (*map)(root->data);
  if (data == 0) { return 0; }
  mapped = tree_node_new();
  mapped->data = data;
  for_each_child_node(root, it) {
    mc = tree_map(it, map);
    if (mc == 0) { continue; }
    tree_add_node(mapped, mc);
  }
  return mapped;
}

/**
 * map [the datas of] all nodes in a tree to a new tree, in post-order
 * note that because this occurs in post-order, the callback function will
 * receive a zero terminated array of pointers to mapped children for each non-leaf node
 *
 * \param root - tree root node
 * \param map - callback to map the data of each node
 * \return - mapped tree root node
 */
tree_node_t *tree_map_postorder(tree_node_t *root, void *(*map)(void*, void**)) {
  tree_node_t *mapped, **c_mapped, *it, *mc;
  void *data, **cd_mapped, *md;
  int count, i;

  count=tree_child_count(root);
  c_mapped=(tree_node_t**)malloc(sizeof(tree_node_t*)*count);
  cd_mapped=(void**)malloc(sizeof(void*)*(count+1));
  for_each_child_node_indexed(root, it, i) {
    mc = tree_map_postorder(it, map);
    c_mapped[i] = mc;
    cd_mapped[i] = mc->data;
  }
  cd_mapped[i]=0;
  md=(*map)(root->data, cd_mapped);
  mapped=tree_node_new();
  mapped->data=md;
  for (i=0;i<count;i++)
    tree_add_node(mapped, c_mapped[i]);
  free(c_mapped);
  free(cd_mapped);
  return mapped;
}

/**
 * recursively map the [datas of all] nodes of a tree to a new [non-tree or tree-like]
 * object, in post-order
 *
 * the callback function should determine how to combine the mapped/combined children
 * and unmapped node into a new object
 *
 * \param root - tree root node
 * \param map - callback to combine the data for a node and the mapped/combined children
 * \returns - new object
 */
void *tree_map_to(tree_node_t *root, void *(*map)(void*, void**)) {
  void *mapped, **c_mapped, *data;
  tree_node_t *it;
  int i, count;

  count=tree_child_count(root);
  c_mapped=(void**)malloc(sizeof(void*)*(count+1));
  for_each_child_node_indexed(root, it, i) {
    c_mapped[i] = tree_map_to(it, map);
  }
  c_mapped[i]=0;
  mapped=map(root->data, c_mapped);
  free(c_mapped);
  return mapped;
}

/**
 * map a non-tree or tree-like object to a new tree.
 *
 * (given callbacks to extract an array of the children for the object
 *  [and for any of the extracted child objects, recursively]
 *  and to map each extracted object to the desired data objects)
 *
 * \param data - non-tree or tree-like object
 * \param child_fn - callback to extract zero terminated array of [pointers to] children
 * \param map - callback to map an extracted child
 * \returns - new tree root node
 */
tree_node_t *tree_map_from(void *data, void **child_fn(void*), void *(*map)(void*)) {
  void *mapped, **children, *child;
  tree_node_t *root, *parent, *mc;
  int i, count;

  root=tree_node_new();
  root->data=map?map(data):data;
  children = child_fn(data);
  for (i=0;children[i];i++) {
    child = children[i];
    mc = tree_map_from(child, child_fn, map);
    tree_add_node(root, mc);
  }
  free(children);
  return root;
}

tree_delta_t *tree_delta_alloc() {
  tree_delta_t *delta;

  delta = (tree_delta_t*)malloc(sizeof(tree_delta_t));
  return delta;
}

void tree_delta_free(tree_delta_t *delta) {
  free(delta);
}

/**
 * get a list of the differences between 2 trees
 *
 * currently nodes are considered equal if the datas as integer value
 * are equal
 *
 * differences will consist of
 *  op=1:add, op=2:remove, op=3:move, op=4:swap, op=5:new root
 * the list of differences along with the first tree can be used to
 * construct the second tree
 *
 * \param src - source tree root node
 * \param dst - dest tree root node
 * \returns list_t* - list of differences (tree_delta_t)
 */
list_t *tree_changes(tree_node_t *src, tree_node_t *dst) {
  /* flatten src and dst; then get src - dst, dst - src */
  tree_node_t *sr, *node, *sn, *dn, *nl, *nr, *it;
  tree_delta_t *delta;
  list_t *sf, *df, *sd, *dd, *sc, *dl;
  int si, di;
  void *val;

  dl = list_new();
  sf = tree_flatten(src);
  df = tree_flatten(dst);
  sd = list_difference(sf, df, 0);
  dd = list_difference(df, sf, 0);
  /* build intermediary list by removing sd nodes from src */
  sr = tree_copy(src);
  list_for_each(sd, val) {
    delta = tree_delta_alloc();
    delta->op = 2;
    delta->value = val;
    list_append(dl, delta);
    sr = tree_remove_inherit(sr, val);
  }
  list_for_each(dd, val) {
    /*
       for each node in dst only
       if it hasnt already been added to the intermediary list
       create a new node for it to be placed in the intermediary list
       and while its parent value in the dst list is also only in dst
         create new nodes for each parent and so on that is only in dst
         until one is found with a value also in src
         or until there is no more parent (root of dst is reached)
       add the new node or the topmost new node to the parent with existing
       value in intermediary list
    */
    if (!tree_contains(sr, val)) {
      node = tree_find_node(dst, val);
      sn = tree_node_new();
      sn->data = val;
      while (node && node->parent && !tree_contains(sr, node->parent->data)) {
        node = node->parent;
        sn->parent = tree_node_new();
        sn = sn->parent;
        sn->data = node->data;
      }
      delta = tree_delta_alloc();
      if (!node) {
        delta->op = 5; /* new root; add old root as child */
        delta->parent = 0;
        delta->prev = 0;
        delta->value = sn->data;
        list_append(dl, delta);
        for (node=sn->child;node;node=node->child) {
          delta = tree_delta_alloc();
          delta->op = 1;
          delta->parent = node->parent->data;
          delta->prev = 0;
          delta->value = node->data;
          list_append(dl, delta);
        }
        tree_add_node(sn, sr);
        sr = sn;
      }
      else {
        delta->op = 1;
        delta->parent = node->parent->data;
        delta->prev = node->prev ? node->prev->data : 0;
        delta->value = node->data;
        list_append(dl, delta);
        node = tree_find_node(sr, node->parent->data);
        tree_add_node(node, sn);
      }
    }
  }
  sc = tree_flatten(sr);
  list_for_each_reverse(sc, val) {
    sn = tree_find_node(sr, val);
    dn = tree_find_node(dst, val);
    if ((sn->parent ? sn->parent->data:0) != (dn->parent ? dn->parent->data:0)) {
      node = tree_find_node(sr, dn->parent->data);
      delta = tree_delta_alloc();
      delta->op = 3; /* move */
      delta->parent = dn->parent->data;
      tree_remove_node(sn);
      tree_add_node(node, sn);
      delta->prev = sn->prev ? sn->prev->data: 0;
      delta->value = sn->data;
      list_append(dl, delta);
    }
  }
  list_for_each(sc, val) {
    sn = tree_find_node(sr, val);
    while (sn->parent) {
      dn = tree_find_node(dst, val);
      si = tree_child_node_index(sn->parent, sn);
      di = tree_child_node_index(dn->parent, dn);
      if (si != di) {
        nl = sn;
        nr = tree_child_node_at_index(sn->parent, di);
        delta = tree_delta_alloc();
        delta->op = 4; /* swap */
        delta->v1 = nl->data;
        delta->v2 = nr->data;
        list_append(dl, delta);
        tree_swap_nodes(nl, nr);
      }
      else { break; }
    }
  }
  return dl;
}

/**
 * apply a list of deltas to a tree to form a new tree
 *
 * \param tree_node_t* src - source tree root node
 * \param list_t* deltas - list of differences (tree_delta_t)
 * \returns tree_node_t - new tree root node
 */
tree_node_t *tree_apply(tree_node_t *src, list_t *deltas) {
  tree_node_t *dst;
  tree_node_t *node, *parent, *prev, *l, *r;
  tree_delta_t *delta;

  dst = tree_copy(src);
  list_for_each(deltas, delta) {
    switch (delta->op) {
      case 2: // remove
        tree_remove_inherit(dst, delta->value);
        break;
      case 3: // move
        node = tree_find_node(dst, delta->value);
        parent = tree_find_node(dst, delta->parent);
        prev = tree_find_node(dst, delta->prev);
        tree_remove_node(node);
        tree_insert_node(parent, prev, node);
        break;
      case 1: // add
        node = tree_node_new();
        node->data = delta->value;
        parent = tree_find_node(dst, delta->parent);
        prev = tree_find_node(dst, delta->prev);
        tree_insert_node(parent, prev, node);
        break;
      case 4: // swap
        l = tree_find_node(dst, delta->v1);
        r = tree_find_node(dst, delta->v2);
        tree_swap_nodes(l, r);
        break;
      case 5:
        node = tree_node_new();
        node->data = delta->value;
        tree_add_node(node, dst);
        dst = node;
        break;
    }
  }
  return dst;
}

/* tree strings; formatted view of tree used for debugging */
typedef struct {
  int w;
  list_t cells;
} text_column;

typedef struct {
  int idx;
  char *str;
} text_cell;

text_column *text_column_alloc() {
  text_column *column;

  column = (text_column*)malloc(sizeof(text_column));
  return column;
}

void text_column_free(text_column *column) {
  free(column);
}

text_column *text_column_new(int w) {
  text_column *column;

  column = text_column_alloc();
  column->w = w;
  list_init(&column->cells);
  return column;
}

text_cell *text_cell_alloc() {
  text_cell *cell;

  cell = (text_cell*)malloc(sizeof(text_cell));
  return cell;
}

void text_cell_free(text_cell *cell) {
  free(cell);
}

text_cell *text_cell_new(int idx, char *str) {
  text_cell *cell;

  cell = text_cell_alloc();
  cell->idx = idx;
  cell->str = str;
  return cell;
}

char tree_str_buf[8192*16*16];
char tree_str_buf2[8192*16*16];
int tree_strs_len = 0;

#define max(a,b) ((a)>(b)?(a):(b))
void *tree_node_str(void *data) {
  char buf[32];
  int i, len, plen;

  sprintf(buf, "%i", *(int*)data);
  len = strlen(buf);
  sprintf(tree_str_buf+tree_strs_len, "%i", *(int*)data);
  tree_strs_len += len+1;
  return tree_str_buf+tree_strs_len-(len+1);
}

void tree_str_r(tree_node_t *root, list_t *columns, text_column *pcol, int level) {
  text_cell *cell;
  text_column *column;
  tree_node_t *it;
  list_node_t *n;
  list_t l_columns;
  int i, len, count;

  count = tree_child_count(root);
  list_init(&l_columns);
  for_each_child_node_indexed(root, it, i) {
    /* odd number of children and middle child? */
    if ((count % 2) && (i == (count/2))) {
      column = pcol; /* reuse the parent node's column */
      column->w = max(column->w, strlen(it->data));
    }
    else {
      len = strlen(it->data);
      column = text_column_new(len); /* create a new column */
      if (i == 0)
        n = list_insert_before(columns, pcol, column);
      else if (i != (count/2))
        n = list_insert(columns, n->data, column);
      else
        n = list_insert(columns, pcol, column);
    }
    cell = text_cell_new(level, it->data);
    list_append(&column->cells, cell);
    list_append(&l_columns, column);
  }
  for_each_child_node(root, it) {
    column = list_pop(&l_columns, 0);
    tree_str_r(it, columns, column, level+1);
  }
}

static void tree_str_c(tree_node_t *strs, list_t *columns) {
  text_column *column;
  text_cell *cell;
  int len;

  list_init(columns);
  len = strlen(strs->data);
  column = text_column_new(len);
  cell = text_cell_new(0, strs->data);
  list_append(&column->cells, cell);
  list_append(columns, column);
  tree_str_r(strs, columns, column, 1);
}

char *tree_str(tree_node_t *root) {
  list_t columns;
  tree_node_t *strs, *n;
  text_column *column;
  text_cell *cell;
  int i, level, found;
  int tree_str_len, cpad;
  char *str;

  /* map the tree to a tree of strings */
  tree_strs_len = 0;
  strs = tree_map(root, tree_node_str);
  /* convert string valued nodes to columns */
  tree_str_c(strs, &columns);
  /* iterate columns and generate the result string */
  level = 0;
  list_for_each(&columns, column) {
    list_for_each(&column->cells, cell) {
      level = max(level, cell->idx);
    }
  }
  cpad = 0;
  str = tree_str_buf2;
  for (i=0;i<=level;i++) {
    list_for_each(&columns, column) {
      found = 0;
      list_for_each(&column->cells, cell) {
        if (cell->idx != i) { continue; }
        found = 1;
        cpad = column->w - strlen(cell->str);
        str += sprintf(str,
          "%*s%s%*s", (cpad/2)+(cpad%2), "", cell->str, cpad/2+1, "");
      }
      if (!found)
        str += sprintf(str, "%*s", column->w+1, "");
    }
    str += sprintf(str, "\n");
  }
  /* free the columns and cells */
  list_for_each(&columns, column) {
    list_for_each(&column->cells, cell) {
      free(cell);
    }
    free(column);
  }
  /* free the mapped tree */
  tree_node_free(strs, 1);
  return tree_str_buf2;
}
