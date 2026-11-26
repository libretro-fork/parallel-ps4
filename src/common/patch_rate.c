/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common/patch_rate.h"

typedef struct patch_rate_parser {
    const char* p;
    double n;
    int ok;
} patch_rate_parser_t;

static double patch_rate_sum(patch_rate_parser_t* ps);

static void patch_rate_space(patch_rate_parser_t* ps) {
    while (*ps->p == ' ' || *ps->p == '\t')
        ps->p++;
}

/* A number, N, a parenthesised formula, or one of those after a minus */
static double patch_rate_atom(patch_rate_parser_t* ps) {
    patch_rate_space(ps);
    if (*ps->p == '-') {
        ps->p++;
        return -patch_rate_atom(ps);
    }
    if (*ps->p == '(') {
        double v;
        ps->p++;
        v = patch_rate_sum(ps);
        patch_rate_space(ps);
        if (*ps->p != ')') {
            ps->ok = 0;
            return 0.0;
        }
        ps->p++;
        return v;
    }
    if (*ps->p == 'N') {
        ps->p++;
        return ps->n;
    }
    if ((*ps->p >= '0' && *ps->p <= '9') || *ps->p == '.') {
        char* end = NULL;
        /* As the game's float constant: single precision */
        const float f = (float)strtod(ps->p, &end);
        if (end == ps->p) {
            ps->ok = 0;
            return 0.0;
        }
        ps->p = end;
        return (double)f;
    }
    ps->ok = 0;
    return 0.0;
}

/* Powers, right to left: 2^3^2 is 2^9 */
static double patch_rate_power(patch_rate_parser_t* ps) {
    const double base = patch_rate_atom(ps);
    patch_rate_space(ps);
    if (*ps->p == '^') {
        ps->p++;
        return pow(base, patch_rate_power(ps));
    }
    return base;
}

static double patch_rate_product(patch_rate_parser_t* ps) {
    double v = patch_rate_power(ps);
    for (;;) {
        patch_rate_space(ps);
        if (*ps->p == '*') {
            ps->p++;
            v *= patch_rate_power(ps);
        } else if (*ps->p == '/') {
            double d;
            ps->p++;
            d = patch_rate_power(ps);
            if (d == 0.0) {
                ps->ok = 0;
                return 0.0;
            }
            v /= d;
        } else {
            return v;
        }
    }
}

static double patch_rate_sum(patch_rate_parser_t* ps) {
    double v = patch_rate_product(ps);
    for (;;) {
        patch_rate_space(ps);
        if (*ps->p == '+') {
            ps->p++;
            v += patch_rate_product(ps);
        } else if (*ps->p == '-') {
            ps->p++;
            v -= patch_rate_product(ps);
        } else {
            return v;
        }
    }
}

int patch_rate_eval(const char* formula, double n, double* out) {
    patch_rate_parser_t ps;
    double v;
    if (!formula)
        return 0;
    ps.p = formula;
    ps.n = n;
    ps.ok = 1;
    v = patch_rate_sum(&ps);
    patch_rate_space(&ps);
    if (!ps.ok || *ps.p != '\0' || v != v || v - v != 0.0)
        return 0;
    *out = v;
    return 1;
}

int patch_rate_is_line(const char* type) {
    return type && (strcmp(type, "rate_f32") == 0 || strcmp(type, "rate_u32") == 0 ||
                    strcmp(type, "rate_dec") == 0);
}

static void patch_rate_hex_byte(char* hex, unsigned byte) {
    static const char digits[] = "0123456789abcdef";
    hex[0] = digits[(byte >> 4) & 0xf];
    hex[1] = digits[byte & 0xf];
}

int patch_rate_hex(const char* type, unsigned size, double value, char* hex, size_t len) {
    unsigned i;
    if (!type || len < 41)
        return 0;
    if (strcmp(type, "rate_f32") == 0) {
        const float f = (float)value;
        unsigned int u;
        memcpy(&u, &f, sizeof(u));
        /* Little-endian, whatever the host's order: from the float's own bits */
        for (i = 0; i < 4; i++)
            patch_rate_hex_byte(hex + 2 * i, (u >> (8 * i)) & 0xff);
        hex[8] = '\0';
        return 1;
    }
    if (strcmp(type, "rate_u32") == 0) {
        const double whole = floor(value);
        unsigned long u;
        if (whole < 0.0 || whole > 4294967295.0)
            return 0;
        u = (unsigned long)whole;
        for (i = 0; i < 4; i++)
            patch_rate_hex_byte(hex + 2 * i, (unsigned)((u >> (8 * i)) & 0xff));
        hex[8] = '\0';
        return 1;
    }
    if (strcmp(type, "rate_dec") == 0) {
        double whole = floor(value);
        if (size < 1 || size > 20 || whole < 0.0 || whole >= pow(10.0, (double)size))
            return 0;
        /* The digits from the last, zeros before */
        for (i = size; i-- > 0;) {
            const double digit = fmod(whole, 10.0);
            patch_rate_hex_byte(hex + 2 * i, (unsigned)('0' + (int)digit));
            whole = floor(whole / 10.0);
        }
        hex[2 * size] = '\0';
        return 1;
    }
    return 0;
}

int patch_rate_in_range(const char* min, const char* max, unsigned n) {
    if (min && *min && n < (unsigned)strtoul(min, NULL, 10))
        return 0;
    if (max && *max && n > (unsigned)strtoul(max, NULL, 10))
        return 0;
    return 1;
}
