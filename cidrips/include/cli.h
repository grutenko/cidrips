#ifndef cli_h
#define cli_h

#include <stdio.h>

void cli_help(FILE *o);

#define MODE_UNKNOWN 0
#define MODE_LEVEL 1
#define MODE_COUNT 2

typedef struct {
  int version;
  char input[256];
  char out[256];
  int mode;
  int level;
  int count;
  char prefix[256];
  char postfix[256];
  int yes;
  int help;
} cli_args_t;

int cli_parse(int argc, const char **argv, cli_args_t *args);

#endif