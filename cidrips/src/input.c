#include "input.h"
#include "cidrips.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static inline int charmask(int c, const char *e) {
  while (*e != '\0') {
    if (c == *e)
      return 1;
    e++;
  }
  return 0;
}

int read_addr_list(FILE *o, int ver, addr_list_t **head, addr_list_t **tail,
                   int flags) {
  const char *used_charmask;
  if (ver == AddrIPv4) {
    used_charmask = "0123456789./";
  } else if (ver == AddrIPv6) {
    used_charmask = "0123456789abcdefABCDEF:/";
  }

  int ch, rc;
  int line = 1, column = 0, b_line = 0, b_column = 0;
  char b[64], *bp = (void *)b;
  addr_node_t *node = node_alloc();
  if (!node) {
    fprintf(stderr, "Cannot allocate memory.\n");
    goto failure;
  }
  addr_node_t *node_root = (void *)0;
  while (!feof(o)) {
    ch = getc(o);
    if (ch == EOF) {
      if(ferror(o)) {
        fprintf(stderr, "I/o error: %s\n", strerror(errno));
        goto failure;
      }
    }
    if (charmask(ch, used_charmask)) {
      // push to address buffer
      if (bp - b >= 64) {
        fprintf(stderr, "Address buffer overflow 64 characters at %d:%d\n",
                line, column);
        goto failure;
      }
      *bp = ch;
      bp++;
      column++;
      if (bp - b == 1) {
        // update column and line counter into begin of address
        b_line = line;
        b_column = column;
      }
    } else if (charmask(ch, "\n\r\t ,") || ch == EOF) {
      // skip delimiters and start parsing
      if (bp > b) {
        *bp = 0;
        if (ver == AddrIPv4) {
          rc = addr_parse_v4(b, &(node->addr));
        } else if (ver == AddrIPv6) {
          rc = addr_parse_v6(b, &(node->addr));
        }
        if (rc) {
          // find equal node (with target address) or setup new node to tree.
          // If node with target address already exist return this. We didnt
          // allocate new node, but rewrite this on next step
          if (tree_node_find(&node_root, node) == node) {
            node = node_alloc();
            if (!node) {
              fprintf(stderr, "Cannot allocate memory.\n");
              goto failure;
            }
          }
        } else {
          fprintf(stderr, "Invalid address %s at %d:%d\n", b, b_line, b_column);
          goto failure;
        }
        bp = b;

        if(ch == EOF) {
          break;
        }
      }
      if (ch == '\r') {
      } else if (ch == '\n') {
        column = 0;
        line++;
      } else {
        column++;
      }
    } else {
      fprintf(stderr, "Invalid character: '%c' at %d:%d\n", ch, line, column);
      goto failure;
    }
  }

  // move all data from tree into list
  if (!addr_list_push_from_addr_tree(head, tail, node_root)) {
    fprintf(stderr, "Cannot allocate memory.\n");
    goto failure;
  }
  // all address now in list. Remove all tree nodes.
  nodes_cleanup();
  return 1;
failure:
  nodes_cleanup();
  return 0;
}