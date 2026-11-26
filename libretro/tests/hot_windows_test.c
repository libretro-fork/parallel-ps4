/* The table of windows a game has lately read back (hot_windows.c): what it remembers, in what
 * order it gives them, and what it forgets. Checked against a plain model. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_core/buffer_cache/hot_windows.h"

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #c);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static unsigned long long rs = 0x9E3779B97F4A7C15ULL;
static unsigned rnd(void) {
    rs ^= rs << 13;
    rs ^= rs >> 7;
    rs ^= rs << 17;
    return (unsigned)(rs >> 11);
}

/* The model: every window ever noted, with when */
#define MODEL_MAX 512
static uint64_t m_addr[MODEL_MAX];
static uint32_t m_seen[MODEL_MAX];
static int m_live[MODEL_MAX];
static uint32_t m_seq;

int main(void) {
    static hot_windows_t t;
    uint64_t out[HOT_WINDOWS_MAX + 4];
    unsigned i, j, n;
    long round, collects = 0, given = 0;

    hot_windows_init(&t);
    CHECK(hot_windows_collect(&t, 0, out, HOT_WINDOWS_MAX) == 0);

    hot_windows_note(&t, 0x1000);
    hot_windows_note(&t, 0x2000);
    hot_windows_note(&t, 0x3000);
    hot_windows_note(&t, 0x1000);
    n = hot_windows_collect(&t, 0x3000, out, 8);
    CHECK(n == 2 && out[0] == 0x1000 && out[1] == 0x2000);
    n = hot_windows_collect(&t, 0, out, 1);
    CHECK(n == 1 && out[0] == 0x1000);
    CHECK(hot_windows_collect(&t, 0, out, 0) == 0);
    hot_windows_drop(&t, 0x1000);
    hot_windows_drop(&t, 0x9000);
    n = hot_windows_collect(&t, 0, out, 8);
    CHECK(n == 2 && out[0] == 0x3000 && out[1] == 0x2000);

    /* A window not read for HOT_WINDOWS_AGE read backs is forgotten, one read just within is not */
    hot_windows_init(&t);
    hot_windows_note(&t, 0xA000);
    hot_windows_note(&t, 0xB000);
    for (i = 0; i < HOT_WINDOWS_AGE; ++i) {
        hot_windows_note(&t, 0xC000);
    }
    n = hot_windows_collect(&t, 0xC000, out, 8);
    CHECK(n == 1 && out[0] == 0xB000);
    CHECK(t.count == 2);

    /* Against the model, with fewer windows than the table holds: nothing is pushed out, so
     * what is given is exactly the windows noted within the age, youngest first */
    hot_windows_init(&t);
    for (round = 0; round < 200000; ++round) {
        const unsigned op = rnd() % 16;
        const uint64_t w = (uint64_t)(rnd() % 48) << 19;
        if (op < 12) {
            hot_windows_note(&t, w);
            ++m_seq;
            m_addr[w >> 19] = w;
            m_seen[w >> 19] = m_seq;
            m_live[w >> 19] = 1;
        } else if (op == 12) {
            /* a long quiet stretch on one window */
            unsigned k, quiet = rnd() % 3 == 0 ? HOT_WINDOWS_AGE + rnd() % 64 : rnd() % 512;
            for (k = 0; k < quiet; ++k) {
                hot_windows_note(&t, w);
            }
            m_seq += quiet;
            if (quiet) {
                m_addr[w >> 19] = w;
                m_seen[w >> 19] = m_seq;
                m_live[w >> 19] = 1;
            }
        } else if (op == 13) {
            hot_windows_drop(&t, w);
            m_live[w >> 19] = 0;
        } else {
            const unsigned max = rnd() % (HOT_WINDOWS_MAX + 1);
            const uint64_t except = rnd() % 2 ? w : ~(uint64_t)0;
            unsigned want = 0;
            uint32_t last_age = 0;
            n = hot_windows_collect(&t, except, out, max);
            for (i = 0; i < 48; ++i) {
                if (m_live[i] && m_seq - m_seen[i] > HOT_WINDOWS_AGE) {
                    m_live[i] = 0;
                }
                want += m_live[i] && m_addr[i] != except;
            }
            CHECK(n == (want < max ? want : max));
            for (i = 0; i < n; ++i) {
                const unsigned slot = (unsigned)(out[i] >> 19);
                uint32_t age;
                CHECK(slot < 48 && m_live[slot] && out[i] != except);
                age = m_seq - m_seen[slot];
                CHECK(i == 0 || age > last_age);
                last_age = age;
                for (j = 0; j < i; ++j) {
                    CHECK(out[j] != out[i]);
                }
            }
            /* none left out is younger than the oldest given */
            if (n == max && n > 0) {
                for (i = 0; i < 48; ++i) {
                    unsigned in = 0;
                    for (j = 0; j < n; ++j) {
                        in |= out[j] == m_addr[i];
                    }
                    CHECK(!m_live[i] || m_addr[i] == except || in ||
                          m_seq - m_seen[i] > last_age);
                }
            }
            ++collects;
            given += n;
        }
        CHECK(t.count <= HOT_WINDOWS_MAX);
    }

    /* More windows than the table holds: the one read longest ago makes room */
    hot_windows_init(&t);
    for (i = 0; i < HOT_WINDOWS_MAX + 10; ++i) {
        hot_windows_note(&t, (uint64_t)(i + 1) << 19);
    }
    CHECK(t.count == HOT_WINDOWS_MAX);
    n = hot_windows_collect(&t, 0, out, HOT_WINDOWS_MAX);
    CHECK(n == HOT_WINDOWS_MAX);
    CHECK(out[0] == (uint64_t)(HOT_WINDOWS_MAX + 10) << 19);
    CHECK(out[n - 1] == (uint64_t)11 << 19);

    printf("hot_windows_test: %ld collects gave %ld windows, all as the model has them\n",
           collects, given);
    return 0;
}
