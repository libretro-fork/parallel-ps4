/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>

#include "common/c_bridge.h"
#include "core/file_sys/fs_c.h"
#include "core/guest_call_c.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/ngs2/ngs2.h"
#include "core/libraries/ngs2/ngs2_core.h"
#include "core/libraries/ngs2/ngs2_custom.h"
#include "core/libraries/ngs2/ngs2_error.h"
#include "core/libraries/ngs2/ngs2_geom.h"
#include "core/libraries/ngs2/ngs2_impl.h"
#include "core/libraries/ngs2/ngs2_lib.h"
#include "core/libraries/ngs2/ngs2_mastering.h"
#include "core/libraries/ngs2/ngs2_pan.h"
#include "core/libraries/ngs2/ngs2_report.h"
#include "core/libraries/ngs2/ngs2_reverb.h"
#include "core/libraries/ngs2/ngs2_sampler.h"
#include "core/libraries/ngs2/ngs2_submixer.h"
#include "core/libraries/ngs2/ngs2_util.h"

/* The game's structures and the engine's are laid out alike */
#define NGS2_SAME_SIZE(a, b, name) typedef char name[sizeof(a) == sizeof(b) ? 1 : -1]
NGS2_SAME_SIZE(ngs2_waveform_info, OrbisNgs2WaveformInfo, ngs2_check_waveform_info);
NGS2_SAME_SIZE(ngs2_pan_work, OrbisNgs2PanWork, ngs2_check_pan_work);
NGS2_SAME_SIZE(ngs2_pan_param, OrbisNgs2PanParam, ngs2_check_pan_param);
NGS2_SAME_SIZE(ngs2_geom_listener, OrbisNgs2GeomListenerParam, ngs2_check_geom_listener);
NGS2_SAME_SIZE(ngs2_geom_work, OrbisNgs2GeomListenerWork, ngs2_check_geom_work);
NGS2_SAME_SIZE(ngs2_geom_source, OrbisNgs2GeomSourceParam, ngs2_check_geom_source);
NGS2_SAME_SIZE(ngs2_geom_attribute, OrbisNgs2GeomAttribute, ngs2_check_geom_attribute);
NGS2_SAME_SIZE(ngs2_user_fx_context, OrbisNgs2UserFxProcessContext, ngs2_check_user_fx);
NGS2_SAME_SIZE(ngs2_user_fx2_setup_context, OrbisNgs2UserFx2SetupContext, ngs2_check_fx2_setup);
NGS2_SAME_SIZE(ngs2_user_fx2_control_context, OrbisNgs2UserFx2ControlContext,
               ngs2_check_fx2_control);
NGS2_SAME_SIZE(ngs2_user_fx2_process_context, OrbisNgs2UserFx2ProcessContext,
               ngs2_check_fx2_process);
NGS2_SAME_SIZE(ngs2_callback_info, OrbisNgs2VoiceCallbackInfo, ngs2_check_callback_info);
NGS2_SAME_SIZE(ngs2_render_buffer, OrbisNgs2RenderBufferInfo, ngs2_check_render_buffer);
NGS2_SAME_SIZE(ngs2_waveform_block, OrbisNgs2WaveformBlock, ngs2_check_waveform_block);
NGS2_SAME_SIZE(ngs2_waveform_format, OrbisNgs2WaveformFormat, ngs2_check_waveform_format);
NGS2_SAME_SIZE(ngs2_port_info, OrbisNgs2VoicePortInfo, ngs2_check_port_info);
NGS2_SAME_SIZE(ngs2_sampler_state, OrbisNgs2SamplerVoiceState, ngs2_check_sampler_state);
NGS2_SAME_SIZE(ngs2_i3dl2, OrbisNgs2ReverbI3DL2Param, ngs2_check_i3dl2);
NGS2_SAME_SIZE(ngs2_param_header, OrbisNgs2VoiceParamHeader, ngs2_check_param_header);

/* Ngs2 */

s32 PS4_SYSV_ABI sceNgs2CalcWaveformBlock(const OrbisNgs2WaveformFormat* format, u32 samplePos,
                                          u32 numSamples, OrbisNgs2WaveformBlock* outBlock) {
    return ngs2_calc_waveform_block((const ngs2_waveform_format*)format, samplePos, numSamples,
                                    (ngs2_waveform_block*)outBlock);
}

s32 PS4_SYSV_ABI sceNgs2GetWaveformFrameInfo(const OrbisNgs2WaveformFormat* format,
                                             u32* outFrameSize, u32* outNumFrameSamples,
                                             u32* outUnitsPerFrame, u32* outNumDelaySamples) {
    return ngs2_waveform_frame_info((const ngs2_waveform_format*)format, outFrameSize,
                                    outNumFrameSamples, outUnitsPerFrame, outNumDelaySamples);
}

s32 PS4_SYSV_ABI sceNgs2ParseWaveformData(const void* data, size_t dataSize,
                                          OrbisNgs2WaveformInfo* outInfo) {
    const s32 result = ngs2_parse_waveform_data(data, dataSize, (ngs2_waveform_info*)outInfo);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ParseWaveformData",
              "dataSize = %llu: 0x%x", (unsigned long long)dataSize, (u32)result);
    }
    return result;
}

typedef struct ngs2_file_source {
    RFILE* file;
    u64 base;
} ngs2_file_source;

static int read_file_source(void* user, uint64_t offset, void* dst, size_t size) {
    const ngs2_file_source* source = (const ngs2_file_source*)user;
    if (filestream_seek(source->file, (s64)(source->base + offset), RETRO_VFS_SEEK_POSITION_START) <
        0) {
        return -1;
    }
    return filestream_read(source->file, dst, (s64)size) == (s64)size ? 0 : -1;
}

typedef struct ngs2_user_source {
    OrbisNgs2ParseReadHandler handler;
    uintptr_t user_data;
} ngs2_user_source;

static int read_user_source(void* user, uint64_t offset, void* dst, size_t size) {
    const ngs2_user_source* source = (const ngs2_user_source*)user;
    return (s32)core_call_guest4((const void*)source->handler, (u64)source->user_data,
                                 (u64)(u32)offset, (u64)(uintptr_t)dst, (u64)size) < 0
               ? -1
               : 0;
}

/* Long enough for any host path the game's can become */
#define NGS2_HOST_PATH_SIZE 4096

