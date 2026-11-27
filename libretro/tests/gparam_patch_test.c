/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Graphics param files changed as the game reads them (common/gparam_patch.h):
 * a GPARAM file made here, alone, in a BND4 and in a DCX. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/deflate.h>

#include "common/gparam_patch.h"

static int failures;

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                    \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static void put32(unsigned char* p, unsigned long v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static unsigned long get32(const unsigned char* p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static size_t wide(unsigned char* p, const char* s) {
    size_t n = 0;
    while (*s) {
        p[n++] = (unsigned char)*s++;
        p[n++] = 0;
    }
    p[n++] = 0;
    p[n++] = 0;
    return n;
}

static float getf(const unsigned char* p) {
    unsigned long u = get32(p);
    unsigned int v = (unsigned int)u;
    float f;
    memcpy(&f, &v, 4);
    return f;
}

static void putf(unsigned char* p, float f) {
    unsigned int v;
    memcpy(&v, &f, 4);
    put32(p, v);
}

/* The values area's offsets within the file made by make_gparam */
#define RES_AT 0x280
#define FAR_AT 0x288
#define FLAG_AT 0x290
#define OTHER_AT 0x294

/* One GPARAM, version 3: params "ShadowParam" (fields "Shadow Map Resolution",
 * int, two values 0 and 3; "Far Fade Start", float, 2.0 and 5.0; "Use Thing",
 * bool) and "Other " (field "Shadow Map Resolution", int 1). 0x300 bytes. */
static size_t make_gparam(unsigned char* d) {
    size_t p;
    memset(d, 0, 0x300);
    memcpy(d, "f\0i\0l\0t\0", 8);
    put32(d + 8, 3);
    put32(d + 0x10, 2);     /* params */
    put32(d + 0x18, 0x30);  /* param offsets */
    put32(d + 0x1c, 0x40);  /* params */
    put32(d + 0x20, 0xc0);  /* field offsets */
    put32(d + 0x24, 0xe0);  /* fields */
    put32(d + 0x28, 0x280); /* values */
    put32(d + 0x30, 0);
    put32(d + 0x34, 0x60);
    /* Param 0 at 0x40 */
    put32(d + 0x40, 3);
    put32(d + 0x44, 0);
    p = 0x48;
    p += wide(d + p, "ShadowParam ParamEditor");
    wide(d + p, "ShadowParam");
    /* Param 1 at 0xa0 */
    put32(d + 0xa0, 1);
    put32(d + 0xa4, 0x10);
    p = 0xa8;
    p += wide(d + p, "OK");
    wide(d + p, "Other ");
    /* Field offsets */
    put32(d + 0xc0, 0);
    put32(d + 0xc4, 0x60);
    put32(d + 0xc8, 0xc0);
    put32(d + 0xd0, 0x120);
    /* Fields at 0xe0 + offset: values offset, value ids offset, type, capacity */
    put32(d + 0xe0, RES_AT - 0x280);
    d[0xe8] = 3;
    d[0xe9] = 2;
    p = 0xec;
    p += wide(d + p, "Res Key");
    wide(d + p, "Shadow Map Resolution");
    put32(d + 0x140, FAR_AT - 0x280);
    d[0x148] = 9;
    d[0x149] = 2;
    p = 0x14c;
    p += wide(d + p, "Far Key");
    wide(d + p, " Far Fade Start");
    put32(d + 0x1a0, FLAG_AT - 0x280);
    d[0x1a8] = 11;
    d[0x1a9] = 1;
    p = 0x1ac;
    p += wide(d + p, "Flag Key");
    wide(d + p, "Use Thing");
    put32(d + 0x200, OTHER_AT - 0x280);
    d[0x208] = 3;
    d[0x209] = 1;
    p = 0x20c;
    p += wide(d + p, "Other Res Key");
    wide(d + p, "Shadow Map Resolution");
    /* Values */
    put32(d + RES_AT, 0);
    put32(d + RES_AT + 4, 3);
    putf(d + FAR_AT, 2.0f);
    putf(d + FAR_AT + 4, 5.0f);
    d[FLAG_AT] = 0;
    put32(d + OTHER_AT, 1);
    return 0x300;
}

static void test_rules(void) {
    gparam_rule_t r;
    CHECK(gparam_rule_parse(&r, "ShadowParam", "Shadow Map Resolution", "max", "3"));
    CHECK(r.op == GPARAM_OP_MAX && r.value == 3.0);
    CHECK(gparam_rule_parse(&r, "A", "B", "", "1.5") && r.op == GPARAM_OP_SET);
    CHECK(gparam_rule_parse(&r, "A", "B", "mul", "2") && r.op == GPARAM_OP_MUL);
    CHECK(!gparam_rule_parse(&r, "A", "B", "add", "2"));
    CHECK(!gparam_rule_parse(&r, "", "B", "set", "2"));
    CHECK(!gparam_rule_parse(&r, "A", "B", "set", "2x"));
    CHECK(!gparam_rule_parse(&r, "A", "B", "set", ""));
}

static void test_plain(void) {
    static unsigned char d[0x300];
    gparam_rule_t rules[3];
    size_t len = make_gparam(d);
    CHECK(gparam_rule_parse(&rules[0], "shadowparam", "shadow map resolution", "max", "2"));
    CHECK(gparam_rule_parse(&rules[1], "ShadowParam", "Far Fade Start", "mul", "1.5"));
    CHECK(gparam_rule_parse(&rules[2], "ShadowParam", "Use Thing", "set", "1"));
    /* 0 raised to 2, 3 kept; both floats; the bool; not Other's field */
    CHECK(gparam_apply(d, len, rules, 3) == 4);
    CHECK(get32(d + RES_AT) == 2 && get32(d + RES_AT + 4) == 3);
    CHECK(getf(d + FAR_AT) == 3.0f && getf(d + FAR_AT + 4) == 7.5f);
    CHECK(d[FLAG_AT] == 1);
    CHECK(get32(d + OTHER_AT) == 1);
    /* Again: max and set change nothing, mul does */
    CHECK(gparam_apply(d, len, rules, 1) == 0);
    CHECK(gparam_rule_parse(&rules[0], "Other", "Shadow Map Resolution", "min", "0"));
    CHECK(gparam_apply(d, len, rules, 1) == 1 && get32(d + OTHER_AT) == 0);
    /* Not a GPARAM; a broken one is left alone */
    d[8] = 2;
    CHECK(gparam_apply(d, len, rules, 1) == -1);
    d[8] = 3;
    put32(d + 0x10, 0x7fffffff);
    CHECK(gparam_apply(d, len, rules, 1) == -1);
}

/* make_gparam's file in a BND4 as Bloodborne's .gparambnd has it: one file,
 * format 0x74, entries of 0x24 bytes, the file stored at 0xe0 */
static size_t make_bnd(unsigned char* b) {
    memset(b, 0, 0xe0);
    memcpy(b, "BND4", 4);
    b[0x0a] = 1;
    put32(b + 0x0c, 1);
    put32(b + 0x10, 0x40);
    memcpy(b + 0x18, "07D7R6", 6);
    put32(b + 0x20, 0x24);
    put32(b + 0x28, 0xe0);
    b[0x30] = 1;
    b[0x31] = 0x74;
    b[0x40] = 0x40;
    put32(b + 0x44, 0xffffffffUL);
    put32(b + 0x48, 0x300);
    put32(b + 0x50, 0x300);
    put32(b + 0x58, 0xe0);
    make_gparam(b + 0xe0);
    return 0xe0 + 0x300;
}

static void test_bnd(void) {
    static unsigned char b[0x3e0];
    gparam_rule_t r;
    size_t len = make_bnd(b);
    CHECK(gparam_rule_parse(&r, "ShadowParam", "Shadow Map Resolution", "set", "3"));
    CHECK(gparam_apply(b, len, &r, 1) == 1);
    CHECK(get32(b + 0xe0 + RES_AT) == 3);
    /* A compressed entry (sizes differ) is not touched */
    make_bnd(b);
    put32(b + 0x48, 0x200);
    CHECK(gparam_apply(b, len, &r, 1) == 0 && get32(b + 0xe0 + RES_AT) == 0);
}

static size_t deflate_into(const unsigned char* in, size_t len, unsigned char* out, size_t cap) {
    void* s = rdeflate_new(6, 15);
    size_t total = 0;
    int status;
    rdeflate_set_in(s, in, len);
    rdeflate_set_out(s, out, cap);
    rdeflate_finish(s);
    do {
        size_t r = 0, w = 0;
        status = rdeflate_process(s, &r, &w);
        total += w;
        if (r == 0 && w == 0 && status == RDEFLATE_PROCESS_NEXT)
            break;
    } while (status == RDEFLATE_PROCESS_NEXT);
    rdeflate_free(s);
    return status == RDEFLATE_PROCESS_END ? total : 0;
}

static void put_be32(unsigned char* p, unsigned long v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void test_dcx(void) {
    static unsigned char plain[0x300];
    static unsigned char dcx[0x1000];
    static unsigned char out[0x1000];
    static unsigned char back[0x300];
    gparam_rule_t r;
    size_t packed, out_len = 0, got = 0;
    void* s;
    make_gparam(plain);
    memset(dcx, 0, sizeof(dcx));
    memcpy(dcx, "DCX\0", 4);
    put_be32(dcx + 4, 0x10000);
    put_be32(dcx + 8, 0x18);
    put_be32(dcx + 0x0c, 0x24);
    put_be32(dcx + 0x10, 0x44);
    put_be32(dcx + 0x14, 0x4c);
    memcpy(dcx + 0x18, "DCS\0", 4);
    put_be32(dcx + 0x1c, 0x300);
    memcpy(dcx + 0x24, "DCP\0DFLT", 8);
    put_be32(dcx + 0x2c, 0x20);
    dcx[0x30] = 9;
    memcpy(dcx + 0x44, "DCA\0", 4);
    put_be32(dcx + 0x48, 8);
    packed = deflate_into(plain, 0x300, dcx + 0x4c, sizeof(dcx) - 0x4c);
    CHECK(packed > 0);
    put_be32(dcx + 0x20, (unsigned long)packed);

    CHECK(gparam_rule_parse(&r, "ShadowParam", "Shadow Map Resolution", "max", "3"));
    CHECK(gparam_patch_dcx(dcx, 0x4c + packed, &r, 1, out, sizeof(out), &out_len) == 1);
    CHECK(out_len > 0x4c && memcmp(out, dcx, 0x20) == 0);
    s = rinflate_new(15);
    rinflate_set_in(s, out + 0x4c, out_len - 0x4c);
    rinflate_set_out(s, back, sizeof(back));
    CHECK(rinflate_process(s, NULL, &got) == RDEFLATE_PROCESS_END);
    rinflate_free(s);
    CHECK(got == 0x300);
    plain[RES_AT] = 3;
    CHECK(memcmp(back, plain, 0x300) == 0);
    /* Nothing to change: nothing written */
    CHECK(gparam_rule_parse(&r, "ShadowParam", "Shadow Map Resolution", "min", "3"));
    CHECK(gparam_patch_dcx(dcx, 0x4c + packed, &r, 1, out, sizeof(out), &out_len) == 0);
    CHECK(out_len == 0);
    /* No room */
    CHECK(gparam_rule_parse(&r, "ShadowParam", "Shadow Map Resolution", "set", "2"));
    CHECK(gparam_patch_dcx(dcx, 0x4c + packed, &r, 1, out, 0x50, &out_len) == -1);
    /* Not a DCX */
    dcx[0] = 'X';
    CHECK(gparam_patch_dcx(dcx, 0x4c + packed, &r, 1, out, sizeof(out), &out_len) == -1);
}

int main(void) {
    test_rules();
    test_plain();
    test_bnd();
    test_dcx();
    if (failures) {
        printf("gparam_patch_test: %d failures\n", failures);
        return 1;
    }
    printf("gparam_patch_test: ok\n");
    return 0;
}
