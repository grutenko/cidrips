#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

enum { _ADDR_IPV4 = 0, _ADDR_IPV6 = 1 };

struct _addr {
  int ver;
  union {
    u8 v4[4];
    u16 v6[8];
  } v;
  int cidr;
};

enum {
  TOK_BEGIN = 0b0000001,
  TOK_DELIM = 0b0000010,
  TOK_COLON = 0b0000100,
  TOK_UINT = 0b0001000,
  TOK_DOT = 0b0010000,
  TOK_UNKNOWN = 0b0100000,
  TOK_EOF = 0b1000000,
};

static int _toklist[] = {TOK_BEGIN, TOK_COLON,   TOK_DELIM, TOK_DOT,
                         TOK_UINT,  TOK_UNKNOWN, TOK_EOF};
static int _toklist_size = 7;

static struct {
  int tok;
  char *name;
} _lex_tokname[] = {{.tok = TOK_BEGIN, .name = "<begin-of-file>"},
                    {.tok = TOK_COLON, .name = "<colon>"},
                    {.tok = TOK_DELIM, .name = "<delimiter>"},
                    {.tok = TOK_DOT, .name = "<dot>"},
                    {.tok = TOK_UINT, .name = "<number>"},
                    {.tok = TOK_UNKNOWN, .name = "<unknown sequence>"},
                    {.tok = TOK_EOF, .name = "<end-of-file>"}};
int _lex_tokname_size = 7;

static const char *tokname(int tok) {
  int i;
  for (i = 0; i < _lex_tokname_size; i++) {
    if (_lex_tokname[i].tok == tok) {
      return _lex_tokname[i].name;
    }
  }
  return NULL;
}

enum { LEX_OK = 0, LEX_EOVERFLOW };

enum { LEX_INIT = 0, LEX_CONTINUE = 1, LEX_EOF = 2 };

#define LEX_MAX_BUF 32

static int _lexerr = 0;
static int _lextok = TOK_BEGIN;
static int _lexstate = LEX_INIT;
static int _lexrow = 1;
static int _lexcol = 1;
static char _lexval[LEX_MAX_BUF];

static void lex(FILE *input) {
  char *p = _lexval;
  int _nexttok;
  int _singlechar = 0;
  int c;

  while (1) {
    c = getc(input);
    switch (c) {
    case EOF:
      _nexttok = TOK_EOF;
      _singlechar = 1;
      break;
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
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
      _nexttok = TOK_UINT;
      break;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
      _nexttok = TOK_UINT;
      c = c + ('a' - 'A'); // lowercase
      break;
    case ':':
      _nexttok = TOK_COLON;
      _singlechar = 1;
      break;
    case '.':
      _nexttok = TOK_DOT;
      _singlechar = 1;
      break;
    case ',':
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      _nexttok = TOK_DELIM;
      break;
    default:
      _nexttok = TOK_UNKNOWN;
      _singlechar = 1;
      break;
    }

    if (_lexstate == LEX_INIT) {
      if (_singlechar) {
        p[0] = c;
        p[1] = 0;
        _lextok = _nexttok;
        break;
      } else {
        // Read first character of token and change state to continue reading
        *(p++) = c;
        _lextok = _nexttok;
        _lexstate = LEX_CONTINUE;
      }
    } else if (_lexstate == LEX_CONTINUE) {
      if (_nexttok == _lextok) {
        *(p++) = c;
        if (p - _lexval >= LEX_MAX_BUF - 1)
          goto __overflow;
      } else {
        // readed other token character.
        // End of this token, pushback to stream readed character, and change
        // state to read new token.
        *p = 0;
        _lexstate = LEX_INIT;
        ungetc(c, input);
        break;
      }
    }
  }
  return;
__overflow:
  _lexerr = LEX_EOVERFLOW;
}

