#include "cidrips.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static struct node_mempage {
  unsigned char busy[8];
  addr_node_t p[64];
  struct node_mempage *next;
} *_memhead = (void *)0;
static struct node_mempage *_memtail = (void *)0;

static unsigned char _busy[8] = {0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff};

static inline int page_busy(struct node_mempage *page) {
  return ~page->busy[0] == 0 && ~page->busy[1] == 0 && ~page->busy[2] == 0 &&
         ~page->busy[3] == 0 && ~page->busy[4] == 0 && ~page->busy[5] == 0 &&
         ~page->busy[6] == 0 && ~page->busy[7] == 0;
}

addr_node_t *node_alloc() {
  // allocate node in memhead
  struct node_mempage **pp = &_memhead;
  // find free node
  while (*pp && page_busy(*pp))
    pp = &((*pp)->next);

  if (!*pp) {
    // cannot find free page. Allocate new
    *pp = malloc(sizeof(struct node_mempage));
    if (!*pp) {
      // cannot allocate memory
      return (void *)0;
    }
    memset((*pp)->busy, 0, 8);
    (*pp)->next = 0ULL;
  }

  int i = 0;
  unsigned char *busy = (*pp)->busy;
  unsigned char b;
  while (*busy == 0xff) {
    busy++;
    i += 8;
  }
  b = *busy;
  while (b & 1)
    b >>= 1, i++;
  *busy |= 1 << (i % 8);

  return &((*pp)->p[i]);
}

void node_free(addr_node_t *node) {
  if (!node) {
    return;
  }

  // free node from memhead
  struct node_mempage *p = _memhead;
  // find free node
  while (p && (void *)(p->p) < (void *)node ||
         ((void *)(p->p) + sizeof(addr_node_t) * 64) >= (void *)node)
    p = p->next;

  assert(p != 0);

  int i;
  if (p) {
    i = (void *)node - (void *)(p->p);
    p->busy[i >> 3] &= ~(1 << (i & 0x7));
  }
  // if p == NULL then node is not part of this page allocation
  // nothing to do
}

void nodes_cleanup() {
  // free all pages
  struct node_mempage *p = _memhead, *t;
  while (p) {
    t = p;
    free(p);
    p = t->next;
  }
}