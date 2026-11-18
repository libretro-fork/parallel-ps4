/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common/target_frames.h"

typedef struct target_entry {
    uint32_t drawn;  /* the last frame it was drawn into */
    uint32_t before; /* and the one before that, 0: none */
} target_entry_t;

bool target_frames_init(target_frames_t* t, size_t reserve) {
    t->frame = 1;
    return u64_map_init(&t->map, sizeof(target_entry_t), reserve);
}

void target_frames_free(target_frames_t* t) {
    u64_map_free(&t->map);
}

void target_frames_next(target_frames_t* t) {
    t->frame++;
    /* Now and then: what the game has stopped drawing into is forgotten. One
     * at a time, since an erase moves the entries after it. */
    if (t->frame % TARGET_FRAMES_KEEP == 0) {
        for (;;) {
            size_t pos = 0;
            uint64_t key = 0;
            uint64_t old = 0;
            int found = 0;
            const target_entry_t* e;
            while ((e = (const target_entry_t*)u64_map_next(&t->map, &pos, &key)) != NULL) {
                if (t->frame - e->drawn >= TARGET_FRAMES_KEEP) {
                    old = key;
                    found = 1;
                    break;
                }
            }
            if (!found)
                break;
            u64_map_erase(&t->map, old);
        }
    }
}

int target_frames_draw(target_frames_t* t, uint64_t address) {
    target_entry_t* e = (target_entry_t*)u64_map_insert(&t->map, address, NULL);
    if (!e)
        return 0; /* no room to know: as an image never seen, the draw waits */
    if (e->drawn != t->frame) {
        e->before = e->drawn;
        e->drawn = t->frame;
    }
    return e->before != 0 && e->before + 1 == t->frame;
}