enum {
  PARSE_INIT = 0,
  PARSE_DOT = 1,
  PARSE_COLON = 2,
  PARSE_BLOCK_DELIM = 3,
  PARSE_SECOND_COLON = 4, // second colon for zero-grouping, only for beginning
  PARSE_BLOCK = 5,        // parse unsuggested block
  PARSE_IPv4_BLOCK = 6,   // any IPv4 or IPv6 block
  PARSE_IPv6_BLOCK_DOUBLE_COLON = 7, // IPv6 block or second colon
  PARSE_IPv6_BLOCK = 8,              // IPv^ block vithout second colon
  PARSE_IPv6_BLOCK_END = 9,
  PARSE_COLON_END = 10,
  PARSE_END = 11
};

static int _expected_tokens[] = {TOK_DELIM | TOK_COLON | TOK_UINT | TOK_EOF,
                                 TOK_DOT,
                                 TOK_COLON,
                                 TOK_COLON | TOK_DOT,
                                 TOK_COLON,
                                 TOK_UINT,
                                 TOK_UINT,
                                 TOK_UINT | TOK_COLON,
                                 TOK_UINT,
                                 TOK_UINT | TOK_DELIM | TOK_EOF,
                                 TOK_COLON | TOK_EOF | TOK_DELIM,
                                 TOK_DELIM | TOK_EOF};

static char _parseerr[256];
int _parse_row = 1;
int _parse_col = 1;

static char _hexchars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
static int _hexchars_size = 16;

static int _ishexstr(char *str) {
  int i, j;
  for (i = 0; i < _hexchars_size; i++) {
    while (str[j] != 0 && str[j] != _hexchars[i])
      j++;
    if (str[j] != 0) {
      return 1;
    }
  }
  return 0;
}

static void _make_expected_error(int expected, int actual) {
  char *p = _parseerr;
  int rc, i;
  rc = sprintf(p, "Unexpected input. expected: ");
  p += rc;
  for (i = 0; i < _toklist_size; i++) {
    if (expected & _toklist[i]) {
      strcpy(p, tokname(_toklist[i]));
      p += strlen(tokname(_toklist[i]));
      strcpy(p, " or ");
      p += 4;
    }
  }
  sprintf(p, " Actual: %s, row: %d, col: %d", tokname(_lextok), _lexrow,
          _lexcol);
}

/**
 * Suggest version of address by this block.
 * return:
 * 0 - block is invalid
 * 1 - block ip v4
 * 2 - block ip v6
 * You cannot strongly suggest type of address by their block.
 * Therefore, we assume that all valid blocks are ipv4 blocks
 */
static int _suggest_addr_ver(char *lexval) {
  int _hex = 0;
  char *p = lexval;
  while (*p != 0 && p - lexval <= 4) {
    switch (*p) {
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
      _hex = 1;
      break;
    }
    p++;
  }
  if (p - lexval > 4) {
    return 0;
  }
  if (!_hex) {
    if (strtol(lexval, NULL, 10) <= 255) {
      return 1;
    } else {
      return 2;
    }
  }
  return 2;
}

enum { ADDRVER_UNKNOWN = 0, ADDRVER_IPV4, ADDRVER_IPV6 };

