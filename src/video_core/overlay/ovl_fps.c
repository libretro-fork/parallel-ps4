/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The FPS counter, as ImGui drew it (it was imgui/fps_layer.cpp): a window in the top right
 * corner, sized to its line, the rate red below 10, orange below 20, white otherwise. It
 * follows the core option, which can change while the game runs. */

#include <math.h>
#include <stdio.h>

#include "core/emulator_settings.h"
#include "ovl_layers.h"

void ovl_fps_draw(float framerate) {
    char line[64];
    ovl_vec4_t colour;
    ovl_vec2_t pos;
    float rate = framerate;
    if (!emu_settings_get_show_fps_counter())
        return;
    if (ovl_begin("Video Info", OVL_WINDOW_NO_NAV | OVL_WINDOW_NO_DECORATION |
                                    OVL_WINDOW_ALWAYS_AUTO_RESIZE | OVL_WINDOW_NO_INPUTS)) {
        pos.x = ovl_display_size().x - ovl_window_size().x;
        pos.y = 0.0f;
        ovl_set_window_pos(pos);
        colour.x = 1.0f;
        colour.y = framerate < 10.0f ? 0.0f : framerate < 20.0f ? 0.5f : 1.0f;
        colour.z = framerate < 20.0f ? 0.0f : 1.0f;
        colour.w = 1.0f;
        /* Within what the line holds; the same as ImGui's for any rate a game runs at */
        if (!(rate >= 1e-3f))
            rate = 1e-3f;
        if (rate > 1e6f)
            rate = 1e6f;
        sprintf(line, "%d FPS (%.1f ms)", (int)floor(rate + 0.5f), (double)(1000.0f / rate));
        ovl_push_color(OVL_COLOR_TEXT, colour);
        ovl_text(line, NULL);
        ovl_pop_color(1);
    }
    ovl_end();
}
