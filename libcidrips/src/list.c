#include "cidrips.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

addr_list_t *addr_list_item_new() {
  addr_list_t *p;
  p = malloc(sizeof(addr_list_t));
  if (p) {
    p->addrcount_v4 = 0;
    memset(p->addrcount_v6, 0, 17);
    p->next = (void *)0;
    p->prev = (void *)0;
  }
  return p;
}

void addr_list_push(addr_list_t **head, addr_list_t **tail, addr_list_t *item) {
  // push addr
  item->prev = *tail;
  item->next = (void *)0;
  if (*tail) {
    (*tail)->next = item;
  } else {
    *head = item;
  }
  *tail = item;
}

int addr_list_push_from_addr_tree(addr_list_t **head, addr_list_t **tail,
                                  addr_node_t *tree) {
  assert(head != (void *)0ULL && tail != (void *)0ULL);
  int direct = DIRECT_LEFT;
  addr_node_t *p = tree_node_walk(tree, &direct);
  addr_list_t *item;
  bc134_byte_t weight[17];
  while (p) {
    item = addr_list_item_new();
    if (!item) {
      return 0;
    }
    memmove(&(item->addr), &(p->addr), sizeof(addr_t));
    if (item->addr.verno == AddrIPv4) {
      item->addrcount_v4 = addr_zone_v4_weight(item->addr.cidr);
    } else if (item->addr.verno == AddrIPv6) {
      memmove(item->addrcount_v6, addr_zone_v6_weight(weight, item->addr.cidr),
              17);
    }
    item->fully = 1;
    addr_list_push(head, tail, item);
    p = tree_node_walk(p, &direct);
  }
  return 1;
}

void addr_list_cut(addr_list_t **ohead, addr_list_t **otail,
                   addr_list_t **nhead, addr_list_t **ntail, addr_list_t *head,
                   addr_list_t *tail) {

  assert(*nhead == (void *)0 && *ntail == (void *)0);
  assert((head == (void *)0 && tail == (void *)0) ||
         (head != (void *)0 && tail != (void *)0));

  // [ohead] [] [head] [] [tail] [] [] [] [otail]----+
  //              ^__________^ --+                   |
  // [nhead] [] [ntail] <--------+                   |
  // [otail] [] [] [otail]<--------------------------+

  if (head->prev) {
    head->prev->next = tail->next;
  } else {
    *ohead = tail->next;
  }

  if (tail->next) {
    tail->next->prev = head->prev;
  } else {
    *otail = head->prev;
  }

  head->prev = (void *)0;
  tail->next = (void *)0;
  *nhead = head;
  *ntail = tail;
}

inline addr_list_t *addr_list_next(addr_list_t *item) { return item->next; }

int addr_list_copy(addr_list_t **ohead, addr_list_t **otail,
                   addr_list_t **nhead, addr_list_t **ntail) {
  assert(*nhead == (void *)0 && *ntail == (void *)0);

  addr_list_t *p = *ohead;
  addr_list_t *item;
  while (p) {
    item = addr_list_item_new();
    if (!item) {
      return 0;
    }
    memmove(&(item->addr), &(p->addr), sizeof(addr_t));
    if (item->addr.verno == AddrIPv4) {
      item->addrcount_v4 = p->addrcount_v4;
    } else if (item->addr.verno == AddrIPv6) {
      memmove(item->addrcount_v6, p->addrcount_v6, 17);
    }
    addr_list_push(nhead, ntail, item);
    p = addr_list_next(p);
  }

  return 1;
}

void addr_list_free(addr_list_t **head, addr_list_t **tail) {
  addr_list_t *p = *head;
  addr_list_t *t;
  while (p) {
    t = p->next;
    free(p);
    p = t;
  }
  *head = (void *)0;
  *tail = (void *)0;
}

size_t addr_list_size(addr_list_t *head, addr_list_t *tail) {
  size_t size = 0;
  addr_list_t *p = head;
  while (p) {
    size++;
    p = addr_list_next(p);
  }
  return size;
}

void addr_list_remove(addr_list_t **head, addr_list_t **tail, addr_list_t *item) {
  addr_list_t *h = (void *)0ULL, *t = (void *)0ULL;
  addr_list_cut(head, tail, &h, &t, item, item);
}