// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>
#include <new>
#include <retro_atomic.h>
#include "common/event_count.h"
#include "common/logging/log.h"
#include "core/guest_call.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "core/libraries/ngs2/ngs2.h"
#include "core/libraries/ngs2/ngs2_core.h"
#include "core/libraries/ngs2/ngs2_custom.h"
#include "core/libraries/ngs2/ngs2_error.h"
#include "core/libraries/ngs2/ngs2_geom.h"
#include "core/libraries/ngs2/ngs2_impl.h"
#include "core/libraries/ngs2/ngs2_mastering.h"
#include "core/libraries/ngs2/ngs2_pan.h"
#include "core/libraries/ngs2/ngs2_report.h"
#include "core/libraries/ngs2/ngs2_reverb.h"
#include "core/libraries/ngs2/ngs2_sampler.h"
#include "core/libraries/ngs2/ngs2_submixer.h"

namespace Libraries::Ngs2 {

// Ngs2

s32 PS4_SYSV_ABI sceNgs2CalcWaveformBlock(const OrbisNgs2WaveformFormat* format, u32 samplePos,
                                          u32 numSamples, OrbisNgs2WaveformBlock* outBlock) {
    LOG_ERROR(Lib_Ngs2, "samplePos = {}, numSamples = {}", samplePos, numSamples);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2GetWaveformFrameInfo(const OrbisNgs2WaveformFormat* format,
                                             u32* outFrameSize, u32* outNumFrameSamples,
                                             u32* outUnitsPerFrame, u32* outNumDelaySamples) {
    LOG_ERROR(Lib_Ngs2, "called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ParseWaveformData(const void* data, size_t dataSize,
                                          OrbisNgs2WaveformInfo* outInfo) {
    LOG_ERROR(Lib_Ngs2, "dataSize = {}", dataSize);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ParseWaveformFile(const char* path, u64 offset,
                                          OrbisNgs2WaveformInfo* outInfo) {
    LOG_ERROR(Lib_Ngs2, "path = {}, offset = {}", path, offset);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ParseWaveformUser(OrbisNgs2ParseReadHandler handler, uintptr_t userData,
                                          OrbisNgs2WaveformInfo* outInfo) {
    LOG_ERROR(Lib_Ngs2, "userData = {}", userData);
    return ORBIS_OK;
}

// What follows is the library over ngs2_core.c: handles, the system's lock, the game's options
// turned into the engine's, and the calls into the game's code.
//
// A system or a rack lives in the buffer the game gives for it: an Object in front, the engine's
// own after. The handle is the engine's.

namespace {

/// The lock a system has on the PS4, which sceNgs2SystemLock gives the game itself: one thread
/// at a time in the system's voices, and that thread as often as it likes. Built on
/// retro_atomic and a retro_eventcount: a thread that finds it held waits there.
class SystemLock {
public:
    SystemLock() {
        retro_atomic_ptr_init(&owner, nullptr);
    }

    void Lock() {
        void* const self = ThisThread();
        if (retro_atomic_load_relaxed_ptr(&owner) == self) {
            ++depth;
            return;
        }
        free.Wait([&] { return retro_atomic_cas_ptr(&owner, nullptr, self); });
        depth = 1;
    }

    /// False if the caller does not hold it
    bool Unlock() {
        if (retro_atomic_load_relaxed_ptr(&owner) != ThisThread()) {
            return false;
        }
        if (--depth == 0) {
            (void)retro_atomic_exchange_ptr(&owner, nullptr);
            free.Notify();
        }
        return true;
    }

private:
    static void* ThisThread() {
        static thread_local char tag;
        return &tag;
    }

    retro_atomic_ptr_t owner;
    u32 depth{0}; // the owner's alone
    Common::EventCount free;
};

struct Object {
    OrbisNgs2ContextBufferInfo buffer; // as the game gave it
    OrbisNgs2BufferFreeHandler free;   // made with an allocator: how it goes back
    SystemLock* lock;                  // a system's
};

constexpr size_t ObjectSize = (sizeof(Object) + sizeof(SystemLock) + 15) & ~size_t{15};
// A buffer may start anywhere: what is laid out in it starts at the next 16
constexpr size_t Slack = 16;

Object* ObjectOf(void* engine) {
    return reinterpret_cast<Object*>(static_cast<u8*>(engine) - ObjectSize);
}

u8* Aligned(void* buffer) {
    return reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(buffer) + 15) & ~uintptr_t{15});
}

SystemLock& LockOf(ngs2_system* system) {
    return *static_cast<SystemLock*>(ngs2_system_host_data(system));
}

struct Locked {
    explicit Locked(ngs2_system* system) : lock{LockOf(system)} {
        lock.Lock();
    }
    ~Locked() {
        lock.Unlock();
    }
    SystemLock& lock;
};

int HostUserFx(void* handler, ngs2_user_fx_context* context) {
    return Core::CallGuest(reinterpret_cast<OrbisNgs2UserFxProcessHandler>(handler),
                           reinterpret_cast<OrbisNgs2UserFxProcessContext*>(context));
}

int HostUserFx2Setup(void* handler, ngs2_user_fx2_setup_context* context) {
    return Core::CallGuest(reinterpret_cast<OrbisNgs2UserFx2SetupHandler>(handler),
                           reinterpret_cast<OrbisNgs2UserFx2SetupContext*>(context));
}

int HostUserFx2Cleanup(void* handler, ngs2_user_fx2_setup_context* context) {
    return Core::CallGuest(reinterpret_cast<OrbisNgs2UserFx2CleanupHandler>(handler),
                           reinterpret_cast<OrbisNgs2UserFx2CleanupContext*>(context));
}

int HostUserFx2Control(void* handler, ngs2_user_fx2_control_context* context) {
    return Core::CallGuest(reinterpret_cast<OrbisNgs2UserFx2ControlHandler>(handler),
                           reinterpret_cast<OrbisNgs2UserFx2ControlContext*>(context));
}

int HostUserFx2Process(void* handler, ngs2_user_fx2_process_context* context) {
    return Core::CallGuest(reinterpret_cast<OrbisNgs2UserFx2ProcessHandler>(handler),
                           reinterpret_cast<OrbisNgs2UserFx2ProcessContext*>(context));
}

constexpr ngs2_host Host = {HostUserFx, HostUserFx2Setup, HostUserFx2Cleanup, HostUserFx2Control,
                            HostUserFx2Process};

static_assert(sizeof(ngs2_user_fx_context) == sizeof(OrbisNgs2UserFxProcessContext));
static_assert(sizeof(ngs2_user_fx2_setup_context) == sizeof(OrbisNgs2UserFx2SetupContext));
static_assert(sizeof(ngs2_user_fx2_control_context) == sizeof(OrbisNgs2UserFx2ControlContext));
static_assert(sizeof(ngs2_user_fx2_process_context) == sizeof(OrbisNgs2UserFx2ProcessContext));
static_assert(sizeof(ngs2_callback_info) == sizeof(OrbisNgs2VoiceCallbackInfo));
static_assert(sizeof(ngs2_render_buffer) == sizeof(OrbisNgs2RenderBufferInfo));
static_assert(sizeof(ngs2_waveform_block) == sizeof(OrbisNgs2WaveformBlock));
static_assert(sizeof(ngs2_waveform_format) == sizeof(OrbisNgs2WaveformFormat));
static_assert(sizeof(ngs2_port_info) == sizeof(OrbisNgs2VoicePortInfo));
static_assert(sizeof(ngs2_sampler_state) == sizeof(OrbisNgs2SamplerVoiceState));
static_assert(sizeof(ngs2_i3dl2) == sizeof(OrbisNgs2ReverbI3DL2Param));
static_assert(sizeof(ngs2_param_header) == sizeof(OrbisNgs2VoiceParamHeader));

struct SystemSetting {
    char name[ORBIS_NGS2_SYSTEM_NAME_LENGTH];
    u32 max_grain;
    u32 grain;
    u32 rate;
};

s32 ReadSystemOption(const OrbisNgs2SystemOption* option, SystemSetting& out) {
    out = {{}, 512, 256, 48000};
    if (!option) {
        return ORBIS_OK;
    }
    if (option->size < sizeof(OrbisNgs2SystemOption)) {
        return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
    }
    std::memcpy(out.name, option->name, sizeof(out.name));
    out.max_grain = option->maxGrainSamples;
    out.grain = option->numGrainSamples;
    out.rate = option->sampleRate;
    if (ngs2_system_size(out.max_grain) == 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    }
    if (out.grain < 64 || out.grain > out.max_grain || out.grain % 64 != 0) {
        return ORBIS_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
    }
    return ORBIS_OK;
}

/// The game's options for a rack as the engine takes them, with the library's values where the
/// game gives none.
s32 ReadRackOption(u32 rack_id, const OrbisNgs2RackOption* option, ngs2_rack_desc& desc) {
    desc = {};
    desc.rack_id = rack_id;
    desc.max_grain_samples = 512;
    desc.max_voices = 1;
    desc.max_matrices = 1;
    desc.max_ports = 8;
    desc.max_channels = 8;
    desc.max_inputs = 1;
    desc.num_buffers = 1;
    const OrbisNgs2CustomRackOption* custom = nullptr;
    switch (rack_id) {
    case NGS2_RACK_SAMPLER:
        desc.max_voices = 256;
        desc.max_waveform_blocks = 4;
        desc.max_filters = 8;
        desc.max_envelope_points = 8;
        desc.max_atrac9_decoders = 256;
        if (option) {
            if (option->size < sizeof(OrbisNgs2SamplerRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            const auto& o = *reinterpret_cast<const OrbisNgs2SamplerRackOption*>(option);
            desc.max_waveform_blocks = o.maxWaveformBlocks;
            desc.max_filters = o.maxFilters;
            desc.max_envelope_points = o.maxEnvelopePoints;
            desc.max_atrac9_decoders = o.maxAtrac9Decoders;
        }
        break;
    case NGS2_RACK_SUBMIXER:
        desc.max_filters = 8;
        desc.max_envelope_points = 8;
        if (option) {
            if (option->size < sizeof(OrbisNgs2SubmixerRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            const auto& o = *reinterpret_cast<const OrbisNgs2SubmixerRackOption*>(option);
            desc.max_channels = o.maxChannels;
            desc.max_filters = o.maxFilters;
            desc.max_envelope_points = o.maxEnvelopePoints;
            desc.max_inputs = o.maxInputs;
        }
        break;
    case NGS2_RACK_REVERB:
        desc.reverb_size = 1;
        if (option) {
            if (option->size < sizeof(OrbisNgs2ReverbRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            const auto& o = *reinterpret_cast<const OrbisNgs2ReverbRackOption*>(option);
            desc.max_channels = o.maxChannels;
            desc.reverb_size = o.reverbSize;
        }
        break;
    case NGS2_RACK_MASTERING:
        desc.max_matrices = 0;
        desc.max_ports = 0;
        if (option) {
            if (option->size < sizeof(OrbisNgs2MasteringRackOption)) {
                return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
            }
            desc.max_channels =
                reinterpret_cast<const OrbisNgs2MasteringRackOption*>(option)->maxChannels;
        }
        break;
    case NGS2_RACK_CUSTOM_SAMPLER: {
        if (!option) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_ADDRESS;
        }
        if (option->size < sizeof(OrbisNgs2CustomSamplerRackOption)) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
        }
        const auto& o = *reinterpret_cast<const OrbisNgs2CustomSamplerRackOption*>(option);
        custom = &o.customRackOption;
        desc.max_waveform_blocks = o.maxWaveformBlocks;
        desc.max_atrac9_decoders = o.maxAtrac9Decoders;
        break;
    }
    case NGS2_RACK_CUSTOM_SUBMIXER:
    case NGS2_RACK_CUSTOM_MASTERING: {
        if (!option) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_ADDRESS;
        }
        if (option->size < sizeof(OrbisNgs2CustomSubmixerRackOption)) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
        }
        const auto& o = *reinterpret_cast<const OrbisNgs2CustomSubmixerRackOption*>(option);
        custom = &o.customRackOption;
        desc.max_channels = o.maxChannels;
        desc.max_inputs = o.maxInputs;
        break;
    }
    default:
        return ORBIS_NGS2_ERROR_INVALID_RACK_ID;
    }
    if (option) {
        std::memcpy(desc.name, option->name, sizeof(desc.name));
        desc.max_grain_samples = option->maxGrainSamples;
        desc.max_voices = option->maxVoices;
        desc.max_matrices = option->maxMatrices;
        desc.max_ports = option->maxPorts;
    }
    if (custom) {
        desc.num_buffers = custom->numBuffers;
        desc.num_modules = custom->numModules;
        if (desc.num_modules > NGS2_MAX_MODULES) {
            return ORBIS_NGS2_ERROR_INVALID_OPTION_SIZE;
        }
        for (u32 i = 0; i < desc.num_modules; i++) {
            const auto& in = custom->aModule[i];
            auto& out = desc.modules[i];
            out.id = in.moduleId;
            out.source_buffer = in.sourceBufferId;
            out.extra_buffer = in.extraBufferId;
            out.dest_buffer = in.destBufferId;
            out.state_size = in.stateSize;
            if (in.moduleId == 0x1f && in.option &&
                in.option->size >= sizeof(OrbisNgs2CustomUserFx2ModuleOption)) {
                const auto& fx =
                    *reinterpret_cast<const OrbisNgs2CustomUserFx2ModuleOption*>(in.option);
                out.setup_handler = reinterpret_cast<void*>(fx.setupHandler);
                out.cleanup_handler = reinterpret_cast<void*>(fx.cleanupHandler);
                out.control_handler = reinterpret_cast<void*>(fx.controlHandler);
                out.process_handler = reinterpret_cast<void*>(fx.processHandler);
                out.common_size = fx.commonSize;
                out.param_size = fx.paramSize;
                out.work_size = fx.workSize;
                out.user_data = fx.userData;
            } else if (in.moduleId == 0x10 && in.option &&
                       in.option->size >= sizeof(OrbisNgs2CustomEnvelopeModuleOption)) {
                out.max_points =
                    reinterpret_cast<const OrbisNgs2CustomEnvelopeModuleOption*>(in.option)
                        ->maxPoints;
            } else if (in.moduleId == 0x1c && in.option && in.option->size >= 12) {
                // size, maxPhases, reserved
                std::memcpy(&out.max_phases, reinterpret_cast<const u8*>(in.option) + 4, 4);
            } else if (in.moduleId == 0x1d && in.option &&
                       in.option->size >= sizeof(OrbisNgs2CustomDelayModuleOption)) {
                const auto& dl =
                    *reinterpret_cast<const OrbisNgs2CustomDelayModuleOption*>(in.option);
                out.max_taps = dl.maxTaps;
                out.max_length = dl.maxLength;
            }
        }
        for (u32 i = 0; i < NGS2_MAX_CUSTOM_PORTS; i++) {
            desc.port_buffers[i] = custom->aPort[i].sourceBufferId;
        }
    }
    // More channels than a voice of the engine carries: such a setup is refused, the rest of
    // the rack works
    desc.max_channels = std::min<u32>(desc.max_channels, NGS2_MAX_CHANNELS);
    if (desc.max_grain_samples < 64 || desc.max_grain_samples > 2048 ||
        desc.max_grain_samples % 64 != 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    }
    if (desc.max_voices == 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_VOICES;
    }
    if (desc.max_channels == 0) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_CHANNELS;
    }
    if (ngs2_rack_size(&desc) == 0) {
        return ORBIS_NGS2_ERROR_INVALID_OPTION_FLAG;
    }
    return ORBIS_OK;
}

s32 CreateSystem(const OrbisNgs2SystemOption* option, const OrbisNgs2ContextBufferInfo& buffer,
                 OrbisNgs2BufferFreeHandler free, OrbisNgs2Handle* out_handle) {
    SystemSetting setting;
    if (const s32 result = ReadSystemOption(option, setting); result != ORBIS_OK) {
        return result;
    }
    if (!buffer.hostBuffer) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    }
    u8* const base = Aligned(buffer.hostBuffer);
    const size_t lost = base - static_cast<u8*>(buffer.hostBuffer);
    const size_t need = ObjectSize + ngs2_system_size(setting.max_grain);
    if (buffer.hostBufferSize < lost + need) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_SIZE;
    }
    ngs2_system* const system =
        ngs2_system_init(base + ObjectSize, need - ObjectSize, setting.name, setting.max_grain,
                         setting.grain, setting.rate, &Host);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SAMPLE_RATE;
    }
    auto* const object = new (base) Object{buffer, free, nullptr};
    object->lock = new (base + sizeof(Object)) SystemLock;
    ngs2_system_set_host_data(system, object->lock);
    *out_handle = reinterpret_cast<OrbisNgs2Handle>(system);
    LOG_INFO(Lib_Ngs2, "system '{}': {} Hz, grains of {} (at most {})", setting.name, setting.rate,
             setting.grain, setting.max_grain);
    return ORBIS_OK;
}

s32 CreateRack(ngs2_system* system, u32 rack_id, const OrbisNgs2RackOption* option,
               const OrbisNgs2ContextBufferInfo& buffer, OrbisNgs2BufferFreeHandler free,
               OrbisNgs2Handle* out_handle) {
    ngs2_rack_desc desc;
    if (const s32 result = ReadRackOption(rack_id, option, desc); result != ORBIS_OK) {
        return result;
    }
    if (!buffer.hostBuffer) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    }
    u8* const base = Aligned(buffer.hostBuffer);
    const size_t lost = base - static_cast<u8*>(buffer.hostBuffer);
    const size_t need = ObjectSize + ngs2_rack_size(&desc);
    if (buffer.hostBufferSize < lost + need) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_SIZE;
    }
    const Locked locked{system};
    ngs2_rack* const rack = ngs2_rack_init(system, base + ObjectSize, need - ObjectSize, &desc);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    }
    new (base) Object{buffer, free, nullptr};
    *out_handle = reinterpret_cast<OrbisNgs2Handle>(rack);
    LOG_INFO(Lib_Ngs2, "rack {:#x} '{}': {} voices, {} channels, {} ports, {} matrices, {} modules",
             rack_id, desc.name, desc.max_voices, desc.max_channels, desc.max_ports,
             desc.max_matrices, desc.num_modules);
    for (u32 i = 0; i < desc.num_modules; i++) {
        const auto& m = desc.modules[i];
        // The modules the engine has; another passes its sound on as it came
        const bool known =
            m.id == 0x13 || m.id == 0x15 || m.id == 0x19 || m.id == 0x1b || m.id == 0x1f;
        LOG_INFO(Lib_Ngs2, "  module {}: {:#x}{}, buffer {} to {}", i, m.id,
                 known ? "" : " (not in: passed through)", m.source_buffer, m.dest_buffer);
    }
    return ORBIS_OK;
}

