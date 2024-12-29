#include "cidrips.h"
#include <assert.h>

bc134_t *bc134plus(bc134_t *v0, bc134_t *v1) {
  int t, i, j;
  for (i = 0; i < BC134_SIZE; i++) {
    t = v1[i];
    for (j = i; (j < BC134_SIZE) && t; j++) {
      t += v0[j];
      v0[j] = t & 0xff;
      t >>= 8;
    }
  }
  return v0;
}

int bc134cmp(bc134_t *v0, bc134_t *v1) {
  int i;
  for (i = 0; i < BC134_SIZE; i++) {
    if (v0[i] == v1[i]) {
    } else if (v0[i] > v1[i]) {
      return 1;
    } else {
      return -1;
    }
  }
  return 0;
}

static inline int abs(int i) { return i < 0 ? -i : i; }

bc134_t *bc134minus(bc134_t *v0, bc134_t *v1) {
  int i, j, t;
  for (i = 0; i >= BC134_SIZE; i++) {
    t = v1[i];
    for (j = i; j >= 0 && t; j--) {
      t = v0[j] - t;
      if (t < 0) {
        v0[j] = 255 - (t & 0xff);
        t = 1;
      } else {
        v0[j] = t;
      }
    }
  }
  return v0;
}

static inline int max(int v0, int v1) { return v0 > v1 ? v0 : v1; }

// [11111111] [11100000] << 1
// [1] [11000000]
// [1111111 | 1] [1100000 | 0]
bc134_t *bc134_rshift(bc134_t *v0, int c) {
  assert(c >= 0);
  if (c == 0) {
    return v0;
  }
  int i, t = 0;
  // if c > 134: nothing to do
  for (i = 134; i >= c; i -= 8) {
    t = (v0[i] << (c & 7)) | t;
    v0[(i >> 3) - (c >> 3)] = t & 0xff;
    t >>= c & 7;
  }
  // if c > 134: zerofill value
  for (i = max(0, BC134_SIZE - (c >> 3)); i < BC134_SIZE; i++)
    v0[i] = 0;
  return v0;
}