s32 PS4_SYSV_ABI sceNgs2ParseWaveformFile(const char* path, u64 offset,
                                          OrbisNgs2WaveformInfo* outInfo) {
    char host[NGS2_HOST_PATH_SIZE];
    RFILE* file;
    s64 size;
    s32 result = ORBIS_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    u32 i;
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    memset(outInfo, 0, sizeof(*outInfo));
    if (!path) {
        return ORBIS_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    }
    (void)fs_guest_host_path(path, host, sizeof(host));
    file = filestream_open(host, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!file) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ParseWaveformFile", "cannot open %s",
              path);
        return ORBIS_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    }
    size = filestream_get_size(file);
    if (size >= 0 && offset <= (u64)size) {
        ngs2_file_source source;
        source.file = file;
        source.base = offset;
        result = ngs2_parse_waveform(read_file_source, &source, (u64)size - offset,
                                     (ngs2_waveform_info*)outInfo);
    }
    filestream_close(file);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ParseWaveformFile",
              "path = %s, offset = %llu: 0x%x", path, (unsigned long long)offset, (u32)result);
        return result;
    }
    /* Where things are in the file, not in the waveform that starts at the offset */
    outInfo->dataOffset += (u32)offset;
    for (i = 0; i < outInfo->numBlocks; i++) {
        outInfo->aBlock[i].dataOffset += (u32)offset;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ParseWaveformUser(OrbisNgs2ParseReadHandler handler, uintptr_t userData,
                                          OrbisNgs2WaveformInfo* outInfo) {
    ngs2_user_source source;
    s32 result;
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (!handler) {
        memset(outInfo, 0, sizeof(*outInfo));
        return ORBIS_NGS2_ERROR_INVALID_WAVEFORM_DATA;
    }
    /* How much there is only the game's reader knows: it refuses what is past the end */
    source.handler = handler;
    source.user_data = userData;
    result =
        ngs2_parse_waveform(read_user_source, &source, 0xffffffffull, (ngs2_waveform_info*)outInfo);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ParseWaveformUser", "0x%x", (u32)result);
    }
    return result;
}

/* What follows is the library over ngs2_core.c: handles, the system's lock, the game's options
 * turned into the engine's, and the calls into the game's code.
 *
 * A system or a rack lives in the buffer the game gives for it: an object in front, the
 * engine's own after. The handle is the engine's. */

/* The lock a system has on the PS4, which sceNgs2SystemLock gives the game itself: one thread
 * at a time in the system's voices, and that thread as often as it likes. Built on retro_atomic
 * and a retro_eventcount: a thread that finds it held waits there. */
typedef struct ngs2_system_lock {
    retro_atomic_ptr_t owner;
    u32 depth; /* the owner's alone */
    retro_eventcount_t free;
} ngs2_system_lock;

/* The calling thread, as the lock's owner: never NULL */
static void* this_thread(void) {
    return (void*)(sthread_get_current_thread_id() | 1);
}

static void lock_init(ngs2_system_lock* lock) {
    retro_atomic_ptr_init(&lock->owner, NULL);
    lock->depth = 0;
    memset(&lock->free, 0, sizeof(lock->free));
    retro_eventcount_init(&lock->free);
}

static int lock_take(void* ctx) {
    ngs2_system_lock* lock = (ngs2_system_lock*)ctx;
    return retro_atomic_cas_ptr(&lock->owner, NULL, this_thread()) ? 1 : 0;
}

static void lock_lock(ngs2_system_lock* lock) {
    if (retro_atomic_load_relaxed_ptr(&lock->owner) == this_thread()) {
        ++lock->depth;
        return;
    }
    /* Not counted running meanwhile: the time moves on for the thread that has it */
    kernel_game_wait(&lock->free, 0, lock_take, lock);
    lock->depth = 1;
}

/* False if the caller does not hold it */
static bool lock_unlock(ngs2_system_lock* lock) {
    if (retro_atomic_load_relaxed_ptr(&lock->owner) != this_thread()) {
        return false;
    }
    if (--lock->depth == 0) {
        (void)retro_atomic_exchange_ptr(&lock->owner, NULL);
        retro_eventcount_notify(&lock->free);
    }
    return true;
}

typedef struct ngs2_object {
    OrbisNgs2ContextBufferInfo buffer; /* as the game gave it */
    OrbisNgs2BufferFreeHandler free;   /* made with an allocator: how it goes back */
    ngs2_system_lock* lock;            /* a system's */
} ngs2_object;

#define NGS2_OBJECT_SIZE ((sizeof(ngs2_object) + sizeof(ngs2_system_lock) + 15) & ~(size_t)15)
/* A buffer may start anywhere: what is laid out in it starts at the next 16 */
#define NGS2_SLACK 16

static ngs2_object* object_of(void* engine) {
    return (ngs2_object*)((u8*)engine - NGS2_OBJECT_SIZE);
}

static u8* aligned(void* buffer) {
    return (u8*)(((uintptr_t)buffer + 15) & ~(uintptr_t)15);
}

static ngs2_system_lock* lock_of(ngs2_system* system) {
    return (ngs2_system_lock*)ngs2_system_host_data(system);
}

static s32 call_guest_buffer(OrbisNgs2BufferAllocHandler handler,
                             OrbisNgs2ContextBufferInfo* buffer) {
    return (s32)core_call_guest4((const void*)handler, (u64)(uintptr_t)buffer, 0, 0, 0);
}

static int host_user_fx(void* handler, ngs2_user_fx_context* context) {
    return (s32)core_call_guest4(handler, (u64)(uintptr_t)context, 0, 0, 0);
}

static int host_user_fx2_setup(void* handler, ngs2_user_fx2_setup_context* context) {
    return (s32)core_call_guest4(handler, (u64)(uintptr_t)context, 0, 0, 0);
}

static int host_user_fx2_cleanup(void* handler, ngs2_user_fx2_setup_context* context) {
    return (s32)core_call_guest4(handler, (u64)(uintptr_t)context, 0, 0, 0);
}

static int host_user_fx2_control(void* handler, ngs2_user_fx2_control_context* context) {
    return (s32)core_call_guest4(handler, (u64)(uintptr_t)context, 0, 0, 0);
}

static int host_user_fx2_process(void* handler, ngs2_user_fx2_process_context* context) {
    return (s32)core_call_guest4(handler, (u64)(uintptr_t)context, 0, 0, 0);
}

static const ngs2_host g_host = {host_user_fx, host_user_fx2_setup, host_user_fx2_cleanup,
                                 host_user_fx2_control, host_user_fx2_process};

