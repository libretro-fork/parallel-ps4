/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* What a page of guest memory held just before the guest wrote to it, kept for the pages that
 * also hold bytes the GPU has written (compute shader output: Bloodborne's face and skinned
 * vertices).
 *
 * With GPU readbacks off, what the GPU writes never reaches guest memory. When the guest then
 * writes anything else in the same 4 KiB page, the buffer cache uploaded the whole page from
 * guest memory, and the GPU's bytes were replaced with the stale ones beneath them: vertices
 * thrown across the screen. With the page as it was before the write, the upload can tell the
 * bytes the guest changed from the ones it left alone, and upload only those: the GPU's bytes
 * stay, and the guest never waits on the GPU for it, as it does with readbacks on.
 *
 * A fixed table, made once: nothing is allocated when a snapshot is taken, and a page of the
 * table is only resident once a snapshot has used it. Snapshots are taken from the threads
 * that fault and claimed from the GPU thread, with no lock (libretro-common's retro_atomic).
 * When the table has no room for a page, its oldest neighbour is dropped; a page with no
 * snapshot is uploaded whole, as before. */

#ifndef WRITE_SNAPSHOTS_H
#define WRITE_SNAPSHOTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WRITE_SNAPSHOT_PAGE 4096u
#define WRITE_SNAPSHOT_SLOTS 2048u

typedef struct write_snapshots write_snapshots_t;

/* Null if the table cannot be made. */
write_snapshots_t* write_snapshots_create(void);
void write_snapshots_destroy(write_snapshots_t* snapshots);

/* Keeps the WRITE_SNAPSHOT_PAGE bytes at @content as page @page's (its address, a multiple of
 * the page size). For the thread about to let a write through, before it does. */
void write_snapshots_take(write_snapshots_t* snapshots, uint64_t page, const void* content);

/* A snapshot taken before this call is older than the value returned, one taken after is not.
 * Read where no snapshot of the pages in question can be taken at the same time. */
uint64_t write_snapshots_cutoff(const write_snapshots_t* snapshots);

/* Page @page's newest snapshot older than @cutoff, or null. Older ones of the page are
 * dropped. The bytes are the caller's until write_snapshots_release(*slot). One thread. */
const unsigned char* write_snapshots_claim(write_snapshots_t* snapshots, uint64_t page,
                                           uint64_t cutoff, uint32_t* slot);
void write_snapshots_release(write_snapshots_t* snapshots, uint32_t slot);

/* Calls @run(ctx, begin, size) for each run of bytes from @begin to @end (offsets into both)
 * where @now differs from @base, in whole 4-byte cells: a value partly changed is all new. */
typedef void (*write_snapshots_run_t)(void* ctx, uint32_t begin, uint32_t size);
void write_snapshots_changed(const unsigned char* now, const unsigned char* base, uint32_t begin,
                             uint32_t end, write_snapshots_run_t run, void* ctx);

/* What the guest wrote, as far as can be told: the runs write_snapshots_changed gives, joined
 * across a gap of at most WRITE_SNAPSHOT_BRIDGE bytes the guest left as they were. A guest
 * writes its data whole, and some of it is what was there before (a zero over a zero): the
 * cells between the ones it changed were written too, and the GPU's bytes under them are not
 * to stay - they are older than the guest's data (memory used again: a vertex buffer where the
 * GPU had drawn, its z of 0 kept as the GPU's 0xFF000000, a vertex thrown to -2^127). The
 * bytes of the page just outside the range are looked at too, so a write that starts before
 * @begin or goes on past @end is joined into the range; and where the range is cut by the page
 * and goes on in the page before or after (@open_begin, @open_end), a short gap at the page's
 * start or end is joined to it, as the guest's writes go on into that page. The runs given
 * lie within the range. */
#define WRITE_SNAPSHOT_BRIDGE 128u
void write_snapshots_written(const unsigned char* now, const unsigned char* base, uint32_t begin,
                             uint32_t end, int open_begin, int open_end, write_snapshots_run_t run,
                             void* ctx);

#ifdef __cplusplus
}
#endif

#endif