static int parse(FILE *input, struct _addr *addr) {
  int state = PARSE_INIT;
  int blocknum = 0;
  int addrver = ADDRVER_UNKNOWN;
  int ipv6_double_colon = 0;
  int ipv6_blocknum_a = 0;
  u8 v4[4];
  memset(v4, 0, sizeof(u8) * 4);
  u16 v6b[8];
  memset(v6b, 0, sizeof(u16) * 8);
  u16 v6[8];
  memset(v6, 0, sizeof(u16) * 8);
  int rc;

__parse_next_token_label:

  lex(input);
  if (_lexerr) {
    strcpy(_parseerr, "Buffer overflow error.");
    return 0;
  }
  // Read next token and check its expected for current state
  if (!(_expected_tokens[state] & _lextok)) {
    _make_expected_error(_expected_tokens[state], _lextok);
    return 0;
  }

// goto to this label if reparse current token with other parse state is needed
__parse_current_token_label:

  switch (state) {
  case PARSE_INIT:
    switch (_lextok) {
    case TOK_DELIM:
      //  54.23.123.42
      // ^
      // nothing to do
      break;
    case TOK_COLON:
      // ::122d:3345:4342
      // ^
      addrver = ADDRVER_IPV6;
      state = PARSE_SECOND_COLON;
      break;
    case TOK_UINT:
      // 1234:1235:1234::
      //+---+
      // or
      // 54.23.123.42
      // ++
      state = PARSE_BLOCK;
      goto __parse_current_token_label; // reparse current token use other state
      break;
    case TOK_EOF:
      return 2;
      break;
    }
    break;
  case PARSE_BLOCK:
    // Parse unsuggested block
    switch (_suggest_addr_ver(_lexval)) {
    case 0:
      goto __invalidaddrblock;
      break;
    case 1:
      // 31.123.255.132
      // ++
      v4[blocknum++] = strtol(_lexval, NULL, 10);
      state = PARSE_BLOCK_DELIM;
      break;
    case 2:
      // 2002:4559:1FE2::4559:1FE2
      // +--+
      v6b[blocknum++] = strtol(_lexval, NULL, 16);
      addrver = ADDRVER_IPV6;
      state = PARSE_COLON;
      break;
    }
    break;
  case PARSE_BLOCK_DELIM:
    // parse unsuggested block - ":", "."
    // 31.123.255.132
    //   ^
    // 2002:4559:1FE2::4559:1FE2
    //     ^
    switch (_lextok) {
    case TOK_COLON:
      if (addrver == ADDRVER_UNKNOWN) {
        // reintepretate first block of ipv4 as first block of ipv6
        char str[8];
        sprintf(str, "%u", v4[0]);
        v6b[0] = strtol(str, NULL, 16);
      }
      addrver = ADDRVER_IPV6;
      state = PARSE_COLON;
      // move first ipv4 block as ipv6
      break;
    case TOK_DOT:
      addrver = ADDRVER_IPV4;
      state = PARSE_DOT;
      break;
    }
    goto __parse_current_token_label; // reparse current token use other state
    break;
  case PARSE_COLON:
    // 2002:4559:1FE2::4559:1FE2
    //     ^    ^    ^     ^
    if (ipv6_double_colon) {
      state = PARSE_IPv6_BLOCK;
    } else {
      state = PARSE_IPv6_BLOCK_DOUBLE_COLON;
    }
    break;
  case PARSE_COLON_END:
    switch (_lextok) {
    case TOK_DELIM:
    case TOK_EOF:
      state = PARSE_END;
      break;
    case TOK_COLON:
      state = PARSE_COLON;
      break;
    }
    goto __parse_current_token_label;
    break;
  case PARSE_DOT:
    // 31.123.255.132
    //   ^   ^   ^
    state = PARSE_IPv4_BLOCK;
    break;
  case PARSE_IPv4_BLOCK:
    // 31.123.255.132
    //    +-+ +-+ +-+
    if (_suggest_addr_ver(_lexval) == 1) {
      v4[blocknum++] = strtol(_lexval, NULL, 10) & 0xff;
      if (blocknum == 4) {
        state = PARSE_END;
      } else {
        state = PARSE_DOT;
      }
    } else {
      goto __invalidaddrblock;
    }
    break;
  case PARSE_IPv6_BLOCK:
    // 2002:4559:1FE2::4559:1FE2
    //      +--+ +--+  +--+ +--+
    if (_suggest_addr_ver(_lexval) != 0) {
      v6b[blocknum++] = strtol(_lexval, NULL, 16) & 0xffff;
      if (blocknum == 8) {
        state = PARSE_END;
      } else {
        if (ipv6_double_colon) {
          state = PARSE_COLON_END;
        } else {
          state = PARSE_COLON;
        }
      }
    } else {
      goto __invalidaddrblock;
    }
    break;
  case PARSE_IPv6_BLOCK_DOUBLE_COLON:
    // Read second colon or next block
    switch (_lextok) {
    case TOK_COLON:
      state = PARSE_SECOND_COLON;
      break;
    case TOK_UINT:
      state = PARSE_IPv6_BLOCK;
      break;
    }
    goto __parse_current_token_label;
    break;
  case PARSE_IPv6_BLOCK_END:
    // 2002:4559:1FE2::1FE2
    //                 +--+
    // or
    // 2002:4559:1FE2::
    //                 ^
    // maybe next block or end of address
    switch (_lextok) {
    case TOK_DELIM:
    case TOK_EOF:
      state = PARSE_END;
      break;
    case TOK_UINT:
      state = PARSE_IPv6_BLOCK;
      break;
    }
    goto __parse_current_token_label;
    break;
  case PARSE_SECOND_COLON:
    // 2002:4559:1FE2::4559:1FE2
    //                ^
    state = PARSE_IPv6_BLOCK_END;
    ipv6_double_colon = 1;
    if (blocknum > 0) {
      memcpy(v6, v6b, blocknum * 2);
    }
    ipv6_blocknum_a = blocknum;
    blocknum = 0;
    break;
  case PARSE_END:
    // 2002:4559:1FE2::4559:1FE2
    //                          ^
    goto __setaddr;
    break;
  }

  goto __parse_next_token_label;

__setaddr:
  if (addrver == ADDRVER_IPV4) {
    addr->ver = _ADDR_IPV4;
    memcpy((char *)addr->v.v4, v4, sizeof(u8) * 4);
    addr->cidr = 32;
  } else {
    addr->ver = _ADDR_IPV6;
    if (ipv6_double_colon) {
      // v6 now contain first part of address (before ::)
      // v6b (buffer) second part (after ::)
      // we copy second part into end of address
      if (blocknum > 0) {
        memcpy(((char *)v6) + ((8 - blocknum) * 2), v6b, blocknum * 2);
      }
    } else {
      // v6 zero-filled
      // v6b (buffer) contain first part of address (all addres)
      memcpy((char *)v6, v6b, sizeof(u16) * 8);
    }
    memcpy((char *)addr->v.v6, (char *)v6, sizeof(u16) * 8);
    addr->cidr = 128;
  }
  return 1;

__invalidaddrblock:
  sprintf(_parseerr, "Invalid address block: %s\n", _lexval);
  return 0;
}