typedef struct ngs2_system_setting {
    char name[ORBIS_NGS2_SYSTEM_NAME_LENGTH];
    u32 max_grain;
    u32 grain;
    u32 rate;
} ngs2_system_setting;

static s32 read_system_option(const OrbisNgs2SystemOption* option, ngs2_system_setting* out) {
    memset(out, 0, sizeof(*out));
    out->max_grain = 512;
    out->grain = 256;
    out->rate = 48000;
    if (!option) {
        return ORBIS_OK;
    }
    if (option->size < sizeof(OrbisNgs2SystemOption)) {
        return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
    }
    memcpy(out->name, option->name, sizeof(out->name));
    out->max_grain = option->maxGrainSamples;
    out->grain = option->numGrainSamples;
    out->rate = option->sampleRate;
    if (ngs2_system_size(out->max_grain) == 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    }
    if (out->grain < 64 || out->grain > out->max_grain || out->grain % 64 != 0) {
        return ORBIS_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
    }
    return ORBIS_OK;
}

/* A custom rack's module option as the engine takes it */
static void read_module_option(const OrbisNgs2CustomRackModuleInfo* in, ngs2_module_desc* out) {
    const OrbisNgs2CustomModuleOption* opt;
    out->id = in->moduleId;
    out->source_buffer = in->sourceBufferId;
    out->extra_buffer = in->extraBufferId;
    out->dest_buffer = in->destBufferId;
    out->state_size = in->stateSize;
    out->state_offset = in->stateOffset;
    opt = in->option;
    if (in->moduleId == 0x1f && opt && opt->size >= sizeof(OrbisNgs2CustomUserFx2ModuleOption)) {
        const OrbisNgs2CustomUserFx2ModuleOption* fx =
            (const OrbisNgs2CustomUserFx2ModuleOption*)opt;
        out->setup_handler = (void*)fx->setupHandler;
        out->cleanup_handler = (void*)fx->cleanupHandler;
        out->control_handler = (void*)fx->controlHandler;
        out->process_handler = (void*)fx->processHandler;
        out->common_size = fx->commonSize;
        out->param_size = fx->paramSize;
        out->work_size = fx->workSize;
        out->user_data = fx->userData;
    } else if (in->moduleId == 0x10 && opt &&
               opt->size >= sizeof(OrbisNgs2CustomEnvelopeModuleOption)) {
        out->max_points = ((const OrbisNgs2CustomEnvelopeModuleOption*)opt)->maxPoints;
    } else if (in->moduleId == 0x1a && opt &&
               opt->size >= sizeof(OrbisNgs2CustomPeakMeterModuleOption)) {
        out->max_points = ((const OrbisNgs2CustomPeakMeterModuleOption*)opt)->numBlocks;
    } else if (in->moduleId == 0x1c && opt && opt->size >= 12) {
        /* size, maxPhases, reserved */
        memcpy(&out->max_phases, (const u8*)opt + 4, 4);
    } else if (in->moduleId == 0x1d && opt &&
               opt->size >= sizeof(OrbisNgs2CustomDelayModuleOption)) {
        const OrbisNgs2CustomDelayModuleOption* dl = (const OrbisNgs2CustomDelayModuleOption*)opt;
        out->max_taps = dl->maxTaps;
        out->max_length = dl->maxLength;
    }
}

/* The game's options for a rack as the engine takes them, with the library's values where the
 * game gives none. */
