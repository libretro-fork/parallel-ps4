/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* pixel_order_xrgb2101010: every channel lands where libretro's format has it,
 * whole. */

#include <stdio.h>

#include "../pixel_order.h"

static int failures = 0;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static uint32_t a2b10g10r10(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  return r | (g << 10) | (b << 20) | (a << 30);
}

static uint32_t xrgb2101010(uint32_t r, uint32_t g, uint32_t b, uint32_t x) {
  return b | (g << 10) | (r << 20) | (x << 30);
}

int main(void) {
  uint32_t src[5], dst[5];
  uint32_t r, ok = 1;
  src[0] = a2b10g10r10(1023, 0, 0, 3);
  src[1] = a2b10g10r10(0, 1023, 0, 0);
  src[2] = a2b10g10r10(0, 0, 1023, 1);
  src[3] = a2b10g10r10(594, 300, 17, 3);
  src[4] = a2b10g10r10(1, 512, 1022, 2);
  pixel_order_xrgb2101010(dst, src, 5);
  CHECK(dst[0] == xrgb2101010(1023, 0, 0, 3), "red");
  CHECK(dst[1] == xrgb2101010(0, 1023, 0, 0), "green");
  CHECK(dst[2] == xrgb2101010(0, 0, 1023, 1), "blue");
  CHECK(dst[3] == xrgb2101010(594, 300, 17, 3), "a colour");
  CHECK(dst[4] == xrgb2101010(1, 512, 1022, 2), "another");
  /* Every value of R through, and back again: the swap is its own inverse */
  for (r = 0; r < 1024; r++) {
    uint32_t p = a2b10g10r10(r, 1023 - r, r ^ 0x155, r & 3), q, back;
    pixel_order_xrgb2101010(&q, &p, 1);
    pixel_order_xrgb2101010(&back, &q, 1);
    if (q != xrgb2101010(r, 1023 - r, r ^ 0x155, r & 3) || back != p)
      ok = 0;
  }
  CHECK(ok, "every value");
  /* In place */
  pixel_order_xrgb2101010(src, src, 5);
  CHECK(src[3] == dst[3], "in place");
  if (failures) {
    printf("pixel_order: %d failed\n", failures);
    return 1;
  }
  printf("pixel_order: all passed\n");
  return 0;
}
