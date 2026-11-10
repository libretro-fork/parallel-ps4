/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_INTRUSIVE_LIST_H
#define SHADPS4_COMMON_INTRUSIVE_LIST_H

/* A doubly linked list threaded through its elements, in C89 (it builds as
 * MSVC C89: no declarations after statements, no // comments, no bool).
 *
 * Each element carries a list_hook_t; the list is a ring through one more
 * hook, its root. A hook that is in no list points at itself, so an empty
 * list is a root pointing at itself. Linking, unlinking and splicing move
 * pointers and allocate nothing.
 *
 * The functions take hooks; the element is found from its hook the usual C
 * way (the hook as the first member, or offsetof). */

#include <stddef.h>

#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

typedef struct list_hook {
    struct list_hook* prev;
    struct list_hook* next;
} list_hook_t;

/* An unlinked hook, or an empty list's root */
static INLINE void list_hook_init(list_hook_t* h) {
    h->prev = h;
    h->next = h;
}

/* Nonzero when @h is in a list (for a root: when the list is not empty) */
static INLINE int list_hook_is_linked(const list_hook_t* h) {
    return h->next != h;
}

/* Takes @h out of whatever list holds it; harmless when it is in none */
static INLINE void list_hook_unlink(list_hook_t* h) {
    h->prev->next = h->next;
    h->next->prev = h->prev;
    h->prev = h;
    h->next = h;
}

/* Links the unlinked hook @h in just before @pos */
static INLINE void list_hook_link_before(list_hook_t* h, list_hook_t* pos) {
    h->prev = pos->prev;
    h->next = pos;
    pos->prev->next = h;
    pos->prev = h;
}

/* Moves the run [@first, @last) to just before @pos, from the same list or
 * another one. @pos must not lie inside the run. */
static INLINE void list_splice_before(list_hook_t* pos, list_hook_t* first, list_hook_t* last) {
    list_hook_t* tail;
    if (first == last)
        return;
    tail = last->prev;
    /* out of where it was */
    first->prev->next = last;
    last->prev = first->prev;
    /* in before pos */
    first->prev = pos->prev;
    tail->next = pos;
    pos->prev->next = first;
    pos->prev = tail;
}

/* Moves every element of the list at @src onto the empty list at @dst */
static INLINE void list_take_all(list_hook_t* dst, list_hook_t* src) {
    if (!list_hook_is_linked(src))
        return;
    dst->next = src->next;
    dst->prev = src->prev;
    dst->next->prev = dst;
    dst->prev->next = dst;
    list_hook_init(src);
}

/* Walks the list at @root; @h is the current hook. The loop body must not
 * unlink @h (use list_for_each_safe for that). */
#define list_for_each(h, root) for ((h) = (root)->next; (h) != (root); (h) = (h)->next)
/* As list_for_each, with @n holding the next hook so the body may unlink @h */
#define list_for_each_safe(h, n, root)                                                             \
    for ((h) = (root)->next, (n) = (h)->next; (h) != (root); (h) = (n), (n) = (h)->next)

/* Unlinks every element of the list at @root */
static INLINE void list_clear(list_hook_t* root) {
    while (list_hook_is_linked(root))
        list_hook_unlink(root->next);
}

/* The number of elements; it walks the list */
static INLINE size_t list_count(const list_hook_t* root) {
    size_t n = 0;
    const list_hook_t* h;
    for (h = root->next; h != root; h = h->next)
        n++;
    return n;
}

RETRO_END_DECLS

#endif
