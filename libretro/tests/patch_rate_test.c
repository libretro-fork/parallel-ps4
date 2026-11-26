/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* patch_rate: a patch line's value as a formula of the rate picked.
 *   - the formulas of Bloodborne's frame rate patch give, for 60, 90, 240 and
 * 960, the bytes shadPS4's 60 to 960 FPS++ wrote, numbers taken as
 * single-precision constants (0.65^(30/N) at 90 and 240 is one unit lower in
 * the last place than in double precision), and 144;
 *   - a formula that does not parse, divides by zero or is not finite gives
 * nothing;
 *   - rate_dec writes its digits with zeros before and refuses a number too
 * long for them;
 *   - a range of rates with either end left open. */

#include <stdio.h>
#include <string.h>

#include "common/patch_rate.h"

static int failures = 0;

#define CHECK(cond, what)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s (line %d)\n", what, __LINE__);                           \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

typedef struct rate_case {
  const char *type;
  const char *formula;
  unsigned size;
  unsigned n;
  const char *hex;
} rate_case_t;

static const rate_case_t cases[] = {
    {"rate_f32", "1/N", 0, 60, "8988883c"},
    {"rate_f32", "3/N", 0, 60, "cdcc4c3d"},
    {"rate_f32", "1/(2*N)", 0, 60, "8988083c"},
    {"rate_f32", "0.65^(30/N)", 0, 60, "d0644e3f"},
    {"rate_f32", "0.8^(30/N)", 0, 60, "2ef9643f"},
    {"rate_f32", "1.2^(30/N)", 0, 60, "8c378c3f"},
    {"rate_f32", "-1.5/N", 0, 60, "cdccccbc"},
    {"rate_f32", "N", 0, 60, "00007042"},
    {"rate_u32", "N/6", 0, 60, "0a000000"},
    {"rate_u32", "N/3", 0, 60, "14000000"},
    {"rate_u32", "990000/N", 0, 60, "74400000"},
    {"rate_dec", "1000000/N", 5, 60, "3136363636"},
    {"rate_f32", "1/N", 0, 90, "610b363c"},
    {"rate_f32", "3/N", 0, 90, "8988083d"},
    {"rate_f32", "1/(2*N)", 0, 90, "610bb63b"},
    {"rate_f32", "0.65^(30/N)", 0, 90, "d8c15d3f"},
    {"rate_f32", "0.8^(30/N)", 0, 90, "3ca66d3f"},
    {"rate_f32", "1.2^(30/N)", 0, 90, "3205883f"},
    {"rate_f32", "-1.5/N", 0, 90, "898888bc"},
    {"rate_f32", "N", 0, 90, "0000b442"},
    {"rate_u32", "N/6", 0, 90, "0f000000"},
    {"rate_u32", "N/3", 0, 90, "1e000000"},
    {"rate_u32", "990000/N", 0, 90, "f82a0000"},
    {"rate_dec", "1000000/N", 5, 90, "3131313131"},
    {"rate_f32", "1/N", 0, 144, "398ee33b"},
    {"rate_f32", "3/N", 0, 144, "abaaaa3c"},
    {"rate_f32", "1/(2*N)", 0, 144, "398e633b"},
    {"rate_f32", "0.65^(30/N)", 0, 144, "95066a3f"},
    {"rate_f32", "0.8^(30/N)", 0, 144, "145f743f"},
    {"rate_f32", "1.2^(30/N)", 0, 144, "97f4843f"},
    {"rate_f32", "-1.5/N", 0, 144, "abaa2abc"},
    {"rate_f32", "N", 0, 144, "00001043"},
    {"rate_u32", "N/6", 0, 144, "18000000"},
    {"rate_u32", "N/3", 0, 144, "30000000"},
    {"rate_u32", "990000/N", 0, 144, "db1a0000"},
    {"rate_dec", "1000000/N", 5, 144, "3036393434"},
    {"rate_f32", "1/N", 0, 240, "8988883b"},
    {"rate_f32", "3/N", 0, 240, "cdcc4c3c"},
    {"rate_f32", "1/(2*N)", 0, 240, "8988083b"},
    {"rate_f32", "0.65^(30/N)", 0, 240, "5b94723f"},
    {"rate_f32", "0.8^(30/N)", 0, 240, "44f5783f"},
    {"rate_f32", "1.2^(30/N)", 0, 240, "5df3823f"},
    {"rate_f32", "-1.5/N", 0, 240, "cdccccbb"},
    {"rate_f32", "N", 0, 240, "00007043"},
    {"rate_u32", "N/6", 0, 240, "28000000"},
    {"rate_u32", "N/3", 0, 240, "50000000"},
    {"rate_u32", "990000/N", 0, 240, "1d100000"},
    {"rate_dec", "1000000/N", 5, 240, "3034313636"},
    {"rate_f32", "1/N", 0, 960, "8988883a"},
    {"rate_f32", "3/N", 0, 960, "cdcc4c3b"},
    {"rate_f32", "1/(2*N)", 0, 960, "8988083a"},
    {"rate_f32", "0.65^(30/N)", 0, 960, "ab937c3f"},
    {"rate_f32", "0.8^(30/N)", 0, 960, "97387e3f"},
    {"rate_f32", "1.2^(30/N)", 0, 960, "3bbb803f"},
    {"rate_f32", "-1.5/N", 0, 960, "cdccccba"},
    {"rate_f32", "N", 0, 960, "00007044"},
    {"rate_u32", "N/6", 0, 960, "a0000000"},
    {"rate_u32", "N/3", 0, 960, "40010000"},
    {"rate_u32", "990000/N", 0, 960, "07040000"},
    {"rate_dec", "1000000/N", 5, 960, "3031303431"},
};

