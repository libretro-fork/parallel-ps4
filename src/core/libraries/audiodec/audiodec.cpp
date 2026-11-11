// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// libSceAudiodec: a game hands over one access unit of ATRAC9, MP3 or AAC at a time and gets
// its PCM back, 16-bit or float. The decoders are libretro-common's.
//
// A decoder is one of 64 slots, taken and given back with a compare-and-swap; a game decodes
// with a handle from one thread at a time, as the PS4's library asks of it. What a decoder
// needs is allocated when it is made, not while it decodes (an ADTS stream's decoder when its
// first header says what it is).

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <formats/raac.h>
#include <formats/ratrac9.h>
#include <formats/rmp3.h>
#include <retro_atomic.h>

#include "common/logging/log.h"
#include "common/types.h"
#include "core/libraries/audiodec/audiodec.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"

namespace Libraries::Audiodec {

namespace {

constexpr s32 ERROR_API_FAIL = static_cast<s32>(0x807F0000);
constexpr s32 ERROR_INVALID_TYPE = static_cast<s32>(0x807F0001);
constexpr s32 ERROR_INVALID_PARAM_SIZE = static_cast<s32>(0x807F0004);
constexpr s32 ERROR_INVALID_BSI_INFO_SIZE = static_cast<s32>(0x807F0005);
constexpr s32 ERROR_INVALID_AU_INFO_SIZE = static_cast<s32>(0x807F0006);
constexpr s32 ERROR_INVALID_PCM_ITEM_SIZE = static_cast<s32>(0x807F0007);
constexpr s32 ERROR_INVALID_CTRL_POINTER = static_cast<s32>(0x807F0008);
constexpr s32 ERROR_INVALID_PARAM_POINTER = static_cast<s32>(0x807F0009);
constexpr s32 ERROR_INVALID_BSI_INFO_POINTER = static_cast<s32>(0x807F000A);
constexpr s32 ERROR_INVALID_AU_INFO_POINTER = static_cast<s32>(0x807F000B);
constexpr s32 ERROR_INVALID_PCM_ITEM_POINTER = static_cast<s32>(0x807F000C);
constexpr s32 ERROR_INVALID_AU_POINTER = static_cast<s32>(0x807F000D);
constexpr s32 ERROR_INVALID_PCM_POINTER = static_cast<s32>(0x807F000E);
constexpr s32 ERROR_INVALID_HANDLE = static_cast<s32>(0x807F000F);
constexpr s32 ERROR_INVALID_WORD_LENGTH = static_cast<s32>(0x807F0010);
constexpr s32 ERROR_INVALID_AU_SIZE = static_cast<s32>(0x807F0011);
constexpr s32 ERROR_INVALID_PCM_SIZE = static_cast<s32>(0x807F0012);
constexpr s32 ERROR_M4AAC_INVALID_SAMPLING_FREQ = static_cast<s32>(0x807F0300);
constexpr s32 ERROR_M4AAC_INVALID_CONFIG_NUMBER = static_cast<s32>(0x807F0303);
constexpr s32 ERROR_M4AAC_INVALID_MAX_CHANNELS = static_cast<s32>(0x807F0304);
constexpr s32 ERROR_AT9_INVALID_CONFIG_DATA = static_cast<s32>(0x807F1000);

constexpr u32 TYPE_AT9 = 1;
constexpr u32 TYPE_MP3 = 2;
constexpr u32 TYPE_M4AAC = 3;
constexpr s32 WORD_16BIT = 1;
constexpr s32 WORD_FLOAT = 2;
constexpr u32 M4AAC_ADTS = 1;
constexpr u32 M4AAC_RAW = 2;
constexpr s32 M4AAC_RESULT_DECODE_ERROR = -4;
constexpr s32 M4AAC_RESULT_INSUFFICIENT_DATA = -5;

struct AuInfo {
    u32 size;
    void* au;
    u32 au_size;
};

struct PcmItem {
    u32 size;
    void* pcm;
    u32 pcm_size;
};

struct Ctrl {
    void* param;
    void* bsi_info;
    AuInfo* au_info;
    PcmItem* pcm_item;
};

struct ParamAt9 {
    u32 size;
    s32 word;
    u8 config[4];
};

struct At9Info {
    u32 size;
    u32 channels;
    u32 bitrate;
    u32 sampling_rate;
    u32 super_frame_size;
    u32 frames_in_super_frame;
    u32 next_frame_size;
    u32 frame_samples;
    s32 result;
};

struct ParamMp3 {
    u32 size;
    s32 word;
};

struct Mp3Info {
    u32 size;
    u32 header;
    u8 crc;
    u8 mode;
    u8 mode_extension;
    u8 copyright;
    u8 original;
    u8 emphasis;
    u8 reserved[2];
    s32 result;
};

struct ParamM4aac {
    u32 size;
    s32 word;
    u32 config_number;
    u32 sampling_freq_index;
    u32 max_channels;
    u32 enable_heaac;
};

struct M4aacInfo {
    u32 size;
    u32 sampling_freq;
    u32 channels;
    u32 heaac;
    s32 result;
};

static_assert(sizeof(AuInfo) == 24 && sizeof(PcmItem) == 24 && sizeof(Ctrl) == 32);
static_assert(sizeof(ParamAt9) == 12 && sizeof(At9Info) == 36);
static_assert(sizeof(ParamMp3) == 8 && sizeof(Mp3Info) == 20);
static_assert(sizeof(ParamM4aac) == 24 && sizeof(M4aacInfo) == 20);

enum : int { SlotFree = 0, SlotTaken = 1 };

struct Slot {
    retro_atomic_int_t state;
    u32 type;
    bool float_out;
    ratrac9_t* at9;
    std::unique_ptr<rmp3dec> mp3;
    raac_t* aac;
    bool adts;
    u8 asc[2]; // the configuration the AAC decoder was opened with

