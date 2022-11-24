#ifndef _TREE_H_
#define _TREE_H_

/**
 * generic tree [node] functionality
 *
 * tree nodes exist in a doubly linked list of all child nodes of their parent
 * like a list node they include next and prev pointers; all of the basic list
 * functionality (iteration, etc.) applies to a 'list' of child nodes
 * the main difference between tree nodes and list nodes is that a tree nodes can
 * have one or more child nodes, whereas list nodes cannot
 * in addition to next and prev pointers, a tree node has a pointer to the first
 * of its children; it also has a pointer to the last of its children and a pointer
 * to the parent node, both of which are stored to avoid otherwise the need to
 * retraverse the tree and recompute pointers to these nodes each time they are needed
 *
 * traversal
 *
 * there are several macros which serve as the 'tree' counterparts to the
 * list_for_each variations which are available for lists;
 * these substitute the appropriate for-loop header so that the logic for each 'visit'
 * can be specified inline in the loop body
 * the available macros are
 *
 * order | conditional | node or data | name
 * pre   | y           | node         | tree_first_node_preorder
 * post  | y           | node         | tree_first_node_postorder
 * level | y           | node         | tree_first_node_levelorder
 * pre   | y           | data         | tree_first_preorder
 * post  | y           | data         | tree_first_postorder
 * level | y           | data         | tree_first_levelorder
 * pre   | n           | node         | tree_preorder_nodes
 * post  | n           | node         | tree_postorder_nodes
 * level | n           | node         | tree_levelorder_nodes
 * pre   | n           | data         | tree_preorder
 * post  | n           | data         | tree_postorder
 * level | n           | data         | tree_levelorder
 *
 * the conditional variants enable the traversal to stop when a condition is met
 * the data variants enable the node data value to be forwarded directly to the
 *  loop body instead of just the node
 * note that there are no 'indexed' variants of the macros that increment an
 * index with each 'traversal' iteration, as nodes are identified by path instead
 * of index; the path function can be used if the node path is needed
 *
 * there are also macros for iteration of 'just' the list of child nodes for a
 * given node; these are functionally equivalent to the macros for list iteration
 *
 * tree construction
 *
 * (node constructors)
 * tree_node_allocate     - allocate a tree node
 * tree_node_free         - free a tree node
 * tree_node_new          - create a new tree node
 * (node variants)
 * tree_add_child_node    - append a node to the list of child nodes for a given parent node
 * tree_insert_child_node - insert a node after a given child node in the list of child nodes
 *                          for a given parent node
 * tree_remove_node       - remove a node from its tree (i.e. from the list of child nodes for
 *                          the parent it belongs to)
 * tree_swap_nodes        - swap the locations of a pair of nodes
 * (data variants)
 * tree_add               - append a new node with specific data to the list of child nodes
 *                          for a given parent
 * tree_insert_sibling    - insert a new node with specific data after an existing node with
 *                          specific data in the list of child nodes for a given parent node
 * tree_remove            - remove the node with specific data from a tree, if such node exists
 * tree_remove_inherit    - remove the node with specific data from a tree, if such node exists,
 *                          causing the parent of that node to inherit any child nodes of
 *                          the removed node
 *
 * tree find/query
 *
 * (node variants)
 * tree_has_node          - return 1 if a tree contains the specified node
 * tree_child_node_index  - return index of a node amongst its siblings (as child of its parent)
 * tree_child_node_at_index - return the node at a given index in the list of child nodes for a
 *                          given parent node
 * (data variants)
 * tree_contains          - return 1 if a tree contains a node with the specified data
 * tree_child_index       - return index of the node with specific data in the list of child nodes
 *                          for a given parent, if any
 * tree_child_at_index    - return the specific data for the node at given index in the list of
 *                          child nodes for a given parent
 * (node statistics)
 * tree_height            - return the height of a node
 * tree_child_count       - return the number of child nodes for a given parent node
 *
 * map/traverse specializations
 *
 * tree_copy              - create a copy of a source tree with the same structure and data
 *                          pointers, but different node objects
 * tree_flatten           - create a -list- of the datas of the nodes of a tree, in pre-order
 *                          (i.e. resulting from pre-order traversal)
 * tree_flatten_nodes     - create a -list- of the nodes of a tree, in pre-order;
 *                          note that the list node links are separate pointers from the -tree- node
 *                          links that exist in the node objects (which maintain the form of the tree)
 * list_unflatten_nodes   - reconstruct a tree from a flattened list of tree nodes created with
 *                          tree_flatten_nodes
 *
 * map
 *
 * tree_map               - map the datas of all nodes in a tree to a new tree,
 *                          via callback function (in pre-order)
 * tree_map_postorder     - map the datas of all nodes in a tree to a new tree,
 *                          via callback function. this occurs in post-order, enabling the callback
 *                          function to receive the list of mapped children for all non-leaf nodes
 * tree_map_to            - recursively map the [datas of all] nodes of a tree to a new object,
 *                          via callback function. this occurs in post-order; the callback
 *                          function should determine how to combine the mapped/combined children
 *                          and unmapped node [for any given unmapped node] into a new object
 * tree_map_from          - map a non-tree or tree-like object to a new tree, given callbacks
 *                          to extract the list of children for a given object and to map each
 *                          extracted object to the desired data objects
 *
 * tree operators
 *
 * tree_changes           - get a list of the differences between 2 trees
 *                          currently nodes are considered equal if the datas as integer value
 *                          are equal
 *                          differences will consist of
 *                           op=1:add, op=2:remove, op=3:move, op=4:swap, op=5:new root
 *                          the list of differences along with the first tree can be used to
 *                          construct the second tree
 */
