#ifndef input_h
#define input_h

#include "cidrips.h"
#include "stdio.h"

int read_addr_list(FILE *o, int ver, addr_list_t **head, addr_list_t **tail,
                   int flags);

#endif