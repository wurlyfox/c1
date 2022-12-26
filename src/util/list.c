#include "list.h"
#include <stdlib.h>
#include <stdio.h>

list_node_t *list_node_alloc() {
  list_node_t *node;

  node = (list_node_t*)malloc(sizeof(list_node_t));
  return node;
}

void list_node_free(list_node_t *node, int free_data) {
  if (free_data) { free(node->data); }
  free(node);
}

list_t *list_alloc() {
  list_t *list;

  list = (list_t*)malloc(sizeof(list_t));
  return list;
}

void list_free(list_t *list, int flags) {
  list_node_t *node, *next;
  if (flags & 1) {
    list_for_each_node_safe(list, node, next) { list_node_free(node, flags&2); }
  }
  free(list);
}

void list_init(list_t *list) {
  list->head = 0;
  list->tail = 0;
}

list_t *list_new() {
  list_t *list;

  list = list_alloc();
  list_init(list);
  return list;
}

void list_append_node(list_t *list, list_node_t *node) {
  if (!list->tail) {
    list->head = node;
    list->tail = node;
  }
  else {
    node->prev = list->tail;
    list->tail->next = node;
    list->tail = node;
  }
}

void list_insert_node(list_t *list, list_node_t *prev, list_node_t *node) {
  if (prev) {
    node->next = prev->next;
    if (prev->next)
      prev->next->prev = node;
    else
      list->tail = node;
    prev->next = node;
  }
  else if (list->head) {
    node->next = list->head;
    node->next->prev = node;
    list->head = node;
  }
  else {
    list->head = node;
    list->tail = node;
  }
  node->prev = prev;
}

void list_remove_node(list_t *list, list_node_t *node) {
  if (!node->prev) { list->head = node->next; }
  else { node->prev->next = node->next; }
  if (!node->next) { list->tail = node->prev; }
  else { node->next->prev = node->prev; }
}

void list_insert_node_before(list_t *list, list_node_t *next, list_node_t *node) {
  if (next) {
    node->prev = next->prev;
    if (next->prev)
      next->prev->next = node;
    else
      list->head = node;
    next->prev = node;
  }
  else if (!list->tail) {
    list->head = node;
    list->tail = node;
  }
  node->next = next;
}

int list_contains_node(list_t *list, list_node_t *node) {
  list_node_t *n;

  list_first_node(list, n, node == n);
  return !!n;
}

list_node_t *list_pop_node(list_t *list, int idx) {
  list_node_t *n;
  void *d;
  int i;

  if (idx == -1) { n = list->tail; }
  else { list_first_node_indexed(list, n, i, i==idx); }
  if (!n) { return 0; }
  list_remove_node(list, n);
  return n;
}

int list_index_node(list_t *list, list_node_t *node) {
  list_node_t *n;
  int i;

  list_first_node_indexed(list, n, i, node==n);
  if (!n) { return -1; }
  return i;
}

list_node_t *list_at_index_node(list_t *list, int idx) {
  list_node_t *n;
  int i;

  list_first_node_indexed(list, n, i, i==idx);
  if (!n) { return 0; }
  return n;
}

int list_insert_node_safe(list_t *list, list_node_t *prev, list_node_t *node) {
  if (!list_contains_node(list, prev)) { return 0; }
  if (list_contains_node(list, node)) { return 0; }
  list_insert_node(list, prev, node);
  return 1;
}

int list_remove_node_safe(list_t *list, list_node_t *node) {
  if (!list_contains_node(list, node)) { return 0; }
  list_remove_node(list, node);
  return 1;
}

list_node_t *list_append(list_t *list, void *data) {
 list_node_t *n;

  n = list_node_alloc();
  n->data = data;
  n->next = 0; n->prev = 0;
  list_append_node(list, n);
  return n;
}

list_node_t *list_insert(list_t *list, void *prev, void *data) {
  list_node_t *n, *p;

  list_first_node(list, p, p->data == prev);
  n = list_node_alloc();
  n->data = data;
  n->next = 0; n->prev = 0;
  list_insert_node(list, p, n);
  return n;
}

