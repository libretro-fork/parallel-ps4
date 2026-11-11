/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <formats/raac.h>
#include <formats/ratrac9.h>
#include <formats/rmp3.h>
#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "core/libraries/audiodec/audiodec.h"

#define SLOTS 64
#define SLOT_FREE 0
#define SLOT_TAKEN 1

#define M4AAC_RESULT_DECODE_ERROR (-4)
#define M4AAC_RESULT_INSUFFICIENT_DATA (-5)

enum { STATUS_OK = 0, STATUS_INVALID, STATUS_PARTIAL, STATUS_NO_ROOM };

typedef struct slot {
    retro_atomic_int_t state;
    uint32_t type;
    int float_out;
    ratrac9_t* at9;
    rmp3dec* mp3;
    raac_t* aac;
    int adts;
    uint8_t asc[2]; /* the configuration the AAC decoder was opened with */
} slot_t;

typedef struct decoded {
    int status;
    size_t consumed;
    size_t produced;
} decoded_t;

static slot_t slots[SLOTS];

static slot_t* find_slot(int32_t handle) {
    slot_t* slot;
    if (handle <= 0 || handle > SLOTS)
        return NULL;
    slot = &slots[handle - 1];
    return retro_atomic_load_acquire_int(&slot->state) == SLOT_TAKEN ? slot : NULL;
}

static void close_slot(slot_t* slot) {
    if (slot->at9)
        ratrac9_close(slot->at9);
    if (slot->aac)
        raac_close(slot->aac);
    free(slot->mp3);
    slot->at9 = NULL;
    slot->aac = NULL;
    slot->mp3 = NULL;
    slot->adts = 0;
}

static int valid_type(uint32_t type) {
    return type == AUDIODEC_TYPE_AT9 || type == AUDIODEC_TYPE_MP3 || type == AUDIODEC_TYPE_M4AAC;
}