    void Close() {
        if (at9) {
            ratrac9_close(at9);
            at9 = nullptr;
        }
        if (aac) {
            raac_close(aac);
            aac = nullptr;
        }
        mp3.reset();
    }
};

std::array<Slot, 64> slots{};

Slot* Find(s32 handle) {
    if (handle <= 0 || static_cast<size_t>(handle) > slots.size()) {
        return nullptr;
    }
    Slot& slot = slots[handle - 1];
    return retro_atomic_load_acquire_int(&slot.state) == SlotTaken ? &slot : nullptr;
}

bool ValidType(u32 type) {
    return type == TYPE_AT9 || type == TYPE_MP3 || type == TYPE_M4AAC;
}

s32 CheckCtrl(const Ctrl* ctrl, u32 type) {
    if (!ctrl) {
        return ERROR_INVALID_CTRL_POINTER;
    }
    if (!ctrl->param) {
        return ERROR_INVALID_PARAM_POINTER;
    }
    if (!ctrl->bsi_info) {
        return ERROR_INVALID_BSI_INFO_POINTER;
    }
    u32 param_size, info_size;
    std::memcpy(&param_size, ctrl->param, sizeof(param_size));
    std::memcpy(&info_size, ctrl->bsi_info, sizeof(info_size));
    switch (type) {
    case TYPE_AT9:
        if (param_size != sizeof(ParamAt9)) {
            return ERROR_INVALID_PARAM_SIZE;
        }
        if (info_size != sizeof(At9Info)) {
            return ERROR_INVALID_BSI_INFO_SIZE;
        }
        break;
    case TYPE_MP3:
        if (param_size != sizeof(ParamMp3)) {
            return ERROR_INVALID_PARAM_SIZE;
        }
        if (info_size != sizeof(Mp3Info)) {
            return ERROR_INVALID_BSI_INFO_SIZE;
        }
        break;
    default:
        // Later versions of the structure carry two more words
        if (param_size != sizeof(ParamM4aac) && param_size != 28 && param_size != 32) {
            return ERROR_INVALID_PARAM_SIZE;
        }
        if (info_size != sizeof(M4aacInfo)) {
            return ERROR_INVALID_BSI_INFO_SIZE;
        }
        break;
    }
    return ORBIS_OK;
}

void FillAt9Info(At9Info* info, const ratrac9_t* at9) {
    const u32 superframe = ratrac9_superframe_bytes(at9);
    const u32 samples = ratrac9_frame_samples(at9) * ratrac9_frames_per_superframe(at9);
    info->channels = ratrac9_channels(at9);
    info->bitrate = static_cast<u32>(u64{superframe} * 8 * ratrac9_sample_rate(at9) / samples);
    info->sampling_rate = ratrac9_sample_rate(at9);
    info->super_frame_size = superframe;
    info->frames_in_super_frame = ratrac9_frames_per_superframe(at9);
    info->next_frame_size = superframe;
    info->frame_samples = ratrac9_frame_samples(at9);
    info->result = 0;
}

u32 AacSampleRate(u32 index) {
    static constexpr u32 rates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                    22050, 16000, 12000, 11025, 8000,  7350};
    return index < std::size(rates) ? rates[index] : 0;
}

bool OpenAac(Slot& slot, const u8 asc[2]) {
    if (slot.aac && std::memcmp(slot.asc, asc, 2) == 0) {
        return true;
    }
    if (slot.aac) {
        raac_close(slot.aac);
    }
    slot.aac = raac_open(asc, 2);
    if (!slot.aac) {
        return false;
    }
    // Games take the channels in the WAVE order
    raac_set_channel_order(slot.aac, RAAC_ORDER_WAVE);
    std::memcpy(slot.asc, asc, 2);
    return true;
}

enum class Status { Ok, Invalid, Partial, NoRoom };

struct Decoded {
    Status status{Status::Ok};
    size_t consumed{};
    size_t produced{};
};

Decoded DecodeAt9(Slot& slot, const u8* au, size_t au_size, u8* pcm, size_t pcm_size) {
    ratrac9_t* const at9 = slot.at9;
    const size_t superframe = ratrac9_superframe_bytes(at9);
    const u32 frames = ratrac9_frames_per_superframe(at9);
    const size_t frame_out = size_t{ratrac9_frame_samples(at9)} * ratrac9_channels(at9) *
                             (slot.float_out ? sizeof(float) : sizeof(s16));
    if (au_size < superframe) {
        return {Status::Partial};
    }
    if (frame_out * frames > pcm_size) {
        return {Status::NoRoom};
    }
    Decoded out{};
    size_t offset = 0;
    for (u32 frame = 0; frame < frames; frame++) {
        size_t used = 0;
        const int result =
            slot.float_out
                ? ratrac9_decode_f32(at9, au + offset, superframe - offset,
                                     reinterpret_cast<float*>(pcm + out.produced), false, &used)
                : ratrac9_decode_s16(at9, au + offset, superframe - offset,
                                     reinterpret_cast<s16*>(pcm + out.produced), false, &used);
        if (result < 0 || used == 0 || offset + used > superframe) {
            ratrac9_reset(at9);
            return {Status::Invalid};
        }
        offset += used;
        out.produced += frame_out;
    }
    out.consumed = superframe;
    return out;
}

Decoded DecodeMp3(Slot& slot, const u8* au, size_t au_size, u8* pcm, size_t pcm_size,
                  Mp3Info* info) {
    // A frame is at most 1152 samples of two channels
    if (pcm_size < RMP3_MAX_SAMPLES_PER_FRAME * sizeof(s16)) {
        return {Status::NoRoom};
    }
    rmp3dec_frame_info frame{};
    const int samples = rmp3dec_decode_s16(slot.mp3.get(), au, static_cast<int>(au_size),
                                           reinterpret_cast<s16*>(pcm), &frame);
    if (frame.frame_bytes <= 0) {
        return {Status::Partial};
    }
    if (samples <= 0) {
        return {Status::Invalid};
    }
    const u8* const h = slot.mp3->header;
    info->header = u32{h[0]} << 24 | u32{h[1]} << 16 | u32{h[2]} << 8 | h[3];
    info->crc = !(h[1] & 1);
    info->mode = h[3] >> 6;
    info->mode_extension = (h[3] >> 4) & 3;
    info->copyright = (h[3] >> 3) & 1;
    info->original = (h[3] >> 2) & 1;
    info->emphasis = h[3] & 3;
    info->result = 0;
    return {Status::Ok, static_cast<size_t>(frame.frame_bytes),
            static_cast<size_t>(samples) * frame.channels * sizeof(s16)};
}

Decoded DecodeAac(Slot& slot, const u8* au, size_t au_size, u8* pcm, size_t pcm_size,
                  M4aacInfo* info) {
    const u8* unit = au;
    size_t unit_size = au_size;
    size_t consumed = 0;
    if (slot.adts) {
        raac_adts_t header;
        if (au_size < 7) {
            return {Status::Partial};
        }
        if (raac_adts_parse(au, au_size, &header) < 0) {
            return {Status::Invalid};
        }
        if (au_size < header.frame_size) {
            return {Status::Partial};
        }
        if (!OpenAac(slot, header.asc)) {
            return {Status::Invalid};
        }
        unit += header.header_size;
        unit_size = header.frame_size - header.header_size;
        consumed = header.frame_size;
    }
    const size_t sample = slot.float_out ? sizeof(float) : sizeof(s16);
    if (size_t{raac_frame_len(slot.aac)} * raac_channels(slot.aac) * sample > pcm_size) {
        return {Status::NoRoom};
    }
    const int samples = raac_decode(slot.aac, unit, unit_size);
    if (samples < 0) {
        return {Status::Invalid};
    }
    const u32 channels = raac_channels(slot.aac);
    if (static_cast<size_t>(samples) * channels * sample > pcm_size) {
        return {Status::NoRoom};
    }
    if (slot.float_out) {
        raac_copy_f32(slot.aac, 0, samples, reinterpret_cast<float*>(pcm));
    } else {
        raac_copy_s16(slot.aac, 0, samples, reinterpret_cast<s16*>(pcm));
    }
    info->sampling_freq = raac_sample_rate(slot.aac);
    info->channels = channels;
    info->heaac = samples == 2048;
    if (!consumed) {
        consumed = raac_last_size(slot.aac);
    }
    return {Status::Ok, consumed, static_cast<size_t>(samples) * channels * sample};
}

} // namespace

