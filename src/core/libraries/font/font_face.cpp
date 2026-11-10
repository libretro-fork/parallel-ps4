// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libraries/font/font_face.h"

namespace Libraries::Font::Internal {

s64 MulFix(s64 a, s64 b) {
    const s64 ab = a * b;
    // rounds half away from zero
    return (ab + 0x8000 + (ab >> 63)) >> 16;
}

s64 DivFix(s64 a, s64 b) {
    int s = 1;
    if (a < 0) {
        a = -a;
        s = -s;
    }
    if (b < 0) {
        b = -b;
        s = -s;
    }
    const u64 q =
        b > 0 ? ((static_cast<u64>(a) << 16) + (static_cast<u64>(b) >> 1)) / static_cast<u64>(b)
              : 0x7FFFFFFFu;
    return s < 0 ? -static_cast<s64>(q) : static_cast<s64>(q);
}

s64 MulDiv(s64 a, s64 b, s64 c) {
    int s = 1;
    if (a < 0) {
        a = -a;
        s = -s;
    }
    if (b < 0) {
        b = -b;
        s = -s;
    }
    if (c < 0) {
        c = -c;
        s = -s;
    }
    const u64 d = c > 0 ? (static_cast<u64>(a) * static_cast<u64>(b) + (static_cast<u64>(c) >> 1)) /
                              static_cast<u64>(c)
                        : 0x7FFFFFFFu;
    return s < 0 ? -static_cast<s64>(d) : static_cast<s64>(d);
}

namespace {

u16 ReadU16(const u8* p) {
    return static_cast<u16>((p[0] << 8) | p[1]);
}

s16 ReadS16(const u8* p) {
    return static_cast<s16>(ReadU16(p));
}

// Collects an outline in 26.6, scaled point by point.
struct OutlineCollector {
    FaceOutline* out;
    double sx, sy; // font units to 26.6
    s64 xs, ys;    // the same, 16.16, for whole units
    s64 dx, dy;    // added after scaling
    s32 contour_start = -1;
    float first_x = 0, first_y = 0; // unscaled, for the closing point
    float last_x = 0, last_y = 0;

    s64 X(float u) const {
        const double r = std::nearbyint(static_cast<double>(u));
        return (r == u ? MulFix(static_cast<s64>(r), xs)
                       : static_cast<s64>(std::floor(u * sx + 0.5))) +
               dx;
    }
    s64 Y(float u) const {
        const double r = std::nearbyint(static_cast<double>(u));
        return (r == u ? MulFix(static_cast<s64>(r), ys)
                       : static_cast<s64>(std::floor(u * sy + 0.5))) +
               dy;
    }
    void Add(float x, float y, u8 tag) {
        out->points.push_back({X(x), Y(y)});
        out->tags.push_back(tag);
        last_x = x;
        last_y = y;
    }
    void EndContour() {
        if (contour_start < 0) {
            return;
        }
        const s32 n = static_cast<s32>(out->points.size());
        // A last point back on the first closes the contour: it is dropped
        if (n - contour_start > 1 && out->tags[n - 1] == FaceOutline::On && last_x == first_x &&
            last_y == first_y) {
            out->points.pop_back();
            out->tags.pop_back();
        }
        out->contours.push_back(static_cast<u16>(out->points.size() - 1));
        contour_start = -1;
    }