list_node_t *list_insert_before(list_t *list, void *next, void *data) {
  list_node_t *n, *x;

  list_first_node(list, x, x->data == next);
  n = list_node_alloc();
  n->data = data;
  n->prev = 0; n->next = 0;
  list_insert_node_before(list, x, n);
  return n;
}

void list_remove(list_t *list, void *data) {
  list_node_t *n;

  list_first_node(list, n, n->data == data);
  list_remove_node(list, n);
  list_node_free(n, 0);
}

void *list_pop(list_t *list, int idx) {
  list_node_t *n;
  void *d;
  int i;

  if (idx == -1) { n = list->tail; }
  else { list_first_node_indexed(list, n, i, i==idx); }
  if (!n) { return 0; }
  d = n->data;
  list_remove_node(list, n);
  list_node_free(n, 0);
  return d;
}

void list_clear(list_t *list, int flags) {
  list_node_t *node, *next;

  list_for_each_node_safe(list, node, next) { list_node_free(node, flags&1); }
  list_init(list);
}

int list_contains(list_t *list, void *data) {
  list_node_t *n;

  list_first_node(list, n, n->data == data);
  return !!n;
}

int list_index(list_t *list, void *data) {
  void *d;
  int i;

  list_first_indexed(list, d, i, data==d);
  if (!d) { return -1; }
  return i;
}

void *list_at_index(list_t *list, int idx) {
  void *d;
  int i;

  list_first_indexed(list, d, i, i==idx);
  if (!d) { return 0; }
  return d;
}

int list_insert_safe(list_t *list, void *prev, void *data) {
  if (!list_contains(list, prev)) { return 0; }
  if (list_contains(list, data)) { return 0; }
  list_insert(list, prev, data);
  return 1;
}

int list_remove_safe(list_t *list, void *data) {
  if (!list_contains(list, data)) { return 0; }
  list_remove(list, data);
  return 1;
}

int list_length(list_t *list) {
  int i;
  void *d;

  list_for_each_indexed(list,d,i);
  return i;
}

list_t *list_distinct(list_t *list) {
  list_t *r;
  void *d;

  r = list_new();
  list_for_each(list, d) {
    if (list_contains(r, d)) { continue; }
    list_append(r, d);
  }
  return r;
}

int list_swap(list_t *list, void *ldata, void *rdata) {
  list_node_t *l, *r;
  void *tmp;

  list_first_node(list, l, l->data == ldata);
  if (l==0) { return 0; }
  list_first_node(list, r, r->data == rdata);
  if (r==0) { return 0; }
  l->data = rdata;
  r->data = ldata;
  return 1;
}

/* map_eq if specified should be a callback that determines the data value that
   both input nodes are equal to when they compare equal */
list_t *list_intersection(list_t *src, list_t *dst, void *(*map_eq)(void*,void*)) {
  list_t *list;
  void *s, *d, *v;

  list = list_new();
  if (!map_eq) {
    list_for_each(src, s) {
      if (list_contains(dst, s))
        list_append(list, s);
    }
  }
  else {
    list_for_each(src, s) {
      list_first(dst, d, v = map_eq(s,d));
      if (v)
        list_append(list, v);
    }
  }
  return list;
}

list_t *list_difference(list_t *src, list_t *dst, int (*eq)(void*,void*)) {
  list_t *list;
  void *s, *d, *v;

  list = list_new();
  if (!eq) {
    list_for_each(src, s) {
      if (!list_contains(dst, s))
        list_append(list, s);
    }
  }
  else {
    list_for_each(src, s) {
      list_first(dst, d, eq(s,d));
      if (!d)
        list_append(list, s);
    }
  }
  return list;
}

char list_str_buf[8192];
char *list_str(list_t *list) {
  char *str;
  list_node_t *n;

  str = list_str_buf;
  str[0] = 0;
  list_for_each_node(list, n) {
    if (!n->next)
      str += sprintf(str, "%i", (int)n->data);
    else
      str += sprintf(str, "%i, ", (int)n->data);
  }
  return list_str_buf;
}
