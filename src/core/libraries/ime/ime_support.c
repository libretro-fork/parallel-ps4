/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "core/libraries/ime/ime_support.h"

#include <string.h>

#include <encodings/utf.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

/* A slot is taken and given up by a compare and swap. The drawing thread raises `drawing`
 * around its walk of the slots; a thread that empties a slot then waits for the walk under way,
 * if any, to end: after that no walk holds the layer it took out. Each side stores, then reads
 * what the other stored, with a full fence between (the store and the load are of different
 * words, so nothing weaker orders them). */
static retro_atomic_ptr_t ime_layers[IME_LAYERS_MAX];
static retro_atomic_int_t ime_layers_drawing;
static retro_eventcount_t ime_layers_drawn;

void ime_layers_init(void) {
    int i;
    for (i = 0; i < IME_LAYERS_MAX; i++)
        retro_atomic_ptr_init(&ime_layers[i], NULL);
    retro_atomic_int_init(&ime_layers_drawing, 0);
    retro_eventcount_init(&ime_layers_drawn);
}

void ime_layers_free(void) {
    retro_eventcount_free(&ime_layers_drawn);
}

int ime_layers_add(ime_layer_t* layer) {
    int i;
    for (i = 0; i < IME_LAYERS_MAX; i++)
        if (retro_atomic_load_acquire_ptr(&ime_layers[i]) == (void*)layer)
            return 0;
    for (i = 0; i < IME_LAYERS_MAX; i++)
        if (retro_atomic_cas_ptr(&ime_layers[i], NULL, layer))
            return 1;
    return 0;
}

int ime_layers_remove(ime_layer_t* layer, int in_draw) {
    int removed = 0;
    int i;
    for (i = 0; i < IME_LAYERS_MAX; i++) {
        if (retro_atomic_cas_ptr(&ime_layers[i], layer, NULL)) {
            removed = 1;
            break;
        }
    }
    if (in_draw)
        return removed;
    retro_atomic_thread_fence_seq_cst();
    while (retro_atomic_load_acquire_int(&ime_layers_drawing)) {
        const int key = retro_eventcount_prepare_wait(&ime_layers_drawn);
        if (!retro_atomic_load_acquire_int(&ime_layers_drawing)) {
            retro_eventcount_cancel_wait(&ime_layers_drawn);
            break;
        }
        retro_eventcount_commit_wait(&ime_layers_drawn, key);
    }
    return removed;
}

void ime_layers_draw(void) {
    int i;
    retro_atomic_store_release_int(&ime_layers_drawing, 1);
    retro_atomic_thread_fence_seq_cst();
    for (i = 0; i < IME_LAYERS_MAX; i++) {
        ime_layer_t* layer = (ime_layer_t*)retro_atomic_load_acquire_ptr(&ime_layers[i]);
        if (layer)
            layer->draw(layer);
    }
    retro_atomic_store_release_int(&ime_layers_drawing, 0);
    retro_eventcount_notify(&ime_layers_drawn);
}

int ime_layers_any(void) {
    int i;
    for (i = 0; i < IME_LAYERS_MAX; i++)
        if (retro_atomic_load_acquire_ptr(&ime_layers[i]))
            return 1;
    return 0;
}

size_t ime_utf16_to_utf8(const unsigned short* text, size_t length, char* out, size_t out_size) {
    size_t units = 0, bytes = 0;
    if (!out || out_size == 0)
        return 0;
    memset(out, 0, out_size);
    if (!text)
        return 0;
    while (units < length && text[units])
        units++;
    /* What is well formed and fits, the end kept: the count first, since the conversion does
     * not stop at a size */
    for (;;) {
        const int whole = utf16_conv_utf8(NULL, &bytes, text, units);
        if ((whole && bytes < out_size) || units == 0)
            break;
        units--;
    }
    if (units == 0)
        return 0;
    utf16_conv_utf8((uint8_t*)out, &bytes, text, units);
    return bytes;
}

size_t ime_utf8_to_utf16(const char* text, size_t length, unsigned short* out, size_t out_size) {
    const char* nul;
    if (!out || out_size == 0)
        return 0;
    memset(out, 0, out_size * sizeof(*out));
    if (!text)
        return 0;
    nul = (const char*)memchr(text, 0, length);
    if (nul)
        length = (size_t)(nul - text);
    /* One unit kept for the terminator */
    return utf8_conv_utf16((uint16_t*)out, out_size - 1, text, length);
}

size_t ime_text_splice(char* text, size_t size, size_t start, size_t end, const char* insert,
                       size_t insert_len) {
    size_t length, tail, at;
    if (!text || size == 0)
        return 0;
    length = strlen(text);
    if (end > length)
        end = length;
    if (start > end)
        start = end;
    /* What is put in, then what was after, as far as each fits before the last byte */
    if (insert_len > size - 1 - start)
        insert_len = size - 1 - start;
    at = start + insert_len;
    tail = length - end;
    if (tail > size - 1 - at)
        tail = size - 1 - at;
    if (at != end)
        memmove(text + at, text + end, tail);
    if (insert_len)
        memcpy(text + start, insert, insert_len);
    text[at + tail] = '\0';
    return at + tail;
}

int ime_text_normalize_newlines(char* text) {
    const char* from = text;
    char* to = text;
    int changed = 0;
    if (!text)
        return 0;
    while (*from) {
        if (*from == '\r') {
            if (from[1] == '\n')
                from++;
            *to++ = '\n';
            from++;
            changed = 1;
        } else {
            *to++ = *from++;
        }
    }
    *to = '\0';
    return changed;
}

int ime_text_clamp_utf16(char* text, size_t max_units) {
    const char* at = text;
    size_t units = 0;
    if (!text)
        return 0;
    while (*at) {
        const char* next = at;
        const uint32_t c = utf8_walk(&next);
        const size_t need = c > 0xFFFF ? 2 : 1;
        if (units + need > max_units) {
            text[at - text] = '\0';
            return 1;
        }
        units += need;
        at = next;
    }
    return 0;
}