#include <stdlib.h>
#include <stdio.h>
#include "list.h"

typedef struct tree_node {
  union {
    struct tree_node *child;
    struct tree_node *head_child;
  };
  struct tree_node *tail_child;
  struct tree_node *next;
  struct tree_node *prev;
  struct tree_node *parent;
  void *data;
} tree_node_t;

/**
  use this macro to define the first 6 fields of a specialized node type
  that can be casted to tree_node_t
  note that data of said type must be initialized with self-reference in 'self'
*/
#define TREE_NODE(t) \
t *child, *tail_child, *next, *prev, *parent, *self;

typedef struct tree_delta {
  int op; /* 1 = add, 2 = remove, 3 = swap */
  union {
    void *value;
    void *v1;
  };
  union {
    void *parent;
    void *v2;
  };
  void *prev;
} tree_delta_t;

/* aliases */
#define tnna(n,r) tree_node_next_ancestor((tree_node_t*)(n), (tree_node_t*)(r))
#define tnnd(n) tree_node_next_descendant((tree_node_t*)(n))
#define tnnc(n,r) tree_node_next_cousin((tree_node_t*)(n), (tree_node_t*)(r))
#define tnpa(n,r) tree_node_prev_ancestor((tree_node_t*)(n), (tree_node_t*)(r))
#define tnld(n) tree_node_last_descendant((tree_node_t*)(n))
#define tnprcn tree_node_preorder_cond_next
#define tnpocn tree_node_postorder_cond_next
#define tnlocn tree_node_levelorder_cond_next
#define tnprcp tree_node_preorder_cond_prev
#define tnpocp tree_node_postorder_cond_prev
#define tnprn tree_node_preorder_next
#define tnpon tree_node_postorder_next
#define tnlon tree_node_levelorder_next
#define tnprp tree_node_preorder_prev
#define tnpop tree_node_postorder_prev

/* iterators for preorder and postorder traversal */
#define tree_node_preorder_next(n,r) \
tree_node_preorder_cond_next(n,1,r)

#define tree_node_postorder_next(n,r) \
tree_node_postorder_cond_next(n,1,r)

#define tree_node_preorder_prev(n,r) \
tree_node_preorder_cond_prev(n,1,r)

#define tree_node_postorder_prev(n,r) \
tree_node_postorder_cond_prev(n,1,r)

#define tree_node_levelorder_next(n,r) \
tree_node_levelorder_cond_next(n,1,r)

#define tree_node_preorder_cond_next(n,c,r) \
(n->child&&(c)?n->child:n->next?n->next:tnna(n,r))

#define tree_node_postorder_cond_next(n,c,r) \
(n->next&&(c)?tnnd(n->next):n->parent)

#define tree_node_preorder_cond_prev(n,c,r) \
(n->prev&&(c)?tnld(n->prev):n->parent)

#define tree_node_postorder_cond_prev(n,c,r) \
(n->tail_child&&(c)?n->tail_child:n->prev?n->prev:tnpa(n,r))

#define tree_node_levelorder_cond_next(n,c,r) \
(n->next?n->next:tnnc(n,r)?tnnc(n,r) \
 :n->parent?n->parent->child->child:n->child)

/* iterate; stop at first value/node that meets cond */
#define first_child(p,d,c) \
d=0; \
for (tree_node_t *n=(tree_node_t*)(p->child);(n||(d=0))&&(d=n->data,!(c));n=n->next)

#define first_child_indexed(p,d,i,c) \
d=0; i=0; \
for (tree_node_t *n=(tree_node_t*)(p->child);(n||(d=0))&&(d=n->data,!(c));n=n->next,i++)

#define first_child_node(p,n,c) \
for (n=(p->child);n&&(!(c));n=n->next)

#define first_child_node_indexed(p,n,i,c) \
for (n=(p->child),i=0;n&&(!(c));n=n->next,i++)

