#include "cli.h"
#include "cidrips.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cli_help(FILE *o) {
  // clang-format off
  fprintf(o,
          "cidrips v%d.%d.%d.rev%s\nUtlity for compress any IP list groupping "
          "by CIDR mask.\n\n",
          cidrips_version_major, cidrips_version_minor, cidrips_version_patch,
          cidrips_git_rev);
  fprintf(o, "Usage:\n");
  fprintf(o, "\tcidrips -v4 -i[FILE]\n");
  fprintf(o, "\tcidrips -v4 -i[FILE] -o[FILE]\n");
  fprintf(o, "\tcidrips -v4 -mlevel [-l[level]] -i[FILE] -o[FILE]\n");
  fprintf(o, "\tcidrips -v4 -mcount [-c[count]] -i[FILE] -o[FILE]\n");
  fprintf(o, "\tcidrips -v4 -i[FILE] -p[PREFIX] -P[POSTFIX]\n\n");
  fprintf(o, "Arguments:\n");
  fprintf(o, "\t-v, --version [4|6]            Input version of IP addresses.\n");
  fprintf(o, "\t-i,--input    [FILE]           Path to file with input ips. (Use \n");
  fprintf(o, "\t                               --input - for stdin)\n");
  fprintf(o, "\t-o,--out      [FILE]           Path to file with output subnets. (Use \n");
  fprintf(o, "\t                               --input - for stdin)\n");
  fprintf(o, "\t-m,--mode     [level|count]    Use this method for generate \n");
  fprintf(o, "\t                               subnets: level >=0, count - maximum count\n");
  fprintf(o, "\t                               of result subnets.\n");
  fprintf(o, "\t                               [Default: level]\n");
  fprintf(o, "\t-l,--level    [LEVEL]          Comression level.  [Default: 0]\n");
  fprintf(o, "\t-c,--count    [COUNT]          Maxmimum count of result subnets.\n");
  fprintf(o, "\t                               [Default: not specifie]\n");
  fprintf(o, "\t-p,--prefix   [prefix]         Prefix for generated subnet in output.\n");
  fprintf(o, "\t                               [Default: \"\"]\n");
  fprintf(o, "\t-P,--postfix  [postfix]        Postfix for generated subnet in output.\n");
  fprintf(o, "\t                               [Default: \"\\n\"\n");
  fprintf(o, "\t-y, --yes                      Auto accept all questions.\n");
  // clang-format on
}

#define ARG_OPTIONAL 0x1
#define ARG_NO_VALUE 0x2

typedef int(arg_handler_cb)(const char *arg_val, cli_args_t *cli_args);

static int arg_version(const char *arg_val, cli_args_t *cli_args) {
  if (arg_val == (void *)0) {
  } else if (strcmp(arg_val, "4") == 0) {
    cli_args->version = AddrIPv4;
  } else if (strcmp(arg_val, "6") == 0) {
    cli_args->version = AddrIPv6;
  } else {
    fprintf(stderr,
            "--version: invalid value, support only 4 or 6. got: \"%s\"\n",
            arg_val);
    return 0;
  }
  return 1;
}

static int arg_input(const char *arg_val, cli_args_t *cli_args) {
  if (arg_val == (void *)0) {
    return 0;
  }
  if (strlen(arg_val) > 255) {
    fprintf(stderr, "--input: argument too long.\n");
    return 0;
  }
  strcpy(cli_args->input, arg_val);
  return 1;
}

static int arg_output(const char *arg_val, cli_args_t *cli_args) {
  if (arg_val == (void *)0) {
    cli_args->out[0] = 0;
  } else if (strlen(arg_val) > 255) {
    fprintf(stderr, "--output: argument too long.\n");
    return 0;
  } else {
    strcpy(cli_args->out, arg_val);
    return 1;
  }
  return 1;
}

static int arg_mode(const char *arg_val, cli_args_t *cli_args) {
  if (arg_val == (void *)0) {
    cli_args->mode = MODE_LEVEL;
  } else if (strcmp(arg_val, "level") == 0) {
    cli_args->mode = MODE_LEVEL;
  } else if (strcmp(arg_val, "count") == 0) {
    cli_args->mode = MODE_COUNT;
  } else {
    fprintf(stderr,
            "--mode: invalid value, support only level or count. got: \"%s\"\n",
            arg_val);
    return 0;
  }
  return 1;
}

static int arg_level(const char *arg_val, cli_args_t *cli_args) {
  if (cli_args->mode != MODE_UNKNOWN && cli_args->mode != MODE_LEVEL) {
    fprintf(stderr, "--level: Unexpected for this mode.\n");
    return 0;
  }
  if (arg_val == (void *)0) {
    cli_args->level = 0;
  } else if (arg_val[0] >= '0' && arg_val[0] <= 9) {
    cli_args->level = atoi(arg_val);
  } else {
    fprintf(stderr, "--level: invalid value, non-negative numbers.\n");
    return 0;
  }
  return 1;
}

static int arg_count(const char *arg_val, cli_args_t *cli_args) {
  if (cli_args->mode != MODE_UNKNOWN && cli_args->mode != MODE_LEVEL) {
    fprintf(stderr, "--count: Unexpected for this mode.\n");
    return 0;
  }
  if (arg_val == (void *)0) {
    cli_args->count = 0;
  } else if (arg_val[0] >= '0' && arg_val[0] <= 9) {
    cli_args->count = atoi(arg_val);
  } else {
    fprintf(stderr, "--count: invalid value, positive numbers.\n");
    return 0;
  }
  return 1;
}