static uint32_t first_word(const void* p) {
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static int32_t check_ctrl(const audiodec_ctrl* ctrl, uint32_t type) {
    uint32_t param_size, info_size;
    if (!ctrl)
        return AUDIODEC_ERROR_INVALID_CTRL_POINTER;
    if (!ctrl->param)
        return AUDIODEC_ERROR_INVALID_PARAM_POINTER;
    if (!ctrl->bsi_info)
        return AUDIODEC_ERROR_INVALID_BSI_INFO_POINTER;
    param_size = first_word(ctrl->param);
    info_size = first_word(ctrl->bsi_info);
    switch (type) {
    case AUDIODEC_TYPE_AT9:
        if (param_size != sizeof(audiodec_param_at9))
            return AUDIODEC_ERROR_INVALID_PARAM_SIZE;
        if (info_size != sizeof(audiodec_at9_info))
            return AUDIODEC_ERROR_INVALID_BSI_INFO_SIZE;
        break;
    case AUDIODEC_TYPE_MP3:
        if (param_size != sizeof(audiodec_param_mp3))
            return AUDIODEC_ERROR_INVALID_PARAM_SIZE;
        if (info_size != sizeof(audiodec_mp3_info))
            return AUDIODEC_ERROR_INVALID_BSI_INFO_SIZE;
        break;
    default:
        /* Later versions of the structure carry two more words */
        if (param_size != sizeof(audiodec_param_m4aac) && param_size != 28 && param_size != 32)
            return AUDIODEC_ERROR_INVALID_PARAM_SIZE;
        if (info_size != sizeof(audiodec_m4aac_info))
            return AUDIODEC_ERROR_INVALID_BSI_INFO_SIZE;
        break;
    }
    return 0;
}

static void fill_at9_info(audiodec_at9_info* info, const ratrac9_t* at9) {
    const uint32_t superframe = ratrac9_superframe_bytes(at9);
    const uint32_t samples = ratrac9_frame_samples(at9) * ratrac9_frames_per_superframe(at9);
    info->channels = ratrac9_channels(at9);
    info->bitrate = (uint32_t)((uint64_t)superframe * 8 * ratrac9_sample_rate(at9) / samples);
    info->sampling_rate = ratrac9_sample_rate(at9);
    info->super_frame_size = superframe;
    info->frames_in_super_frame = ratrac9_frames_per_superframe(at9);
    info->next_frame_size = superframe;
    info->frame_samples = ratrac9_frame_samples(at9);
    info->result = 0;
}

static uint32_t aac_sample_rate(uint32_t index) {
    static const uint32_t rates[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                       22050, 16000, 12000, 11025, 8000,  7350};
    return index < 13 ? rates[index] : 0;
}

static int open_aac(slot_t* slot, const uint8_t* asc) {
    if (slot->aac && memcmp(slot->asc, asc, 2) == 0)
        return 1;
    if (slot->aac)
        raac_close(slot->aac);
    slot->aac = raac_open(asc, 2);
    if (!slot->aac)
        return 0;
    /* Games take the channels in the WAVE order */
    raac_set_channel_order(slot->aac, RAAC_ORDER_WAVE);
    memcpy(slot->asc, asc, 2);
    return 1;
}

static decoded_t result_of(int status, size_t consumed, size_t produced) {
    decoded_t d;
    d.status = status;
    d.consumed = consumed;
    d.produced = produced;
    return d;
}

static decoded_t decode_at9(slot_t* slot, const uint8_t* au, size_t au_size, uint8_t* pcm,
                            size_t pcm_size) {
    ratrac9_t* at9 = slot->at9;
    const size_t superframe = ratrac9_superframe_bytes(at9);
    const uint32_t frames = ratrac9_frames_per_superframe(at9);
    const size_t frame_out = (size_t)ratrac9_frame_samples(at9) * ratrac9_channels(at9) *
                             (slot->float_out ? sizeof(float) : sizeof(int16_t));
    const size_t frame_bytes = ratrac9_frame_bytes(at9);
    size_t produced = 0;
    uint32_t frame;
    if (au_size < superframe)
        return result_of(STATUS_PARTIAL, 0, 0);
    if (frame_out * frames > pcm_size)
        return result_of(STATUS_NO_ROOM, 0, 0);
    /* A frame has its place in the superframe whatever it used of it */
    for (frame = 0; frame < frames; frame++) {
        size_t used = 0;
        const uint8_t* in = au + (size_t)frame * frame_bytes;
        const int result =
            slot->float_out ? ratrac9_decode_f32(at9, in, frame_bytes,
                                                 (float*)(void*)(pcm + produced), false, &used)
                            : ratrac9_decode_s16(at9, in, frame_bytes,
                                                 (int16_t*)(void*)(pcm + produced), false, &used);
        if (result < 0) {
            ratrac9_reset(at9);
            return result_of(STATUS_INVALID, 0, 0);
        }
        produced += frame_out;
    }
    return result_of(STATUS_OK, superframe, produced);
}

static decoded_t decode_mp3(slot_t* slot, const uint8_t* au, size_t au_size, uint8_t* pcm,
                            size_t pcm_size, audiodec_mp3_info* info) {
    rmp3dec_frame_info frame;
    const uint8_t* h;
    int samples;
    /* A frame is at most 1152 samples of two channels */
    if (pcm_size < RMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t))
        return result_of(STATUS_NO_ROOM, 0, 0);
    memset(&frame, 0, sizeof(frame));
    samples = rmp3dec_decode_s16(slot->mp3, au, au_size > 0x7fffffff ? 0x7fffffff : (int)au_size,
                                 (int16_t*)(void*)pcm, &frame);
    if (frame.frame_bytes <= 0)
        return result_of(STATUS_PARTIAL, 0, 0);
    if (samples <= 0)
        return result_of(STATUS_INVALID, 0, 0);
    h = slot->mp3->header;
    info->header = (uint32_t)h[0] << 24 | (uint32_t)h[1] << 16 | (uint32_t)h[2] << 8 | h[3];
    info->crc = (uint8_t) !(h[1] & 1);
    info->mode = (uint8_t)(h[3] >> 6);
    info->mode_extension = (uint8_t)((h[3] >> 4) & 3);
    info->copyright = (uint8_t)((h[3] >> 3) & 1);
    info->original = (uint8_t)((h[3] >> 2) & 1);
    info->emphasis = (uint8_t)(h[3] & 3);
    info->result = 0;
    return result_of(STATUS_OK, (size_t)frame.frame_bytes,
                     (size_t)samples * (size_t)frame.channels * sizeof(int16_t));
}

