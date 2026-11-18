/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* ime_support: what the on-screen keyboard stands on.
 *   - the list of keyboards on screen: added once, drawn by each walk, drawn no more once
 *     removed, IME_LAYERS_MAX at most; one that takes itself out from its own draw;
 *   - a keyboard whose draw is held up while another thread removes it: the removal returns
 *     after the draw, 200 times;
 *   - three threads putting keyboards up and taking them down as fast as they can while a
 *     fourth draws: a keyboard is never drawn after its removal returned (each is marked dead
 *     then, with a plain write the thread sanitizer would catch a draw racing), and every one
 *     that was up for a whole walk was drawn by it;
 *   - text between UTF-16 and UTF-8: Latin, accents, kanji, a character above U+FFFF as a
 *     surrogate pair and four bytes, a text cut at a whole character to fit, the rest of the
 *     buffer zero, a length short of the text's end, a lone surrogate. */

#include <stdio.h>
#include <string.h>

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "core/libraries/ime/ime_support.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

typedef struct board {
    ime_layer_t layer; /* first */
    int alive;         /* plain: written by its owner only while it is not on the list */
    int drawn;         /* by the drawing thread */
    int leave_after;   /* takes itself out at this many draws (0: never) */
    int left;
} board_t;

static retro_atomic_int_t drawn_dead;

static void board_draw(ime_layer_t* layer) {
    board_t* b = (board_t*)layer;
    volatile int spin;
    /* A draw takes a while: looked at again and again, as a keyboard's state is */
    for (spin = 0; spin < 40; spin++)
        if (!b->alive)
            retro_atomic_fetch_add_int(&drawn_dead, 1);
    b->drawn++;
    if (b->leave_after && b->drawn == b->leave_after)
        b->left = ime_layers_remove(layer, 1);
}

#define OWNERS 3
#define ROUNDS 30000

static retro_atomic_int_t owners_done;
static retro_atomic_int_t walks;
static retro_atomic_int_t missed;

static void owner(void* data) {
    board_t b;
    int i;
    (void)data;
    b.layer.draw = board_draw;
    b.leave_after = 0;
    b.left = 0;
    for (i = 0; i < ROUNDS; i++) {
        int before, after;
        b.alive = 1;
        b.drawn = 0;
        if (!ime_layers_add(&b.layer)) {
            retro_atomic_fetch_add_int(&missed, 1);
            continue;
        }
        /* Up for two walks' starts now and then: the walk between them drew it */
        before = retro_atomic_load_acquire_int(&walks);
        if ((i & 255) == 0)
            while (retro_atomic_load_acquire_int(&walks) < before + 2 &&
                   !retro_atomic_load_acquire_int(&owners_done)) {
            }
        after = retro_atomic_load_acquire_int(&walks);
        ime_layers_remove(&b.layer, 0);
        /* No walk holds it now: these are plain writes and reads */
        b.alive = 0;
        if (after >= before + 2 && b.drawn == 0)
            retro_atomic_fetch_add_int(&missed, 1);
    }
}

static void drawer(void* data) {
    (void)data;
    while (!retro_atomic_load_acquire_int(&owners_done)) {
        retro_atomic_fetch_add_int(&walks, 1);
        ime_layers_draw();
    }
}

/* A keyboard whose draw is held up: its removal, on another thread, returns after it */
static retro_atomic_int_t slow_in_draw, slow_release, slow_draw_done, slow_removing;
static retro_atomic_int_t slow_early;

static void slow_draw(ime_layer_t* layer) {
    (void)layer;
    retro_atomic_store_release_int(&slow_in_draw, 1);
    while (!retro_atomic_load_acquire_int(&slow_release)) {
    }
    retro_atomic_store_release_int(&slow_draw_done, 1);
}

static void slow_walker(void* data) {
    (void)data;
    ime_layers_draw();
}

static void slow_remover(void* data) {
    ime_layer_t* layer = (ime_layer_t*)data;
    while (!retro_atomic_load_acquire_int(&slow_in_draw)) {
    }
    retro_atomic_store_release_int(&slow_removing, 1);
    ime_layers_remove(layer, 0);
    if (!retro_atomic_load_acquire_int(&slow_draw_done))
        retro_atomic_fetch_add_int(&slow_early, 1);
}