static s32 read_rack_option(u32 rack_id, const OrbisNgs2RackOption* option, ngs2_rack_desc* desc) {
    const OrbisNgs2CustomRackOption* custom = NULL;
    memset(desc, 0, sizeof(*desc));
    desc->rack_id = rack_id;
    desc->max_grain_samples = 512;
    desc->max_voices = 1;
    desc->max_matrices = 1;
    desc->max_ports = 8;
    desc->max_channels = 8;
    desc->max_inputs = 1;
    desc->num_buffers = 1;
    switch (rack_id) {
    case NGS2_RACK_SAMPLER:
        desc->max_voices = 256;
        desc->max_waveform_blocks = 4;
        desc->max_filters = 8;
        desc->max_envelope_points = 8;
        desc->max_atrac9_decoders = 256;
        if (option) {
            const OrbisNgs2SamplerRackOption* o;
            if (option->size < sizeof(OrbisNgs2SamplerRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            o = (const OrbisNgs2SamplerRackOption*)option;
            desc->max_waveform_blocks = o->maxWaveformBlocks;
            desc->max_filters = o->maxFilters;
            desc->max_envelope_points = o->maxEnvelopePoints;
            desc->peak_meter_blocks = o->numPeakMeterBlocks;
            desc->max_atrac9_decoders = o->maxAtrac9Decoders;
        }
        break;
    case NGS2_RACK_SUBMIXER:
        desc->max_filters = 8;
        desc->max_envelope_points = 8;
        if (option) {
            const OrbisNgs2SubmixerRackOption* o;
            if (option->size < sizeof(OrbisNgs2SubmixerRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            o = (const OrbisNgs2SubmixerRackOption*)option;
            desc->max_channels = o->maxChannels;
            desc->max_filters = o->maxFilters;
            desc->max_envelope_points = o->maxEnvelopePoints;
            desc->peak_meter_blocks = o->numPeakMeterBlocks;
            desc->max_inputs = o->maxInputs;
        }
        break;
    case NGS2_RACK_REVERB:
        desc->reverb_size = 1;
        if (option) {
            const OrbisNgs2ReverbRackOption* o;
            if (option->size < sizeof(OrbisNgs2ReverbRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            o = (const OrbisNgs2ReverbRackOption*)option;
            desc->max_channels = o->maxChannels;
            desc->reverb_size = o->reverbSize;
        }
        break;
    case NGS2_RACK_MASTERING:
        desc->max_matrices = 0;
        desc->max_ports = 0;
        if (option) {
            const OrbisNgs2MasteringRackOption* o;
            if (option->size < sizeof(OrbisNgs2MasteringRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            o = (const OrbisNgs2MasteringRackOption*)option;
            desc->max_channels = o->maxChannels;
            desc->peak_meter_blocks = o->numPeakMeterBlocks;
        }
        break;
    case NGS2_RACK_CUSTOM_SAMPLER: {
        const OrbisNgs2CustomSamplerRackOption* o;
        if (!option) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_ADDRESS;
        }
        if (option->size < sizeof(OrbisNgs2CustomSamplerRackOption)) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
        }
        o = (const OrbisNgs2CustomSamplerRackOption*)option;
        custom = &o->customRackOption;
        desc->max_waveform_blocks = o->maxWaveformBlocks;
        desc->max_atrac9_decoders = o->maxAtrac9Decoders;
        break;
    }
    case NGS2_RACK_CUSTOM_SUBMIXER:
    case NGS2_RACK_CUSTOM_MASTERING: {
        const OrbisNgs2CustomSubmixerRackOption* o;
        if (!option) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_ADDRESS;
        }
        if (option->size < sizeof(OrbisNgs2CustomSubmixerRackOption)) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
        }
        o = (const OrbisNgs2CustomSubmixerRackOption*)option;
        custom = &o->customRackOption;
        desc->max_channels = o->maxChannels;
        desc->max_inputs = o->maxInputs;
        break;
    }
    default:
        return ORBIS_NGS2_ERROR_INVALID_RACK_ID;
    }
    if (option) {
        memcpy(desc->name, option->name, sizeof(desc->name));
        desc->max_grain_samples = option->maxGrainSamples;
        desc->max_voices = option->maxVoices;
        desc->max_matrices = option->maxMatrices;
        desc->max_ports = option->maxPorts;
    }
    if (custom) {
        u32 i;
        desc->num_buffers = custom->numBuffers;
        desc->num_modules = custom->numModules;
        if (desc->num_modules > NGS2_MAX_MODULES) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
        }
        for (i = 0; i < desc->num_modules; i++) {
            read_module_option(&custom->aModule[i], &desc->modules[i]);
        }
        for (i = 0; i < NGS2_MAX_CUSTOM_PORTS; i++) {
            desc->port_buffers[i] = custom->aPort[i].sourceBufferId;
        }
    }
    /* More channels than a voice of the engine carries: such a setup is refused, the rest of
     * the rack works */
    if (desc->max_channels > NGS2_MAX_CHANNELS) {
        desc->max_channels = NGS2_MAX_CHANNELS;
    }
    if (desc->max_grain_samples < 64 || desc->max_grain_samples > 2048 ||
        desc->max_grain_samples % 64 != 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    }
    if (desc->max_voices == 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_VOICES;
    }
    if (desc->max_channels == 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_CHANNELS;
    }
    if (ngs2_rack_size(desc) == 0) {
        return ORBIS_NGS2_ERROR_INVALID_OPTION_FLAG;
    }
    return ORBIS_OK;
}

static s32 create_system(const OrbisNgs2SystemOption* option,
                         const OrbisNgs2ContextBufferInfo* buffer, OrbisNgs2BufferFreeHandler free,
                         OrbisNgs2Handle* out_handle) {
    ngs2_system_setting setting;
    u8* base;
    size_t lost;
    size_t need;
    ngs2_system* system;
    ngs2_object* object;
    const s32 result = read_system_option(option, &setting);
    if (result != ORBIS_OK) {
        return result;
    }
    if (!buffer->hostBuffer) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    }
    base = aligned(buffer->hostBuffer);
    lost = (size_t)(base - (u8*)buffer->hostBuffer);
    need = NGS2_OBJECT_SIZE + ngs2_system_size(setting.max_grain);
    if (buffer->hostBufferSize < lost + need) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_SIZE;
    }
    system = ngs2_system_init(base + NGS2_OBJECT_SIZE, need - NGS2_OBJECT_SIZE, setting.name,
                              setting.max_grain, setting.grain, setting.rate, &g_host);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SAMPLE_RATE;
    }
    object = (ngs2_object*)base;
    object->buffer = *buffer;
    object->free = free;
    object->lock = (ngs2_system_lock*)(base + sizeof(ngs2_object));
    lock_init(object->lock);
    ngs2_system_set_host_data(system, object->lock);
    *out_handle = (OrbisNgs2Handle)system;
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_INFO, "CreateSystem",
          "system '%s': %u Hz, grains of %u (at most %u)", setting.name, setting.rate,
          setting.grain, setting.max_grain);
    return ORBIS_OK;
}

static s32 create_rack(ngs2_system* system, u32 rack_id, const OrbisNgs2RackOption* option,
                       const OrbisNgs2ContextBufferInfo* buffer, OrbisNgs2BufferFreeHandler free,
                       OrbisNgs2Handle* out_handle) {
    ngs2_rack_desc desc;
    u8* base;
    size_t lost;
    size_t need;
    ngs2_rack* rack;
    ngs2_object* object;
    ngs2_system_lock* lock;
    u32 i;
    const s32 result = read_rack_option(rack_id, option, &desc);
    if (result != ORBIS_OK) {
        return result;
    }
    if (!buffer->hostBuffer) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    }
    base = aligned(buffer->hostBuffer);
    lost = (size_t)(base - (u8*)buffer->hostBuffer);
    need = NGS2_OBJECT_SIZE + ngs2_rack_size(&desc);
    if (buffer->hostBufferSize < lost + need) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_SIZE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    rack = ngs2_rack_init(system, base + NGS2_OBJECT_SIZE, need - NGS2_OBJECT_SIZE, &desc);
    if (!rack) {
        lock_unlock(lock);
        return ORBIS_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    }
    object = (ngs2_object*)base;
    object->buffer = *buffer;
    object->free = free;
    object->lock = NULL;
    *out_handle = (OrbisNgs2Handle)rack;
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_INFO, "CreateRack",
          "rack 0x%x '%s': %u voices, %u channels, %u ports, %u matrices, %u modules", rack_id,
          desc.name, desc.max_voices, desc.max_channels, desc.max_ports, desc.max_matrices,
          desc.num_modules);
    for (i = 0; i < desc.num_modules; i++) {
        const ngs2_module_desc* m = &desc.modules[i];
        /* The modules the engine has; another passes its sound on as it came */
        const bool known =
            m->id == 0x13 || m->id == 0x15 || m->id == 0x19 || m->id == 0x1b || m->id == 0x1f;
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_INFO, "CreateRack",
              "  module %u: 0x%x%s, buffer %u to %u", i, m->id,
              known ? "" : " (not in: passed through)", m->source_buffer, m->dest_buffer);
    }
    lock_unlock(lock);
    return ORBIS_OK;
}

/* Takes a rack out and gives its buffer back the way it came */
static void destroy_rack(ngs2_rack* rack, OrbisNgs2ContextBufferInfo* out_buffer) {
    size_t size;
    const ngs2_object* object = object_of(ngs2_rack_memory(rack, &size));
    const ngs2_object copy = *object;
    ngs2_rack_destroy(rack);
    if (out_buffer) {
        *out_buffer = copy.buffer;
    }
    if (copy.free) {
        OrbisNgs2ContextBufferInfo buffer = copy.buffer;
        (void)call_guest_buffer(copy.free, &buffer);
    }
}