enum { v6_PRINT_LEFT = 0, v6_PRINT_RIGHT };

static int s_print_v6_block_part(char *buffer, const u16 *addr,
                                 int blocks, int mode) {
  int n = 0;
  switch (blocks) {
  case 0:
    break;
  case 1:
    n = sprintf(buffer, "%x", addr[(8 - blocks) * mode + 0]);
    break;
  case 2:
    n = sprintf(buffer, "%x:%x", addr[(8 - blocks) * mode + 0],
                addr[(8 - blocks) * mode + 1]);
    break;
  case 3:
    n = sprintf(buffer, "%x:%x:%x", addr[(8 - blocks) * mode + 0],
                addr[(8 - blocks) * mode + 1], addr[(8 - blocks) * mode + 2]);
    break;
  case 4:
    n = sprintf(buffer, "%x:%x:%x:%x", addr[(8 - blocks) * mode + 0],
                addr[(8 - blocks) * mode + 1], addr[(8 - blocks) * mode + 2],
                addr[(8 - blocks) * mode + 3]);
    break;
  case 5:
    n = sprintf(buffer, "%x:%x:%x:%x:%x", addr[(8 - blocks) * mode + 0],
                addr[(8 - blocks) * mode + 1], addr[(8 - blocks) * mode + 2],
                addr[(8 - blocks) * mode + 3], addr[(8 - blocks) * mode + 4]);
    break;
  case 6:
    n = sprintf(buffer, "%x:%x:%x:%x:%x:%x",
                addr[(8 - blocks) * mode + 0], addr[(8 - blocks) * mode + 1],
                addr[(8 - blocks) * mode + 2], addr[(8 - blocks) * mode + 3],
                addr[(8 - blocks) * mode + 4], addr[(8 - blocks) * mode + 5]);
    break;
  case 7:
    n = sprintf(buffer, "%x:%x:%x:%x:%x:%x:%x",
                addr[(8 - blocks) * mode + 0], addr[(8 - blocks) * mode + 1],
                addr[(8 - blocks) * mode + 2], addr[(8 - blocks) * mode + 3],
                addr[(8 - blocks) * mode + 4], addr[(8 - blocks) * mode + 5],
                addr[(8 - blocks) * mode + 6]);
    break;
  case 8:
    n = sprintf(buffer, "%x:%x:%x:%x:%x:%x:%x:%x",
                addr[(8 - blocks) * mode + 0], addr[(8 - blocks) * mode + 1],
                addr[(8 - blocks) * mode + 2], addr[(8 - blocks) * mode + 3],
                addr[(8 - blocks) * mode + 4], addr[(8 - blocks) * mode + 5],
                addr[(8 - blocks) * mode + 6], addr[(8 - blocks) * mode + 7]);
    break;
  }
  return n;
}

