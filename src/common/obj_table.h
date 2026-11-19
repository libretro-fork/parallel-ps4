/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A table of objects that threads look up by slot, use, and remove, without a lock: what the
 * network libraries kept in maps under a lock (contexts, requests, handles). Each slot is one
 * atomic word, its users counted in it, and a pointer: a slot is taken by compare-and-swap,
 * an object found is held until given back, and one removed (doomed) is found no more and goes
 * to whichever thread lets go of it last, which frees it. A thread that must wait for an
 * object's users to leave waits on the table's eventcount, notified as a slot's users drop,
 * its own way (the game's time). Made with its slots; nothing allocated after. */

#ifndef SHADPS4_OBJ_TABLE_H
#define SHADPS4_OBJ_TABLE_H

#include <retro_atomic.h>
#include <retro_common_api.h>
#include <rthreads/retro_eventcount.h>

RETRO_BEGIN_DECLS

typedef struct obj_slot {
    retro_atomic_int_t word; /* users times 4, plus LIVE (1) and DOOMED (2); 0: free */
    retro_atomic_ptr_t object;
} obj_slot_t;

typedef struct obj_table {
    obj_slot_t* slots;
    int count;
    retro_eventcount_t users_left; /* notified as a slot's users drop */
} obj_table_t;

/* @count free slots made now. 0 if they could not be. */
int obj_table_init(obj_table_t* table, int count);
void obj_table_free(obj_table_t* table);

/* @object into the lowest free slot of [@first, @end), live with no users: its index, or -1
 * if none was free. */
int obj_table_add(obj_table_t* table, void* object, int first, int end);

/* The object of @index, held (one more user), if it is live and not doomed; NULL if not.
 * @index out of range: NULL. */
void* obj_table_hold(obj_table_t* table, int index);

/* A user leaves: 1 if it was the last of a doomed object's, and the caller must now free the
 * object and then call obj_table_reuse. */
int obj_table_release(obj_table_t* table, int index);

/* Doomed: found no more. 1 if no one holds it, and the caller must now free the object and
 * call obj_table_reuse; 0 if a holder will be told; -1 if it was not live or already doomed. */
int obj_table_doom(obj_table_t* table, int index);

/* The slot of a doomed object freed, taken again by obj_table_add from here. */
void obj_table_reuse(obj_table_t* table, int index);

/* Its users now (0 if it is free), and whether it is live and not doomed. */
int obj_table_users(obj_table_t* table, int index);
int obj_table_live(obj_table_t* table, int index);
/* The object of @index whatever its state, for a holder: NULL if free. */
void* obj_table_peek(obj_table_t* table, int index);

RETRO_END_DECLS

#endif
