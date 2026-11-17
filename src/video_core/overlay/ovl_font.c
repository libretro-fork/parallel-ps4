/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Text: glyphs rasterized into one atlas the first time they are drawn at a size, made, laid
 * out and placed as ImGui (1.92's stb_truetype loader) does: the same rasterizer (ovl_ttf.c),
 * pixel-height scale, two-times horizontal oversampling with stb's box prefilter, the same
 * ascent rounding, advances, word wrapping and pixel snapping. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ovl_internal.h"

#define MAX_FACES 8
#define MAX_BAKED 24
#define OVERSAMPLE_H 2
#define GLYPH_PADDING 1
#define FALLBACK_A 0xFFFDu
#define FALLBACK_B '?'
/* Every size's glyphs, in one table made at the start: a power of two, kept under 3/4 full */
#define GLYPH_POOL 16384u
/* The largest glyph bitmap made (oversampled); larger ones are spaced but not drawn */
#define MAX_GLYPH_W 320
#define MAX_GLYPH_H 256

typedef struct ovl_face {
    int ttf;            /* ovl_ttf's handle */
    float scale_factor; /* to pixels for a pixel height of 1 (stbtt_ScaleForPixelHeight) */
    int ascent, descent;
} ovl_face_t;

typedef struct ovl_glyph {
    uint32_t cp;           /* 0 for an empty slot */
    uint32_t baked;        /* the size it was made at (ovl_baked_t.id) */
    unsigned char visible; /* 0 spacing only, 1 drawn, 2 known missing */
    float advance;
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
} ovl_glyph_t;

/* A font at one size, with the glyphs made at it */
typedef struct ovl_baked {
    float size;
    float ascent, descent;
    uint32_t id;       /* never 0, never reused */
    uint32_t fallback; /* the character drawn for one no face has, 0 until found */
} ovl_baked_t;

struct ovl_font {
    ovl_face_t faces[MAX_FACES];
    int face_count;
    float size;
    ovl_baked_t baked[MAX_BAKED];
    int baked_count;
    struct ovl_font* next;
};

/* The atlas: R8, with a shelf packer and the rectangle changed since last taken */
static struct {
    uint8_t* pixels;
    int shelf_x, shelf_y, shelf_h;
    int dirty[4];        /* x0, y0, x1, y1; x1 <= x0 when clean */
    int full;            /* a glyph did not fit: everything goes at the next frame */
    int cleared;         /* emptied since last taken: the renderer clears its copy first */
    uint8_t* bitmap;     /* MAX_GLYPH_W x MAX_GLYPH_H */
    ovl_glyph_t* glyphs; /* GLYPH_POOL */
    unsigned glyph_count;
    uint32_t next_baked;
    ovl_glyph_t spare; /* a glyph made while the table is full, for this frame only */
    ovl_font_t* fonts;
} atlas;

static void mark_dirty(int x, int y, int w, int h) {
    if (atlas.dirty[2] <= atlas.dirty[0]) {
        atlas.dirty[0] = x;
        atlas.dirty[1] = y;
        atlas.dirty[2] = x + w;
        atlas.dirty[3] = y + h;
        return;
    }
    if (x < atlas.dirty[0])
        atlas.dirty[0] = x;
    if (y < atlas.dirty[1])
        atlas.dirty[1] = y;
    if (x + w > atlas.dirty[2])
        atlas.dirty[2] = x + w;
    if (y + h > atlas.dirty[3])
        atlas.dirty[3] = y + h;
}

/* Where the baked lines go: right of the white block */
#define LINES_X 4
#define LINES_W (OVL_TEX_LINES_MAX + 2)
#define LINES_H (OVL_TEX_LINES_MAX + 1)

/* Places a w x h block of coverage at (x, y), marked as placed (ovl.h: ImGui's RGBA atlas is
 * white where it placed something, black elsewhere, and that shows in what is sampled at the
 * edges) */
static void atlas_put(int x, int y, int w, int h, const uint8_t* cov, int stride) {
    int row, i;
    for (row = 0; row < h; row++) {
        uint8_t* dst = atlas.pixels + ((size_t)(y + row) * OVL_ATLAS_SIZE + (size_t)x) * 2;
        const uint8_t* src = cov + (size_t)row * (size_t)stride;
        for (i = 0; i < w; i++) {
            dst[i * 2 + 0] = src[i];
            dst[i * 2 + 1] = 0xFF;
        }
    }
}

/* The white texel shapes are drawn with (a 3x3 white block at the corner, sampled at its
 * middle), then ImGui's baked lines (ImFontAtlasBuildRenderLinesTexData: a row per width, that
 * many solid texels between transparent ones), then the glyphs' shelves */
static void atlas_clear(void) {
    static const uint8_t white[3] = {0xFF, 0xFF, 0xFF};
    uint8_t row[LINES_W];
    int y;
    memset(atlas.pixels, 0, (size_t)OVL_ATLAS_SIZE * OVL_ATLAS_SIZE * 2);
    atlas_put(0, 0, 3, 3, white, 0);
    for (y = 0; y <= OVL_TEX_LINES_MAX; y++) {
        const int pad_left = (LINES_W - y) / 2;
        memset(row, 0, sizeof(row));
        memset(row + pad_left, 0xFF, (size_t)y);
        atlas_put(LINES_X, y, LINES_W, 1, row, LINES_W);
    }
    atlas.shelf_x = LINES_X + LINES_W + GLYPH_PADDING;
    atlas.shelf_y = 0;
    atlas.shelf_h = LINES_H + GLYPH_PADDING;
    /* The renderer clears its copy, and uploads only what is in it */
    atlas.cleared = 1;
    atlas.dirty[0] = atlas.dirty[1] = atlas.dirty[2] = atlas.dirty[3] = 0;
    mark_dirty(0, 0, LINES_X + LINES_W, LINES_H);
}

static void lines_uv_init(void) {
    const float scale = 1.0f / OVL_ATLAS_SIZE;
    int n;
    for (n = 0; n <= OVL_TEX_LINES_MAX; n++) {
        const int pad_left = (LINES_W - n) / 2;
        const float v0 = (float)n * scale, v1 = (float)(n + 1) * scale;
        ovl_shared.tex_uv_lines[n].x = (float)(LINES_X + pad_left - 1) * scale;
        ovl_shared.tex_uv_lines[n].y = (v0 + v1) * 0.5f;
        ovl_shared.tex_uv_lines[n].z = (float)(LINES_X + pad_left + n + 1) * scale;
        ovl_shared.tex_uv_lines[n].w = (v0 + v1) * 0.5f;
    }
}

int ovl_atlas_init(void) {
    if (atlas.pixels)
        return 1;
    atlas.pixels = (uint8_t*)malloc((size_t)OVL_ATLAS_SIZE * OVL_ATLAS_SIZE * 2);
    atlas.bitmap = (uint8_t*)malloc((size_t)MAX_GLYPH_W * MAX_GLYPH_H);
    atlas.glyphs = (ovl_glyph_t*)calloc(GLYPH_POOL, sizeof(ovl_glyph_t));
    if (!atlas.pixels || !atlas.bitmap || !atlas.glyphs || !ovl_ttf_init()) {
        free(atlas.pixels);
        free(atlas.bitmap);
        free(atlas.glyphs);
        memset(&atlas, 0, sizeof(atlas));
        return 0;
    }
    atlas.next_baked = 1;
    atlas_clear();
    ovl_draw_shared_init(1.5f / OVL_ATLAS_SIZE, 1.5f / OVL_ATLAS_SIZE);
    lines_uv_init();
    return 1;
}

void ovl_atlas_free(void) {
    ovl_font_t* f = atlas.fonts;
    while (f) {
        ovl_font_t* next = f->next;
        free(f);
        f = next;
    }
    free(atlas.pixels);
    ovl_ttf_free();
    free(atlas.bitmap);
    free(atlas.glyphs);
    memset(&atlas, 0, sizeof(atlas));
}

const uint8_t* ovl_atlas_pixels(void) {
    return atlas.pixels;
}

int ovl_atlas_take_dirty(int rect[4], int* cleared) {
    *cleared = atlas.cleared;
    atlas.cleared = 0;
    if (atlas.dirty[2] <= atlas.dirty[0]) {
        rect[0] = rect[1] = rect[2] = rect[3] = 0;
        return *cleared;
    }
    rect[0] = atlas.dirty[0];
    rect[1] = atlas.dirty[1];
    rect[2] = atlas.dirty[2] - atlas.dirty[0];
    rect[3] = atlas.dirty[3] - atlas.dirty[1];
    atlas.dirty[0] = atlas.dirty[1] = atlas.dirty[2] = atlas.dirty[3] = 0;
    return 1;
}

void ovl_atlas_mark_dirty(int x, int y, int w, int h) {
    if (w > 0 && h > 0)
        mark_dirty(x, y, w, h);
}

/* Where a w x h glyph goes, or 0 when the atlas is full */
static int atlas_pack(int w, int h, int* x, int* y) {
    w += GLYPH_PADDING;
    h += GLYPH_PADDING;
    if (w > OVL_ATLAS_SIZE || h > OVL_ATLAS_SIZE)
        return 0;
    if (atlas.shelf_x + w > OVL_ATLAS_SIZE) {
        atlas.shelf_y += atlas.shelf_h;
        atlas.shelf_x = 0;
        atlas.shelf_h = 0;
    }
    if (atlas.shelf_y + h > OVL_ATLAS_SIZE)
        return 0;
    *x = atlas.shelf_x;
    *y = atlas.shelf_y;
    atlas.shelf_x += w;
    if (h > atlas.shelf_h)
        atlas.shelf_h = h;
    return 1;
}

/* Starts again with an empty atlas: every glyph is made again as it is next drawn */
void ovl_atlas_begin_frame(void) {
    ovl_font_t* f;
    if (!atlas.full)
        return;
    atlas.full = 0;
    for (f = atlas.fonts; f; f = f->next) {
        int i;
        for (i = 0; i < f->baked_count; i++)
            f->baked[i].fallback = 0;
    }
    memset(atlas.glyphs, 0, sizeof(ovl_glyph_t) * GLYPH_POOL);
    atlas.glyph_count = 0;
    atlas_clear();
}

/* ------------------------------------------------------------------------------------------ */

ovl_font_t* ovl_font_add(const ovl_face_desc_t* faces, int count, float size) {
    ovl_font_t* font;
    int i;
    if (!ovl_atlas_init() || count <= 0)
        return NULL;
    font = (ovl_font_t*)calloc(1, sizeof(*font));
    if (!font)
        return NULL;
    font->size = size;
    for (i = 0; i < count && font->face_count < MAX_FACES; i++) {
        ovl_face_t* face = &font->faces[font->face_count];
        int line_gap;
        face->ttf = ovl_ttf_open(faces[i].data, faces[i].size, faces[i].index);
        if (face->ttf < 0)
            continue;
        ovl_ttf_vmetrics(face->ttf, &face->ascent, &face->descent, &line_gap);
        face->scale_factor = ovl_ttf_scale_for_pixel_height(face->ttf, 1.0f);
        font->face_count++;
    }
    if (font->face_count == 0) {
        free(font);
        return NULL;
    }
    font->next = atlas.fonts;
    atlas.fonts = font;
    return font;
}

float ovl_font_size(const ovl_font_t* font) {
    return font->size;
}

static ovl_baked_t* baked_for(ovl_font_t* font, float size) {
    ovl_baked_t* b;
    const ovl_face_t* primary = &font->faces[0];
    int i;
    for (i = 0; i < font->baked_count; i++)
        if (font->baked[i].size == size)
            return &font->baked[i];
    if (font->baked_count == MAX_BAKED) {
        /* The least likely to be wanted: the oldest (its glyphs stay in the table, unfound,
         * until the atlas starts again) */
        memmove(&font->baked[0], &font->baked[1], sizeof(ovl_baked_t) * (MAX_BAKED - 1));
        font->baked_count--;
    }
    b = &font->baked[font->baked_count++];
    memset(b, 0, sizeof(*b));
    b->size = size;
    b->id = atlas.next_baked++;
    if (atlas.next_baked == 0)
        atlas.next_baked = 1;
    b->ascent = (float)ceil((float)primary->ascent * primary->scale_factor * size);
    b->descent = (float)floor((float)primary->descent * primary->scale_factor * size);
    return b;
}

static ovl_glyph_t* table_slot(const ovl_baked_t* b, uint32_t cp) {
    unsigned i = ((cp * 2654435761u) ^ (b->id * 0x9E3779B9u)) & (GLYPH_POOL - 1);
    while (atlas.glyphs[i].cp != 0 && (atlas.glyphs[i].cp != cp || atlas.glyphs[i].baked != b->id))
        i = (i + 1) & (GLYPH_POOL - 1);
    return &atlas.glyphs[i];
}

/* Makes @cp's glyph at @b's size from the first face that has it, as ImGui's
 * ImGui_ImplStbTrueType_FontBakedLoadGlyph; 0 when none has it */
static int make_glyph(ovl_font_t* font, ovl_baked_t* b, uint32_t cp, ovl_glyph_t* out) {
    int f;
    for (f = 0; f < font->face_count; f++) {
        const ovl_face_t* face = &font->faces[f];
        const int gi = ovl_ttf_find_glyph(face->ttf, cp);
        const float layout = face->scale_factor * b->size;
        const float sx = layout * OVERSAMPLE_H, sy = layout;
        int advance, lsb, x0, y0, x1, y1;
        if (gi == 0)
            continue;
        ovl_ttf_bitmap_box(face->ttf, gi, sx, sy, &x0, &y0, &x1, &y1);
        ovl_ttf_hmetrics(face->ttf, gi, &advance, &lsb);
        memset(out, 0, sizeof(*out));
        out->cp = cp;
        out->advance = (float)advance * layout;
        if (x0 != x1 && y0 != y1) {
            const int w = x1 - x0 + OVERSAMPLE_H - 1, h = y1 - y0;
            const float ascent = (float)floor(b->ascent + 0.5f);
            float sub_x;
            int ax, ay;
            if (w > MAX_GLYPH_W || h > MAX_GLYPH_H)
                return 1; /* too big to draw: spacing only */
            if (!atlas_pack(w, h, &ax, &ay)) {
                atlas.full = 1;
                return 1; /* spacing only, this frame */
            }
            ovl_ttf_render(face->ttf, gi, atlas.bitmap, w, h, w, sx, sy, OVERSAMPLE_H, &sub_x);
            atlas_put(ax, ay, w, h, atlas.bitmap, w);
            mark_dirty(ax, ay, w, h);
            out->visible = 1;
            out->x0 = (float)x0 / OVERSAMPLE_H + sub_x;
            out->y0 = (float)y0 + ascent;
            out->x1 = (float)(x0 + w) / OVERSAMPLE_H + sub_x;
            out->y1 = (float)(y0 + h) + ascent;
            out->u0 = (float)ax / OVL_ATLAS_SIZE;
            out->v0 = (float)ay / OVL_ATLAS_SIZE;
            out->u1 = (float)(ax + w) / OVL_ATLAS_SIZE;
            out->v1 = (float)(ay + h) / OVL_ATLAS_SIZE;
        }
        return 1;
    }
    return 0;
}

static const ovl_glyph_t* find_glyph(ovl_font_t* font, ovl_baked_t* b, uint32_t cp, int fallback);

static const ovl_glyph_t* fallback_glyph(ovl_font_t* font, ovl_baked_t* b) {
    static const uint32_t candidates[3] = {FALLBACK_A, FALLBACK_B, ' '};
    int i;
    if (b->fallback)
        return find_glyph(font, b, b->fallback, 0);
    for (i = 0; i < 3; i++) {
        const ovl_glyph_t* g = find_glyph(font, b, candidates[i], 0);
        if (g) {
            b->fallback = candidates[i];
            return g;
        }
    }
    return NULL;
}

static const ovl_glyph_t* find_glyph(ovl_font_t* font, ovl_baked_t* b, uint32_t cp, int fallback) {
    ovl_glyph_t* slot;
    ovl_glyph_t made;
    if (cp == 0)
        cp = FALLBACK_A;
    slot = table_slot(b, cp);
    if (slot->cp == cp)
        return slot->visible == 2 ? (fallback ? fallback_glyph(font, b) : NULL) : slot;
    if ((atlas.glyph_count + 1) * 4 > GLYPH_POOL * 3) {
        /* Full: made for this frame only, and everything starts again at the next */
        atlas.full = 1;
        if (!make_glyph(font, b, cp, &atlas.spare))
            return fallback ? fallback_glyph(font, b) : NULL;
        return &atlas.spare;
    }
    if (!make_glyph(font, b, cp, &made)) {
        /* Known to be missing: noted so it is not looked for again */
        memset(slot, 0, sizeof(*slot));
        slot->cp = cp;
        slot->baked = b->id;
        slot->visible = 2;
        atlas.glyph_count++;
        return fallback ? fallback_glyph(font, b) : NULL;
    }
    made.baked = b->id;
    *slot = made;
    atlas.glyph_count++;
    return slot;
}

/* ------------------------------------------------------------------------------------------ */
/* UTF-8 and layout, as ImGui's ImTextCharFromUtf8, CalcTextSize and CalcWordWrapPosition */

static int utf8_decode(uint32_t* out, const char* s, const char* end) {
    static const unsigned char lengths[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                              0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0};
    static const int masks[] = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
    static const uint32_t mins[] = {0x400000, 0, 0x80, 0x800, 0x10000};
    static const int shiftc[] = {0, 18, 12, 6, 0};
    static const int shifte[] = {0, 6, 4, 2, 0};
    const unsigned char* p = (const unsigned char*)s;
    int len = lengths[p[0] >> 3];
    int wanted = len + (len ? 0 : 1);
    unsigned char b[4];
    int e;
    if (end == NULL)
        end = s + wanted;
    b[0] = (end - s) > 0 ? p[0] : 0;
    b[1] = (end - s) > 1 ? p[1] : 0;
    b[2] = (end - s) > 2 ? p[2] : 0;
    b[3] = (end - s) > 3 ? p[3] : 0;
    *out = (uint32_t)(b[0] & masks[len]) << 18;
    *out |= (uint32_t)(b[1] & 0x3f) << 12;
    *out |= (uint32_t)(b[2] & 0x3f) << 6;
    *out |= (uint32_t)(b[3] & 0x3f) << 0;
    *out >>= shiftc[len];
    e = (*out < mins[len]) << 6;
    e |= ((*out >> 11) == 0x1b) << 7;
    e |= (*out > 0x10FFFF) << 8;
    e |= (b[1] & 0xc0) >> 2;
    e |= (b[2] & 0xc0) >> 4;
    e |= (b[3]) >> 6;
    e ^= 0x2a;
    e >>= shifte[len];
    if (e) {
        wanted = wanted < (int)(end - s) ? wanted : (int)(end - s);
        wanted = wanted > 0 ? wanted : 1;
        *out = FALLBACK_A;
    }
    return wanted;
}

#define CLASS_OTHER 0
#define CLASS_BLANK 1
#define CLASS_PUNCT 2

static int char_class(uint32_t c) {
    if (c == ' ' || c == '\t')
        return CLASS_BLANK;
    if (c == '.' || c == ',' || c == ';' || c == '!' || c == '?' || c == '"')
        return CLASS_PUNCT;
    if (c >= 0x3000 && c < 0x3010) {
        if (c == 0x3000)
            return CLASS_BLANK;
        if (c == 0x3001 || c == 0x3002)
            return CLASS_PUNCT;
    }
    return CLASS_OTHER;
}

static float advance_of(ovl_font_t* font, ovl_baked_t* b, uint32_t c) {
    const ovl_glyph_t* g = find_glyph(font, b, c, 1);
    return g ? g->advance : 0.0f;
}

static const char* word_wrap_position(ovl_font_t* font, ovl_baked_t* b, const char* text,
                                      const char* text_end, float wrap_width) {
    float line_width = 0.0f, blank_width = 0.0f, span_width = 0.0f;
    const char* s = text;
    const char* span_end = s;
    int prev_type = CLASS_OTHER;
    while (s < text_end) {
        uint32_t c = (unsigned char)*s;
        const char* next_s;
        float w;
        int type;
        if (c < 0x80)
            next_s = s + 1;
        else
            next_s = s + utf8_decode(&c, s, text_end);
        if (c < 32) {
            if (c == '\n')
                return s;
            if (c == '\r') {
                s = next_s;
                continue;
            }
        }
        w = advance_of(font, b, c);
        type = char_class(c);
        if (type == CLASS_BLANK) {
            if (prev_type != CLASS_BLANK) {
                span_end = s;
                line_width += span_width;
                span_width = 0.0f;
            }
            blank_width += w;
        } else {
            if (prev_type == CLASS_PUNCT && type != CLASS_PUNCT && !(c >= '0' && c <= '9')) {
                span_end = s;
                line_width += span_width + blank_width;
                span_width = blank_width = 0.0f;
            }
            span_width += w;
        }
        if (span_width + blank_width + line_width > wrap_width) {
            if (span_width + blank_width > wrap_width)
                break;
            return span_end;
        }
        prev_type = type;
        s = next_s;
    }
    if (s == text && text < text_end) {
        uint32_t c;
        return s + utf8_decode(&c, s, text_end);
    }
    return s;
}

static const char* wrap_next_line_start(const char* s, const char* end) {
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    if (s < end && *s == '\n')
        s++;
    return s;
}

ovl_vec2_t ovl_font_calc_text_size(ovl_font_t* font, float size, float wrap_width, const char* text,
                                   const char* end) {
    ovl_vec2_t r;
    ovl_baked_t* b = baked_for(font, size);
    float line_width = 0.0f;
    const char* s = text;
    const char* eol = NULL;
    const int wrap = wrap_width > 0.0f;
    r.x = r.y = 0.0f;
    if (!end)
        end = text + strlen(text);
    while (s < end) {
        uint32_t c;
        if (wrap) {
            if (!eol)
                eol = word_wrap_position(font, b, s, end, wrap_width - line_width);
            if (s >= eol) {
                if (r.x < line_width)
                    r.x = line_width;
                r.y += size;
                line_width = 0.0f;
                s = wrap_next_line_start(s, end);
                eol = NULL;
                continue;
            }
        }
        c = (unsigned char)*s;
        if (c < 0x80)
            s++;
        else
            s += utf8_decode(&c, s, end);
        if (c == '\n') {
            if (r.x < line_width)
                r.x = line_width;
            r.y += size;
            line_width = 0.0f;
            continue;
        }
        if (c == '\r')
            continue;
        line_width += advance_of(font, b, c);
    }
    if (r.x < line_width)
        r.x = line_width;
    if (line_width > 0.0f || r.y == 0.0f)
        r.y += size;
    return r;
}

float ovl_font_line_height(const ovl_font_t* font, float size) {
    (void)font;
    return size;
}

void ovl_add_text(ovl_draw_list_t* dl, ovl_font_t* font, float size, ovl_vec2_t pos, ovl_col_t col,
                  const char* text, const char* end, float wrap_width) {
    ovl_baked_t* b;
    const float* clip = ovl_draw_list_clip(dl);
    float x, y, origin_x;
    const char* s = text;
    const char* eol = NULL;
    const int wrap = wrap_width > 0.0f;
    const ovl_tex_t was = dl->tex;
    if ((col & 0xFF000000u) == 0 || !font)
        return;
    if (!end)
        end = text + strlen(text);
    b = baked_for(font, size);
    x = (float)(int)pos.x; /* ImGui snaps with IM_TRUNC, toward 0 */
    y = (float)(int)pos.y;
    origin_x = x;
    if (y > clip[3])
        return;
    /* Lines above the clip rectangle are passed over, as ImGui's RenderText */
    while (y + size < clip[1] && s < end) {
        const char* line_end = (const char*)memchr(s, '\n', (size_t)(end - s));
        if (wrap) {
            s = word_wrap_position(font, b, s, line_end ? line_end : end, wrap_width);
            s = wrap_next_line_start(s, end);
        } else {
            s = line_end ? line_end + 1 : end;
        }
        y += size;
    }
    dl->tex = OVL_TEX_ATLAS;
    while (s < end) {
        uint32_t c;
        const ovl_glyph_t* g;
        if (wrap) {
            if (!eol)
                eol = word_wrap_position(font, b, s, end, wrap_width - (x - origin_x));
            if (s >= eol) {
                x = origin_x;
                y += size;
                if (y > clip[3])
                    break;
                eol = NULL;
                s = wrap_next_line_start(s, end);
                continue;
            }
        }
        c = (unsigned char)*s;
        if (c < 0x80)
            s++;
        else
            s += utf8_decode(&c, s, end);
        if (c < 32) {
            if (c == '\n') {
                x = origin_x;
                y += size;
                if (y > clip[3])
                    break;
                continue;
            }
            if (c == '\r')
                continue;
        }
        g = find_glyph(font, b, c, 1);
        if (!g)
            continue;
        if (g->visible == 1) {
            const float x1 = x + g->x0, x2 = x + g->x1;
            const float y1 = y + g->y0, y2 = y + g->y1;
            if (x1 <= clip[2] && x2 >= clip[0]) {
                ovl_vec2_t a, c2, ua, uc;
                a.x = x1;
                a.y = y1;
                c2.x = x2;
                c2.y = y2;
                ua.x = g->u0;
                ua.y = g->v0;
                uc.x = g->u1;
                uc.y = g->v1;
                ovl_prim_rect_uv(dl, a, c2, ua, uc, col);
            }
        }
        x += g->advance;
    }
    dl->tex = was;
}