enum {
  ADDR_PRINT_NORMAL = 0,
  ADDR_PRINT_COMPACT,
};

static int s_print_addr(char *buffer, int count, struct _addr *addr, int mode) {
  int zero_num = 0;
  int zero_block_begin = 0;
  int zero_block_max_begin = 0;
  int zero_block_max_num = 0;
  int i;
  char *bufp = buffer;

  if (addr->ver == _ADDR_IPV4) {
    i = sprintf(buffer, "%u.%u.%u.%u", addr->v.v4[0], addr->v.v4[1],
                addr->v.v4[2], addr->v.v4[3]);
    bufp += i;
    if (i < 0) {
      return 1;
    }
    if (addr->cidr < 32) {
      i = sprintf(bufp, "/%u", addr->cidr);
      if (i < 0) {
        return 1;
      }
    }
    return 0;
  }

  if (mode == ADDR_PRINT_COMPACT) {
    for (i = 0; i < 8; i++) {
      if (addr->v.v6[i] == 0) {
        if (zero_num == 0) {
          zero_block_begin = i;
        }
        if (zero_num > zero_block_max_num) {
          zero_block_max_begin = zero_block_begin;
          zero_block_max_num = zero_num;
        }
        zero_num++;
      } else {
        zero_num = 0;
      }
    }

    if (zero_block_max_num > 0) {
      i = s_print_v6_block_part(buffer, addr->v.v6, zero_block_max_begin,
                                v6_PRINT_LEFT);

      if (i < 0) {
        return 1;
      }
      bufp += i;
      *(bufp++) = ':';
      *(bufp++) = ':';
      *bufp = 0;

      i = s_print_v6_block_part(
          bufp, addr->v.v6,
          8 - (zero_block_max_begin + zero_block_max_num + 1), v6_PRINT_RIGHT);
      if (i < 0) {
        return 1;
      }
      bufp += i;
      if (addr->cidr < 128) {
        i = sprintf(bufp, "/%u", addr->cidr);
        if (i < 0) {
          return 1;
        }
      }
      return 0;
    }
  }

  sprintf(buffer, "%x:%x:%x:%x:%x:%x:%x:%x", addr->v.v6[0], addr->v.v6[1],
          addr->v.v6[2], addr->v.v6[3], addr->v.v6[4], addr->v.v6[5],
          addr->v.v6[6], addr->v.v6[7]);

  if (addr->cidr < 128) {
    sprintf(bufp, "/%u", addr->cidr);
  }
  return 0;
}

static inline u32 mask_v4(int cidr) {
  return 0xffffffff << (32 - cidr);
}

static int subnode_weight(int cidr) {
  return 1 << (32 - cidr);
}

static int subnode_weight_v6(int cidr) {}

static struct _args {
  int outmode;
  int quality;
  int help;
  int skip_single;
} _args;

static int strpos(const char *haystack, const char *needle) {
  char *p;
  p = strstr(haystack, needle);
  if (p) {
    return p - haystack;
  }
  return -1;
}

