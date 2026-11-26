/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AMDGPU_LOD_STATS_H
#define AMDGPU_LOD_STATS_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* The GPU's texture detail statistics (the IT_GET_LOD_STATS packet). A texture descriptor with
 * LOD_HW_CNT_EN set counts, in the counter bank it names (COUNTER_BANK_ID, 256 of them), how
 * its texture was sampled; the packet has the banks written to the game's memory. A game that
 * streams its textures loads their fine mips by these numbers: told nothing, Shadow of the
 * Colossus never loads more than the coarsest, and its ground, rocks and walls are smears.
 *
 * The console knows the mip each sample needed. Here every texture a draw or a dispatch bound
 * since the last report counts as sampled at one same mip, LOD_STATS_MIP, and the rest as not
 * sampled: what Pink-shadPS4 does, and the game's PC port after it. */

#define LOD_STATS_BANKS 256
/* The words of a report: 256 banks of 64 bits */
#define LOD_STATS_REPORT_BYTES (LOD_STATS_BANKS * 8)
/* A report is at this offset in the buffer the packet names, and another after it, as many as
 * the buffer holds: the game reads the two in turn */
#define LOD_STATS_REPORT_OFFSET 0x40

/* The mip a sampled texture is said to need. 0 is the finest; the PC port found that asking
 * for the finest everywhere has the game stream more than its texture pool holds, until it
 * breaks its own memory, and settled on 2 (512 by 512 of a 2048 texture). */
#define LOD_STATS_MIP 2

/* A texture descriptor with LOD_HW_CNT_EN was bound: its bank was sampled. From the thread
 * that binds. */
void lod_stats_mark(unsigned bank);

/* A report: for each bank, the mip in bits 56 to 59 (15: not sampled) and how often it was
 * bound in bits 32 to 55 (the game's priority among what it streams). The banks start anew.
 * From the thread that runs the packets. */
void lod_stats_collect(uint64_t entries[LOD_STATS_BANKS]);

/* Where a packet's reports go. @words are the packet's three after its header: the buffer's
 * size less 32, its address shifted right by 6, and the address's bits 38 and 39 in the low
 * two bits of the last. @return how many reports the buffer holds. */
unsigned lod_stats_packet(const uint32_t words[3], uint64_t* address);

RETRO_END_DECLS

#endif
