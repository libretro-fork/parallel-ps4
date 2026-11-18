/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* What the on-screen keyboard (libSceIme, libSceImeDialog) stands on that is not the keyboard
 * itself: the list of keyboards on screen, shared by the game's threads and the overlay's with
 * no lock; and text between the console's UTF-16 and the UTF-8 the keyboard works in. */

#ifndef SHADPS4_IME_SUPPORT_H
#define SHADPS4_IME_SUPPORT_H

#include <stddef.h>

#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

/* A keyboard on screen: what draws it. First in whatever holds it. */
typedef struct ime_layer {
    void (*draw)(struct ime_layer* layer);
} ime_layer_t;

#define IME_LAYERS_MAX 4

/* Before any other call, and after the last. */
void ime_layers_init(void);
void ime_layers_free(void);

/* From any thread: @layer is drawn by each ime_layers_draw from now on. 0 when it is there
 * already or IME_LAYERS_MAX are. */
int ime_layers_add(ime_layer_t* layer);

/* From any thread: @layer is drawn no more. Unless @in_draw (the layer taking itself out from
 * its own draw, on the drawing thread), it returns only when no ime_layers_draw under way can
 * still be drawing it: the layer can then be changed or freed. The drawing thread never waits.
 * 0 when it was not there. */
int ime_layers_remove(ime_layer_t* layer, int in_draw);

/* On one thread, the overlay's: every layer there is drawn. */
void ime_layers_draw(void);

/* Whether a layer is there. */
int ime_layers_any(void);

/* UTF-16, to @length units or its end, as UTF-8 into @out of @out_size bytes; UTF-8, to
 * @length bytes or its end, as UTF-16 into @out of @out_size units. A character above U+FFFF is
 * a surrogate pair and four bytes. @out is filled with zeroes after the text, which is cut at a
 * whole character to leave one. The text's length in @out. */
size_t ime_utf16_to_utf8(const unsigned short* text, size_t length, char* out, size_t out_size);
size_t ime_utf8_to_utf16(const char* text, size_t length, unsigned short* out, size_t out_size);

/* The keyboard's text, edited where it is: nothing is copied out and back, nothing allocated.
 * @text is a string in a buffer of @size bytes.
 *
 * ime_text_splice: its bytes from @start to @end give way to @insert_len bytes of @insert
 * (not within @text); what comes after moves up or down. What does not fit before the
 * buffer's last byte is cut off the end. The new length.
 *
 * ime_text_normalize_newlines: every CR LF and every CR alone becomes LF. Nonzero if any was.
 *
 * ime_text_clamp_utf16: cut after the last character with which it is no more than @max_units
 * UTF-16 units (a character above U+FFFF is two). Nonzero if it was cut. */
size_t ime_text_splice(char* text, size_t size, size_t start, size_t end, const char* insert,
                       size_t insert_len);
int ime_text_normalize_newlines(char* text);
int ime_text_clamp_utf16(char* text, size_t max_units);

RETRO_END_DECLS

#endif
