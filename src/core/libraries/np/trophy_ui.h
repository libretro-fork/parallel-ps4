/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A trophy's notification (trophy_ui.c): its sound, made once, and its popup, which the overlay
 * shows (video_core/overlay/ovl_trophy.c) */

#ifndef SHADPS4_LIBRARIES_NP_TROPHY_UI_H
#define SHADPS4_LIBRARIES_NP_TROPHY_UI_H

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Makes the trophy sound ready for the core's mix: the custom one in the user's folder, or the
 * built-in one. Called once, before the game runs, so nothing is decoded when a trophy unlocks. */
void trophy_ui_preload_sound(void);

/* Queues a trophy's popup: its icon at @icon_path (UTF-8), its name, and its rarity ("P", "G",
 * "S" or "B"). @after: shown once the one showing (or queued before it) has had its time, rather
 * than in its place */
void trophy_ui_queue(const char* icon_path, const char* name, const char* rarity, bool after);

#ifdef __cplusplus
}
#endif

#endif
