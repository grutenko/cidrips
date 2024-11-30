#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SEP_COMMA = 0, SEP_LINE = 1 };
enum { WALK_LEFT = 0, WALK_RIGHT = 1, WALK_END = 2 };

static void print_help(FILE *stream) {
  fprintf(stream,
          "cidrips. Utlity for group list of ips by the CIDR subnets.\n\n");
  fprintf(stream,
          "Usage: cidrips.exe --comma-separated < ips.txt > subnodes.txt\n\n");
  fprintf(stream, "Arguments:\n");
  fprintf(
      stream,
      "-c,--comma-separated        Use comma separated input and output.\n");
  fprintf(stream,
          "-l,--line-separated         Use line separated input and output.\n");
  fprintf(stream, "--cidr=<n>                    Use CIDR 0.0.0.0/<n>\n");
  fprintf(stream, "--help\n");
}

static struct node {
  unsigned long addr;
  int walk;
  struct node *p, *left, *right;
} *subnet_head = NULL;

static int find_subnet(int subnet) {
  struct node **p = &subnet_head, *pp = *p;
  while (*p) {
    pp = *p;
    if (subnet > (*p)->addr)
      p = &((*p)->right);
    else if (subnet < (*p)->addr)
      p = &((*p)->left);
    else
      return 1;
  }
  // create new node
  struct node *o = malloc(sizeof(struct node));
  if (o) {
    o->addr = subnet;
    o->walk = WALK_LEFT;
    o->left = o->right = NULL;
    o->p = pp;
    *p = o;
  }
  return !!o;
}

static unsigned int __cidrmask[] = {
    0x00000000, 0x80000000, 0xC0000000, 0xE0000000, 0xF0000000, 0xF8000000,
    0xFC000000, 0xFE000000, 0xFF000000, 0xFF800000, 0xFFC00000, 0xFFE00000,
    0xFFF00000, 0xFFF80000, 0xFFFC0000, 0xFFFE0000, 0xFFFF0000, 0xFFFF8000,
    0xFFFFC000, 0xFFFFE000, 0xFFFFF000, 0xFFFFF800, 0xFFFFFC00, 0xFFFFFE00,
    0xFFFFFF00, 0xFFFFFF80, 0xFFFFFFC0, 0xFFFFFFE0, 0xFFFFFFF0, 0xFFFFFFF8,
    0xFFFFFFFC, 0xFFFFFFFE, 0xFFFFFFFF,
};

int main(int argc, char **argv) {
  int __sep = SEP_LINE;
  int __cidr = 24;
  int __help = 0;
  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--comma-separated") == 0 || strcmp(argv[i], "-c") == 0)
      __sep = SEP_COMMA;
    else if (strcmp(argv[i], "--line-separated") == 0 ||
             strcmp(argv[i], "-l") == 0)
      __sep = SEP_LINE;
    else if (strstr(argv[i], "--cidr=") == argv[i]) {
      if (sscanf_s(argv[i], "--cidr=%u", &__cidr) != 1 || __cidr > 32) {
        fprintf(stderr, "Invalid argument: %s.\n", argv[i]);
        return EXIT_FAILURE;
      }
    } else if (strcmp(argv[i], "--help") == 0)
      __help = 1;
    else {
      fprintf(stderr, "Unknown argument: %s.\n", argv[i]);
      print_help(stderr);
      return EXIT_FAILURE;
    }
  }

  if (__help) {
    print_help(stdout);
    return EXIT_SUCCESS;
  }

  char addr[128];
  char *paddr = addr;
  int rc;
  unsigned int _a, _b, _c, _d;
  char c;
  int __row = 1, __col = 1;
  while (!feof(stdin)) {
    c = getc(stdin);
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '.':
      if (paddr - addr >= 15) {
        goto __input_error;
      }
      *(paddr++) = c;
      __col++;
      break;
    case ',':
    case '\r':
    case '\n':
    case ' ':
    case '\t':
      if (c == '\n') {
        __row++;
        __col = 1;
      } else {
        __col++;
      }
      if (paddr - addr > 0) {
        *paddr = '\0';
        rc = sscanf_s(addr, "%u.%u.%u.%u", &_a, &_b, &_c, &_d);
        if (rc != 4 || rc == EOF || _a > 255 || _b > 255 || _c > 255 ||
            _d > 255) {
          goto __invalid_ip_error;
        }
        if (!find_subnet(((_a << 24) | (_b << 16) | (_c << 8) | _d) &
                         __cidrmask[__cidr])) {
          goto __memory_error;
        }
      }
      paddr = addr;
      break;
    default:
      if (feof(stdin)) {
        break;
      } else {
        goto __input_error;
      }
      break;
    }
  }

  char __sepchars[3];
  switch (__sep) {
  case SEP_COMMA:
    __sepchars[0] = ',';
    __sepchars[1] = ' ';
    __sepchars[2] = 0;
    break;
  case SEP_LINE:
    __sepchars[0] = '\n';
    __sepchars[1] = 0;
    break;
  }

  struct node *p = subnet_head, *pp;
  while (p) {
    switch (p->walk) {
    case WALK_LEFT:
      p->walk = WALK_RIGHT;
      if (p->left)
        p = p->left;
      break;
    case WALK_RIGHT:
      p->walk = WALK_END;
      if (p->right)
        p = p->right;
      break;
    case WALK_END:
      printf("%lu.%lu.%lu.%lu/%d", (p->addr >> 24), (p->addr >> 16 & 0xff),
             (p->addr >> 8 & 0xff), (p->addr & 0xff), __cidr);
      pp = p;
      p = p->p;
      free(pp);
      if (p) {
        fprintf(stdout, "%s", __sepchars);
      }
      break;
    }
  }

  return EXIT_SUCCESS;
__input_error:
  fprintf(stderr, "Invalid character at %d:%d\n", __row, __col);
  return EXIT_FAILURE;
__invalid_ip_error:
  fprintf(stderr, "Invalid ip addr: %s\n", addr);
  return EXIT_FAILURE;
__memory_error:
  fprintf(stderr, "Cannot allocate memory.\n");
  return EXIT_FAILURE;
}