static decoded_t decode_aac(slot_t* slot, const uint8_t* au, size_t au_size, uint8_t* pcm,
                            size_t pcm_size, audiodec_m4aac_info* info) {
    const uint8_t* unit = au;
    size_t unit_size = au_size, consumed = 0;
    const size_t sample = slot->float_out ? sizeof(float) : sizeof(int16_t);
    uint32_t channels;
    int samples;
    if (slot->adts) {
        raac_adts_t header;
        if (au_size < 7)
            return result_of(STATUS_PARTIAL, 0, 0);
        if (raac_adts_parse(au, au_size, &header) < 0)
            return result_of(STATUS_INVALID, 0, 0);
        if (au_size < header.frame_size)
            return result_of(STATUS_PARTIAL, 0, 0);
        if (!open_aac(slot, header.asc))
            return result_of(STATUS_INVALID, 0, 0);
        unit += header.header_size;
        unit_size = header.frame_size - header.header_size;
        consumed = header.frame_size;
    }
    channels = raac_channels(slot->aac);
    if ((size_t)raac_frame_len(slot->aac) * channels * sample > pcm_size)
        return result_of(STATUS_NO_ROOM, 0, 0);
    samples = raac_decode(slot->aac, unit, unit_size);
    if (samples < 0)
        return result_of(STATUS_INVALID, 0, 0);
    if ((size_t)samples * channels * sample > pcm_size)
        return result_of(STATUS_NO_ROOM, 0, 0);
    if (slot->float_out)
        raac_copy_f32(slot->aac, 0, (size_t)samples, (float*)(void*)pcm);
    else
        raac_copy_s16(slot->aac, 0, (size_t)samples, (int16_t*)(void*)pcm);
    info->sampling_freq = raac_sample_rate(slot->aac);
    info->channels = channels;
    info->heaac = samples == 2048;
    if (!consumed)
        consumed = raac_last_size(slot->aac);
    return result_of(STATUS_OK, consumed, (size_t)samples * channels * sample);
}

int32_t AUDIODEC_GUEST_ABI sceAudiodecInitLibrary(uint32_t codec_type) {
    c_log(C_LOG_LIB_AUDIODEC, C_LOG_LEVEL_INFO, "sceAudiodecInitLibrary", "codec %u",
          (unsigned)codec_type);
    return valid_type(codec_type) ? 0 : AUDIODEC_ERROR_INVALID_TYPE;
}

int32_t AUDIODEC_GUEST_ABI sceAudiodecTermLibrary(uint32_t codec_type) {
    return valid_type(codec_type) ? 0 : AUDIODEC_ERROR_INVALID_TYPE;
}