s32 PS4_SYSV_ABI sceAudiodecInitLibrary(u32 codec_type) {
    LOG_INFO(Lib_Audiodec, "codec {}", codec_type);
    return ValidType(codec_type) ? ORBIS_OK : ERROR_INVALID_TYPE;
}

s32 PS4_SYSV_ABI sceAudiodecTermLibrary(u32 codec_type) {
    return ValidType(codec_type) ? ORBIS_OK : ERROR_INVALID_TYPE;
}

s32 PS4_SYSV_ABI sceAudiodecCreateDecoder(Ctrl* ctrl, u32 codec_type) {
    if (!ValidType(codec_type)) {
        return ERROR_INVALID_TYPE;
    }
    if (const s32 result = CheckCtrl(ctrl, codec_type); result != ORBIS_OK) {
        return result;
    }
    s32 word;
    std::memcpy(&word, static_cast<const u8*>(ctrl->param) + 4, sizeof(word));
    if (word != WORD_16BIT && !(word == WORD_FLOAT && codec_type != TYPE_MP3)) {
        return ERROR_INVALID_WORD_LENGTH;
    }
    for (size_t i = 0; i < slots.size(); i++) {
        Slot& slot = slots[i];
        if (!retro_atomic_cas_int(&slot.state, SlotFree, SlotTaken)) {
            continue;
        }
        slot.type = codec_type;
        slot.float_out = word == WORD_FLOAT;
        s32 error = ORBIS_OK;
        if (codec_type == TYPE_AT9) {
            const auto* param = static_cast<const ParamAt9*>(ctrl->param);
            slot.at9 = ratrac9_open(param->config);
            if (slot.at9) {
                FillAt9Info(static_cast<At9Info*>(ctrl->bsi_info), slot.at9);
            } else {
                error = ERROR_AT9_INVALID_CONFIG_DATA;
            }
        } else if (codec_type == TYPE_MP3) {
            slot.mp3 = std::make_unique<rmp3dec>();
            rmp3dec_init(slot.mp3.get());
        } else {
            const auto* param = static_cast<const ParamM4aac*>(ctrl->param);
            auto* info = static_cast<M4aacInfo*>(ctrl->bsi_info);
            const u32 rate = AacSampleRate(param->sampling_freq_index);
            const u32 channels = param->max_channels;
            if (param->config_number != M4AAC_ADTS && param->config_number != M4AAC_RAW) {
                error = ERROR_M4AAC_INVALID_CONFIG_NUMBER;
            } else if (param->config_number == M4AAC_RAW && rate == 0) {
                error = ERROR_M4AAC_INVALID_SAMPLING_FREQ;
            } else if (channels != 1 && channels != 2 && channels != 6 && channels != 8) {
                error = ERROR_M4AAC_INVALID_MAX_CHANNELS;
            } else {
                slot.adts = param->config_number == M4AAC_ADTS;
                if (!slot.adts) {
                    // AAC-LC at that rate, in the layout that many channels have
                    const u32 layout = channels == 8 ? 7 : channels;
                    const u8 asc[2] = {
                        static_cast<u8>(2 << 3 | param->sampling_freq_index >> 1),
                        static_cast<u8>((param->sampling_freq_index & 1) << 7 | layout << 3)};
                    if (!OpenAac(slot, asc)) {
                        error = ERROR_API_FAIL;
                    }
                }
                info->sampling_freq = rate;
                info->channels = channels;
                info->heaac = 0;
                info->result = 0;
            }
        }
        if (error != ORBIS_OK) {
            slot.Close();
            (void)retro_atomic_exchange_int(&slot.state, SlotFree);
            LOG_ERROR(Lib_Audiodec, "codec {}: {:#x}", codec_type, static_cast<u32>(error));
            return error;
        }
        return static_cast<s32>(i + 1);
    }
    LOG_ERROR(Lib_Audiodec, "no decoder left");
    return ERROR_API_FAIL;
}