static void check_removal_waits(void) {
    ime_layer_t slow;
    int round;
    volatile int spin;
    slow.draw = slow_draw;
    retro_atomic_int_init(&slow_early, 0);
    for (round = 0; round < 200; round++) {
        sthread_t* walker;
        sthread_t* remover;
        retro_atomic_int_init(&slow_in_draw, 0);
        retro_atomic_int_init(&slow_release, 0);
        retro_atomic_int_init(&slow_draw_done, 0);
        retro_atomic_int_init(&slow_removing, 0);
        ime_layers_add(&slow);
        walker = sthread_create(slow_walker, NULL);
        remover = sthread_create(slow_remover, &slow);
        /* The removal is under way while the draw is: give it time to return too early */
        while (!retro_atomic_load_acquire_int(&slow_removing)) {
        }
        for (spin = 0; spin < 20000; spin++) {
        }
        retro_atomic_store_release_int(&slow_release, 1);
        sthread_join(remover);
        sthread_join(walker);
    }
    CHECK(retro_atomic_load_acquire_int(&slow_early) == 0,
          "a removal returns after the draw under way");
}

static void check_layers(void) {
    board_t b[IME_LAYERS_MAX + 1];
    sthread_t* threads[OWNERS];
    sthread_t* draw_thread;
    int i;

    ime_layers_init();
    memset(b, 0, sizeof(b));
    for (i = 0; i <= IME_LAYERS_MAX; i++) {
        b[i].layer.draw = board_draw;
        b[i].alive = 1;
    }
    retro_atomic_int_init(&drawn_dead, 0);
    CHECK(!ime_layers_any(), "none at first");
    ime_layers_draw();
    CHECK(ime_layers_add(&b[0].layer) && ime_layers_any(), "one added");
    CHECK(!ime_layers_add(&b[0].layer), "not twice");
    ime_layers_draw();
    ime_layers_draw();
    CHECK(b[0].drawn == 2, "drawn by each walk");
    for (i = 1; i < IME_LAYERS_MAX; i++)
        CHECK(ime_layers_add(&b[i].layer), "as many as there is room for");
    CHECK(!ime_layers_add(&b[IME_LAYERS_MAX].layer), "and no more");
    ime_layers_draw();
    CHECK(b[0].drawn == 3 && b[IME_LAYERS_MAX - 1].drawn == 1 && b[IME_LAYERS_MAX].drawn == 0,
          "all of them drawn");
    CHECK(ime_layers_remove(&b[1].layer, 0) && !ime_layers_remove(&b[1].layer, 0), "removed, once");
    ime_layers_draw();
    CHECK(b[1].drawn == 1 && b[2].drawn == 2, "drawn no more; the others are");
    /* One that takes itself out at its third draw */
    b[2].leave_after = 3;
    ime_layers_draw();
    ime_layers_draw();
    CHECK(b[2].drawn == 3 && b[2].left == 1, "one takes itself out from its own draw");
    CHECK(ime_layers_add(&b[IME_LAYERS_MAX].layer), "its place free");
    for (i = 0; i <= IME_LAYERS_MAX; i++)
        ime_layers_remove(&b[i].layer, 0);
    CHECK(!ime_layers_any(), "none left");
    CHECK(retro_atomic_load_acquire_int(&drawn_dead) == 0, "none drawn dead");

    check_removal_waits();

    retro_atomic_int_init(&owners_done, 0);
    retro_atomic_int_init(&walks, 0);
    retro_atomic_int_init(&missed, 0);
    draw_thread = sthread_create(drawer, NULL);
    for (i = 0; i < OWNERS; i++)
        threads[i] = sthread_create(owner, NULL);
    for (i = 0; i < OWNERS; i++)
        sthread_join(threads[i]);
    retro_atomic_store_release_int(&owners_done, 1);
    sthread_join(draw_thread);
    CHECK(retro_atomic_load_acquire_int(&drawn_dead) == 0,
          "never drawn after its removal returned");
    CHECK(retro_atomic_load_acquire_int(&missed) == 0,
          "always room for three, and one up for a whole walk was drawn");
    CHECK(!ime_layers_any(), "none left by the threads");
    printf("ime_support: %d keyboards up and down on %d threads, %d walks\n", OWNERS * ROUNDS,
           OWNERS, retro_atomic_load_acquire_int(&walks));
    ime_layers_free();
}