static void parse_args(int argc, char **argv, struct _args *args) {
  int i;
  int rc;
  char *p;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--line") == 0) {
      args->outmode = 0;

    } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--comma") == 0) {
      args->outmode = 1;

    } else if (strpos(argv[i], "-q=") == 0) {
      rc = sscanf(argv[i], "-q=%d", &(args->quality));
      if (rc != 1 || args->quality < 0 || args->quality > 32) {
        fprintf(stderr, "Invalid argument: --quality\n");
        exit(EXIT_FAILURE);
      }
    } else if(strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--skip-single") == 0) {
      args->skip_single = 1;
    } else if (strpos(argv[i], "--quality=") == 0) {
      rc = sscanf(argv[i], "--quality=%d", &(args->quality));
      if (rc != 1 || args->quality < 0 || args->quality > 32) {
        fprintf(stderr, "Invalid argument: --quality\n");
        exit(EXIT_FAILURE);
      }
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      args->help = 1;
    } else {
      fprintf(stderr, "Invalid argument: %s\n", argv[i]);
      exit(EXIT_FAILURE);
    }
  }
}

static void show_help(FILE *stream) {
  fprintf(stream, "usage: cdrips.exe [options]\n");
  fprintf(stream, "\toptions:\n");
  fprintf(stream, "\t\t-l,  --line            Row-separated output.\n");
  fprintf(stream, "\t\t-c,  --comma           Comma-separated output.\n");
  fprintf(stream, "\t\t-q,  --quality=[n]     Quality of finding subnets. From "
                  "0 - find subnets\n");
  fprintf(stream, "\t\t                       cover all transmitted addresses, "
                  "1 - 50%%, 2 - 25%%, etc...\n");
  fprintf(stream, "\t\t-S, --skip-single      Not cover subnets with one address\n");
  fprintf(stream, "\t\t-h,  --help            Show help.\n");
}

static struct _addrlist {
  struct _addr subnode;
  struct _addrlist *next, *prev;
  int addrnum;
} *_head_v4 = 0, *_tail_v4 = 0;
static struct _addrlist *_head_v6 = 0, *_tail_v6 = 0;
static struct _addrlist *_head_deleted = 0, *_tail_deleted = 0;

static int addrlist_push(struct _addrlist **head, struct _addrlist **tail,
                         struct _addr *addr) {
  struct _addrlist *o = malloc(sizeof(struct _addrlist));
  if (o) {
    o->addrnum = 1;
    memcpy(&(o->subnode), addr, sizeof(struct _addr));
    o->next = 0;
    o->prev = 0;
    if (*tail) {
      o->prev = *tail;
      (*tail)->next = o;
      *tail = o;
    } else {
      *tail = o;
      *head = o;
    }
  }
  return !!o;
}

static void addrlist_delete(struct _addrlist **head, struct _addrlist **tail,
                            struct _addrlist *o) {
  if (o->next)
    o->next->prev = o->prev;
  if (o->prev)
    o->prev->next = o->next;
  if (o == *head)
    *head = o->next;
  if (o == *tail)
    *tail = o->prev;

  if (_tail_deleted) {
    o->next = 0;
    _tail_deleted->next = o;
    o->prev = _tail_deleted;
    _tail_deleted = o;
  } else {
    o->next = 0;
    o->prev = 0;
    _head_deleted = o;
    _tail_deleted = o;
  }
}

static void addrlist_free(struct _addrlist **head, struct _addrlist **tail) {
  struct _addrlist *p = *head, *pp;
  while (p) {
    pp = p;
    p = p->next;
    free(p);
  }
  *head = 0;
  *tail = 0;
}

static void addrlist_free_deleted() {
  addrlist_free(&_head_deleted, &_tail_deleted);
}

#if !defined(TEST) || TEST != 1

#define REALLOC_STEP 128

struct _addrlist **_group = NULL;
struct _addrlist **_group_p = NULL;
int _group_alloc = 0;

void _group_revert() { _group_p = _group; }

