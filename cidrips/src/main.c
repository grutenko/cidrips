#include "cidrips.h"
#include "cli.h"
#include "input.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char **argv) {
  int rc;
  cli_args_t args = {0};
  FILE *o;
  rc = cli_parse(argc, argv, &args);
  if (!rc) {
    return EXIT_FAILURE;
  }

  if (args.help) {
    cli_help(stdout);
    return EXIT_SUCCESS;
  }

  if (strcmp(args.input, "-") == 0) {
    o = stdin;
  } else {
    o = fopen(args.input, "r");
    if (!o) {
      fprintf(stderr, "--input: \"%s\" %s\n", args.input, strerror(errno));
      return EXIT_FAILURE;
    }
  }

  addr_list_t *head;
  addr_list_t *tail;
  if (!read_addr_list(o, args.version, &head, &tail, 0)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}