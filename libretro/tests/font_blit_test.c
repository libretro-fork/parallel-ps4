/* A glyph into a game's surface, read straight from the face's bitmap, against how it was done
 * before: the bitmap copied to a buffer of its own first, then every pixel tested against the
 * glyph's edges on its way in. The same bytes in the surface, none outside it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/libraries/font/font_blit.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("font_blit: FAILED: %s\n", what);                                           \
            failures++;                                                                        \
        }                                                                                      \
    } while (0)

static int imax(int a, int b) {
    return a > b ? a : b;
}
static int imin(int a, int b) {
    return a < b ? a : b;
}

/* As RenderCharGlyphImage did it */
static void reference(unsigned char* dst_base, size_t width_byte, int bpp,
                      const unsigned char* face_bitmap, int glyph_w, int glyph_h, int dest_x,
                      int dest_y, int start_row, int end_row, int start_col, int end_col) {
    unsigned char* glyph_bitmap = (unsigned char*)malloc((size_t)glyph_w * (size_t)glyph_h + 1);
    int row, col;
    for (row = 0; row < glyph_h; row++)
        memcpy(glyph_bitmap + (size_t)row * glyph_w, face_bitmap + (size_t)row * glyph_w,
               (size_t)glyph_w);
    for (row = start_row; row < end_row; row++) {
        const int src_y = row - dest_y;
        const unsigned char* src_row;
        unsigned char* dst_row;
        if (src_y < 0 || src_y >= glyph_h)
            continue;
        src_row = glyph_bitmap + (size_t)src_y * glyph_w;
        dst_row = dst_base + (size_t)row * width_byte;
        for (col = start_col; col < end_col; col++) {
            const int src_x = col - dest_x;
            unsigned char cov;
            unsigned char* dst;
            if (src_x < 0 || src_x >= glyph_w)
                continue;
            cov = src_row[src_x];
            dst = dst_row + (size_t)col * bpp;
            if (bpp == 1) {
                dst[0] = cov;
            } else {
                dst[0] = cov;
                dst[1] = cov;
                dst[2] = cov;
                dst[3] = cov;
            }
        }
    }
    free(glyph_bitmap);
}

static unsigned seed = 2026;
static int rnd(int n) {
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (unsigned)n);
}

int main(void) {
    int i, whole = 0, cut = 0, none = 0;
    for (i = 0; i < 40000; i++) {
        const int bpp = rnd(2) ? 4 : 1;
        const int surface_w = 1 + rnd(40), surface_h = 1 + rnd(24);
        /* Rows wider than the pixels need, as a game's surface can be */
        const size_t width_byte = (size_t)surface_w * bpp + (size_t)rnd(5);
        const size_t bytes = width_byte * surface_h;
        /* Half of them small glyphs near a surface with no clip rectangle of the game's, the
         * other half anything anywhere */
        const int near = rnd(2);
        const int glyph_w = near ? 1 + rnd(8) : rnd(20), glyph_h = near ? 1 + rnd(8) : rnd(20);
        const int dest_x = near ? rnd(surface_w + 8) - 8 : rnd(70) - 25;
        const int dest_y = near ? rnd(surface_h + 8) - 8 : rnd(50) - 20;
        /* The game's clip rectangle, cut to the surface as the caller cuts it */
        const int cx0 = near ? 0 : rnd(surface_w + 1), cy0 = near ? 0 : rnd(surface_h + 1);
        const int cx1 = near ? surface_w : imin(surface_w, cx0 + rnd(surface_w + 2));
        const int cy1 = near ? surface_h : imin(surface_h, cy0 + rnd(surface_h + 2));
        const int start_row = imax(dest_y, cy0), end_row = imin(dest_y + glyph_h, cy1);
        const int start_col = imax(dest_x, cx0), end_col = imin(dest_x + glyph_w, cx1);
        /* Each surface and the bitmap in an allocation of exactly its size: the sanitizer sees
         * a byte read or written outside */
        unsigned char* a = (unsigned char*)malloc(bytes);
        unsigned char* b = (unsigned char*)malloc(bytes);
        unsigned char* cov = (unsigned char*)malloc((size_t)glyph_w * glyph_h + 1);
        size_t k;
        for (k = 0; k < bytes; k++)
            a[k] = b[k] = (unsigned char)(0x80 | rnd(64));
        for (k = 0; k < (size_t)glyph_w * glyph_h; k++)
            cov[k] = (unsigned char)rnd(64);
        if (glyph_w > 0 && glyph_h > 0 && cx1 > cx0 && cy1 > cy0) {
            reference(a, width_byte, bpp, cov, glyph_w, glyph_h, dest_x, dest_y, start_row,
                      end_row, start_col, end_col);
            font_glyph_blit(b, width_byte, bpp, cov, glyph_w, dest_x, dest_y, start_row,
                            end_row, start_col, end_col);
            if (end_row <= start_row || end_col <= start_col)
                none++;
            else if (end_row - start_row == glyph_h && end_col - start_col == glyph_w)
                whole++;
            else
                cut++;
        }
        if (memcmp(a, b, bytes) != 0) {
            CHECK(0, "the surface as it was written before");
            free(a);
            free(b);
            free(cov);
            break;
        }
        free(a);
        free(b);
        free(cov);
    }
    CHECK(whole > 500 && cut > 500 && none > 500,
          "glyphs wholly in the surface, cut by its edges and outside it were all tried");
    if (failures) {
        printf("font_blit: %d failed\n", failures);
        return 1;
    }
    printf("font_blit: all passed (%d glyphs whole, %d cut, %d outside)\n", whole, cut, none);
    return 0;
}