/* traverse; stop at first value/node that meets cond */
#define tree_first_preorder(r,d,c) \
d=0; \
for (tree_node_t *n=(tree_node_t*)(r);(n||(d=0))&&(d=n->data,!(c));n=tnprn(n,r))

#define tree_first_node_preorder(r,n,c) \
for (n=(r);n&&(!(c));n=tnprn(n,r))

#define tree_first_postorder(r,d,c) \
d=0; \
for (tree_node_t *n=(tree_node_t*)tnnd(r);(n||(d=0))&&(d=n->data,!(c));n=tnpon(n,r))

#define tree_first_node_postorder(r,n,c) \
for (n=tnnd(r);n&&(!(c));n=tnpon(n,r))

#define tree_first_levelorder(r,d,c) \
d=0; \
for (tree_node_t *n=(tree_node_t*)(r);(n||(d=0))&&(d=n->data,!(c));n=tnlon(n,r))

#define tree_first_node_levelorder(r,n,c) \
for (n=(r);n&&(!(c));n=tnlon(n,r))

/* non-conditional/complete iteration */
#define for_each_child(p,d)                first_child(p,d,0)
#define for_each_child_node(p,n)           first_child_node(p,n,0)
#define for_each_child_indexed(p,d,i)      first_child_indexed(p,d,i,0)
#define for_each_child_node_indexed(p,n,i) first_child_node_indexed(p,n,i,0)

/* non-conditional/complete traversal */
#define tree_preorder(r,d)                 tree_first_preorder(r,d,0)
#define tree_preorder_nodes(r,n)           tree_first_node_preorder(r,n,0)
#define tree_postorder(r,d)                tree_first_postorder(r,d,0)
#define tree_postorder_nodes(r,n)          tree_first_node_postorder(r,n,0)
#define tree_levelorder(r,d)               tree_first_levelorder(r,d,0)
#define tree_levelorder_nodes(r,n)         tree_first_node_levelorder(r,n,0)
#define tree_first_node                    tree_first_node_preorder

/* tree iterator impl */
extern tree_node_t *tree_node_next_ancestor(tree_node_t *node, tree_node_t *root);
extern tree_node_t *tree_node_next_descendant(tree_node_t *node);
extern tree_node_t *tree_node_prev_ancestor(tree_node_t *node, tree_node_t *root);
extern tree_node_t *tree_node_last_descendant(tree_node_t *node);
extern tree_node_t *tree_node_next_cousin(tree_node_t *node, tree_node_t *root);

/* tree construction */
extern tree_node_t *tree_node_alloc();
extern void tree_node_free(tree_node_t *node, int free_desc);
extern tree_node_t *tree_node_new();
extern void tree_add_node(tree_node_t *parent, tree_node_t *node);
extern void tree_insert_node(tree_node_t *parent, tree_node_t *prev, tree_node_t *node);
extern void tree_remove_node(tree_node_t *node);
extern tree_node_t *tree_remove_node_inherit(tree_node_t *node);
extern int tree_add(tree_node_t *root, void *parent, void *data);
extern void tree_insert(tree_node_t *root, void *prev, void *data);
extern int tree_remove(tree_node_t *root, void *data);
extern tree_node_t *tree_remove_inherit(tree_node_t *root, void *data);
extern tree_node_t *tree_swap_nodes(tree_node_t *l, tree_node_t *r);

/* tree find/query */
extern int tree_has_node(tree_node_t *root, tree_node_t *node);
extern int tree_child_node_index(tree_node_t *parent, tree_node_t *node);
extern tree_node_t *tree_child_node_at_index(tree_node_t *parent, int idx);
extern tree_node_t *tree_find_node(tree_node_t *root, void *data);
extern int tree_contains(tree_node_t *root, void *data);
extern int tree_height(tree_node_t *root);
extern int tree_child_index(tree_node_t *parent, void *data);
extern void *tree_child_at_index(tree_node_t *parent, int idx);
extern int tree_child_count(tree_node_t *parent);

/* tree map/traverse specializations */
extern tree_node_t *tree_copy(tree_node_t *root);
extern list_t *tree_flatten(tree_node_t *root);
extern list_t *tree_flatten_nodes(tree_node_t *root);
extern tree_node_t *list_unflatten_nodes(list_t *list);

/* tree map */
extern tree_node_t *tree_map(tree_node_t *root, void *(*map)(void*));
extern void *tree_map_to(tree_node_t *root, void *(*map)(void*, void**));
extern tree_node_t *tree_map_from(void *data, void **child_fn(void*), void *(*map)(void*));

/* tree binary ops */
extern list_t *tree_changes(tree_node_t *src, tree_node_t *dst);
extern tree_node_t *tree_apply(tree_node_t *src, list_t *deltas);

/* tree misc */
extern char *tree_str(tree_node_t *root);
extern void tree_print_tmp(tree_node_t *node);

#endif /* _TREE_H_ */
