#include "cidrips.h"
#include <assert.h>

addr_node_t *tree_node_find(addr_node_t **root, addr_node_t *node) {
  addr_node_t **pp = root;
  addr_node_t *p = (void *)0ULL;
  int cmp;
  int branch;

  while (*pp) {
    assert((*pp)->addr.verno == node->addr.verno);

    if (node->addr.verno == AddrIPv4) {
      cmp = addrcmp_v4(&((*pp)->addr), &(node->addr));
    } else {
      cmp = addrcmp_v6(&((*pp)->addr), &(node->addr));
    }
    if (cmp == 0) {
      // using find on already pushed nodes is error.
      assert(*pp == node);
      return *pp;
    } else if (cmp > 0) {
      branch = BRANCH_LEFT;
      pp = &((*pp)->right);
    } else {
      branch = BRANCH_RIGHT;
      pp = &((*pp)->left);
    }
    p = *pp;
  }

  node->p = p;
  node->branch = branch;
  *pp = node;

  return node;
}

addr_node_t *tree_node_walk(addr_node_t *node, int *direct) {
  // return next node
}