int main(void) {
  size_t i;
  double v = 0.0;
  char hex[64];
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const rate_case_t *c = &cases[i];
    int ok = patch_rate_eval(c->formula, (double)c->n, &v);
    CHECK(ok, c->formula);
    ok = ok && patch_rate_hex(c->type, c->size, v, hex, sizeof(hex));
    CHECK(ok && strcmp(hex, c->hex) == 0, c->formula);
    if (!ok || strcmp(hex, c->hex) != 0)
      printf("  %s at %u: %s, not %s\n", c->formula, c->n, ok ? hex : "-",
             c->hex);
  }
  CHECK(!patch_rate_eval("1/(N", 60.0, &v), "an open parenthesis");
  CHECK(!patch_rate_eval("1/0", 60.0, &v), "a division by zero");
  CHECK(!patch_rate_eval("1/N x", 60.0, &v), "what follows the formula");
  CHECK(!patch_rate_eval("", 60.0, &v), "nothing");
  CHECK(!patch_rate_eval("10^400", 60.0, &v), "not finite");
  CHECK(patch_rate_eval("2^3^2", 60.0, &v) && v == 512.0,
        "powers from the right");
  CHECK(patch_rate_eval("-N + 2*3", 60.0, &v) && v == -54.0,
        "a minus and products first");
  CHECK(patch_rate_hex("rate_dec", 3, 42.9, hex, sizeof(hex)) &&
            strcmp(hex, "303432") == 0,
        "zeros before, rounded down");
  CHECK(!patch_rate_hex("rate_dec", 3, 1000.0, hex, sizeof(hex)),
        "too many digits");
  CHECK(!patch_rate_hex("rate_u32", 0, -1.0, hex, sizeof(hex)), "below zero");
  CHECK(!patch_rate_hex("bytes", 0, 1.0, hex, sizeof(hex)), "not a rate line");
  CHECK(patch_rate_is_line("rate_f32") && !patch_rate_is_line("bytes32"),
        "rate lines");
  CHECK(patch_rate_in_range("61", "", 90) &&
            !patch_rate_in_range("61", NULL, 60),
        "from a rate");
  CHECK(patch_rate_in_range(NULL, "60", 60) &&
            !patch_rate_in_range("", "60", 61),
        "to a rate");
  if (failures) {
    printf("patch_rate: %d failed\n", failures);
    return 1;
  }
  printf("patch_rate: all passed\n");
  return 0;
}
