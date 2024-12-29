#include "cidrips.h"
#include <assert.h>
#include <string.h>

inline int addrcmp_v4(addr_t *addr0, addr_t *addr1) {
  assert(addr0->verno == AddrIPv4 && addr1->verno == AddrIPv4);

  int i;
  while (i < 4 && addr0->v.v4[i] == addr1->v.v4[i])
    ;
  if (i == 4) {
    if (addr0->cidr == addr1->cidr) {
      return 0;
    } else if (addr0->cidr > addr1->cidr) {
      return 1;
    } else {
      return -1;
    }
  } else if (addr0->v.v4[i] < addr1->v.v4[i]) {
    return 1;
  }
  return -1;
}

inline int addrcmp_v6(addr_t *addr0, addr_t *addr1) {
  assert(addr0->verno == AddrIPv6 && addr1->verno == AddrIPv6);

  int i;
  while (i < 16 && addr0->v.v6[i] == addr1->v.v6[i])
    ;
  if (i == 16) {
    if (addr0->cidr == addr1->cidr) {
      return 0;
    } else if (addr0->cidr < addr1->cidr) {
      return 1;
    } else {
      return -1;
    }
  } else if (addr0->v.v6[i] < addr1->v.v6[i]) {
    return 1;
  }
  return -1;
}

inline void addr_set(addr_t *addr, int ver, unsigned char *v, int cidr) {
  assert(ver == AddrIPv4 || ver == AddrIPv6);

  addr->verno = ver;
  if (cidr >= 0) {
    addr->cidr = cidr;
  }
  if (v) {
    if (ver == AddrIPv4) {
      memmove(addr->v.v4, v, 4);
    } else if (ver == AddrIPv6) {
      memmove(addr->v.v6, v, 16);
    }
  }
}

inline size_t addr_zone_v4_weight(int cidr) {
  assert(cidr >= 0 && cidr <= 32);
  return 1ULL << (32 - cidr);
}

// clang-format on

inline bc134_t *addr_zone_v6_weight(bc134_t *out, int cidr) {
  assert(cidr >= 0 && cidr <= 128);
  memset(out, 0, BC134_SIZE);
  out[cidr >> 3] |= 1 << (cidr & 7);
  return out;
}

static inline unsigned int addr_cidr_mask_v4(int cidr) {
  return 0xffffffff << (32 - cidr);
}

inline unsigned int addr_mask_raw_v4(addr_t *addr, int cidr) {
  return ((addr->v.v4[0] << 24) | (addr->v.v4[1] << 16) | (addr->v.v4[2] << 8) |
          addr->v.v4[3]) &
         addr_cidr_mask_v4(cidr);
}

static inline bc134_t *addr_cidr_mask_v6(bc134_t *out, int cidr) {
  memset(out, 0xff, 16);
  int i;
  for (i = 0; i < (cidr >> 3); i++) {
    out[i] = 0;
  }
  out[(cidr >> 3)] = (0xff << (cidr & 7)) & 0xff;
  return out;
}

inline bc134_t *addr_mask_raw_v6(bc134_t *out, addr_t *addr, int cidr) {
  bc134_byte_t addr_1[BC134_SIZE];
  int i;
  addr_cidr_mask_v6(addr_1, cidr);
  for (i = 0; i < 16; i++)
    out[i] = addr->v.v6[i] & addr_1[i];
  return out;
}

inline int addr_sub_zone_of(addr_t *haystack, addr_t *needle) {
  assert(haystack->cidr <= needle->cidr);
  bc134_byte_t maskv6_0[BC134_SIZE], maskv6_1[BC134_SIZE];
  if (haystack->verno == AddrIPv4) {
    return addr_mask_raw_v4(haystack, haystack->cidr) ==
           addr_mask_raw_v4(needle, haystack->cidr);
  } else if (haystack->verno == AddrIPv6) {
    addr_mask_raw_v6(maskv6_0, haystack, haystack->cidr);
    addr_mask_raw_v6(maskv6_1, needle, haystack->cidr);
    return bc134cmp(maskv6_0, maskv6_1) == 0;
  }
  return 0;
}

int addr_parse_v4(const char *p, addr_t *addr) {
  int l, rc;
  unsigned char a, b, c, d, cidr;
  rc = sscanf(p, "%hhu.%hhu.%hhu.%hhu%n", &a, &b, &c, &d, &l);
  if (rc == 4) {
    addr->verno = AddrIPv4;
    addr->v.v4[0] = a;
    addr->v.v4[1] = b;
    addr->v.v4[2] = c;
    addr->v.v4[3] = d;
    addr->cidr = 32;
    p += l;
  } else {
    return 0;
  }
  if (strlen(p) > 0) {
    rc = sscanf(p, "/%hhu", &cidr);
    if (rc == 1 && cidr <= 32) {
      addr->cidr = cidr;
    } else {
      return 0;
    }
  }
  return 1;
}

int addr_parse_v6(const char *p, addr_t *addr) {}

int addr_print(FILE *o, addr_t *addr, int mode) {}