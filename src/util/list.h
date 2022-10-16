#ifndef _LIST_H_
#define _LIST_H_

typedef struct list_node {
  struct list_node *next;
  struct list_node *prev;
  void *data;
} list_node_t;

typedef struct {
  list_node_t *head;
  list_node_t *tail;
} list_t;


#define _list_iter(i,t,l,f,nx,n,c,p,inc) \
i; \
for (t n=(l)->f;n&&(p,!(c));(inc),n=n->nx)

/*
#define _list_node_iter(i,l,f,nx,n,c,inc) \
_list_iter(i,,l,f,nx,n,c,1,inc)

#define _list_value_iter(i,l,f,nx,d,c,inc) \
_list_iter(i,list_node_t*,l,f,nx,n,c,d=n->data,inc)

#define _list_node_forward_iter(i,l,n,c,inc) \
_list_node_iter(i,l,head,next,n,c,inc)

#define _list_node_reverse_iter(i,l,n,c,inc) \
_list_node_iter(i,l,tail,prev,n,c,inc)

#define _list_forward_iter(i,l,d,c,inc) \
_list_value_iter(i,l,head,next,d,c,inc)

#define _list_reverse_iter(i,l,d,c,inc) \
_list_value_iter(i,l,tail,prev,d,c,inc)

#define _list_node_forward_iter_indexed(l,n,i,c) \
_list_node_forward_iter(i=0,l,n,c,i++)

#define _list_node_reverse_iter_indexed(l,n,i,c) \
_list_node_reverse_iter(i=list_length(l)-1,l,n,c,i--)

#define _list_node_forward_iter_noindex(l,n,c) \
_list_node_forward_iter(,l,n,c,)

#define _list_node_reverse_iter_noindex(l,n,c) \
_list_node_reverse_iter(,l,n,c,)

#define _list_forward_iter_indexed(l,d,i,c) \
_list_forward_iter(i=0,l,d,c,i++) \

#define _list_reverse_iter_indexed(l,d,i,c) \
_list_reverse_iter(i=list_length(l)-1,l,d,c,i--) \

#define _list_forward_iter_noindex(l,d,c) \
_list_forward_iter(,l,d,c,)

#define _list_reverse_iter_noindex(l,d,c) \
_list_reverse_iter(,l,d,c,)

#define list_first_node(l,n,c) \
for (n=(l)->head;n&&(!(c));n=n->next)

#define list_first_node_indexed(l,n,i,c) \
for (i=0,n=(l)->head;n&&(!(c));i++,n=n->next)
*/

#define list_first(l,d,c) \
d=0; \
for (list_node_t *n=(l)->head;(n||(d=0))&&(d=n->data,!(c));n=n->next)

#define list_first_indexed(l,d,i,c) \
d=0; i=0; \
for (list_node_t *n=(l)->head;(n||(d=0))&&(d=n->data,!(c));i++,n=n->next)

#define list_first_node(l,n,c) \
for (n=(l)->head;n&&(!(c));n=n->next)

#define list_first_node_indexed(l,n,i,c) \
for (i=0,n=(l)->head;n&&(!(c));i++,n=n->next)

#define list_first_node_safe(l,n,x,c) \
for (n=(l)->head;n&&(x=n->next,!(c));n=x)

#define list_first_reverse(l,d,c) \
for (list_node_t *n=(l)->tail;(n||(d=0))&&(d=n->data,!(c));n=n->prev)

#define list_first_reverse_indexed(l,d,i,c) \
i=0; \
for (list_node_t *n=(l)->tail;(n||(d=0))&&(d=n->data,!(c));i++,n=n->prev)

#define list_first_node_reverse(l,n,c) \
for (n=(l)->tail;n&&(!(c));n=n->tail)

#define list_first_node_reverse_indexed(l,n,i,c) \
for (i=list_length(l)-1,n=(l)->tail;n&&(!(c));i--,n=n->tail)

#define list_for_each_node(l,n)                   list_first_node(l,n,0)
#define list_for_each(l,d)                        list_first(l,d,0)
#define list_for_each_node_reverse(l,n)           list_first_node_reverse(l,n,0)
#define list_for_each_reverse(l,d)                list_first_reverse(l,d,0)
#define list_for_each_node_indexed(l,n,i)         list_first_node_indexed(l,n,i,0)
#define list_for_each_indexed(l,d,i)              list_first_indexed(l,d,i,0)
#define list_for_each_node_reverse_indexed(l,n,i) list_first_node_reverse_indexed(l,n,i,0)
#define list_for_each_reverse_indexed(l,d,i)      list_first_reverse_indexed(l,d,i,0)
#define list_for_each_node_safe(l,n,x)            list_first_node_safe(l,n,x,0)

#define list_map(l,d,x) \
({list_t *lm=list_new(); list_for_each(l,d) { list_append(lm, (void*)(x)); }; lm;})
#define list_reduce(l,d,a,v,x) \
({a = v; list_for_each(l,d) { a = x; }; a;})


extern list_node_t *list_node_alloc();
extern void list_node_free(list_node_t *node, int free_data);

extern list_t *list_alloc();
extern void list_free(list_t *list, int flags);
extern void list_init(list_t *list);
extern list_t *list_new();

extern void list_append_node(list_t *list, list_node_t *node);
extern void list_insert_node(list_t *list, list_node_t *prev, list_node_t *node);
extern void list_remove_node(list_t *list, list_node_t *node);
extern int list_insert_node_safe(list_t *list, list_node_t *prev, list_node_t *node);
extern int list_remove_node_safe(list_t *list, list_node_t *node);
extern int list_contains_node(list_t *list, list_node_t *node);
extern int list_index_node(list_t *list, list_node_t *node);
extern list_node_t *list_at_index_node(list_t *list, int idx);

extern list_node_t *list_append(list_t *list, void *data);
extern list_node_t *list_insert(list_t *list, void *prev, void *data);
extern list_node_t *list_insert_before(list_t *list, void *next, void *data);
extern void list_remove(list_t *list, void *data);
extern int list_insert_safe(list_t *list, void *prev, void *data);
extern int list_remove_safe(list_t *list, void *data);
extern void *list_pop(list_t *list, int idx);
extern int list_contains(list_t *list, void *data);
extern int list_index(list_t *list, void *data);
extern void *list_at_index(list_t *list, int idx);
extern int list_length(list_t *list);

extern list_t *list_distinct(list_t *src);

extern int list_swap(list_t *list, void *ldata, void *rdata);
extern list_t *list_intersection(list_t *src, list_t *dst, void *(*map_eq)(void*,void*));
extern list_t *list_difference(list_t *src, list_t *dst, int (*eq)(void*,void*));

extern char *list_str(list_t *list);

#endif /* _LIST_H_ */
