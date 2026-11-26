/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "video_core/buffer_cache/hot_windows.h"

void hot_windows_init(hot_windows_t* t) {
    t->seq = 0;
    t->count = 0;
}

static void remove_at(hot_windows_t* t, unsigned i) {
    --t->count;
    t->addr[i] = t->addr[t->count];
    t->seen[i] = t->seen[t->count];
}

void hot_windows_note(hot_windows_t* t, uint64_t window) {
    unsigned i;
    unsigned oldest = 0;
    uint32_t oldest_age = 0;
    ++t->seq;
    for (i = 0; i < t->count; ++i) {
        const uint32_t age = t->seq - t->seen[i];
        if (t->addr[i] == window) {
            t->seen[i] = t->seq;
            return;
        }
        if (age >= oldest_age) {
            oldest_age = age;
            oldest = i;
        }
    }
    if (t->count < HOT_WINDOWS_MAX) {
        oldest = t->count++;
    }
    t->addr[oldest] = window;
    t->seen[oldest] = t->seq;
}

unsigned hot_windows_collect(hot_windows_t* t, uint64_t except, uint64_t* out, unsigned max) {
    uint32_t ages[HOT_WINDOWS_MAX];
    unsigned n = 0;
    unsigned i = 0;
    while (i < t->count) {
        const uint32_t age = t->seq - t->seen[i];
        unsigned at;
        if (age > HOT_WINDOWS_AGE) {
            remove_at(t, i);
            continue;
        }
        if (t->addr[i] == except) {
            ++i;
            continue;
        }
        /* Kept in order of age, the youngest first; one past @max falls off the end */
        at = n;
        while (at > 0 && ages[at - 1] > age) {
            --at;
        }
        if (at < max) {
            unsigned k = n < max ? n : max - 1;
            for (; k > at; --k) {
                out[k] = out[k - 1];
                ages[k] = ages[k - 1];
            }
            out[at] = t->addr[i];
            ages[at] = age;
            if (n < max) {
                ++n;
            }
        }
        ++i;
    }
    return n;
}

void hot_windows_drop(hot_windows_t* t, uint64_t window) {
    unsigned i;
    for (i = 0; i < t->count; ++i) {
        if (t->addr[i] == window) {
            remove_at(t, i);
            return;
        }
    }
}
