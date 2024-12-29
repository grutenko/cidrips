#ifndef cidrips_h
#define cidrips_h

#include <stdio.h>

#define BC134_SIZE 17

typedef unsigned char bc134_byte_t;
typedef bc134_byte_t bc134_t;

bc134_t *bc134plus(bc134_t *v0, bc134_t *v1);

int bc134cmp(bc134_t *v0, bc134_t *v);
/**
 * v0 - v1 (if v0 < v1: undefined behaviour)
 */
bc134_t *bc134_minus(bc134_t *v0, bc134_t *v1);

bc134_t *bc134_rshift(bc134_t *v0, int c);

#define AddrIPv4 0
#define AddrIPv6 1

typedef struct {
  int verno;
  union {
    unsigned char v4[4];
    unsigned char v6[16];
  } v;
  unsigned char cidr;
} addr_t;
/**
 * Comapre two addresses as number with cidr and return: -1 - addr0 > addr1, 0
 * addr0 == addr1, 1 - addr0 < addr1
 *
 * _If addr0 and addr1 contain different types of address undefined behaviour_
 */
int addrcmp_v4(addr_t *addr0, addr_t *addr1);

int addrcmp_v6(addr_t *addr0, addr_t *addr1);

/**
 * return true if haystack fully contain needle.
 *
 * _If addr0 and addr1 contain different types of address undefined behaviour_
 */
int addr_sub_zone_of(addr_t *haystack, addr_t *needle);

void addr_set(addr_t *addr, int ver, unsigned char *v, int cidr);

enum {
  ADDR_PRINT_NORMAL = 0,
  ADDR_PRINT_COMPACT,
};

size_t addr_zone_v4_weight(int cidr);

bc134_t *addr_zone_v6_weight(bc134_t *out, int cidr);

unsigned int addr_mask_raw_v4(addr_t *addr, int cidr);

bc134_t *addr_mask_raw_v6(bc134_t *out, addr_t *addr, int cidr);

#define ADDR_COMPACT 0x1

int addr_print(FILE *o, addr_t *addr, int mode);

int addr_parse_v4(const char *p, addr_t *addr);

int addr_parse_v6(const char *p, addr_t *addr);

#define BRANCH_LEFT 0
#define BRANCH_RIGHT 1

typedef struct _addr_node {
  int branch;
  addr_t addr;
  struct _addr_node *p;
  struct _addr_node *left;
  struct _addr_node *right;
} addr_node_t;

/**
 * Find find node if exists or push
 *
 * __dont use different types of addresses in same tree. This is undefined
 * behaviour__
 *
 * Return finded node. If node already exists return this pointer. If not push
 * and return.
 */
addr_node_t *tree_node_find(addr_node_t **root, addr_node_t *node);

void tree_free(addr_node_t **root, void (*free)(void *));

enum {
  DIRECT_LEFT = 0,
  DIRECT_RIGHT = 1,
  DIRECT_PARENT = 2,
};

addr_node_t *tree_node_walk(addr_node_t *node, int *direct);

/**
 * Page allocate node
 */
addr_node_t *node_alloc();

void node_free(addr_node_t *node);

void nodes_cleanup();

typedef struct _addr_list {
  addr_t addr;
  size_t addrcount_v4;
  bc134_byte_t addrcount_v6[BC134_SIZE];
  int fully;
  struct _addr_list *prev;
  struct _addr_list *next;
} addr_list_t;

addr_list_t *addr_list_item_new();

void addr_list_push(addr_list_t **head, addr_list_t **tail, addr_list_t *item);

int addr_list_push_from_addr_tree(addr_list_t **head, addr_list_t **tail,
                                  addr_node_t *tree);
/**
 * Cut sublist to new location.
 *
 * !!__nhead, ntail must be pointers to NULL__!!
 */
void addr_list_cut(addr_list_t **ohead, addr_list_t **otail,
                   addr_list_t **nhead, addr_list_t **ntail, addr_list_t *head,
                   addr_list_t *tail);

addr_list_t *addr_list_next(addr_list_t *item);

/**
 * Copy all items from ohead - ohtail to new into nhead - ntail
 *
 * !!__nhead, ntail must be pointers to NULL__!!
 *
 * Return 1 if success and 0 if memory error.
 * Just call addr_list_free() on target list to remove broken list.
 */
int addr_list_copy(addr_list_t **ohead, addr_list_t **otail,
                   addr_list_t **nhead, addr_list_t **ntail);

/**
 * Free list and set pointers head, tail to NULL
 */
void addr_list_free(addr_list_t **head, addr_list_t **tail);

/**
 * Return count items in list
 */
size_t addr_list_size(addr_list_t *head, addr_list_t *tail);

void addr_list_remove(addr_list_t **head, addr_list_t **tail,
                      addr_list_t *item);

void compress_v4(addr_list_t **head, addr_list_t **tail, int level);

void compress_v6(addr_list_t **head, addr_list_t **tail, int level);
#endif