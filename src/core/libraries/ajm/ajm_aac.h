// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/enum.h"
#include "common/types.h"
#include "core/libraries/ajm/ajm_instance.h"

#include <array>
#include <span>

typedef struct raac raac_t;

namespace Libraries::Ajm {

enum ConfigType : u32 {
    ADTS = 1,
    RAW = 2,
};

enum AjmAacCodecFlags : u32 {
    EnableSbrDecode = 1 << 0,
    EnableNondelayOutput = 1 << 1,
    SurroundChannelInterleaveOrderExtlExtrLsRs = 1 << 2,
    SurroundChannelInterleaveOrderLsRsExtlExtr = 1 << 3,
};
DECLARE_ENUM_FLAG_OPERATORS(AjmAacCodecFlags)

struct AjmSidebandDecM4aacCodecInfo {
    u32 heaac;
    u32 reserved;
};

struct AjmAacDecoder final : AjmCodec {
    explicit AjmAacDecoder(AjmFormatEncoding format, AjmAacCodecFlags flags, u32 channels);
    ~AjmAacDecoder() override;

    void Reset() override;
    void Initialize(const void* buffer, u32 buffer_size) override;
    void GetInfo(void* out_info) const override;
    AjmSidebandFormat GetFormat() const override;
    u32 GetMinimumInputSize() const override;
    u32 GetNextFrameSize(const AjmInstanceGapless& gapless) const override;
    DecoderResult ProcessData(std::span<u8>& input, SparseOutputBuffer& output,
                              AjmInstanceGapless& gapless) override;

private:
    struct InitializeParameters {
        ConfigType config_type;
        u32 sampling_freq_type;
    };

    // Opens the decoder for an AudioSpecificConfig, keeping the open one when it is the same.
    bool Open(const u8* asc, size_t asc_size);
    void Close();

    const AjmFormatEncoding m_format;
    const AjmAacCodecFlags m_flags;
    const u32 m_channels;

    u32 m_skip_frames = 0;
    u32 m_last_frame_bytes = 0;
    InitializeParameters m_init_params = {};
    raac_t* m_decoder = nullptr;
    std::array<u8, 32> m_asc{};
    size_t m_asc_size = 0;
};

} // namespace Libraries::Ajm