static void check_text(void) {
    /* "Aé日" then U+1F600, as UTF-8 and as UTF-16 */
    static const char utf8[] = "A\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80";
    static const unsigned short utf16[] = {'A', 0xE9, 0x65E5, 0xD83D, 0xDE00, 0};
    unsigned short w[16];
    char c[16];
    size_t n;

    memset(c, 'x', sizeof(c));
    n = ime_utf16_to_utf8(utf16, 99, c, sizeof(c));
    CHECK(n == 10 && memcmp(c, utf8, 11) == 0 && c[15] == 0, "UTF-16 to UTF-8, zero after");
    memset(w, 0xFF, sizeof(w));
    n = ime_utf8_to_utf16(utf8, strlen(utf8), w, 16);
    CHECK(n == 5 && memcmp(w, utf16, sizeof(utf16)) == 0 && w[15] == 0,
          "UTF-8 to UTF-16, a pair above U+FFFF, zero after");
    /* A length short of the end */
    n = ime_utf16_to_utf8(utf16, 2, c, sizeof(c));
    CHECK(n == 3 && memcmp(c, "A\xC3\xA9", 4) == 0, "two units of UTF-16");
    n = ime_utf8_to_utf16(utf8, 3, w, 16);
    CHECK(n == 2 && w[0] == 'A' && w[1] == 0xE9 && w[2] == 0, "three bytes of UTF-8");
    n = ime_utf8_to_utf16(utf8, 4, w, 16);
    CHECK(n == 2 && w[2] == 0, "a length inside a character: without it");
    /* Cut to fit, at a whole character, an end kept */
    n = ime_utf16_to_utf8(utf16, 99, c, 6);
    CHECK(n == 3 && memcmp(c, "A\xC3\xA9\0\0", 6) == 0, "six bytes: the kanji would not fit");
    n = ime_utf16_to_utf8(utf16, 99, c, 7);
    CHECK(n == 6 && c[6] == 0, "seven: it does, the pair's four bytes do not");
    n = ime_utf8_to_utf16(utf8, strlen(utf8), w, 5);
    CHECK(n == 3 && w[3] == 0 && w[4] == 0, "five units: not half a pair");
    n = ime_utf8_to_utf16(utf8, strlen(utf8), w, 6);
    CHECK(n == 5 && w[5] == 0, "six: the pair");
    /* A lone surrogate ends the text */
    {
        static const unsigned short lone[] = {'A', 0xD83D, 'B', 0};
        n = ime_utf16_to_utf8(lone, 99, c, sizeof(c));
        CHECK(n == 1 && c[0] == 'A' && c[1] == 0, "a lone surrogate: the text before it");
    }
    CHECK(ime_utf16_to_utf8(NULL, 4, c, sizeof(c)) == 0 && c[0] == 0 &&
              ime_utf8_to_utf16(NULL, 4, w, 16) == 0 && w[0] == 0 &&
              ime_utf16_to_utf8(utf16, 99, c, 0) == 0 && ime_utf8_to_utf16(utf8, 9, w, 0) == 0,
          "no text, no room");
}

/* The text edited where it is, against the same edit made the long way: the parts copied out
 * and put together, then cut to the buffer */
static void splice_reference(char* out, size_t size, const char* text, size_t start, size_t end,
                             const char* insert, size_t insert_len) {
    char all[256];
    size_t length = strlen(text), n = 0;
    if (end > length)
        end = length;
    if (start > end)
        start = end;
    memcpy(all + n, text, start);
    n += start;
    memcpy(all + n, insert, insert_len);
    n += insert_len;
    memcpy(all + n, text + end, length - end);
    n += length - end;
    if (n > size - 1)
        n = size - 1;
    memcpy(out, all, n);
    out[n] = '\0';
}