s32 PS4_SYSV_ABI sceAudiodecDeleteDecoder(s32 handle) {
    Slot* const slot = Find(handle);
    if (!slot) {
        return ERROR_INVALID_HANDLE;
    }
    slot->Close();
    (void)retro_atomic_exchange_int(&slot->state, SlotFree);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudiodecClearContext(s32 handle) {
    Slot* const slot = Find(handle);
    if (!slot) {
        return ERROR_INVALID_HANDLE;
    }
    if (slot->at9) {
        ratrac9_reset(slot->at9);
    }
    if (slot->mp3) {
        rmp3dec_init(slot->mp3.get());
    }
    if (slot->aac) {
        raac_reset(slot->aac);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudiodecDecode(s32 handle, Ctrl* ctrl) {
    if (!ctrl) {
        return ERROR_INVALID_CTRL_POINTER;
    }
    if (!ctrl->param) {
        return ERROR_INVALID_PARAM_POINTER;
    }
    if (!ctrl->bsi_info) {
        return ERROR_INVALID_BSI_INFO_POINTER;
    }
    if (!ctrl->au_info) {
        return ERROR_INVALID_AU_INFO_POINTER;
    }
    if (!ctrl->au_info->au) {
        return ERROR_INVALID_AU_POINTER;
    }
    if (!ctrl->pcm_item) {
        return ERROR_INVALID_PCM_ITEM_POINTER;
    }
    if (!ctrl->pcm_item->pcm) {
        return ERROR_INVALID_PCM_POINTER;
    }
    if (ctrl->au_info->au_size == 0 || ctrl->au_info->au_size == UINT32_MAX) {
        return ERROR_INVALID_AU_SIZE;
    }
    if (ctrl->pcm_item->pcm_size == 0 || ctrl->pcm_item->pcm_size == UINT32_MAX) {
        return ERROR_INVALID_PCM_SIZE;
    }
    if (ctrl->au_info->size != sizeof(AuInfo)) {
        return ERROR_INVALID_AU_INFO_SIZE;
    }
    if (ctrl->pcm_item->size != sizeof(PcmItem)) {
        return ERROR_INVALID_PCM_ITEM_SIZE;
    }
    Slot* const slot = Find(handle);
    if (!slot) {
        return ERROR_INVALID_HANDLE;
    }
    const auto* const au = static_cast<const u8*>(ctrl->au_info->au);
    auto* const pcm = static_cast<u8*>(ctrl->pcm_item->pcm);
    const size_t au_size = ctrl->au_info->au_size;
    const size_t pcm_size = ctrl->pcm_item->pcm_size;
    Decoded decoded;
    if (slot->type == TYPE_AT9) {
        decoded = DecodeAt9(*slot, au, au_size, pcm, pcm_size);
        if (decoded.status == Status::Ok) {
            FillAt9Info(static_cast<At9Info*>(ctrl->bsi_info), slot->at9);
        }
    } else if (slot->type == TYPE_MP3) {
        decoded =
            DecodeMp3(*slot, au, au_size, pcm, pcm_size, static_cast<Mp3Info*>(ctrl->bsi_info));
    } else {
        auto* info = static_cast<M4aacInfo*>(ctrl->bsi_info);
        decoded = DecodeAac(*slot, au, au_size, pcm, pcm_size, info);
        info->result = decoded.status == Status::Invalid   ? M4AAC_RESULT_DECODE_ERROR
                       : decoded.status == Status::Partial ? M4AAC_RESULT_INSUFFICIENT_DATA
                                                           : 0;
    }
    if (decoded.status == Status::NoRoom) {
        return ERROR_INVALID_PCM_SIZE;
    }
    if (decoded.status != Status::Ok) {
        ctrl->au_info->au_size = 0;
        ctrl->pcm_item->pcm_size = 0;
        return ERROR_API_FAIL;
    }
    ctrl->au_info->au_size = static_cast<u32>(decoded.consumed);
    ctrl->pcm_item->pcm_size = static_cast<u32>(decoded.produced);
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("VjhsmxpcezI", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecInitLibrary);
    LIB_FUNCTION("h5jSB2QIDV0", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecTermLibrary);
    LIB_FUNCTION("O3f1sLMWRvs", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecCreateDecoder);
    LIB_FUNCTION("Tp+ZEy69mLk", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecDeleteDecoder);
    LIB_FUNCTION("6Vf9WTLDoss", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecClearContext);
    LIB_FUNCTION("KHXHMDLkILw", "libSceAudiodec", 1, "libSceAudiodec", sceAudiodecDecode);
}

} // namespace Libraries::Audiodec