/// Takes a rack out and gives its buffer back the way it came
void DestroyRack(ngs2_rack* rack, OrbisNgs2ContextBufferInfo* out_buffer) {
    size_t size;
    Object* const object = ObjectOf(ngs2_rack_memory(rack, &size));
    const Object copy = *object;
    ngs2_rack_destroy(rack);
    if (out_buffer) {
        *out_buffer = copy.buffer;
    }
    if (copy.free) {
        OrbisNgs2ContextBufferInfo buffer = copy.buffer;
        Core::CallGuest(copy.free, &buffer);
    }
}

ngs2_system* SystemOfVoice(OrbisNgs2Handle handle) {
    ngs2_voice* const voice = ngs2_voice_from(handle);
    return voice ? ngs2_rack_system(ngs2_voice_rack(voice)) : nullptr;
}

} // namespace

s32 PS4_SYSV_ABI sceNgs2RackQueryBufferSize(u32 rackId, const OrbisNgs2RackOption* option,
                                            OrbisNgs2ContextBufferInfo* outBufferInfo);
s32 PS4_SYSV_ABI sceNgs2SystemQueryBufferSize(const OrbisNgs2SystemOption* option,
                                              OrbisNgs2ContextBufferInfo* outBufferInfo);

s32 PS4_SYSV_ABI sceNgs2RackCreate(OrbisNgs2Handle systemHandle, u32 rackId,
                                   const OrbisNgs2RackOption* option,
                                   const OrbisNgs2ContextBufferInfo* bufferInfo,
                                   OrbisNgs2Handle* outHandle) {
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
    const s32 result = CreateRack(system, rackId, option, *bufferInfo, nullptr, outHandle);
    if (result != ORBIS_OK) {
        LOG_ERROR(Lib_Ngs2, "rack {:#x} not made: {:#x}", rackId, static_cast<u32>(result));
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2RackCreateWithAllocator(OrbisNgs2Handle systemHandle, u32 rackId,
                                                const OrbisNgs2RackOption* option,
                                                const OrbisNgs2BufferAllocator* allocator,
                                                OrbisNgs2Handle* outHandle) {
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
    OrbisNgs2ContextBufferInfo buffer{};
    s32 result = sceNgs2RackQueryBufferSize(rackId, option, &buffer);
    if (result != ORBIS_OK) {
        return result;
    }
    buffer.userData = allocator->userData;
    result = Core::CallGuest(allocator->allocHandler, &buffer);
    if (result < 0) {
        return result;
    }
    result = CreateRack(system, rackId, option, buffer, allocator->freeHandler, outHandle);
    if (result != ORBIS_OK) {
        LOG_ERROR(Lib_Ngs2, "rack {:#x} not made: {:#x}", rackId, static_cast<u32>(result));
        if (allocator->freeHandler) {
            Core::CallGuest(allocator->freeHandler, &buffer);
        }
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2RackDestroy(OrbisNgs2Handle rackHandle,
                                    OrbisNgs2ContextBufferInfo* outBufferInfo) {
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    const Locked locked{ngs2_rack_system(rack)};
    DestroyRack(rack, outBufferInfo);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackGetInfo(OrbisNgs2Handle rackHandle, OrbisNgs2RackInfo* outInfo,
                                    size_t infoSize) {
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
    const Locked locked{ngs2_rack_system(rack)};
    const ngs2_rack_desc& desc = *ngs2_rack_description(rack);
    size_t size;
    std::memset(outInfo, 0, infoSize);
    std::memcpy(outInfo->name, desc.name, sizeof(outInfo->name));
    outInfo->rackHandle = rackHandle;
    outInfo->bufferInfo = ObjectOf(ngs2_rack_memory(rack, &size))->buffer;
    outInfo->ownerSystemHandle = reinterpret_cast<OrbisNgs2Handle>(ngs2_rack_system(rack));
    outInfo->type = desc.rack_id >> 12;
    outInfo->rackId = desc.rack_id;
    outInfo->minGrainSamples = 64;
    outInfo->maxGrainSamples = desc.max_grain_samples;
    outInfo->maxVoices = desc.max_voices;
    outInfo->maxInputs = desc.max_inputs;
    outInfo->maxMatrices = desc.max_matrices;
    outInfo->maxPorts = desc.max_ports;
    outInfo->renderCount = ngs2_system_render_count(ngs2_rack_system(rack));
    outInfo->activeVoiceCount = ngs2_rack_active_voices(rack);
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
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    ngs2_voice* const voice = ngs2_rack_voice(rack, voiceIndex);
    if (!voice) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_INDEX;
    }
    *outHandle = reinterpret_cast<OrbisNgs2Handle>(voice);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackLock(OrbisNgs2Handle rackHandle) {
    ngs2_rack* const rack = ngs2_rack_from(rackHandle);
    if (!rack) {
        return ORBIS_NGS2_ERROR_INVALID_RACK_HANDLE;
    }
    LockOf(ngs2_rack_system(rack)).Lock();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2RackQueryBufferSize(u32 rackId, const OrbisNgs2RackOption* option,
                                            OrbisNgs2ContextBufferInfo* outBufferInfo) {
    if (!outBufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    ngs2_rack_desc desc;
    if (const s32 result = ReadRackOption(rackId, option, desc); result != ORBIS_OK) {
        LOG_ERROR(Lib_Ngs2, "rack {:#x}: {:#x}", rackId, static_cast<u32>(result));
        return result;
    }
    *outBufferInfo = {};
    outBufferInfo->hostBufferSize = Slack + ObjectSize + ngs2_rack_size(&desc);
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
    return LockOf(ngs2_rack_system(rack)).Unlock() ? ORBIS_OK : ORBIS_NGS2_ERROR_INVALID_OPERATION;
}

s32 PS4_SYSV_ABI sceNgs2SystemCreate(const OrbisNgs2SystemOption* option,
                                     const OrbisNgs2ContextBufferInfo* bufferInfo,
                                     OrbisNgs2Handle* outHandle) {
    if (!bufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_INFO;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    const s32 result = CreateSystem(option, *bufferInfo, nullptr, outHandle);
    if (result != ORBIS_OK) {
        LOG_ERROR(Lib_Ngs2, "system not made: {:#x}", static_cast<u32>(result));
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2SystemCreateWithAllocator(const OrbisNgs2SystemOption* option,
                                                  const OrbisNgs2BufferAllocator* allocator,
                                                  OrbisNgs2Handle* outHandle) {
    if (!allocator || !allocator->allocHandler) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ALLOCATOR;
    }
    if (!outHandle) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    OrbisNgs2ContextBufferInfo buffer{};
    s32 result = sceNgs2SystemQueryBufferSize(option, &buffer);
    if (result != ORBIS_OK) {
        return result;
    }
    buffer.userData = allocator->userData;
    result = Core::CallGuest(allocator->allocHandler, &buffer);
    if (result < 0) {
        return result;
    }
    result = CreateSystem(option, buffer, allocator->freeHandler, outHandle);
    if (result != ORBIS_OK) {
        LOG_ERROR(Lib_Ngs2, "system not made: {:#x}", static_cast<u32>(result));
        if (allocator->freeHandler) {
            Core::CallGuest(allocator->freeHandler, &buffer);
        }
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2SystemDestroy(OrbisNgs2Handle systemHandle,
                                      OrbisNgs2ContextBufferInfo* outBufferInfo) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    size_t size;
    Object* const object = ObjectOf(ngs2_system_memory(system, &size));
    const Object copy = *object;
    copy.lock->Lock();
    while (ngs2_rack* const rack = ngs2_system_rack(system, 0)) {
        DestroyRack(rack, nullptr);
    }
    ngs2_system_destroy(system);
    copy.lock->Unlock();
    copy.lock->~SystemLock();
    if (outBufferInfo) {
        *outBufferInfo = copy.buffer;
    }
    if (copy.free) {
        OrbisNgs2ContextBufferInfo buffer = copy.buffer;
        Core::CallGuest(copy.free, &buffer);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemEnumHandles(OrbisNgs2Handle* aOutHandle, u32 maxHandles) {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) maxHandles = {}", maxHandles);
    return 0;
}

s32 PS4_SYSV_ABI sceNgs2SystemEnumRackHandles(OrbisNgs2Handle systemHandle,
                                              OrbisNgs2Handle* aOutHandle, u32 maxHandles) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    const Locked locked{system};
    u32 count = 0;
    while (ngs2_rack* const rack = ngs2_system_rack(system, count)) {
        if (aOutHandle && count < maxHandles) {
            aOutHandle[count] = reinterpret_cast<OrbisNgs2Handle>(rack);
        }
        count++;
    }
    return static_cast<s32>(count);
}

s32 PS4_SYSV_ABI sceNgs2SystemGetInfo(OrbisNgs2Handle systemHandle, OrbisNgs2SystemInfo* outInfo,
                                      size_t infoSize) {
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
    const Locked locked{system};
    size_t size;
    std::memset(outInfo, 0, infoSize);
    outInfo->systemHandle = systemHandle;
    outInfo->bufferInfo = ObjectOf(ngs2_system_memory(system, &size))->buffer;
    outInfo->minGrainSamples = 64;
    outInfo->maxGrainSamples = ngs2_system_grain_samples(system);
    outInfo->rackCount = ngs2_system_rack_count(system);
    outInfo->renderCount = static_cast<s64>(ngs2_system_render_count(system));
    outInfo->sampleRate = ngs2_system_sample_rate(system);
    outInfo->numGrainSamples = ngs2_system_grain_samples(system);
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
    LockOf(system).Lock();
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemQueryBufferSize(const OrbisNgs2SystemOption* option,
                                              OrbisNgs2ContextBufferInfo* outBufferInfo) {
    if (!outBufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    SystemSetting setting;
    if (const s32 result = ReadSystemOption(option, setting); result != ORBIS_OK) {
        return result;
    }
    *outBufferInfo = {};
    outBufferInfo->hostBufferSize = Slack + ObjectSize + ngs2_system_size(setting.max_grain);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemRender(OrbisNgs2Handle systemHandle,
                                     const OrbisNgs2RenderBufferInfo* aBufferInfo,
                                     u32 numBufferInfo) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    if (numBufferInfo && !aBufferInfo) {
        return ORBIS_NGS2_ERROR_INVALID_BUFFER_ADDRESS;
    }
    SystemLock& lock = LockOf(system);
    lock.Lock();
    const s32 result = ngs2_system_render(
        system, reinterpret_cast<const ngs2_render_buffer*>(aBufferInfo), numBufferInfo);
    // The game hears of the blocks that ended with the lock let go: what it does there may
    // wait on a thread of its own that is setting a voice
    for (;;) {
        void* handler;
        ngs2_callback_info info;
        if (!ngs2_system_next_callback(system, &handler, &info)) {
            break;
        }
        lock.Unlock();
        Core::CallGuest(reinterpret_cast<OrbisNgs2VoiceCallbackHandler>(handler),
                        reinterpret_cast<const OrbisNgs2VoiceCallbackInfo*>(&info));
        lock.Lock();
    }
    lock.Unlock();
    return result;
}

static s32 PS4_SYSV_ABI sceNgs2SystemResetOption(OrbisNgs2SystemOption* outOption) {
    static const OrbisNgs2SystemOption option = {
        sizeof(OrbisNgs2SystemOption), "", 0, 512, 256, 48000, {0}};
    if (!outOption) {
        return ORBIS_NGS2_ERROR_INVALID_OPTION_ADDRESS;
    }
    *outOption = option;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2SystemSetGrainSamples(OrbisNgs2Handle systemHandle, u32 numSamples) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    const Locked locked{system};
    return ngs2_system_set_grain_samples(system, numSamples)
               ? ORBIS_OK
               : ORBIS_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
}

s32 PS4_SYSV_ABI sceNgs2SystemSetSampleRate(OrbisNgs2Handle systemHandle, u32 sampleRate) {
    ngs2_system* const system = ngs2_system_from(systemHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    }
    const Locked locked{system};
    return ngs2_system_set_sample_rate(system, sampleRate) ? ORBIS_OK
                                                           : ORBIS_NGS2_ERROR_INVALID_SAMPLE_RATE;
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
    return LockOf(system).Unlock() ? ORBIS_OK : ORBIS_NGS2_ERROR_INVALID_OPERATION;
}

s32 PS4_SYSV_ABI sceNgs2VoiceControl(OrbisNgs2Handle voiceHandle,
                                     const OrbisNgs2VoiceParamHeader* paramList) {
    ngs2_system* const system = SystemOfVoice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    const Locked locked{system};
    ngs2_voice* const voice = ngs2_voice_from(voiceHandle);
    if (!voice) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    const s32 result =
        ngs2_voice_control(voice, reinterpret_cast<const ngs2_param_header*>(paramList));
    if (result != ORBIS_OK) {
        // The list's first parameter and its first word, to tell which call of the game's it was
        const OrbisNgs2VoiceParamHeader* param = paramList;
        u32 word = 0;
        if (param && param->size >= 12) {
            std::memcpy(&word, param + 1, sizeof(word));
        }
        LOG_ERROR(Lib_Ngs2, "voice {} of rack {:#x}: {:#x} (list starts with {:#x}, {:#x})",
                  ngs2_voice_index(voice), ngs2_rack_description(ngs2_voice_rack(voice))->rack_id,
                  static_cast<u32>(result), param ? param->id : 0, word);
    }
    return result;
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetMatrixInfo(OrbisNgs2Handle voiceHandle, u32 matrixId,
                                           OrbisNgs2VoiceMatrixInfo* outInfo, size_t outInfoSize) {
    ngs2_system* const system = SystemOfVoice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (outInfoSize < sizeof(OrbisNgs2VoiceMatrixInfo)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    const Locked locked{system};
    *outInfo = {};
    return ngs2_voice_matrix_info(ngs2_voice_from(voiceHandle), matrixId, &outInfo->numLevels,
                                  outInfo->aLevel);
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetOwner(OrbisNgs2Handle voiceHandle, OrbisNgs2Handle* outRackHandle,
                                      u32* outVoiceId) {
    ngs2_voice* const voice = ngs2_voice_from(voiceHandle);
    if (!voice) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (outRackHandle) {
        *outRackHandle = reinterpret_cast<OrbisNgs2Handle>(ngs2_voice_rack(voice));
    }
    if (outVoiceId) {
        *outVoiceId = ngs2_voice_index(voice);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetPortInfo(OrbisNgs2Handle voiceHandle, u32 port,
                                         OrbisNgs2VoicePortInfo* outInfo, size_t outInfoSize) {
    ngs2_system* const system = SystemOfVoice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outInfo) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (outInfoSize < sizeof(OrbisNgs2VoicePortInfo)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    const Locked locked{system};
    return ngs2_voice_port_info(ngs2_voice_from(voiceHandle), port,
                                reinterpret_cast<ngs2_port_info*>(outInfo));
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetState(OrbisNgs2Handle voiceHandle, OrbisNgs2VoiceState* outState,
                                      size_t stateSize) {
    ngs2_system* const system = SystemOfVoice(voiceHandle);
    if (!system) {
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outState) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    if (stateSize < sizeof(OrbisNgs2VoiceState)) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_SIZE;
    }
    const Locked locked{system};
    const ngs2_voice* const voice = ngs2_voice_from(voiceHandle);
    // As much of the rack's kind of state as the game has room for
    const auto give = [&](const auto& state) {
        std::memset(outState, 0, stateSize);
        std::memcpy(outState, &state, std::min(stateSize, sizeof(state)));
    };
    ngs2_sampler_state sampler;
    ngs2_voice_sampler_state(voice, &sampler);
    switch (ngs2_rack_description(ngs2_voice_rack(voice))->rack_id) {
    case NGS2_RACK_SAMPLER:
        give(sampler);
        break;
    case NGS2_RACK_CUSTOM_SAMPLER: {
        OrbisNgs2CustomSamplerVoiceState state{};
        state.voiceState.stateFlags = sampler.state_flags;
        state.waveformData = sampler.waveform_data;
        state.numDecodedSamples = sampler.decoded_samples;
        state.decodedDataSize = sampler.decoded_bytes;
        state.userData = sampler.user_data;
        give(state);
        break;
    }
    case NGS2_RACK_SUBMIXER: {
        OrbisNgs2SubmixerVoiceState state{};
        state.voiceState.stateFlags = sampler.state_flags;
        state.envelopeHeight = 1.0f;
        state.peakHeight = ngs2_voice_peak(voice);
        give(state);
        break;
    }
    case NGS2_RACK_MASTERING: {
        OrbisNgs2MasteringVoiceState state{};
        state.voiceState.stateFlags = sampler.state_flags;
        give(state);
        break;
    }
    default: {
        OrbisNgs2VoiceState state{};
        state.stateFlags = sampler.state_flags;
        give(state);
        break;
    }
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2VoiceGetStateFlags(OrbisNgs2Handle voiceHandle, u32* outStateFlags) {
    ngs2_system* const system = SystemOfVoice(voiceHandle);
    if (!system) {
        if (outStateFlags) {
            *outStateFlags = 0;
        }
        return ORBIS_NGS2_ERROR_INVALID_VOICE_HANDLE;
    }
    if (!outStateFlags) {
        return ORBIS_NGS2_ERROR_INVALID_OUT_ADDRESS;
    }
    const Locked locked{system};
    *outStateFlags = ngs2_voice_state_flags(ngs2_voice_from(voiceHandle));
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2CustomRackGetModuleInfo(OrbisNgs2Handle rackHandle, u32 moduleIndex,
                                                OrbisNgs2CustomModuleInfo* outInfo,
                                                size_t infoSize) {
    LOG_ERROR(Lib_Ngs2, "moduleIndex = {}, infoSize = {}", moduleIndex, infoSize);
    return ORBIS_OK;
}

// Ngs2Geom

s32 PS4_SYSV_ABI sceNgs2GeomResetListenerParam(OrbisNgs2GeomListenerParam* outListenerParam) {
    LOG_ERROR(Lib_Ngs2, "called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2GeomResetSourceParam(OrbisNgs2GeomSourceParam* outSourceParam) {
    LOG_ERROR(Lib_Ngs2, "called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2GeomCalcListener(const OrbisNgs2GeomListenerParam* param,
                                         OrbisNgs2GeomListenerWork* outWork, u32 flags) {
    LOG_ERROR(Lib_Ngs2, "flags = {}", flags);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2GeomApply(const OrbisNgs2GeomListenerWork* listener,
                                  const OrbisNgs2GeomSourceParam* source,
                                  OrbisNgs2GeomAttribute* outAttrib, u32 flags) {
    LOG_ERROR(Lib_Ngs2, "flags = {}", flags);
    return ORBIS_OK;
}

// Ngs2Pan

s32 PS4_SYSV_ABI sceNgs2PanInit(OrbisNgs2PanWork* work, const float* aSpeakerAngle, float unitAngle,
                                u32 numSpeakers) {
    LOG_ERROR(Lib_Ngs2, "unitAngle = {}, numSpeakers = {}", unitAngle, numSpeakers);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2PanGetVolumeMatrix(OrbisNgs2PanWork* work, const OrbisNgs2PanParam* aParam,
                                           u32 numParams, u32 matrixFormat,
                                           float* outVolumeMatrix) {
    LOG_ERROR(Lib_Ngs2, "numParams = {}, matrixFormat = {}", numParams, matrixFormat);
    return ORBIS_OK;
}

// Ngs2Report

s32 PS4_SYSV_ABI sceNgs2ReportRegisterHandler(u32 reportType, OrbisNgs2ReportHandler handler,
                                              uintptr_t userData, OrbisNgs2Handle* outHandle) {
    LOG_INFO(Lib_Ngs2, "reportType = {}, userData = {}", reportType, userData);
    if (!handler) {
        LOG_ERROR(Lib_Ngs2, "handler is nullptr");
        return ORBIS_NGS2_ERROR_INVALID_REPORT_HANDLE;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNgs2ReportUnregisterHandler(OrbisNgs2Handle reportHandle) {
    if (!reportHandle) {
        LOG_ERROR(Lib_Ngs2, "reportHandle is nullptr");
        return ORBIS_NGS2_ERROR_INVALID_REPORT_HANDLE;
    }
    LOG_INFO(Lib_Ngs2, "called");
    return ORBIS_OK;
}

// Unknown

int PS4_SYSV_ABI sceNgs2FftInit() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2FftProcess() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2FftQuerySize() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2JobSchedulerResetOption() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2ModuleArrayEnumItems() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2ModuleEnumConfigs() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2ModuleQueueEnumItems() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2RackQueryInfo() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2RackRunCommands() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2SystemQueryInfo() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2SystemRunCommands() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2SystemSetLoudThreshold() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2StreamCreate() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2StreamCreateWithAllocator() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2StreamDestroy() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2StreamQueryBufferSize() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2StreamQueryInfo() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2StreamResetOption() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2StreamRunCommands() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2VoiceQueryInfo() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceNgs2VoiceRunCommands() {
    LOG_ERROR(Lib_Ngs2, "(STUBBED) called");
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("3pCNbVM11UA", "libSceNgs2", 1, "libSceNgs2", sceNgs2CalcWaveformBlock);
    LIB_FUNCTION("6qN1zaEZuN0", "libSceNgs2", 1, "libSceNgs2", sceNgs2CustomRackGetModuleInfo);
    LIB_FUNCTION("Kg1MA5j7KFk", "libSceNgs2", 1, "libSceNgs2", sceNgs2FftInit);
    LIB_FUNCTION("D8eCqBxSojA", "libSceNgs2", 1, "libSceNgs2", sceNgs2FftProcess);
    LIB_FUNCTION("-YNfTO6KOMY", "libSceNgs2", 1, "libSceNgs2", sceNgs2FftQuerySize);
    LIB_FUNCTION("eF8yRCC6W64", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomApply);
    LIB_FUNCTION("1WsleK-MTkE", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomCalcListener);
    LIB_FUNCTION("7Lcfo8SmpsU", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomResetListenerParam);
    LIB_FUNCTION("0lbbayqDNoE", "libSceNgs2", 1, "libSceNgs2", sceNgs2GeomResetSourceParam);
    LIB_FUNCTION("ekGJmmoc8j4", "libSceNgs2", 1, "libSceNgs2", sceNgs2GetWaveformFrameInfo);
    LIB_FUNCTION("BcoPfWfpvVI", "libSceNgs2", 1, "libSceNgs2", sceNgs2JobSchedulerResetOption);
    LIB_FUNCTION("EEemGEQCjO8", "libSceNgs2", 1, "libSceNgs2", sceNgs2ModuleArrayEnumItems);
    LIB_FUNCTION("TaoNtmMKkXQ", "libSceNgs2", 1, "libSceNgs2", sceNgs2ModuleEnumConfigs);
    LIB_FUNCTION("ve6bZi+1sYQ", "libSceNgs2", 1, "libSceNgs2", sceNgs2ModuleQueueEnumItems);
    LIB_FUNCTION("gbMKV+8Enuo", "libSceNgs2", 1, "libSceNgs2", sceNgs2PanGetVolumeMatrix);
    LIB_FUNCTION("xa8oL9dmXkM", "libSceNgs2", 1, "libSceNgs2", sceNgs2PanInit);
    LIB_FUNCTION("hyVLT2VlOYk", "libSceNgs2", 1, "libSceNgs2", sceNgs2ParseWaveformData);
    LIB_FUNCTION("iprCTXPVWMI", "libSceNgs2", 1, "libSceNgs2", sceNgs2ParseWaveformFile);
    LIB_FUNCTION("t9T0QM17Kvo", "libSceNgs2", 1, "libSceNgs2", sceNgs2ParseWaveformUser);
    LIB_FUNCTION("cLV4aiT9JpA", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackCreate);
    LIB_FUNCTION("U546k6orxQo", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackCreateWithAllocator);
    LIB_FUNCTION("lCqD7oycmIM", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackDestroy);
    LIB_FUNCTION("M4LYATRhRUE", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackGetInfo);
    LIB_FUNCTION("Mn4XNDg03XY", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackGetUserData);
    LIB_FUNCTION("MwmHz8pAdAo", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackGetVoiceHandle);
    LIB_FUNCTION("MzTa7VLjogY", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackLock);
    LIB_FUNCTION("0eFLVCfWVds", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackQueryBufferSize);
    LIB_FUNCTION("TZqb8E-j3dY", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackQueryInfo);
    LIB_FUNCTION("MI2VmBx2RbM", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackRunCommands);
    LIB_FUNCTION("JNTMIaBIbV4", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackSetUserData);
    LIB_FUNCTION("++YZ7P9e87U", "libSceNgs2", 1, "libSceNgs2", sceNgs2RackUnlock);
    LIB_FUNCTION("uBIN24Tv2MI", "libSceNgs2", 1, "libSceNgs2", sceNgs2ReportRegisterHandler);
    LIB_FUNCTION("nPzb7Ly-VjE", "libSceNgs2", 1, "libSceNgs2", sceNgs2ReportUnregisterHandler);
    LIB_FUNCTION("koBbCMvOKWw", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemCreate);
    LIB_FUNCTION("mPYgU4oYpuY", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemCreateWithAllocator);
    LIB_FUNCTION("u-WrYDaJA3k", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemDestroy);
    LIB_FUNCTION("vubFP0T6MP0", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemEnumHandles);
    LIB_FUNCTION("U-+7HsswcIs", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemEnumRackHandles);
    LIB_FUNCTION("vU7TQ62pItw", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemGetInfo);
    LIB_FUNCTION("4lFaRxd-aLs", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemGetUserData);
    LIB_FUNCTION("gThZqM5PYlQ", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemLock);
    LIB_FUNCTION("pgFAiLR5qT4", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemQueryBufferSize);
    LIB_FUNCTION("3oIK7y7O4k0", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemQueryInfo);
    LIB_FUNCTION("i0VnXM-C9fc", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemRender);
    LIB_FUNCTION("AQkj7C0f3PY", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemResetOption);
    LIB_FUNCTION("gXiormHoZZ4", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemRunCommands);
    LIB_FUNCTION("l4Q2dWEH6UM", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetGrainSamples);
    LIB_FUNCTION("Wdlx0ZFTV9s", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetLoudThreshold);
    LIB_FUNCTION("-tbc2SxQD60", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetSampleRate);
    LIB_FUNCTION("GZB2v0XnG0k", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemSetUserData);
    LIB_FUNCTION("JXRC5n0RQls", "libSceNgs2", 1, "libSceNgs2", sceNgs2SystemUnlock);
    LIB_FUNCTION("sU2St3agdjg", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamCreate);
    LIB_FUNCTION("I+RLwaauggA", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamCreateWithAllocator);
    LIB_FUNCTION("bfoMXnTRtwE", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamDestroy);
    LIB_FUNCTION("dxulc33msHM", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamQueryBufferSize);
    LIB_FUNCTION("rfw6ufRsmow", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamQueryInfo);
    LIB_FUNCTION("q+2W8YdK0F8", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamResetOption);
    LIB_FUNCTION("qQHCi9pjDps", "libSceNgs2", 1, "libSceNgs2", sceNgs2StreamRunCommands);
    LIB_FUNCTION("uu94irFOGpA", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceControl);
    LIB_FUNCTION("jjBVvPN9964", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetMatrixInfo);
    LIB_FUNCTION("W-Z8wWMBnhk", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetOwner);
    LIB_FUNCTION("WCayTgob7-o", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetPortInfo);
    LIB_FUNCTION("-TOuuAQ-buE", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetState);
    LIB_FUNCTION("rEh728kXk3w", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceGetStateFlags);
    LIB_FUNCTION("9eic4AmjGVI", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceQueryInfo);
    LIB_FUNCTION("AbYvTOZ8Pts", "libSceNgs2", 1, "libSceNgs2", sceNgs2VoiceRunCommands);
};

} // namespace Libraries::Ngs2