    static void Move(void* user, float x, float y) {
        auto* c = static_cast<OutlineCollector*>(user);
        c->EndContour();
        c->contour_start = static_cast<s32>(c->out->points.size());
        c->first_x = x;
        c->first_y = y;
        c->Add(x, y, FaceOutline::On);
    }
    static void Line(void* user, float x, float y) {
        static_cast<OutlineCollector*>(user)->Add(x, y, FaceOutline::On);
    }
    static void Quad(void* user, float cx, float cy, float x, float y) {
        auto* c = static_cast<OutlineCollector*>(user);
        c->Add(cx, cy, FaceOutline::Conic);
        c->Add(x, y, FaceOutline::On);
    }
    static void Cubic(void* user, float ax, float ay, float bx, float by, float x, float y) {
        auto* c = static_cast<OutlineCollector*>(user);
        c->Add(ax, ay, FaceOutline::Cubic);
        c->Add(bx, by, FaceOutline::Cubic);
        c->Add(x, y, FaceOutline::On);
    }
};

// A TrueType glyph's outline as FreeType builds it: each simple glyph's points scaled one by
// one, then a composite's components transformed and moved in 26.6.
bool LoadTrueTypePoints(const rtt_font_t* font, int gi, s64 xs, s64 ys, FaceOutline& out,
                        int depth) {
    if (depth > 8) {
        return false;
    }
    rtt_component_t comps[64];
    const int n = rtt_glyph_components(font, gi, comps, 64);
    if (n < 0) {
        return false;
    }
    if (n == 0) {
        int ncont = 0;
        const int npts =
            rtt_glyph_points(font, gi, nullptr, nullptr, nullptr, nullptr, 0, 0, &ncont);
        if (npts <= 0) {
            return npts == 0;
        }
        std::vector<s32> px(npts), py(npts);
        std::vector<u8> on(npts);
        std::vector<u16> ends(ncont);
        if (rtt_glyph_points(font, gi, px.data(), py.data(), on.data(), ends.data(), npts, ncont,
                             &ncont) != npts) {
            return false;
        }
        const std::size_t base = out.points.size();
        for (int i = 0; i < npts; i++) {
            out.points.push_back({MulFix(px[i], xs), MulFix(py[i], ys)});
            out.tags.push_back(on[i] ? FaceOutline::On : FaceOutline::Conic);
        }
        for (int c = 0; c < ncont; c++) {
            out.contours.push_back(static_cast<u16>(base + ends[c]));
        }
        return true;
    }
    const std::size_t start_point = out.points.size();
    for (int k = 0; k < n && k < 64; k++) {
        const rtt_component_t& c = comps[k];
        const std::size_t num_base = out.points.size();
        if (!LoadTrueTypePoints(font, c.glyph, xs, ys, out, depth + 1)) {
            return false;
        }
        const bool have_scale = (c.flags & (0x0008 | 0x0040 | 0x0080)) != 0;
        if (have_scale) {
            // 2.14 to 16.16
            const s64 xx = static_cast<s64>(c.xx) * 4, xy = static_cast<s64>(c.xy) * 4;
            const s64 yx = static_cast<s64>(c.yx) * 4, yy = static_cast<s64>(c.yy) * 4;
            for (std::size_t i = num_base; i < out.points.size(); i++) {
                const s64 x = out.points[i].x, y = out.points[i].y;
                out.points[i].x = MulFix(x, xx) + MulFix(y, xy);
                out.points[i].y = MulFix(x, yx) + MulFix(y, yy);
            }
        }
        s64 x, y;
        if (!(c.flags & 0x0002)) {
            // match a point of this component to one placed before it
            const std::size_t pk = start_point + static_cast<std::size_t>(c.arg1);
            const std::size_t pl = num_base + static_cast<std::size_t>(c.arg2);
            if (pk >= num_base || pl >= out.points.size()) {
                return false;
            }
            x = out.points[pk].x - out.points[pl].x;
            y = out.points[pk].y - out.points[pl].y;
        } else {
            x = c.arg1;
            y = c.arg2;
            if (!x && !y) {
                continue;
            }
            if (have_scale && (c.flags & 0x0800)) { // SCALED_COMPONENT_OFFSET
                const double mx = std::hypot(static_cast<double>(c.xx) * 4, c.xy * 4.0);
                const double my = std::hypot(static_cast<double>(c.yy) * 4, c.yx * 4.0);
                x = MulFix(x, static_cast<s64>(mx + 0.5));
                y = MulFix(y, static_cast<s64>(my + 0.5));
            }
            x = MulFix(x, xs);
            y = MulFix(y, ys);
        }
        for (std::size_t i = num_base; i < out.points.size(); i++) {
            out.points[i].x += x;
            out.points[i].y += y;
        }
    }
    return true;
}

void SynthesizeVertical(FaceGlyphMetrics& m, s64 advance) {
    s64 height = m.height;
    // a glyph whose box lies above or below the baseline
    if (m.hori_bearing_y < 0) {
        if (height < m.hori_bearing_y) {
            height = m.hori_bearing_y;
        }
    } else if (m.hori_bearing_y > 0) {
        height -= m.hori_bearing_y;
    }
    if (!advance) {
        advance = height * 12 / 10;
    }
    m.vert_bearing_x = m.hori_bearing_x - m.hori_advance / 2;
    m.vert_bearing_y = (advance - height) / 2;
    m.vert_advance = advance;
}

} // namespace

Face* Face::Open(const u8* data, std::size_t size, u32 index, std::shared_ptr<const void> keep) {
    if (!data || size == 0) {
        return nullptr;
    }
    const int off = rtt_font_offset_for_index(data, size, static_cast<int>(index));
    Face* face = new (std::nothrow) Face();
    if (!face) {
        return nullptr;
    }
    if (off < 0 || !rtt_init(&face->font_, data, size, off)) {
        delete face;
        return nullptr;
    }
    face->keep_ = std::move(keep);
    // FreeType's flattening is finer than the 0.35 pixel default
    rtt_set_flatness(&face->font_, 0.1f);

    auto& f = *face;
    f.units_per_em = static_cast<u16>(face->font_.units_per_em);
    f.num_glyphs = face->font_.num_glyphs;
    f.is_cff = face->font_.is_cff != 0;
    f.has_kerning = face->font_.kern != 0;

    u32 len = 0;
    if (const u8* head = f.Table("head", &len); head && len >= 54) {
        f.integer_ppem_ = !f.is_cff && (ReadU16(head + 16) & 8) != 0;
        f.bbox_x_min = ReadS16(head + 36);
        f.bbox_y_min = ReadS16(head + 38);
        f.bbox_x_max = ReadS16(head + 40);
        f.bbox_y_max = ReadS16(head + 42);
    }
    if (const u8* t = f.Table("hhea", &len); t && len >= 36) {
        f.hhea.present = true;
        f.hhea.ascender = ReadS16(t + 4);
        f.hhea.descender = ReadS16(t + 6);
        f.hhea.line_gap = ReadS16(t + 8);
        f.hhea.advance_width_max = ReadU16(t + 10);
        f.hhea.caret_slope_rise = ReadS16(t + 18);
    }
    if (const u8* t = f.Table("vhea", &len); t && len >= 36) {
        f.vhea.present = true;
        f.vhea.ascender = ReadS16(t + 4);
        f.vhea.descender = ReadS16(t + 6);
        f.vhea.line_gap = ReadS16(t + 8);
        f.vhea.advance_height_max = ReadU16(t + 10);
    }
    f.has_vertical = f.vhea.present && face->font_.num_vmetrics > 0;
    if (const u8* t = f.Table("OS/2", &len); t && len >= 78) {
        f.os2.present = true;
        f.os2.fs_selection = ReadU16(t + 62);
        f.os2.typo_ascender = ReadS16(t + 68);
        f.os2.typo_descender = ReadS16(t + 70);
        f.os2.typo_line_gap = ReadS16(t + 72);
        f.os2.win_ascent = ReadU16(t + 74);
        f.os2.win_descent = ReadU16(t + 76);
    }

    // The face's ascender, descender and height, chosen as FreeType chooses them
    if (f.os2.present && (f.os2.fs_selection & 128)) {
        f.ascender = f.os2.typo_ascender;
        f.descender = f.os2.typo_descender;
        f.height = static_cast<s16>(f.ascender - f.descender + f.os2.typo_line_gap);
    } else {
        f.ascender = f.hhea.ascender;
        f.descender = f.hhea.descender;
        f.height = static_cast<s16>(f.ascender - f.descender + f.hhea.line_gap);
        if (!(f.ascender || f.descender) && f.os2.present) {
            if (f.os2.typo_ascender || f.os2.typo_descender) {
                f.ascender = f.os2.typo_ascender;
                f.descender = f.os2.typo_descender;
                f.height = static_cast<s16>(f.ascender - f.descender + f.os2.typo_line_gap);
            } else {
                f.ascender = static_cast<s16>(f.os2.win_ascent);
                f.descender = static_cast<s16>(-static_cast<s16>(f.os2.win_descent));
                f.height = static_cast<s16>(f.ascender - f.descender);
            }
        }
    }
    f.max_advance_width = static_cast<s16>(f.hhea.advance_width_max);
    f.max_advance_height = f.has_vertical ? static_cast<s16>(f.vhea.advance_height_max) : f.height;
    return face;
}

void Face::Close(Face*& face) {
    if (face) {
        rtt_scratch_free(&face->scratch_);
        delete face;
        face = nullptr;
    }
}

const u8* Face::Table(const char* tag, u32* len) const {
    return rtt_table(&font_, tag, len);
}

u32 Face::CharIndex(u32 codepoint) const {
    const int g = rtt_find_glyph(&font_, codepoint);
    return g > 0 && g < num_glyphs ? static_cast<u32>(g) : 0;
}

bool Face::SetCharSize(s64 char_width, s64 char_height, u32 horz_dpi, u32 vert_dpi) {
    if (!char_width) {
        char_width = char_height;
    } else if (!char_height) {
        char_height = char_width;
    }
    if (!horz_dpi) {
        horz_dpi = vert_dpi;
    } else if (!vert_dpi) {
        vert_dpi = horz_dpi;
    }
    if (char_width < 64) {
        char_width = 64;
    }
    if (char_height < 64) {
        char_height = 64;
    }
    if (!horz_dpi) {
        horz_dpi = vert_dpi = 72;
    }
    if (!units_per_em) {
        return false;
    }
    // A nominal request: the em is the requested size
    const s64 scaled_w = (char_width * static_cast<s64>(horz_dpi) + 36) / 72;
    const s64 scaled_h = (char_height * static_cast<s64>(vert_dpi) + 36) / 72;
    FaceSizeMetrics m;
    m.x_scale = DivFix(scaled_w, units_per_em);
    m.y_scale = DivFix(scaled_h, units_per_em);
    const s64 ppem_w = (scaled_w + 32) >> 6;
    const s64 ppem_h = (scaled_h + 32) >> 6;
    if (ppem_w > 0xFFFF || ppem_h > 0xFFFF) {
        return false;
    }
    m.x_ppem = static_cast<u16>(ppem_w);
    m.y_ppem = static_cast<u16>(ppem_h);
    m.ascender = PixCeil(MulFix(ascender, m.y_scale));
    m.descender = PixFloor(MulFix(descender, m.y_scale));
    m.height = PixRound(MulFix(height, m.y_scale));
    m.max_advance = PixRound(MulFix(max_advance_width, m.x_scale));
    // TrueType refuses a zero ppem
    if (!is_cff && (m.x_ppem == 0 || m.y_ppem == 0)) {
        return false;
    }
    size = m;
    return true;
}

bool Face::LoadGlyph(u32 glyph_index, u32 flags) {
    if (glyph_index >= static_cast<u32>(num_glyphs)) {
        return false;
    }
    FaceGlyph& g = glyph;
    g.index = glyph_index;
    g.metrics = {};
    g.outline.Clear();
    g.bitmap_width = g.bitmap_rows = 0;
    g.bitmap_left = g.bitmap_top = 0;

    // Unhinted, glyphs scale as asked, not at whole ppems; unscaled, a 16.16 one leaves units
    const bool no_scale = (flags & FaceLoadNoScale) != 0;
    const s64 xs = no_scale ? 0x10000 : size.x_scale;
    const s64 ys = no_scale ? 0x10000 : size.y_scale;

    int adv_units = 0;
    int lsb_units = 0;
    rtt_glyph_hmetrics(&font_, static_cast<int>(glyph_index), &adv_units, &lsb_units);

    // The outline, scaled point by point
    s64 shift_x = 0; // TrueType's phantom point: the outline moves to it
    s64 hori_advance = 0;
    if (!is_cff) {
        int hx0 = 0, hy1 = 0;
        if (rtt_glyph_box(&font_, static_cast<int>(glyph_index), &hx0, nullptr, nullptr, &hy1)) {
            const s64 pp1 = MulFix(hx0 - lsb_units, xs);
            const s64 pp2 = MulFix(hx0 - lsb_units + adv_units, xs);
            shift_x = pp1;
            hori_advance = pp2 - pp1;
        } else {
            hori_advance = MulFix(adv_units, xs);
        }
    } else {
        hori_advance = MulFix(adv_units, xs);
    }

    if (is_cff) {
        OutlineCollector collect{&g.outline,
                                 static_cast<double>(xs) / 65536.0,
                                 static_cast<double>(ys) / 65536.0,
                                 xs,
                                 ys,
                                 0,
                                 0};
        const rtt_outline_sink sink{&collect, OutlineCollector::Move, OutlineCollector::Line,
                                    OutlineCollector::Quad, OutlineCollector::Cubic};
        rtt_glyph_outline(&font_, &scratch_, static_cast<int>(glyph_index), &sink);
        collect.EndContour();
    } else if (!LoadTrueTypePoints(&font_, static_cast<int>(glyph_index), xs, ys, g.outline, 0)) {
        g.outline.Clear();
    } else if (shift_x) {
        for (auto& p : g.outline.points) {
            p.x -= shift_x;
        }
    }

    // The control box of every point, 26.6
    s64 bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    bool any = false;
    if (!g.outline.points.empty()) {
        any = true;
        bx0 = bx1 = g.outline.points[0].x;
        by0 = by1 = g.outline.points[0].y;
        for (const auto& p : g.outline.points) {
            bx0 = std::min(bx0, p.x);
            bx1 = std::max(bx1, p.x);
            by0 = std::min(by0, p.y);
            by1 = std::max(by1, p.y);
        }
    }

    FaceGlyphMetrics& m = g.metrics;
    m.hori_bearing_x = bx0;
    m.hori_bearing_y = by1;
    m.width = bx1 - bx0;
    m.height = by1 - by0;
    m.hori_advance = hori_advance;
    g.linear_hori_advance = MulDiv(adv_units, size.x_scale, 64);

    // Vertical metrics: from vmtx, or made up from the font's ascent
    int vadv_units = 0, tsb_units = 0;
    const bool vmtx = has_vertical && rtt_glyph_vmetrics(&font_, static_cast<int>(glyph_index),
                                                         &vadv_units, &tsb_units);
    const s64 made_up =
        os2.present ? os2.typo_ascender - os2.typo_descender : hhea.ascender - hhea.descender;
    if (!is_cff) {
        s64 top = 0;
        s64 advance = 0;
        if (vmtx) {
            int hy1 = 0;
            rtt_glyph_box(&font_, static_cast<int>(glyph_index), nullptr, nullptr, nullptr, &hy1);
            const s64 pp3 = MulFix(hy1 + tsb_units, ys);
            const s64 pp4 = MulFix(hy1 + tsb_units - vadv_units, ys);
            top = static_cast<s16>(DivFix(pp3 - by1, ys));
            advance = pp3 <= pp4 ? 0 : static_cast<u16>(DivFix(pp3 - pp4, ys));
        } else {
            const s64 h = static_cast<s16>(DivFix(by1 - by0, ys));
            advance = made_up;
            top = (advance - h) / 2;
        }
        g.linear_vert_advance = MulDiv(advance, size.y_scale, 64);
        m.vert_bearing_x = m.hori_bearing_x - m.hori_advance / 2;
        m.vert_bearing_y = MulFix(top, ys);
        m.vert_advance = MulFix(advance, ys);
    } else {
        const s64 advance = vmtx ? vadv_units : made_up;
        g.linear_vert_advance = MulDiv(advance, size.y_scale, 64);
        m.vert_advance = MulFix(advance, ys);
        if (vmtx) {
            m.vert_bearing_x = m.hori_bearing_x - m.hori_advance / 2;
            m.vert_bearing_y = MulFix(tsb_units, ys);
        } else if (flags & FaceLoadVerticalLayout) {
            SynthesizeVertical(m, m.vert_advance);
        }
    }

    if (flags & FaceLoadVerticalLayout) {
        g.advance_x = 0;
        g.advance_y = m.vert_advance;
    } else {
        g.advance_x = m.hori_advance;
        g.advance_y = 0;
    }

    // The offset moves the outline, not the metrics
    if (delta_x || delta_y) {
        for (auto& p : g.outline.points) {
            p.x += delta_x;
            p.y += delta_y;
        }
    }

    // The bitmap's box: the control box out to whole pixels
    if (any && !no_scale) {
        const s64 x0 = bx0 + delta_x, x1 = bx1 + delta_x;
        const s64 y0 = by0 + delta_y, y1 = by1 + delta_y;
        const s64 left = x0 >> 6;
        const s64 right = (x1 + 63) >> 6;
        const s64 bottom = y0 >> 6;
        const s64 top = (y1 + 63) >> 6;
        g.bitmap_left = static_cast<s32>(left);
        g.bitmap_top = static_cast<s32>(top);
        if (right - left <= 0xFFFF && top - bottom <= 0xFFFF) {
            g.bitmap_width = static_cast<u32>(std::max<s64>(right - left, 0));
            g.bitmap_rows = static_cast<u32>(std::max<s64>(top - bottom, 0));
        }
    }
    if ((flags & FaceLoadRender) && g.bitmap_width && g.bitmap_rows) {
        RenderGlyph(glyph_index, xs, ys);
    }
    (void)shift_x;
    return true;
}

void Face::RenderGlyph(u32 glyph_index, s64 load_x_scale, s64 load_y_scale) {
    FaceGlyph& g = glyph;
    const std::size_t bytes = static_cast<std::size_t>(g.bitmap_width) * g.bitmap_rows;
    if (g.bitmap.size() < bytes) {
        g.bitmap.resize(bytes);
    }
    // A font unit lands at (x * sx - ox, -y * sy - oy) in the bitmap
    const double sx = static_cast<double>(load_x_scale) / (65536.0 * 64.0);
    const double sy = static_cast<double>(load_y_scale) / (65536.0 * 64.0);
    int adv = 0, lsb = 0, hx0 = 0;
    s64 shift = 0;
    if (!is_cff &&
        rtt_glyph_box(&font_, static_cast<int>(glyph_index), &hx0, nullptr, nullptr, nullptr)) {
        rtt_glyph_hmetrics(&font_, static_cast<int>(glyph_index), &adv, &lsb);
        shift = MulFix(hx0 - lsb, load_x_scale);
    }
    const double ox = g.bitmap_left - static_cast<double>(delta_x - shift) / 64.0;
    const double oy = static_cast<double>(delta_y) / 64.0 - g.bitmap_top;
    rtt_render_glyph_at(&font_, &scratch_, g.bitmap.data(), static_cast<int>(g.bitmap_width),
                        static_cast<int>(g.bitmap_rows), static_cast<int>(g.bitmap_width),
                        static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(ox),
                        static_cast<float>(oy), static_cast<int>(glyph_index), 0);
}

s32 Face::KerningUnits(u32 left, u32 right) const {
    return rtt_kerning(&font_, static_cast<int>(left), static_cast<int>(right));
}

void Face::Kerning(u32 left, u32 right, s64* x, s64* y) const {
    s64 kx = MulFix(KerningUnits(left, right), size.x_scale);
    // small sizes kern less, so rounding does not overdo it
    if (size.x_ppem < 25) {
        kx = MulDiv(kx, size.x_ppem, 25);
    }
    if (x) {
        *x = PixRound(kx);
    }
    if (y) {
        *y = 0;
    }
}

} // namespace Libraries::Font::Internal