/* What @slot decodes with, as the game's parameters ask. 0 or an error. */
static int32_t open_slot(slot_t* slot, const audiodec_ctrl* ctrl, uint32_t codec_type) {
    if (codec_type == AUDIODEC_TYPE_AT9) {
        const audiodec_param_at9* param = (const audiodec_param_at9*)ctrl->param;
        slot->at9 = ratrac9_open(param->config);
        if (!slot->at9)
            return AUDIODEC_ERROR_AT9_INVALID_CONFIG_DATA;
        fill_at9_info((audiodec_at9_info*)ctrl->bsi_info, slot->at9);
        return 0;
    }
    if (codec_type == AUDIODEC_TYPE_MP3) {
        slot->mp3 = (rmp3dec*)malloc(sizeof(rmp3dec));
        if (!slot->mp3)
            return AUDIODEC_ERROR_API_FAIL;
        rmp3dec_init(slot->mp3);
        return 0;
    }
    {
        const audiodec_param_m4aac* param = (const audiodec_param_m4aac*)ctrl->param;
        audiodec_m4aac_info* info = (audiodec_m4aac_info*)ctrl->bsi_info;
        const uint32_t rate = aac_sample_rate(param->sampling_freq_index);
        const uint32_t channels = param->max_channels;
        if (param->config_number != AUDIODEC_M4AAC_ADTS &&
            param->config_number != AUDIODEC_M4AAC_RAW)
            return AUDIODEC_ERROR_M4AAC_INVALID_CONFIG_NUMBER;
        if (param->config_number == AUDIODEC_M4AAC_RAW && rate == 0)
            return AUDIODEC_ERROR_M4AAC_INVALID_SAMPLING_FREQ;
        if (channels != 1 && channels != 2 && channels != 6 && channels != 8)
            return AUDIODEC_ERROR_M4AAC_INVALID_MAX_CHANNELS;
        slot->adts = param->config_number == AUDIODEC_M4AAC_ADTS;
        if (!slot->adts) {
            /* AAC-LC at that rate, in the layout that many channels have */
            const uint32_t layout = channels == 8 ? 7 : channels;
            uint8_t asc[2];
            asc[0] = (uint8_t)(2u << 3 | param->sampling_freq_index >> 1);
            asc[1] = (uint8_t)((param->sampling_freq_index & 1u) << 7 | layout << 3);
            if (!open_aac(slot, asc))
                return AUDIODEC_ERROR_API_FAIL;
        }
        info->sampling_freq = rate;
        info->channels = channels;
        info->heaac = 0;
        info->result = 0;
    }
    return 0;
}

int32_t AUDIODEC_GUEST_ABI sceAudiodecCreateDecoder(audiodec_ctrl* ctrl, uint32_t codec_type) {
    int32_t result, word;
    int i;
    if (!valid_type(codec_type))
        return AUDIODEC_ERROR_INVALID_TYPE;
    result = check_ctrl(ctrl, codec_type);
    if (result != 0)
        return result;
    memcpy(&word, (const uint8_t*)ctrl->param + 4, sizeof(word));
    if (word != AUDIODEC_WORD_16BIT &&
        !(word == AUDIODEC_WORD_FLOAT && codec_type != AUDIODEC_TYPE_MP3))
        return AUDIODEC_ERROR_INVALID_WORD_LENGTH;
    for (i = 0; i < SLOTS; i++) {
        slot_t* slot = &slots[i];
        if (!retro_atomic_cas_int(&slot->state, SLOT_FREE, SLOT_TAKEN))
            continue;
        slot->type = codec_type;
        slot->float_out = word == AUDIODEC_WORD_FLOAT;
        result = open_slot(slot, ctrl, codec_type);
        if (result != 0) {
            close_slot(slot);
            (void)retro_atomic_exchange_int(&slot->state, SLOT_FREE);
            c_log(C_LOG_LIB_AUDIODEC, C_LOG_LEVEL_ERROR, "sceAudiodecCreateDecoder",
                  "codec %u: 0x%x", (unsigned)codec_type, (unsigned)result);
            return result;
        }
        return i + 1;
    }
    c_log(C_LOG_LIB_AUDIODEC, C_LOG_LEVEL_ERROR, "sceAudiodecCreateDecoder", "no decoder left");
    return AUDIODEC_ERROR_API_FAIL;
}

int32_t AUDIODEC_GUEST_ABI sceAudiodecDeleteDecoder(int32_t handle) {
    slot_t* slot = find_slot(handle);
    if (!slot)
        return AUDIODEC_ERROR_INVALID_HANDLE;
    close_slot(slot);
    (void)retro_atomic_exchange_int(&slot->state, SLOT_FREE);
    return 0;
}