int _group_size() {
  if (!_group) {
    return 0;
  }
  return ((void *)_group_p - (void *)_group) / sizeof(struct _addrlist *);
}

int _group_push(struct _addrlist *value) {
  void *_t;
  if (!_group) {
    _group_alloc = REALLOC_STEP;
    _t = malloc(_group_alloc);
  } else if ((void *)_group_p - (void *)_group >= _group_alloc) {
    _group_alloc += REALLOC_STEP;
    _t = realloc(_group, _group_alloc);
  } else {
    _t = _group;
  }
  if (_t) {
    if (!_group_p) {
      _group_p = _t;
    } else {
      _group_p = _t + ((void *)_group_p - (void *)_group);
    }
    _group = _t;
    *_group_p = value;
    _group_p++;
  }
  return !!_t;
}

static inline u32 rshift(u32 v, int i) {
  return v >> i;
}

int main(int argc, char **argv) {
  struct _addr addr;
  int rc;
  int i;
  char buffer[256];
  _args.help = 0;
  _args.outmode = 0;
  _args.quality = 0;
  _args.skip_single = 0;
  parse_args(argc, argv, &_args);

  if (_args.help) {
    show_help(stdout);
    return EXIT_SUCCESS;
  }

  while (1) {
    rc = parse(stdin, &addr);
    if (rc == 0) {
      fprintf(stderr, "%s\n", _parseerr);
      return EXIT_FAILURE;
    } else if (rc == 1) {
      if (addr.ver == _ADDR_IPV4) {
        addrlist_push(&_head_v4, &_tail_v4, &addr);
      } else {
        addrlist_push(&_head_v6, &_tail_v6, &addr);
      }
    } else if (rc == 2) {
      // expected eof
      break;
    }
  }

  struct _addrlist *p, *pp;
  int _weight;
  int _mask;
  u32 _limit_quality;
  u32 _group_count = 0;
  u32 _addr_int_v4_1, _addr_int_v4_2;
  int _group_i;
  for (i = 31; i > 0; i--) {
    _weight = subnode_weight(i);
    _mask = mask_v4(i);
    _limit_quality = rshift(_weight, _args.quality);
    p = _head_v4;

    while (p) {
      pp = _head_v4;
      _group_p = _group;

      if (!_group_push(p))
        goto __memory_error;
      _group_count += p->addrnum;

      _addr_int_v4_1 = (p->subnode.v.v4[0] << 24) | (p->subnode.v.v4[1] << 16) |
                       (p->subnode.v.v4[2] << 8) | p->subnode.v.v4[3];

      while (pp) {
        if (p == pp) {
          // nothing to do
        } else {
          _addr_int_v4_2 = (pp->subnode.v.v4[0] << 24) |
                           (pp->subnode.v.v4[1] << 16) |
                           (pp->subnode.v.v4[2] << 8) | pp->subnode.v.v4[3];

          if ((_addr_int_v4_1 & _mask) == (_addr_int_v4_2 & _mask)) {
            if (!_group_push(pp))
              goto __memory_error;
            _group_count += pp->addrnum;
          }
        }
        pp = pp->next;
      }

      // remove
      if (_group_count >= _limit_quality && (_group_count > 1 || !_args.skip_single)) {
        for (_group_i = 1; _group_i < _group_size(); _group_i++) {
          addrlist_delete(&_head_v4, &_tail_v4, _group[_group_i]);
        }
        _addr_int_v4_1 = _addr_int_v4_1 & _mask;
        _group[0]->addrnum = _group_count;
        _group[0]->subnode.v.v4[0] = _addr_int_v4_1 >> 24;
        _group[0]->subnode.v.v4[1] = (_addr_int_v4_1 >> 16) & 0xff;
        _group[0]->subnode.v.v4[2] = (_addr_int_v4_1 >> 8) & 0xff;
        _group[0]->subnode.v.v4[3] = _addr_int_v4_1 & 0xff;
        _group[0]->subnode.cidr = i;
      }

      _group_revert();
      _group_count = 0LL;

      p = p->next;
    }
  }

  p = _head_v4;
  while (p) {
    s_print_addr(buffer, 255, &(p->subnode), ADDR_PRINT_COMPACT);
    printf("%s", buffer);
    p = p->next;
    if (p) {
      if (_args.outmode == 0) {
        printf("\n");
      } else {
        printf(", ");
      }
    }
    fflush(stdout);
  }

  addrlist_free(&_head_v4, &_tail_v4);
  addrlist_free(&_head_v6, &_tail_v6);
  addrlist_free_deleted();
  free(_group);

  return EXIT_SUCCESS;
__memory_error:
  addrlist_free(&_head_v4, &_tail_v4);
  addrlist_free(&_head_v6, &_tail_v6);
  addrlist_free_deleted();
  free(_group);
  fprintf(stderr, "Cannot allocate memory.\n");
  return EXIT_FAILURE;
}

