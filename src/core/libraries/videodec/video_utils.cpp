// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>

#include <formats/rh264.h>
#include <formats/rh265.h>

#include "common/alignment.h"
#include "video_utils.h"

namespace Libraries::Videodec {

SwDecoder::SwDecoder(bool hevc) {
    if (hevc) {
        m_h265 = rh265_video_open();
    } else {
        m_h264 = rh264_video_open();
    }
}

SwDecoder::~SwDecoder() {
    if (m_h264) {
        rh264_video_close(m_h264);
    }
    if (m_h265) {
        rh265_video_close(m_h265);
    }
}

bool SwDecoder::SetExtradata(const u8* data, size_t size) {
    return m_h264 ? rh264_video_set_extradata(m_h264, data, size) == 0
                  : m_h265 && rh265_video_set_extradata(m_h265, data, size) == 0;
}

int SwDecoder::Decode(const u8* data, size_t size, const PictureMeta& meta) {
    const u64 seq = m_seq++;
    m_ring[seq % MetaRing] = meta;
    int ret;
    s64 tag = 0;
    if (m_h264) {
        rh264_video_set_tag(m_h264, seq);
        ret = rh264_video_decode(m_h264, data, size);
        tag = static_cast<s64>(rh264_video_tag(m_h264));
    } else {
        int64_t out = 0;
        ret = rh265_video_decode_timed(m_h265, data, size, static_cast<int64_t>(seq), &out);
        tag = out;
    }
    if (ret < 0) {
        return ret;
    }
    return ret == 1 && Ready(static_cast<int>(tag % MetaRing)) ? 1 : 0;
}

int SwDecoder::Drain() {
    int ret;
    s64 tag = 0;
    if (m_h264) {
        ret = rh264_video_drain(m_h264);
        tag = static_cast<s64>(rh264_video_tag(m_h264));
    } else {
        int64_t out = 0;
        ret = rh265_video_drain_timed(m_h265, &out);
        tag = out;
    }
    return ret == 0 && Ready(static_cast<int>(tag % MetaRing)) ? 1 : 0;
}

void SwDecoder::Reset() {
    // Handing the held pictures out empties the reorder queue; nothing is freed
    for (int i = 0; i < 64 && Drain(); ++i) {
    }
    m_width = m_height = 0;
}

bool SwDecoder::Ready(int tag) {
    int stride = 0;
    int w = 0;
    int h = 0;
    const bool ok = m_h264 ? rh264_video_plane(m_h264, 0, &stride, &w, &h) != nullptr
                           : rh265_video_plane(m_h265, 0, &stride, &w, &h) != nullptr;
    if (!ok || w <= 0 || h <= 0) {
        return false;
    }
    m_width = static_cast<u32>(w);
    m_height = static_cast<u32>(h);
    m_meta = m_ring[static_cast<size_t>(tag)];
    return true;
}

u32 SwDecoder::Pitch() const {
    return Common::AlignUp<u32>(m_width, 64);
}

u32 SwDecoder::AlignedHeight() const {
    return Common::AlignUp<u32>(m_height, 16);
}

u64 SwDecoder::Nv12Size() const {
    return static_cast<u64>(Pitch()) * AlignedHeight() * 3 / 2;
}

u8 SwDecoder::ColourPrimaries() const {
    if (m_h264) {
        rh264_video_color_t color;
        if (rh264_video_get_color(m_h264, &color) == 0) {
            return static_cast<u8>(color.primaries);
        }
    }
    return 2; // unspecified
}

namespace {
// One row of samples, down to 8 bits.
template <class T>
void Row(u8* dst, const u8* src, u32 n, int shift) {
    if constexpr (sizeof(T) == 1) {
        std::memcpy(dst, src, n);
    } else {
        const T* s = reinterpret_cast<const T*>(src);
        for (u32 x = 0; x < n; ++x) {
            dst[x] = static_cast<u8>(s[x] >> shift);
        }
    }
}

// One row of U and V, interleaved, down to 8 bits.
template <class T>
void ChromaRow(u8* dst, const u8* u, const u8* v, u32 n, int shift) {
    const T* su = reinterpret_cast<const T*>(u);
    const T* sv = reinterpret_cast<const T*>(v);
    for (u32 x = 0; x < n; ++x) {
        dst[2 * x] = static_cast<u8>(su[x] >> shift);
        dst[2 * x + 1] = static_cast<u8>(sv[x] >> shift);
    }
}

template <class T>
void WritePlanes(u8* dst, u32 pitch, u32 aligned_h, u32 w, u32 h, const u8* const planes[3],
                 const int strides[3], u32 chroma_w, u32 chroma_h, int shift) {
    u8* luma = dst;
    u8* chroma = dst + static_cast<size_t>(pitch) * aligned_h;
    const size_t ys = static_cast<size_t>(strides[0]) * sizeof(T);
    for (u32 y = 0; y < aligned_h; ++y) {
        Row<T>(luma + static_cast<size_t>(y) * pitch, planes[0] + std::min(y, h - 1) * ys, w,
               shift);
    }
    // 4:2:2 has a chroma row for every luma row: every other one is taken
    const u32 step = chroma_h >= h ? 2 : 1;
    const u32 rows = (h + 1) / 2;
    const size_t us = static_cast<size_t>(strides[1]) * sizeof(T);
    const size_t vs = static_cast<size_t>(strides[2]) * sizeof(T);
    for (u32 y = 0; y < aligned_h / 2; ++y) {
        const u32 sy = std::min(std::min(y, rows - 1) * step, chroma_h - 1);
        ChromaRow<T>(chroma + static_cast<size_t>(y) * pitch, planes[1] + sy * us,
                     planes[2] + sy * vs, std::min(chroma_w, pitch / 2), shift);
    }
}
} // namespace

bool SwDecoder::WriteNV12(u8* dst, u64 max_size) const {
    if (m_width == 0 || Nv12Size() > max_size) {
        return false;
    }
    const u8* planes[3];
    int strides[3];
    int cw = 0;
    int ch = 0;
    for (int p = 0; p < 3; ++p) {
        int w = 0;
        int h = 0;
        planes[p] = m_h264 ? rh264_video_plane(m_h264, p, &strides[p], &w, &h)
                           : rh265_video_plane(m_h265, p, &strides[p], &w, &h);
        if (!planes[p]) {
            return false;
        }
        if (p == 1) {
            cw = w;
            ch = h;
        }
    }
    const int depth = m_h264 ? rh264_video_bit_depth(m_h264) : rh265_video_bit_depth(m_h265);
    if (depth > 8) {
        WritePlanes<u16>(dst, Pitch(), AlignedHeight(), m_width, m_height, planes, strides,
                         static_cast<u32>(cw), static_cast<u32>(ch), depth - 8);
    } else {
        WritePlanes<u8>(dst, Pitch(), AlignedHeight(), m_width, m_height, planes, strides,
                        static_cast<u32>(cw), static_cast<u32>(ch), 0);
    }
    return true;
}

} // namespace Libraries::Videodec
