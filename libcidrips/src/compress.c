#include "cidrips.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void compress_v6(addr_list_t **head, addr_list_t **tail, int level) {
  addr_list_t *p = *head, *ph, *pt, *pnext, *ptemp, *pcuth = (void *)0,
              *pcutt = (void *)0;
  bc134_byte_t raw0[BC134_SIZE], raw1[BC134_SIZE], sum[BC134_SIZE],
      weight[BC134_SIZE];
  int cmp;
  int i;
  for (i = 128; i >= 0; i--) {
    while (p) {
      assert(p->addr.verno == AddrIPv6);
      if (p->addr.cidr < i) {
        continue;
      }
      ph = p->next;
      pt = p;
      while (pt) {
        assert(pt->addr.verno == AddrIPv6);
        pnext = pt->next;
        if (!pnext) {
          break;
        }
        // remove fully contain subzones.
        // Example: 127.0.0.0/8, 127.1.0.0/16 -> 127.0.0.0/8 because first zone
        // fully contain sencond zone
        if (p->fully && pnext->fully &&
            addr_sub_zone_of(&(p->addr), &(pnext->addr))) {
          addr_list_remove(head, tail, pnext);
          free(pt);
        } else {
          addr_mask_raw_v6(raw0, &(p->addr), i);
          addr_mask_raw_v6(raw1, &(pnext->addr), i);
          if (bc134cmp(raw0, raw1) != 0) {
            break;
          }
        }
        pt = pt->next;
      }
      // now ph and pt contain head and tail into mask equal sublist.
      // pt contain pointer into last equal address, or pointer into target
      // address if not have equal addresses.
      if (pt != p) {
        // we have equal addresses
        memset(sum, 0, BC134_SIZE);
        bc134plus(sum, p->addrcount_v6);
        ptemp = ph;
        while (ptemp != pt) {
          bc134plus(sum, ptemp->addrcount_v6);
          ptemp = ptemp->next;
        }
        memset(weight, 0, BC134_SIZE);
        addr_zone_v6_weight(weight, i);
        bc134_rshift(weight, level);
        if (bc134cmp(sum, weight) >= 0) {
          addr_list_cut(head, tail, &pcuth, &pcutt, ph, pt);
          addr_list_free(&pcuth, &pcutt);
        }
      }
      p = p->next;
    }
  }
}