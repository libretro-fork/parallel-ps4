// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <gfx/font/rtt.h>

#include "common/types.h"

namespace Libraries::Font::Internal {

// A font face read in place with libretro-common's rtt. Sony's libSceFont works in FreeType's
// numbers, so sizes, metrics and bitmaps here are computed the way FreeType computes them for
// an unhinted scalable glyph: 16.16 scales, 26.6 positions, the same rounding.

// 26.6 and 16.16 arithmetic, rounded as FreeType rounds.
s64 MulFix(s64 a, s64 b);
s64 DivFix(s64 a, s64 b);
s64 MulDiv(s64 a, s64 b, s64 c);
inline s64 PixFloor(s64 x) {
    return x & ~s64{63};
}
inline s64 PixCeil(s64 x) {
    return PixFloor(x + 63);
}
inline s64 PixRound(s64 x) {
    return PixFloor(x + 32);
}

// The OS/2 and header fields the font library reads.
struct FaceOs2 {
    bool present = false;
    u16 fs_selection = 0;
    s16 typo_ascender = 0;
    s16 typo_descender = 0;
    s16 typo_line_gap = 0;
    u16 win_ascent = 0;
    u16 win_descent = 0;
};
struct FaceHhea {
    bool present = false;
    s16 ascender = 0;
    s16 descender = 0;
    s16 line_gap = 0;
    u16 advance_width_max = 0;
    s16 caret_slope_rise = 0;
};
struct FaceVhea {
    bool present = false;
    s16 ascender = 0;
    s16 descender = 0;
    s16 line_gap = 0;
    u16 advance_height_max = 0;
};

struct FaceSizeMetrics {
    u16 x_ppem = 0;
    u16 y_ppem = 0;
    s64 x_scale = 0x10000; // 16.16
    s64 y_scale = 0x10000;
    s64 ascender = 0; // 26.6
    s64 descender = 0;
    s64 height = 0;
    s64 max_advance = 0;
};

struct FaceGlyphMetrics { // 26.6
    s64 width = 0;
    s64 height = 0;
    s64 hori_bearing_x = 0;
    s64 hori_bearing_y = 0;
    s64 hori_advance = 0;
    s64 vert_bearing_x = 0;
    s64 vert_bearing_y = 0;
    s64 vert_advance = 0;
};

struct FaceOutline {
    struct Point {
        s64 x, y; // 26.6
    };
    enum Tag : u8 { Conic = 0, On = 1, Cubic = 2 };
    std::vector<Point> points;
    std::vector<u8> tags;
    std::vector<u16> contours; // last point of each
    void Clear() {
        points.clear();
        tags.clear();
        contours.clear();
    }
};

// What the last LoadGlyph left; its buffers are kept and reused from glyph to glyph.
struct FaceGlyph {
    u32 index = 0;
    FaceGlyphMetrics metrics;
    s64 advance_x = 0; // 26.6
    s64 advance_y = 0;
    s64 linear_hori_advance = 0; // 16.16 pixels
    s64 linear_vert_advance = 0;
    FaceOutline outline;
    // 8-bit coverage, width bytes a row; empty until rendered
    std::vector<u8> bitmap;
    u32 bitmap_width = 0;
    u32 bitmap_rows = 0;
    s32 bitmap_left = 0;
    s32 bitmap_top = 0;
};

enum FaceLoadFlags : u32 {
    FaceLoadDefault = 0,
    FaceLoadVerticalLayout = 1u << 0,
    FaceLoadRender = 1u << 1,
    // Outline and metrics in font units; no bitmap
    FaceLoadNoScale = 1u << 2,
};

struct FaceVector {
    s64 x = 0;
    s64 y = 0;
};

class Face {
public:
    // The bytes are read in place: they must outlive the face. @keep, if set, holds them.
    static Face* Open(const u8* data, std::size_t size, u32 index,
                      std::shared_ptr<const void> keep = nullptr);
    static void Close(Face*& face);

    Face(const Face&) = delete;
    Face& operator=(const Face&) = delete;

    // The face as FreeType describes it
    u16 units_per_em = 0;
    s32 num_glyphs = 0;
    s16 ascender = 0;
    s16 descender = 0;
    s16 height = 0;
    s16 max_advance_width = 0;
    s16 max_advance_height = 0;
    s16 bbox_x_min = 0, bbox_y_min = 0, bbox_x_max = 0, bbox_y_max = 0;
    bool is_cff = false;
    bool has_vertical = false;
    bool has_kerning = false;
    FaceOs2 os2;
    FaceHhea hhea;
    FaceVhea vhea;

    FaceSizeMetrics size;
    FaceGlyph glyph;

    u32 CharIndex(u32 codepoint) const;

    // FreeType's FT_Set_Char_Size: 26.6 sizes at a resolution in dpi; 0 for the other.
    bool SetCharSize(s64 char_width, s64 char_height, u32 horz_dpi, u32 vert_dpi);
    // An offset, 26.6, added to every glyph loaded from here on
    void SetDelta(s64 x, s64 y) {
        delta_x = x;
        delta_y = y;
    }

    bool LoadGlyph(u32 glyph_index, u32 flags);

    // FT_KERNING_DEFAULT: scaled and rounded to whole pixels
    void Kerning(u32 left, u32 right, s64* x, s64* y) const;
    // Unscaled, in font units
    s32 KerningUnits(u32 left, u32 right) const;

    // A table, read in place; nullptr when absent
    const u8* Table(const char* tag, u32* len) const;

private:
    Face() = default;
    void RenderGlyph(u32 glyph_index, s64 load_x_scale, s64 load_y_scale);

    std::shared_ptr<const void> keep_;
    rtt_font_t font_{};
    rtt_scratch_t scratch_{};
    bool integer_ppem_ = false; // TrueType head flag 3: glyphs scale at whole ppems
    s64 delta_x = 0;
    s64 delta_y = 0;
};

using FaceHandle = Face*;
using FaceGlyphPtr = FaceGlyph*;

// The font library's calls report success as 0, as they did through FreeType.
inline int FaceSetCharSizeResult(Face* face, s64 w, s64 h, u32 horz_dpi, u32 vert_dpi) {
    return face && face->SetCharSize(w, h, horz_dpi, vert_dpi) ? 0 : 1;
}
inline int FaceLoadGlyphResult(Face* face, u32 glyph_index, u32 flags) {
    return face && face->LoadGlyph(glyph_index, flags) ? 0 : 1;
}

// libSceFont's own load bits, which are FreeType's: 1 unscaled, 4 render, 0x10 vertical.
inline u32 FaceLoadFlagsFromSony(u32 bits) {
    return ((bits & 0x01) ? FaceLoadNoScale : 0u) | ((bits & 0x04) ? FaceLoadRender : 0u) |
           ((bits & 0x10) ? FaceLoadVerticalLayout : 0u);
}

} // namespace Libraries::Font::Internal