#else

static void test_parser_output(const char *addr, const char *expected) {
  int rc;
  char buffer[255];
  FILE *f = fopen("test_input.txt", "w");
  if (!f) {
    printf("Cannot create tempfile.\n");
    exit(EXIT_FAILURE);
  }
  rc = fwrite(addr, 1, strlen(addr), f);
  if (rc != strlen(addr)) {
    printf("IO error.\n");
    goto __exit;
  }
  fclose(f);
  f = fopen("test_input.txt", "r");
  if (!f) {
    printf("Cannot create tempfile.\n");
    exit(EXIT_FAILURE);
  }
  struct _addr addr_s;
  rc = parse(f, &addr_s);
  printf("%s ", addr);
  if (rc == 0) {
    // error
    printf("[FAIL] %s\n", _parseerr);
    goto __exit;
  } else if (rc == 1) {
    rc = s_print_addr(buffer, 255, &addr_s, ADDR_PRINT_COMPACT);
    if (rc != 0) {
      printf("[FAIL] Cannot print address.");
      goto __exit;
    }
    if (strcmp(expected, buffer) == 0) {
      printf("[OK]\n");
    } else {
      printf("[FAIL]\tExpected: %s, Actual: %s\n", expected, buffer);
      goto __exit;
    }
  } else {
    printf("[FAIL] Unexpected EOF\n");
    goto __exit;
  }

__exit:
  fclose(f);
  remove("test_input.txt");
}

int main() {
  test_parser_output("2001:0db8:85a3:0000:0000:8a2e:0370:7334",
                     "2001:db8:85a3::8a2e:370:7334");
  test_parser_output("2001:db8:85a3::8a2e:370:7334",
                     "2001:db8:85a3::8a2e:370:7334");
  test_parser_output("2001:db8:85a3:0:0:8a2e:370:7334",
                     "2001:db8:85a3::8a2e:370:7334");
  test_parser_output("::1", "::1");
  test_parser_output("1::", "1::");
  test_parser_output("ff02::1", "ff02::1");
  test_parser_output("fd12:3456:789a:1::1", "fd12:3456:789a:1::1");
  test_parser_output("fe80::1ff:fe23:4567:890a", "fe80::1ff:fe23:4567:890a");
  test_parser_output("2001::1", "2001::1");
  test_parser_output("0:0:0:0:0:0:0:1", "::1");
  test_parser_output("2001:db8::", "2001:db8::");
  test_parser_output("2001:4860:4860::8888", "2001:4860:4860::8888");
  test_parser_output("2001:0db8::1", "2001:db8::1");
  test_parser_output("ff02::2", "ff02::2");
  test_parser_output("100::1", "100::1");
  test_parser_output("2001:db8:85a3:0000:0000:0000:8a2e:0370",
                     "2001:db8:85a3::8a2e:370");
  test_parser_output("3ffe:1900:4545:3:200:f8ff:fe21:67cf",
                     "3ffe:1900:4545:3:200:f8ff:fe21:67cf");
  test_parser_output("192.168.1.1", "192.168.1.1");
  test_parser_output("0.0.0.0", "0.0.0.0");
  remove("test_input.txt");
}

#endif