static int arg_prefix(const char *arg_val, cli_args_t *cli_args) {
  if (strlen(arg_val) > 255) {
    fprintf(stderr, "--prefix: argument too long.\n");
    return 0;
  }
  strcpy(cli_args->prefix, arg_val);
  return 1;
}

static int arg_postfix(const char *arg_val, cli_args_t *cli_args) {
  if (strlen(arg_val) > 255) {
    fprintf(stderr, "--postfix: argument too long.\n");
    return 0;
  }
  strcpy(cli_args->postfix, arg_val);
  return 1;
}

static int arg_yes(const char *arg_val, cli_args_t *cli_args) {
  cli_args->yes = 1;
  return 1;
}

static int arg_help(const char *arg_val, cli_args_t *cli_args) {
  cli_args->help = 1;
  return 1;
}

static struct argtab {
  int arg_index;
  const char arg_short;
  const char *arg_long;
  int arg_flags;
  const char *arg_default;
  const char *arg_description;
  arg_handler_cb *arg_handler;
} _argtab[] = {
    // clang-format off
    {0, 'h', "help", ARG_OPTIONAL | ARG_NO_VALUE, 0, "Show help.", arg_help},
    {1, 'v', "version", ARG_OPTIONAL, 0, "Version of using IP addresses.", arg_version},
    {2, 'i', "input", 0, 0, "Path to input file with ip addresses.", arg_input},
    {3, 'o', "output", ARG_OPTIONAL, 0, "Path to output file.", arg_output},
    {4, 'm', "mode", ARG_OPTIONAL, "level", "Compression mode.", arg_mode},
    {5, 'l', "level", ARG_OPTIONAL, "0", "Compress level. Required for --mode=level.", arg_level},
    {6, 'c', "count", ARG_OPTIONAL, 0, "Max count of output. Required for --mode=count.", arg_count},
    {7, 'p', "prefix", ARG_OPTIONAL, "", "Prefix for ouput ip address.", arg_prefix},
    {8, 'P', "postfix", ARG_OPTIONAL, "\n", "Postfix for ouput ip address", arg_postfix},
    {9, 'y', "yes", ARG_OPTIONAL | ARG_NO_VALUE, 0, "Auto accept all questions.", arg_yes}};
// clang-format on
static int _argtab_size = 10;

static int cli_arg_val_contain(char arg_short, const char *arg_long,
                               const char *argv) {
  // clang-format off
  if (argv[0] == '-' && argv[1] == arg_short && strlen(argv) == 2)
    return 0;
  if (argv[0] == '-' && argv[1] == '-' && strstr(argv, arg_long) == argv + 2 &&
      strlen(argv) == strlen(arg_long) + 2 && argv[strlen(arg_long) + 2] != '=')
    return 0;
  // clang-format on
  return 1;
}

static char *cli_arg_val(char arg_short, const char *arg_long,
                         const char *argv) {
  char *p, *pp;
  if (argv[0] == '-' && argv[1] == arg_short)
    p = (char *)argv + 2;
  if (argv[0] == '-' && argv[1] == '-' && strstr(argv, arg_long) == argv + 2)
    p = (char *)argv + strlen(arg_long) + 2;
  if (*p == '\0') {
    return (char *)0;
  }
  if (*p == '=') {
    p++;
  }
  if (*p == '"') {
    p++;
    pp = strchr(p, '"');
    if (pp) {
      *pp = 0;
    }
  }
  if (*p == '\0') {
    return (void *)0;
  }
  return p;
}

struct argtab *find_arg(const char *argv) {
  int i;
  for (i = 0; i < _argtab_size; i++) {
    if (argv[0] == '-' && argv[1] == _argtab[i].arg_short)
      return &_argtab[i];
    if (argv[0] == '-' && strstr(argv, _argtab[i].arg_long) == argv + 2)
      return &_argtab[i];
  }
  return (void *)0;
}

int cli_parse(int argc, const char **argv, cli_args_t *args) {
  int i, rc;
  struct argtab *argtab;
  int arg_passed[64] = {0};
  for (i = 1; i < argc; i++) {
    argtab = find_arg(argv[i]);
    if (argtab) {
      arg_passed[argtab->arg_index] = 1;
      if (cli_arg_val_contain(argtab->arg_short, argtab->arg_long, argv[i])) {
        if (argtab->arg_flags & ARG_NO_VALUE) {
          fprintf(stderr, "--%s: Unexpected value.\n", argtab->arg_long);
          return 0;
        } else {
          rc = argtab->arg_handler(
              cli_arg_val(argtab->arg_short, argtab->arg_long, argv[i]), args);
          if (!rc) {
            return 0;
          }
        }
      } else {
        if (i == argc - 1) {
          if (argtab->arg_flags & ARG_NO_VALUE) {
            rc = argtab->arg_handler(argv[i], args);
            if (!rc) {
              return 0;
            }
          } else {
            fprintf(stderr, "--%s: Expected argument value.\n",
                    argtab->arg_long);
            return 0;
          }
        } else {
          if (argtab->arg_flags & ARG_NO_VALUE) {
            rc = argtab->arg_handler(argv[i], args);
            if (!rc) {
              return 0;
            }
          } else {
            i++;
            rc = argtab->arg_handler(argv[i], args);
            if (!rc) {
              return 0;
            }
          }
        }
      }
      if (argtab->arg_short == 'h') {
        return 1;
      }
    } else {
      fprintf(stderr, "%s: Unknown argument\n", argv[i]);
      return 0;
    }
  }

  for (i = 0; i < _argtab_size; i++) {
    if (arg_passed[i] == 0 && !(_argtab[i].arg_flags & ARG_OPTIONAL)) {
      fprintf(stderr, "--%s: required.\n", _argtab[i].arg_long);
      return 0;
    }
  }
  return 1;
}