static ngs2_system* system_of_voice(OrbisNgs2Handle handle) {
    ngs2_voice* const voice = ngs2_voice_from(handle);
    return voice ? ngs2_rack_system(ngs2_voice_rack(voice)) : NULL;
}

s32 PS4_SYSV_ABI sceNgs2RackCreate(OrbisNgs2Handle systemHandle, u32 rackId,
                                   const OrbisNgs2RackOption* option,
                                   const OrbisNgs2ContextBufferInfo* bufferInfo,
                                   OrbisNgs2Handle* outHandle) {
    s32 result;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    if (!bufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_INFO;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    result = create_rack(system, rackId, option, bufferInfo, NULL, outHandle);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2RackCreate", "rack 0x%x not made: 0x%x",
              rackId, (u32)result);
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2RackCreateWithAllocator(OrbisNgs2Handle systemHandle, u32 rackId,
                                                const OrbisNgs2RackOption* option,
                                                const OrbisNgs2BufferAllocator* allocator,
                                                OrbisNgs2Handle* outHandle) {
    OrbisNgs2ContextBufferInfo buffer;
    s32 result;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    if (!allocator || !allocator->allocHandler) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ALLOCATOR;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    memset(&buffer, 0, sizeof(buffer));
    result = sceNgs2RackQueryBufferSize(rackId, option, &buffer);
    if (result != ORBIS_OK) {
        return result;
    }
    buffer.userData = allocator->userData;
    result = call_guest_buffer(allocator->allocHandler, &buffer);
    if (result < 0) {
        return result;
    }
    result = create_rack(system, rackId, option, &buffer, allocator->freeHandler, outHandle);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2RackCreateWithAllocator",
              "rack 0x%x not made: 0x%x", rackId, (u32)result);
        if (allocator->freeHandler) {
            (void)call_guest_buffer(allocator->freeHandler, &buffer);
        }
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2RackDestroy(OrbisNgs2Handle rackHandle,
                                    OrbisNgs2ContextBufferInfo* outBufferInfo) {
    ngs2_system_lock* lock;
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    lock = lock_of(ngs2_rack_system(rack));
    lock_lock(lock);
    destroy_rack(rack, outBufferInfo);
    lock_unlock(lock);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackGetInfo(OrbisNgs2Handle rackHandle, OrbisNgs2RackInfo* outInfo,
                                    size_t infoSize) {
    ngs2_system_lock* lock;
    const ngs2_rack_desc* desc;
    size_t size;
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (infoSize < sizeof(OrbisNgs2RackInfo)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    lock = lock_of(ngs2_rack_system(rack));
    lock_lock(lock);
    desc = ngs2_rack_description(rack);
    memset(outInfo, 0, infoSize);
    memcpy(outInfo->name, desc->name, sizeof(outInfo->name));
    outInfo->rackHandle = rackHandle;
    outInfo->bufferInfo = object_of(ngs2_rack_memory(rack, &size))->buffer;
    outInfo->ownerSystemHandle = (OrbisNgs2Handle)ngs2_rack_system(rack);
    outInfo->type = desc->rack_id >> 12;
    outInfo->rackId = desc->rack_id;
    outInfo->minGrainSamples = 64;
    outInfo->maxGrainSamples = desc->max_grain_samples;
    outInfo->maxVoices = desc->max_voices;
    outInfo->maxInputs = desc->max_inputs;
    outInfo->maxMatrices = desc->max_matrices;
    outInfo->maxPorts = desc->max_ports;
    outInfo->renderCount = ngs2_system_render_count(ngs2_rack_system(rack));
    outInfo->activeVoiceCount = ngs2_rack_active_voices(rack);
    lock_unlock(lock);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackGetUserData(OrbisNgs2Handle rackHandle, uintptr_t* outUserData) {
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    if (!outUserData) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    *outUserData = ngs2_rack_user_data(rack);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackGetVoiceHandle(OrbisNgs2Handle rackHandle, u32 voiceIndex,
                                           OrbisNgs2Handle* outHandle) {
    ngs2_voice* voice;
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    voice = ngs2_rack_voice(rack, voiceIndex);
    if (!voice) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_INDEX;
    }
    *outHandle = (OrbisNgs2Handle)voice;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackLock(OrbisNgs2Handle rackHandle) {
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    lock_lock(lock_of(ngs2_rack_system(rack)));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackQueryBufferSize(u32 rackId, const OrbisNgs2RackOption* option,
                                            OrbisNgs2ContextBufferInfo* outBufferInfo) {
    ngs2_rack_desc desc;
    s32 result;
    if (!outBufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    result = read_rack_option(rackId, option, &desc);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2RackQueryBufferSize", "rack 0x%x: 0x%x",
              rackId, (u32)result);
        return result;
    }
    memset(outBufferInfo, 0, sizeof(*outBufferInfo));
    outBufferInfo->hostBufferSize = NGS2_SLACK + NGS2_OBJECT_SIZE + ngs2_rack_size(&desc);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackSetUserData(OrbisNgs2Handle rackHandle, uintptr_t userData) {
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    ngs2_rack_set_user_data(rack, userData);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackUnlock(OrbisNgs2Handle rackHandle) {
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    return lock_unlock(lock_of(ngs2_rack_system(rack))) ? ORBIS_OK
                                                        : ORBIS_NGS2_ERROR_INVALID_OPERATION;
}

s32 PS4_SYSV_ABI sceNgs2SystemCreate(const OrbisNgs2SystemOption* option,
                                     const OrbisNgs2ContextBufferInfo* bufferInfo,
                                     OrbisNgs2Handle* outHandle) {
    s32 result;
    if (!bufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_INFO;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    result = create_system(option, bufferInfo, NULL, outHandle);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2SystemCreate", "system not made: 0x%x",
              (u32)result);
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2SystemCreateWithAllocator(const OrbisNgs2SystemOption* option,
                                                  const OrbisNgs2BufferAllocator* allocator,
                                                  OrbisNgs2Handle* outHandle) {
    OrbisNgs2ContextBufferInfo buffer;
    s32 result;
    if (!allocator || !allocator->allocHandler) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ALLOCATOR;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    memset(&buffer, 0, sizeof(buffer));
    result = sceNgs2SystemQueryBufferSize(option, &buffer);
    if (result != ORBIS_OK) {
        return result;
    }
    buffer.userData = allocator->userData;
    result = call_guest_buffer(allocator->allocHandler, &buffer);
    if (result < 0) {
        return result;
    }
    result = create_system(option, &buffer, allocator->freeHandler, outHandle);
    if (result != ORBIS_OK) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2SystemCreateWithAllocator",
              "system not made: 0x%x", (u32)result);
        if (allocator->freeHandler) {
            (void)call_guest_buffer(allocator->freeHandler, &buffer);
        }
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2SystemDestroy(OrbisNgs2Handle systemHandle,
                                      OrbisNgs2ContextBufferInfo* outBufferInfo) {
    size_t size;
    ngs2_object copy;
    ngs2_rack* rack;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    copy = *object_of(ngs2_system_memory(system, &size));
    lock_lock(copy.lock);
    while ((rack = ngs2_system_rack(system, 0)) != NULL) {
        destroy_rack(rack, NULL);
    }
    ngs2_system_destroy(system);
    lock_unlock(copy.lock);
    retro_eventcount_free(&copy.lock->free);
    if (outBufferInfo) {
        *outBufferInfo = copy.buffer;
    }
    if (copy.free) {
        OrbisNgs2ContextBufferInfo buffer = copy.buffer;
        (void)call_guest_buffer(copy.free, &buffer);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemEnumHandles(OrbisNgs2Handle* aOutHandle, u32 maxHandles) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2SystemEnumHandles",
          "(STUBBED) maxHandles = %u", maxHandles);
    return 0;
}

s32 PS4_SYSV_ABI sceNgs2SystemEnumRackHandles(OrbisNgs2Handle systemHandle,
                                              OrbisNgs2Handle* aOutHandle, u32 maxHandles) {
    ngs2_system_lock* lock;
    ngs2_rack* rack;
    u32 count = 0;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    while ((rack = ngs2_system_rack(system, count)) != NULL) {
        if (aOutHandle && count < maxHandles) {
            aOutHandle[count] = (OrbisNgs2Handle)rack;
        }
        count++;
    }
    lock_unlock(lock);
    return (s32)count;
}

s32 PS4_SYSV_ABI sceNgs2SystemGetInfo(OrbisNgs2Handle systemHandle, OrbisNgs2SystemInfo* outInfo,
                                      size_t infoSize) {
    ngs2_system_lock* lock;
    size_t size;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (infoSize < sizeof(OrbisNgs2SystemInfo)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    memset(outInfo, 0, infoSize);
    outInfo->systemHandle = systemHandle;
    outInfo->bufferInfo = object_of(ngs2_system_memory(system, &size))->buffer;
    outInfo->minGrainSamples = 64;
    outInfo->maxGrainSamples = ngs2_system_grain_samples(system);
    outInfo->rackCount = ngs2_system_rack_count(system);
    outInfo->renderCount = (s64)ngs2_system_render_count(system);
    outInfo->sampleRate = ngs2_system_sample_rate(system);
    outInfo->numGrainSamples = ngs2_system_grain_samples(system);
    lock_unlock(lock);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemGetUserData(OrbisNgs2Handle systemHandle, uintptr_t* outUserData) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    if (!outUserData) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    *outUserData = ngs2_system_user_data(system);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemLock(OrbisNgs2Handle systemHandle) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    lock_lock(lock_of(system));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemQueryBufferSize(const OrbisNgs2SystemOption* option,
                                              OrbisNgs2ContextBufferInfo* outBufferInfo) {
    ngs2_system_setting setting;
    s32 result;
    if (!outBufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    result = read_system_option(option, &setting);
    if (result != ORBIS_OK) {
        return result;
    }
    memset(outBufferInfo, 0, sizeof(*outBufferInfo));
    outBufferInfo->hostBufferSize =
        NGS2_SLACK + NGS2_OBJECT_SIZE + ngs2_system_size(setting.max_grain);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemRender(OrbisNgs2Handle systemHandle,
                                     const OrbisNgs2RenderBufferInfo* aBufferInfo,
                                     u32 numBufferInfo) {
    ngs2_system_lock* lock;
    s32 result;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    if (numBufferInfo && !aBufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    }
    lock = lock_of(system);
    lock_lock(lock);
    result = ngs2_system_render(system, (const ngs2_render_buffer*)aBufferInfo, numBufferInfo);
    /* The game hears of the blocks that ended with the lock let go: what it does there may
     * wait on a thread of its own that is setting a voice */
    for (;;) {
        void* handler;
        ngs2_callback_info info;
        if (!ngs2_system_next_callback(system, &handler, &info)) {
            break;
        }
        lock_unlock(lock);
        (void)core_call_guest4(handler, (u64)(uintptr_t)&info, 0, 0, 0);
        lock_lock(lock);
    }
    lock_unlock(lock);
    return result;
}

s32 PS4_SYSV_ABI sceNgs2SystemResetOption(OrbisNgs2SystemOption* outOption) {
    if (!outOption) {
        return ORBIS_NGS2_ERROR_INVALID_OPTION_ADDRESS;
    }
    memset(outOption, 0, sizeof(*outOption));
    outOption->size = sizeof(OrbisNgs2SystemOption);
    outOption->maxGrainSamples = 512;
    outOption->numGrainSamples = 256;
    outOption->sampleRate = 48000;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemSetGrainSamples(OrbisNgs2Handle systemHandle, u32 numSamples) {
    ngs2_system_lock* lock;
    bool ok;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    ok = ngs2_system_set_grain_samples(system, numSamples) != 0;
    lock_unlock(lock);
    return ok ? ORBIS_OK : ORBIS_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
}

s32 PS4_SYSV_ABI sceNgs2SystemSetSampleRate(OrbisNgs2Handle systemHandle, u32 sampleRate) {
    ngs2_system_lock* lock;
    bool ok;
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    ok = ngs2_system_set_sample_rate(system, sampleRate) != 0;
    lock_unlock(lock);
    return ok ? ORBIS_OK : ORBIS_NGS2_ERROR_INVALID_SAMPLE_RATE;
}

s32 PS4_SYSV_ABI sceNgs2SystemSetUserData(OrbisNgs2Handle systemHandle, uintptr_t userData) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    ngs2_system_set_user_data(system, userData);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemUnlock(OrbisNgs2Handle systemHandle) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    return lock_unlock(lock_of(system)) ? ORBIS_OK : ORBIS_NGS2_ERROR_INVALID_OPERATION;
}

s32 PS4_SYSV_ABI sceNgs2VoiceControl(OrbisNgs2Handle voiceHandle,
                                     const OrbisNgs2VoiceParamHeader* paramList) {
    ngs2_system_lock* lock;
    ngs2_voice* voice;
    s32 result;
    ngs2_system* const system = system_of_voice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    voice = ngs2_voice_from(voiceHandle);
    if (!voice) {
        lock_unlock(lock);
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    result = ngs2_voice_control(voice, (const ngs2_param_header*)paramList);
    if (result != ORBIS_OK) {
        /* The list's first parameter and its first word, to tell which call of the game's it
         * was */
        const OrbisNgs2VoiceParamHeader* param = paramList;
        u32 word = 0;
        if (param && param->size >= 12) {
            memcpy(&word, param + 1, sizeof(word));
        }
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2VoiceControl",
              "voice %u of rack 0x%x: 0x%x (list starts with 0x%x, 0x%x)", ngs2_voice_index(voice),
              ngs2_rack_description(ngs2_voice_rack(voice))->rack_id, (u32)result,
              param ? param->id : 0, word);
        /* A patch refused: where it was to go, to tell which of its checks failed */
        if (result == ORBIS_NGS2_ERROR_INVALID_PATCH && param && param->id == 5 &&
            param->size >= 24) {
            u32 input;
            uintptr_t handle;
            const ngs2_voice* dest;
            memcpy(&input, (const u8*)param + 12, sizeof(input));
            memcpy(&handle, (const u8*)param + 16, sizeof(handle));
            dest = ngs2_voice_from(handle);
            if (!dest) {
                c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2VoiceControl",
                      "  patch to handle 0x%llx, input %u: not a voice", (unsigned long long)handle,
                      input);
            } else {
                const ngs2_rack* dest_rack = ngs2_voice_rack(dest);
                const ngs2_rack_desc* d = ngs2_rack_description(dest_rack);
                c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2VoiceControl",
                      "  patch to voice %u of rack 0x%x, input %u (inputs %u, buffers %u)%s%s",
                      ngs2_voice_index(dest), d->rack_id, input, d->max_inputs, d->num_buffers,
                      dest == voice ? ", itself" : "",
                      ngs2_rack_system(dest_rack) != system ? ", another system" : "");
            }
        }
    }
    lock_unlock(lock);
    return result;
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetMatrixInfo(OrbisNgs2Handle voiceHandle, u32 matrixId,
                                           OrbisNgs2VoiceMatrixInfo* outInfo, size_t outInfoSize) {
    ngs2_system_lock* lock;
    s32 result;
    ngs2_system* const system = system_of_voice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (outInfoSize < sizeof(OrbisNgs2VoiceMatrixInfo)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    memset(outInfo, 0, sizeof(*outInfo));
    result = ngs2_voice_matrix_info(ngs2_voice_from(voiceHandle), matrixId, &outInfo->numLevels,
                                    outInfo->aLevel);
    lock_unlock(lock);
    return result;
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetOwner(OrbisNgs2Handle voiceHandle, OrbisNgs2Handle* outRackHandle,
                                      u32* outVoiceId) {
    ngs2_voice* const voice = ngs2_voice_from(voiceHandle);
    if (!voice) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (outRackHandle) {
        *outRackHandle = (OrbisNgs2Handle)ngs2_voice_rack(voice);
    }
    if (outVoiceId) {
        *outVoiceId = ngs2_voice_index(voice);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetPortInfo(OrbisNgs2Handle voiceHandle, u32 port,
                                         OrbisNgs2VoicePortInfo* outInfo, size_t outInfoSize) {
    ngs2_system_lock* lock;
    s32 result;
    ngs2_system* const system = system_of_voice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (outInfoSize < sizeof(OrbisNgs2VoicePortInfo)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    result = ngs2_voice_port_info(ngs2_voice_from(voiceHandle), port, (ngs2_port_info*)outInfo);
    lock_unlock(lock);
    return result;
}

/* As much of the rack's kind of state as the game has room for */
static void give_state(OrbisNgs2VoiceState* out, size_t out_size, const void* state, size_t size) {
    memset(out, 0, out_size);
    memcpy(out, state, out_size < size ? out_size : size);
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetState(OrbisNgs2Handle voiceHandle, OrbisNgs2VoiceState* outState,
                                      size_t stateSize) {
    ngs2_system_lock* lock;
    const ngs2_voice* voice;
    const ngs2_rack_desc* desc;
    ngs2_sampler_state sampler;
    ngs2_mixer_state mixer;
    u32 i;
    ngs2_system* const system = system_of_voice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outState) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (stateSize < sizeof(OrbisNgs2VoiceState)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    lock = lock_of(system);
    lock_lock(lock);
    voice = ngs2_voice_from(voiceHandle);
    ngs2_voice_sampler_state(voice, &sampler);
    ngs2_voice_mixer_state(voice, &mixer);
    switch (ngs2_rack_description(ngs2_voice_rack(voice))->rack_id) {
    case NGS2_RACK_SAMPLER:
        give_state(outState, stateSize, &sampler, sizeof(sampler));
        break;
    case NGS2_RACK_CUSTOM_SAMPLER: {
        OrbisNgs2CustomSamplerVoiceState state;
        memset(&state, 0, sizeof(state));
        state.voiceState.stateFlags = sampler.state_flags;
        state.waveformData = sampler.waveform_data;
        state.numDecodedSamples = sampler.decoded_samples;
        state.decodedDataSize = sampler.decoded_bytes;
        state.userData = sampler.user_data;
        give_state(outState, stateSize, &state, sizeof(state));
        break;
    }
    case NGS2_RACK_SUBMIXER: {
        OrbisNgs2SubmixerVoiceState state;
        memset(&state, 0, sizeof(state));
        state.voiceState.stateFlags = sampler.state_flags;
        state.envelopeHeight = mixer.envelope_height;
        state.peakHeight = mixer.peak_height;
        state.compressorHeight = mixer.compressor_height;
        give_state(outState, stateSize, &state, sizeof(state));
        break;
    }
    case NGS2_RACK_MASTERING: {
        OrbisNgs2MasteringVoiceState state;
        memset(&state, 0, sizeof(state));
        state.voiceState.stateFlags = sampler.state_flags;
        state.limiterPeakLevel = mixer.limiter_peak;
        state.limiterPressLevel = mixer.limiter_press;
        memcpy(state.aInputPeakHeight, mixer.input_peak, sizeof(state.aInputPeakHeight));
        memcpy(state.aOutputPeakHeight, mixer.output_peak, sizeof(state.aOutputPeakHeight));
        give_state(outState, stateSize, &state, sizeof(state));
        break;
    }
    default: {
        OrbisNgs2VoiceState state;
        memset(&state, 0, sizeof(state));
        state.stateFlags = sampler.state_flags;
        give_state(outState, stateSize, &state, sizeof(state));
        break;
    }
    }
    /* A custom rack's modules that have a state, where the game said each is to go */
    desc = ngs2_rack_description(ngs2_voice_rack(voice));
    for (i = 0; i < desc->num_modules; i++) {
        const ngs2_module_desc* md = &desc->modules[i];
        size_t room;
        if (!md->state_size || md->state_offset < sizeof(OrbisNgs2VoiceState) ||
            md->state_offset > stateSize) {
            continue;
        }
        room = stateSize - md->state_offset;
        ngs2_voice_module_state(voice, i, (u8*)(void*)outState + md->state_offset,
                                md->state_size < room ? md->state_size : room);
    }
    lock_unlock(lock);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetStateFlags(OrbisNgs2Handle voiceHandle, u32* outStateFlags) {
    ngs2_system_lock* lock;
    ngs2_system* const system = system_of_voice(voiceHandle);
    if (!system) {
        if (outStateFlags) {
            *outStateFlags = 0;
        }
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outStateFlags) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    lock = lock_of(system);
    lock_lock(lock);
    *outStateFlags = ngs2_voice_state_flags(ngs2_voice_from(voiceHandle));
    lock_unlock(lock);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2CustomRackGetModuleInfo(OrbisNgs2Handle rackHandle, u32 moduleIndex,
                                                OrbisNgs2CustomModuleInfo* outInfo,
                                                size_t infoSize) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2CustomRackGetModuleInfo",
          "moduleIndex = %u, infoSize = %llu", moduleIndex, (unsigned long long)infoSize);
    return ORBIS_OK;
}

/* Ngs2Geom */

s32 PS4_SYSV_ABI sceNgs2GeomResetListenerParam(OrbisNgs2GeomListenerParam* outListenerParam) {
    return ngs2_geom_reset_listener((ngs2_geom_listener*)outListenerParam);
}

s32 PS4_SYSV_ABI sceNgs2GeomResetSourceParam(OrbisNgs2GeomSourceParam* outSourceParam) {
    return ngs2_geom_reset_source((ngs2_geom_source*)outSourceParam);
}

s32 PS4_SYSV_ABI sceNgs2GeomCalcListener(const OrbisNgs2GeomListenerParam* param,
                                         OrbisNgs2GeomListenerWork* outWork, u32 flags) {
    return ngs2_geom_calc_listener((const ngs2_geom_listener*)param, (ngs2_geom_work*)outWork,
                                   flags);
}

s32 PS4_SYSV_ABI sceNgs2GeomApply(const OrbisNgs2GeomListenerWork* listener,
                                  const OrbisNgs2GeomSourceParam* source,
                                  OrbisNgs2GeomAttribute* outAttrib, u32 flags) {
    return ngs2_geom_apply((const ngs2_geom_work*)listener, (const ngs2_geom_source*)source,
                           (ngs2_geom_attribute*)outAttrib, flags);
}

/* Ngs2Pan */

s32 PS4_SYSV_ABI sceNgs2PanInit(OrbisNgs2PanWork* work, const float* aSpeakerAngle, float unitAngle,
                                u32 numSpeakers) {
    return ngs2_pan_init((ngs2_pan_work*)work, aSpeakerAngle, unitAngle, numSpeakers);
}

s32 PS4_SYSV_ABI sceNgs2PanGetVolumeMatrix(OrbisNgs2PanWork* work, const OrbisNgs2PanParam* aParam,
                                           u32 numParams, u32 matrixFormat,
                                           float* outVolumeMatrix) {
    return ngs2_pan_matrix((const ngs2_pan_work*)work, (const ngs2_pan_param*)aParam, numParams,
                           matrixFormat, outVolumeMatrix);
}

/* Ngs2Report */

s32 PS4_SYSV_ABI sceNgs2ReportRegisterHandler(u32 reportType, OrbisNgs2ReportHandler handler,
                                              uintptr_t userData, OrbisNgs2Handle* outHandle) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_INFO, "sceNgs2ReportRegisterHandler",
          "reportType = %u, userData = %llu", reportType, (unsigned long long)userData);
    if (!handler) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ReportRegisterHandler",
              "handler is nullptr");
        return ORBIS_NGS2_ERROR_INVALID_REPORT_HANDLE;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ReportUnregisterHandler(OrbisNgs2Handle reportHandle) {
    if (!reportHandle) {
        c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ReportUnregisterHandler",
              "reportHandle is nullptr");
        return ORBIS_NGS2_ERROR_INVALID_REPORT_HANDLE;
    }
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_INFO, "sceNgs2ReportUnregisterHandler", "called");
    return ORBIS_OK;
}

/* Ngs2Custom, Ngs2Fft, Ngs2Stream and the rest: not in yet */

s32 PS4_SYSV_ABI sceNgs2FftInit(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2FftInit", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2FftProcess(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2FftProcess", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2FftQuerySize(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2FftQuerySize", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2JobSchedulerResetOption(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2JobSchedulerResetOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ModuleArrayEnumItems(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ModuleArrayEnumItems", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ModuleEnumConfigs(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ModuleEnumConfigs", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ModuleQueueEnumItems(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2ModuleQueueEnumItems", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackQueryInfo(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2RackQueryInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackRunCommands(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2RackRunCommands", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemQueryInfo(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2SystemQueryInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemRunCommands(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2SystemRunCommands", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemSetLoudThreshold(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2SystemSetLoudThreshold", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2StreamCreate(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2StreamCreate", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2StreamCreateWithAllocator(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2StreamCreateWithAllocator",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2StreamDestroy(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2StreamDestroy", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2StreamQueryBufferSize(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2StreamQueryBufferSize", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2StreamQueryInfo(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2StreamQueryInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2StreamResetOption(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2StreamResetOption", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2StreamRunCommands(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2StreamRunCommands", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2VoiceQueryInfo(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2VoiceQueryInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2VoiceRunCommands(void) {
    c_log(C_LOG_LIB_NGS2, C_LOG_LEVEL_ERROR, "sceNgs2VoiceRunCommands", "(STUBBED) called");
    return ORBIS_OK;
}
