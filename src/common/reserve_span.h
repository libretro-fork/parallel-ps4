/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_RESERVE_SPAN_H
#define SHADPS4_COMMON_RESERVE_SPAN_H

/* The part of a free region of the host's address space that can be reserved
 * where reservations start and end on the allocation granularity (Windows:
 * 64 KiB): from the region's start rounded up to its end, or @max if that is
 * lower, rounded down. A free region starts where the allocation before it
 * ends, on a page, not on the granularity; its size rounded down and counted
 * from its start rounded up reaches past its end, into the next allocation. */

#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* 1 and the span in *@addr, *@size, or 0 if nothing of the region can be
 * reserved. @granularity is a power of two. */
int reserve_span(uint64_t start, uint64_t size, uint64_t max, uint64_t granularity,
                 uint64_t* addr, uint64_t* span);

RETRO_END_DECLS

#endif