int32_t AUDIODEC_GUEST_ABI sceAudiodecClearContext(int32_t handle) {
    slot_t* slot = find_slot(handle);
    if (!slot)
        return AUDIODEC_ERROR_INVALID_HANDLE;
    if (slot->at9)
        ratrac9_reset(slot->at9);
    if (slot->mp3)
        rmp3dec_init(slot->mp3);
    if (slot->aac)
        raac_reset(slot->aac);
    return 0;
}

int32_t AUDIODEC_GUEST_ABI sceAudiodecDecode(int32_t handle, audiodec_ctrl* ctrl) {
    slot_t* slot;
    const uint8_t* au;
    uint8_t* pcm;
    size_t au_size, pcm_size;
    decoded_t decoded;
    if (!ctrl)
        return AUDIODEC_ERROR_INVALID_CTRL_POINTER;
    if (!ctrl->param)
        return AUDIODEC_ERROR_INVALID_PARAM_POINTER;
    if (!ctrl->bsi_info)
        return AUDIODEC_ERROR_INVALID_BSI_INFO_POINTER;
    if (!ctrl->au_info)
        return AUDIODEC_ERROR_INVALID_AU_INFO_POINTER;
    if (!ctrl->au_info->au)
        return AUDIODEC_ERROR_INVALID_AU_POINTER;
    if (!ctrl->pcm_item)
        return AUDIODEC_ERROR_INVALID_PCM_ITEM_POINTER;
    if (!ctrl->pcm_item->pcm)
        return AUDIODEC_ERROR_INVALID_PCM_POINTER;
    if (ctrl->au_info->au_size == 0 || ctrl->au_info->au_size == 0xffffffffu)
        return AUDIODEC_ERROR_INVALID_AU_SIZE;
    if (ctrl->pcm_item->pcm_size == 0 || ctrl->pcm_item->pcm_size == 0xffffffffu)
        return AUDIODEC_ERROR_INVALID_PCM_SIZE;
    if (ctrl->au_info->size != sizeof(audiodec_au_info))
        return AUDIODEC_ERROR_INVALID_AU_INFO_SIZE;
    if (ctrl->pcm_item->size != sizeof(audiodec_pcm_item))
        return AUDIODEC_ERROR_INVALID_PCM_ITEM_SIZE;
    slot = find_slot(handle);
    if (!slot)
        return AUDIODEC_ERROR_INVALID_HANDLE;
    au = (const uint8_t*)ctrl->au_info->au;
    pcm = (uint8_t*)ctrl->pcm_item->pcm;
    au_size = ctrl->au_info->au_size;
    pcm_size = ctrl->pcm_item->pcm_size;
    if (slot->type == AUDIODEC_TYPE_AT9) {
        decoded = decode_at9(slot, au, au_size, pcm, pcm_size);
        if (decoded.status == STATUS_OK)
            fill_at9_info((audiodec_at9_info*)ctrl->bsi_info, slot->at9);
    } else if (slot->type == AUDIODEC_TYPE_MP3) {
        decoded = decode_mp3(slot, au, au_size, pcm, pcm_size, (audiodec_mp3_info*)ctrl->bsi_info);
    } else {
        audiodec_m4aac_info* info = (audiodec_m4aac_info*)ctrl->bsi_info;
        decoded = decode_aac(slot, au, au_size, pcm, pcm_size, info);
        info->result = decoded.status == STATUS_INVALID   ? M4AAC_RESULT_DECODE_ERROR
                       : decoded.status == STATUS_PARTIAL ? M4AAC_RESULT_INSUFFICIENT_DATA
                                                          : 0;
    }
    if (decoded.status == STATUS_NO_ROOM)
        return AUDIODEC_ERROR_INVALID_PCM_SIZE;
    if (decoded.status != STATUS_OK) {
        ctrl->au_info->au_size = 0;
        ctrl->pcm_item->pcm_size = 0;
        return AUDIODEC_ERROR_API_FAIL;
    }
    ctrl->au_info->au_size = (uint32_t)decoded.consumed;
    ctrl->pcm_item->pcm_size = (uint32_t)decoded.produced;
    return 0;
}
