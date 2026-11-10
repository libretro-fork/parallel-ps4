// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>

#include "common/types.h"

typedef struct rh264_video rh264_video;
typedef struct rh265_video rh265_video;

namespace Libraries::Videodec {

// What the game gave with an access unit, handed back with the picture it opens.
struct PictureMeta {
    u64 pts = 0;
    u64 dts = 0;
    u64 attached = 0;
};

// An H.264 or HEVC decoder (libretro-common's rh264 and rh265) that writes its pictures as
// the console's NV12 frames: luma then interleaved chroma, the pitch rounded up to 64 and the
// height to 16, the last row repeated below the picture.
class SwDecoder {
public:
    explicit SwDecoder(bool hevc);
    ~SwDecoder();
    SwDecoder(const SwDecoder&) = delete;
    SwDecoder& operator=(const SwDecoder&) = delete;

    bool IsOpen() const {
        return m_h264 || m_h265;
    }
    bool IsHevc() const {
        return m_h265 != nullptr;
    }

    // The avcC/hvcC record of an MP4 track, for its length-prefixed samples.
    bool SetExtradata(const u8* data, std::size_t size);

    // 1 when a picture is ready, 0 when none is (yet), negative for a stream the decoder
    // cannot take.
    int Decode(const u8* data, std::size_t size, const PictureMeta& meta);
    // The next picture still held for display order, at the end of the stream: 1 or 0.
    int Drain();
    // Forgets the pictures held for display order; the stream goes on from its next IDR.
    void Reset();

    // The ready picture, valid until the next Decode, Drain or Reset.
    u32 Width() const {
        return m_width;
    }
    u32 Height() const {
        return m_height;
    }
    u32 Pitch() const;
    u32 AlignedHeight() const;
    u64 Nv12Size() const;
    const PictureMeta& Meta() const {
        return m_meta;
    }
    u8 ColourPrimaries() const;
    // False, writing nothing, when the frame does not fit in max_size.
    bool WriteNV12(u8* dst, u64 max_size) const;

private:
    bool Ready(int tag);

    static constexpr std::size_t MetaRing = 64; // more than any stream holds back
    rh264_video* m_h264 = nullptr;
    rh265_video* m_h265 = nullptr;
    std::array<PictureMeta, MetaRing> m_ring{};
    u64 m_seq = 0;
    PictureMeta m_meta{};
    u32 m_width = 0;
    u32 m_height = 0;
};

} // namespace Libraries::Videodec