static void check_edit(void) {
    /* The buffer in the middle of one with a guard each side: nothing is written outside it */
    char box[48], want[32];
    char* text = box + 8;
    unsigned seed = 12345, i;
    size_t n;

    strcpy(text, "hello world");
    n = ime_text_splice(text, 32, 5, 5, ",", 1);
    CHECK(n == 12 && strcmp(text, "hello, world") == 0, "put in: what is after moves up");
    n = ime_text_splice(text, 32, 5, 6, "", 0);
    CHECK(n == 11 && strcmp(text, "hello world") == 0, "taken out: what is after moves down");
    n = ime_text_splice(text, 32, 6, 11, "there", 5);
    CHECK(n == 11 && strcmp(text, "hello there") == 0, "replaced by as much");
    n = ime_text_splice(text, 32, 0, 99, "x", 1);
    CHECK(n == 1 && strcmp(text, "x") == 0, "an end past the text is the text's");
    strcpy(text, "abcdef");
    n = ime_text_splice(text, 8, 3, 3, "12345", 5);
    CHECK(n == 7 && strcmp(text, "abc1234") == 0,
          "more than fits: what is put in is cut at the buffer's last byte, what was after gone");
    strcpy(text, "abcdef");
    n = ime_text_splice(text, 8, 1, 1, "12", 2);
    CHECK(n == 7 && strcmp(text, "a12bcde") == 0, "what was after is cut there too");
    CHECK(ime_text_splice(NULL, 8, 0, 0, "a", 1) == 0 && ime_text_splice(text, 0, 0, 0, "a", 1) == 0,
          "no text, no buffer");

    for (i = 0; i < 20000; i++) {
        char insert[16];
        size_t size, length, start, end, insert_len, k;
        memset(box, 0x55, sizeof(box));
        seed = seed * 1103515245u + 12345u;
        size = 1 + (seed >> 16) % 32;
        seed = seed * 1103515245u + 12345u;
        length = (seed >> 16) % size;
        for (k = 0; k < length; k++)
            text[k] = (char)('a' + k % 26);
        text[length] = '\0';
        seed = seed * 1103515245u + 12345u;
        start = (seed >> 16) % 36;
        seed = seed * 1103515245u + 12345u;
        end = (seed >> 16) % 36;
        seed = seed * 1103515245u + 12345u;
        insert_len = (seed >> 16) % 16;
        for (k = 0; k < insert_len; k++)
            insert[k] = (char)('0' + k % 10);
        splice_reference(want, size, text, start, end, insert, insert_len);
        n = ime_text_splice(text, size, start, end, insert, insert_len);
        if (n != strlen(want) || strcmp(text, want) != 0 || box[7] != 0x55 ||
            text[size] != 0x55) {
            CHECK(0, "an edit in place as the same edit made of copies, within its buffer");
            break;
        }
    }

    strcpy(text, "a\r\nb\rc\nd\r");
    CHECK(ime_text_normalize_newlines(text) && strcmp(text, "a\nb\nc\nd\n") == 0,
          "CR LF and CR alone are LF");
    CHECK(!ime_text_normalize_newlines(text) && strcmp(text, "a\nb\nc\nd\n") == 0 &&
              !ime_text_normalize_newlines(NULL),
          "nothing to change");

    /* A, e acute, a kanji, a character above U+FFFF (two units), B: 1 + 1 + 1 + 2 + 1 units */
    strcpy(text, "A\xC3\xA9\xE6\xBC\xA2\xF0\x9F\x98\x80" "B");
    CHECK(!ime_text_clamp_utf16(text, 6) && strlen(text) == 11, "six units: all of it");
    CHECK(ime_text_clamp_utf16(text, 5) && strlen(text) == 10, "five: without the B");
    CHECK(ime_text_clamp_utf16(text, 4) && strlen(text) == 6,
          "four: not half of the character that is two");
    CHECK(ime_text_clamp_utf16(text, 2) && strcmp(text, "A\xC3\xA9") == 0, "two");
    CHECK(ime_text_clamp_utf16(text, 0) && text[0] == '\0' && !ime_text_clamp_utf16(text, 0) &&
              !ime_text_clamp_utf16(NULL, 3),
          "none; and nothing to cut");
}

int main(void) {
    check_layers();
    check_text();
    check_edit();
    if (failures) {
        printf("ime_support: %d failed\n", failures);
        return 1;
    }
    printf("ime_support: all passed\n");
    return 0;
}
