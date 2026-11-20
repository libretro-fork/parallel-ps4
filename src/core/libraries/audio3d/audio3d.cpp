// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <vector>
#include "common/enum_names.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/libraries/audio/audioout.h"
#include "core/libraries/audio/audioout_error.h"
#include "core/libraries/audio3d/audio3d.h"
#include "core/libraries/audio3d/audio3d_error.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/libs.h"
#include "libretro/audio_mix.h"

namespace Libraries::Audio3d {

static constexpr u32 AUDIO3D_SAMPLE_RATE = 48000;

// The port the mix is played through, which the game never sees: float, so that the mix
// reaches the core's own mix whole. 7.1 where the core's mix is surround (the frontend takes
// it): a bed of 6 or 8 channels then goes through unfolded. Stereo otherwise.
static u32 OutputChannels() {
    return audio_mix_channels() == 8 ? 8 : 2;
}
static AudioOut::OrbisAudioOutParamFormat OutputFormat() {
    return OutputChannels() == 8 ? AudioOut::OrbisAudioOutParamFormat::Float_8CH
                                 : AudioOut::OrbisAudioOutParamFormat::FloatStereo;
}

static std::unique_ptr<Audio3dState> state;

enum : int { PortFree = 0, PortOpening, PortOpen, PortClosing };
enum : int { AssociatedFree = 0, AssociatedOpening, AssociatedLive };

// A call's hold on an open port: the port is not closed under it. None (nullptr) if the id
// names no open port.
class PortPin {
public:
    explicit PortPin(const OrbisAudio3dPortId port_id) {
        if (!state || port_id >= MaxPorts) {
            return;
        }
        Port& candidate = state->ports[port_id];
        if (!pin_gate_enter(&candidate.gate)) {
            return;
        }
        if (retro_atomic_load_acquire_int(&candidate.state) != PortOpen) {
            pin_gate_leave(&candidate.gate);
            return;
        }
        port = &candidate;
    }
    ~PortPin() {
        if (port) {
            pin_gate_leave(&port->gate);
        }
    }
    PortPin(const PortPin&) = delete;
    PortPin& operator=(const PortPin&) = delete;

    Port* port{};
};

struct AudioOutBufferInfo {
    u32 channels;
    u32 sample_size;
};

static AudioOutBufferInfo GetAudioOutBufferInfo(const AudioOut::OrbisAudioOutParamFormat format) {
    using Format = AudioOut::OrbisAudioOutParamFormat;
    switch (format) {
    case Format::S16Mono:
        return {1, sizeof(s16)};
    case Format::S16Stereo:
        return {2, sizeof(s16)};
    case Format::S16_8CH:
    case Format::S16_8CH_Std:
        return {8, sizeof(s16)};
    case Format::FloatMono:
        return {1, sizeof(float)};
    case Format::FloatStereo:
        return {2, sizeof(float)};
    case Format::Float_8CH:
    case Format::Float_8CH_Std:
        return {8, sizeof(float)};
    default:
        return {2, sizeof(s16)};
    }
}

// What the game output to its own AudioOut ports of this port, sent on: straight from where it
// waits, no copy
static s32 DrainAssociatedPorts(Port& port) {
    for (auto& aout : port.audioout_ports) {
        if (retro_atomic_load_acquire_int(&aout.state) != AssociatedLive) {
            continue;
        }
        const s32 handle = aout.handle;
        for (;;) {
            const unsigned char* const buffer = audio3d_ring_take_begin(&aout.pending, nullptr);
            if (!buffer) {
                break;
            }
            const s32 ret = AudioOut::sceAudioOutOutput(handle, const_cast<unsigned char*>(buffer));
            audio3d_ring_take_end(&aout.pending);
            if (ret < 0) {
                return ret;
            }
        }
    }
    return ORBIS_OK;
}

// The AudioOut port the mix goes out through, opened by the first thread to need it
static s32 PortOutputHandle(Port& port) {
    const s32 handle = retro_atomic_load_acquire_int(&port.audio_out_handle);
    if (handle >= 0) {
        return handle;
    }
    AudioOut::OrbisAudioOutParamExtendedInformation ext_info{};
    ext_info.data_format.Assign(OutputFormat());
    const s32 opened =
        AudioOut::sceAudioOutOpen(0xFF, AudioOut::OrbisAudioOutPort::Audio3d, 0,
                                  port.parameters.granularity, AUDIO3D_SAMPLE_RATE, ext_info);
    if (opened < 0) {
        return opened;
    }
    if (!retro_atomic_cas_int(&port.audio_out_handle, -1, opened)) {
        AudioOut::sceAudioOutClose(opened);
        return retro_atomic_load_acquire_int(&port.audio_out_handle);
    }
    return opened;
}

// The oldest mixed block out: true if one was sent (*sent), false if there was none or
// another thread is sending one
static s32 SubmitMixed(Port& port, const s32 handle, bool& sent) {
    const unsigned char* const block = audio3d_ring_take_begin(&port.core.mixed, nullptr);
    if (!block) {
        sent = false;
        return ORBIS_OK;
    }
    const s32 ret = AudioOut::sceAudioOutOutput(handle, const_cast<unsigned char*>(block));
    audio3d_ring_take_end(&port.core.mixed);
    sent = true;
    return ret < 0 ? ret : ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutClose(const s32 handle) {
    LOG_INFO(Lib_Audio3d, "called, handle = %d", handle);

    bool found = false;
    for (OrbisAudio3dPortId id = 0; id < MaxPorts && !found; id++) {
        PortPin pin{id};
        if (!pin.port) {
            continue;
        }
        for (auto& aout : pin.port->audioout_ports) {
            // Its ring stays until the entry is opened again or the port closes: a push may
            // still be sending from it
            if (retro_atomic_load_acquire_int(&aout.state) == AssociatedLive &&
                aout.handle == handle &&
                retro_atomic_cas_int(&aout.state, AssociatedLive, AssociatedFree)) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        LOG_ERROR(Lib_Audio3d, "handle %d was not opened via sceAudio3dAudioOutOpen", handle);
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    return AudioOut::sceAudioOutClose(handle);
}

// A ring of an entry closed earlier, had for good if no thread is still putting into it or
// taking from it (they are turned away from here): then it may be made anew
static bool ClaimRing(audio3d_ring_t& ring) {
    if (!ring.data) {
        return true;
    }
    if (!retro_atomic_cas_int(&ring.taking, 0, 1)) {
        return false;
    }
    if (!retro_atomic_cas_int(&ring.putting, 0, 1)) {
        retro_atomic_store_release_int(&ring.taking, 0);
        return false;
    }
    return true;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutOpen(
    const OrbisAudio3dPortId port_id, const Libraries::UserService::OrbisUserServiceUserId user_id,
    s32 type, const s32 index, const u32 len, const u32 freq,
    const AudioOut::OrbisAudioOutParamExtendedInformation param) {
    LOG_INFO(Lib_Audio3d,
             "called, port_id = %u, user_id = %d, type = %d, index = %d, len = %u, freq = %u",
             port_id, user_id, type, index, len, freq);

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    Port& port = *pin.port;

    if (len != port.parameters.granularity) {
        LOG_ERROR(Lib_Audio3d, "len != state->ports[port_id].parameters.granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    // A free entry whose ring no thread is still in: had for good by this thread
    AssociatedAudioOutPort* entry = nullptr;
    for (auto& aout : port.audioout_ports) {
        if (!retro_atomic_cas_int(&aout.state, AssociatedFree, AssociatedOpening)) {
            continue;
        }
        if (ClaimRing(aout.pending)) {
            entry = &aout;
            break;
        }
        retro_atomic_store_release_int(&aout.state, AssociatedFree);
    }
    if (!entry) {
        LOG_ERROR(Lib_Audio3d, "no free AudioOut port on port %u", port_id);
        return ORBIS_AUDIO3D_ERROR_OUT_OF_RESOURCES;
    }

    const s32 handle = sceAudioOutOpen(user_id, static_cast<AudioOut::OrbisAudioOutPort>(type),
                                       index, len, freq, param);
    if (handle < 0) {
        if (entry->pending.data) {
            retro_atomic_store_release_int(&entry->pending.putting, 0);
            retro_atomic_store_release_int(&entry->pending.taking, 0);
        }
        retro_atomic_store_release_int(&entry->state, AssociatedFree);
        return handle;
    }

    const auto info = GetAudioOutBufferInfo(param.data_format.Value());
    const u32 buffer_bytes = len * info.channels * info.sample_size;
    // Its queue, made now rather than for each buffer the game sends
    audio3d_ring_free(&entry->pending);
    if (!audio3d_ring_init(&entry->pending, std::max(port.parameters.queue_depth, 1u),
                           buffer_bytes)) {
        AudioOut::sceAudioOutClose(handle);
        retro_atomic_store_release_int(&entry->state, AssociatedFree);
        return ORBIS_AUDIO3D_ERROR_OUT_OF_MEMORY;
    }
    entry->handle = handle;
    entry->samples_per_buffer = len * info.channels;
    entry->is_float = info.sample_size == sizeof(float);
    retro_atomic_store_release_int(&entry->state, AssociatedLive);
    return handle;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutOutput(const s32 handle, void* ptr) {
    LOG_DEBUG(Lib_Audio3d, "called, handle = %d, ptr = 0x%llx", handle,
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(ptr)));

    if (!ptr) {
        LOG_ERROR(Lib_Audio3d, "!ptr");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (handle < 0 || (handle & 0xFFFF) > 25) {
        LOG_ERROR(Lib_Audio3d, "handle < 0 || (handle & 0xFFFF) > 25");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    for (OrbisAudio3dPortId id = 0; id < MaxPorts; id++) {
        PortPin pin{id};
        if (!pin.port) {
            continue;
        }
        for (auto& aout : pin.port->audioout_ports) {
            if (retro_atomic_load_acquire_int(&aout.state) != AssociatedLive ||
                aout.handle != handle) {
                continue;
            }

            const uintptr_t align_mask = aout.is_float ? 3u : 1u;
            if ((reinterpret_cast<uintptr_t>(ptr) & align_mask) != 0) {
                LOG_ERROR(Lib_Audio3d, "sample buffer for handle %d is misaligned", handle);
                return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
            }

            unsigned char* const slot = audio3d_ring_put_begin(&aout.pending);
            if (!slot) {
                return ORBIS_AUDIO3D_ERROR_NOT_READY;
            }
            std::memcpy(slot, ptr, aout.pending.block_bytes);
            audio3d_ring_put_end(&aout.pending, aout.samples_per_buffer, 1, AUDIO3D_S16);

            // Mirror sceAudioOutOutput's return of samples sent.
            return static_cast<s32>(aout.samples_per_buffer);
        }
    }

    LOG_ERROR(Lib_Audio3d, "handle %d was not opened via sceAudio3dAudioOutOpen", handle);
    return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
}

s32 PS4_SYSV_ABI sceAudio3dAudioOutOutputs(AudioOut::OrbisAudioOutOutputParam* param,
                                           const u32 num) {
    LOG_DEBUG(
        Lib_Audio3d, "called, param = 0x%llx, num = %u",
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(static_cast<void*>(param))),
        num);

    if (!param || !num) {
        LOG_ERROR(Lib_Audio3d, "!param || !num");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    for (u32 i = 0; i < num; i++) {
        const s32 ret = sceAudio3dAudioOutOutput(param[i].handle, param[i].ptr);
        if (ret < 0) {
            return ret;
        }
    }

    return ORBIS_OK;
}

// The game's sound into the next block of @ring: a block's worth, the rest silent
static s32 ConvertAndEnqueue(audio3d_ring_t& ring, const OrbisAudio3dPcm& pcm,
                             const u32 num_channels, const u32 granularity) {
    if (!pcm.sample_buffer || !pcm.num_samples) {
        LOG_ERROR(Lib_Audio3d, "!pcm.sample_buffer || !pcm.num_samples");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    const u32 bytes_per_sample =
        (pcm.format == OrbisAudio3dFormat::ORBIS_AUDIO3D_FORMAT_S16) ? sizeof(s16) : sizeof(float);

    if ((reinterpret_cast<uintptr_t>(pcm.sample_buffer) & (bytes_per_sample - 1)) != 0) {
        LOG_ERROR(Lib_Audio3d, "pcm.sample_buffer is misaligned");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    switch (audio3d_ring_put_pcm(
        &ring, pcm.sample_buffer, pcm.num_samples, granularity, num_channels,
        pcm.format == OrbisAudio3dFormat::ORBIS_AUDIO3D_FORMAT_S16 ? AUDIO3D_S16 : AUDIO3D_FLOAT)) {
    case 0:
        return ORBIS_OK;
    case AUDIO3D_RING_FULL:
        LOG_ERROR(Lib_Audio3d, "queue.size() >= max_entries");
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    default:
        LOG_ERROR(Lib_Audio3d, "out of memory");
        return ORBIS_AUDIO3D_ERROR_OUT_OF_MEMORY;
    }
}

s32 PS4_SYSV_ABI sceAudio3dBedWrite(const OrbisAudio3dPortId port_id, const u32 num_channels,
                                    const OrbisAudio3dFormat format, void* buffer,
                                    const u32 num_samples) {
    return sceAudio3dBedWrite2(port_id, num_channels, format, buffer, num_samples,
                               OrbisAudio3dOutputRoute::ORBIS_AUDIO3D_OUTPUT_BOTH, false);
}

s32 PS4_SYSV_ABI sceAudio3dBedWrite2(const OrbisAudio3dPortId port_id, const u32 num_channels,
                                     const OrbisAudio3dFormat format, void* buffer,
                                     const u32 num_samples,
                                     const OrbisAudio3dOutputRoute output_route,
                                     const bool restricted) {
    LOG_DEBUG(
        Lib_Audio3d,
        "called, port_id = %u, num_channels = %u, format = %s, num_samples = %u, output_route "
        "= %s, restricted = %s",
        port_id, num_channels,
        enum_name(&enum_names_Libraries_Audio3d_OrbisAudio3dFormat, static_cast<s64>(format)),
        num_samples,
        enum_name(&enum_names_Libraries_Audio3d_OrbisAudio3dOutputRoute,
                  static_cast<s64>(output_route)),
        (restricted) ? "true" : "false");

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (output_route > OrbisAudio3dOutputRoute::ORBIS_AUDIO3D_OUTPUT_BOTH) {
        LOG_ERROR(Lib_Audio3d, "output_route > ORBIS_AUDIO3D_OUTPUT_BOTH");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (format > OrbisAudio3dFormat::ORBIS_AUDIO3D_FORMAT_FLOAT) {
        LOG_ERROR(Lib_Audio3d, "format > ORBIS_AUDIO3D_FORMAT_FLOAT");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (num_channels != 2 && num_channels != 6 && num_channels != 8) {
        LOG_ERROR(Lib_Audio3d, "num_channels must be 2, 6, or 8");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (!buffer || !num_samples) {
        LOG_ERROR(Lib_Audio3d, "!buffer || !num_samples");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (format == OrbisAudio3dFormat::ORBIS_AUDIO3D_FORMAT_FLOAT) {
        if ((reinterpret_cast<uintptr_t>(buffer) & 3) != 0) {
            LOG_ERROR(Lib_Audio3d, "buffer & 3 != 0");
            return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
        }
    } else if (format == OrbisAudio3dFormat::ORBIS_AUDIO3D_FORMAT_S16) {
        if ((reinterpret_cast<uintptr_t>(buffer) & 1) != 0) {
            LOG_ERROR(Lib_Audio3d, "buffer & 1 != 0");
            return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
        }
    }

    return ConvertAndEnqueue(pin.port->core.bed,
                             OrbisAudio3dPcm{
                                 .format = format,
                                 .sample_buffer = buffer,
                                 .num_samples = num_samples,
                             },
                             num_channels, pin.port->parameters.granularity);
}

s32 PS4_SYSV_ABI sceAudio3dCreateSpeakerArray() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dDeleteSpeakerArray() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetDefaultOpenParameters(OrbisAudio3dOpenParameters* params) {
    LOG_DEBUG(Lib_Audio3d, "called");
    if (params) {
        auto default_params = OrbisAudio3dOpenParameters{
            .size_this = 0x20,
            .granularity = 0x100,
            .rate = OrbisAudio3dRate::ORBIS_AUDIO3D_RATE_48000,
            .max_objects = 512,
            .queue_depth = 2,
            .buffer_mode = OrbisAudio3dBufferMode::ORBIS_AUDIO3D_BUFFER_ADVANCE_AND_PUSH,
        };
        memcpy(params, &default_params, 0x20);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMemorySize() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients2() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dInitialize(const s64 reserved) {
    LOG_INFO(Lib_Audio3d, "called, reserved = %" PRId64, reserved);

    if (reserved != 0) {
        LOG_ERROR(Lib_Audio3d, "reserved != 0");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (state) {
        LOG_ERROR(Lib_Audio3d, "already initialized");
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }

    state = std::make_unique<Audio3dState>();
    for (auto& port : state->ports) {
        pin_gate_init(&port.gate);
    }

    if (const auto init_ret = AudioOut::sceAudioOutInit();
        init_ret < 0 && init_ret != ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT) {
        return init_ret;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dObjectReserve(const OrbisAudio3dPortId port_id,
                                         OrbisAudio3dObjectId* object_id) {
    LOG_INFO(Lib_Audio3d, "called, port_id = %u, object_id = 0x%llx", port_id,
             static_cast<unsigned long long>(
                 reinterpret_cast<uintptr_t>(static_cast<void*>(object_id))));

    if (!object_id) {
        LOG_ERROR(Lib_Audio3d, "!object_id");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    *object_id = ORBIS_AUDIO3D_OBJECT_INVALID;

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    // As many as max_objects at PortOpen time, made then
    unsigned id = 0;
    if (!audio3d_object_reserve(&pin.port->core, &id)) {
        LOG_ERROR(Lib_Audio3d, "port has no available objects (max_objects = %u)",
                  pin.port->parameters.max_objects);
        return ORBIS_AUDIO3D_ERROR_OUT_OF_RESOURCES;
    }
    *object_id = id;
    LOG_INFO(Lib_Audio3d, "reserved object_id = %u", *object_id);

    return ORBIS_OK;
}

// One attribute of a live object. Only the gain is read by the mix: others are taken and
// not kept.
static s32 ApplyAttribute(Port& port, audio3d_object_t& object,
                          const OrbisAudio3dAttributeId attribute_id, const void* value,
                          const u64 value_size) {
    switch (attribute_id) {
    case OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE:
        audio3d_object_reset(&object);
        return ORBIS_OK;
    case OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_PCM:
        if (!value || value_size < sizeof(OrbisAudio3dPcm)) {
            LOG_ERROR(Lib_Audio3d, "PCM attribute value_size too small");
            return ORBIS_OK;
        }
        return ConvertAndEnqueue(object.pcm, *static_cast<const OrbisAudio3dPcm*>(value), 1,
                                 port.parameters.granularity);
    case OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_GAIN:
        if (value && value_size >= sizeof(float)) {
            float gain;
            std::memcpy(&gain, value, sizeof(gain));
            audio3d_object_set_gain(&object, gain);
        }
        return ORBIS_OK;
    default:
        LOG_DEBUG(Lib_Audio3d, "Attribute 0x%x taken", static_cast<u32>(attribute_id));
        return ORBIS_OK;
    }
}

s32 PS4_SYSV_ABI sceAudio3dObjectSetAttribute(const OrbisAudio3dPortId port_id,
                                              const OrbisAudio3dObjectId object_id,
                                              const OrbisAudio3dAttributeId attribute_id,
                                              const void* attribute, const u64 attribute_size) {
    LOG_DEBUG(Lib_Audio3d,
              "called, port_id = %u, object_id = %u, attribute_id = 0x%x, size = %" PRIu64, port_id,
              object_id, static_cast<u32>(attribute_id), attribute_size);

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    const int object_state = audio3d_object_state(&pin.port->core, object_id);
    if (object_state != AUDIO3D_OBJECT_LIVE && object_state != AUDIO3D_OBJECT_LEAVING) {
        LOG_DEBUG(Lib_Audio3d, "object_id %u not reserved (race with Unreserve?), no-op",
                  object_id);
        return ORBIS_OK;
    }

    if (!attribute_size &&
        attribute_id != OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE) {
        LOG_ERROR(Lib_Audio3d, "!attribute_size for non-reset attribute");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    audio3d_object_t* const object = audio3d_object_get(&pin.port->core, object_id);
    if (!object) {
        LOG_ERROR(Lib_Audio3d, "object_id not reserved");
        return ORBIS_AUDIO3D_ERROR_INVALID_OBJECT;
    }

    return ApplyAttribute(*pin.port, *object, attribute_id, attribute, attribute_size);
}

s32 PS4_SYSV_ABI sceAudio3dObjectSetAttributes(const OrbisAudio3dPortId port_id,
                                               OrbisAudio3dObjectId object_id,
                                               const u64 num_attributes,
                                               const OrbisAudio3dAttribute* attribute_array) {
    LOG_DEBUG(Lib_Audio3d,
              "called, port_id = %u, object_id = %u, num_attributes = %" PRIu64
              ", attribute_array = 0x%llx",
              port_id, object_id, num_attributes,
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(attribute_array)));

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (!num_attributes || !attribute_array) {
        LOG_ERROR(Lib_Audio3d, "!num_attributes || !attribute_array");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    const int object_state = audio3d_object_state(&pin.port->core, object_id);
    if (object_state != AUDIO3D_OBJECT_LIVE && object_state != AUDIO3D_OBJECT_LEAVING) {
        LOG_DEBUG(Lib_Audio3d, "object_id %u not reserved", object_id);
        return ORBIS_OK;
    }

    audio3d_object_t* const object = audio3d_object_get(&pin.port->core, object_id);
    if (!object) {
        LOG_ERROR(Lib_Audio3d, "object_id not reserved");
        return ORBIS_AUDIO3D_ERROR_INVALID_OBJECT;
    }

    // A reset first, however often and wherever it is listed
    for (u64 i = 0; i < num_attributes; i++) {
        if (attribute_array[i].attribute_id ==
            OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE) {
            audio3d_object_reset(object);
            break;
        }
    }

    for (u64 i = 0; i < num_attributes; i++) {
        const auto& attribute = attribute_array[i];
        if (attribute.attribute_id ==
            OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE) {
            continue;
        }
        if (const s32 ret = ApplyAttribute(*pin.port, *object, attribute.attribute_id,
                                           attribute.value, attribute.value_size);
            ret != ORBIS_OK) {
            return ret;
        }
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dObjectUnreserve(const OrbisAudio3dPortId port_id,
                                           const OrbisAudio3dObjectId object_id) {
    LOG_DEBUG(Lib_Audio3d, "called, port_id = %u, object_id = %u", port_id, object_id);

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    // Its queued sound is still mixed; then it is free
    if (!audio3d_object_unreserve(&pin.port->core, object_id)) {
        LOG_ERROR(Lib_Audio3d, "object_id not reserved");
        return ORBIS_AUDIO3D_ERROR_INVALID_OBJECT;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortAdvance(const OrbisAudio3dPortId port_id) {
    LOG_DEBUG(Lib_Audio3d, "called, port_id = %u", port_id);

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    Port& port = *pin.port;

    if (port.parameters.buffer_mode == OrbisAudio3dBufferMode::ORBIS_AUDIO3D_BUFFER_NO_ADVANCE) {
        LOG_ERROR(Lib_Audio3d, "port doesn't have advance capability");
        return ORBIS_AUDIO3D_ERROR_NOT_SUPPORTED;
    }

    // The mix is made in the mixed queue's own next block (audio3d_port.c)
    if (!audio3d_port_mix(&port.core)) {
        LOG_WARNING(Lib_Audio3d, "mixed queue full (depth=%u), dropping advance",
                    port.parameters.queue_depth);
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }
    return ORBIS_OK;
}
s32 PS4_SYSV_ABI sceAudio3dPortClose(const OrbisAudio3dPortId port_id) {
    LOG_INFO(Lib_Audio3d, "called, port_id = %u", port_id);

    if (!state || port_id >= MaxPorts ||
        !retro_atomic_cas_int(&state->ports[port_id].state, PortOpen, PortClosing)) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    Port& port = state->ports[port_id];

    // Calls already in it (a push sending a block, which waits on the game's time) leave
    // first: waited for as a game thread, so the time goes on meanwhile
    pin_gate_shut(&port.gate);
    Kernel::GameWait(&port.gate.drained, 0, [&port] { return pin_gate_drained(&port.gate) != 0; });

    const s32 handle = retro_atomic_exchange_int(&port.audio_out_handle, -1);
    if (handle >= 0) {
        AudioOut::sceAudioOutClose(handle);
    }
    for (auto& aout : port.audioout_ports) {
        if (retro_atomic_exchange_int(&aout.state, AssociatedFree) == AssociatedLive) {
            AudioOut::sceAudioOutClose(aout.handle);
        }
        audio3d_ring_free(&aout.pending);
        aout.handle = -1;
    }
    audio3d_port_free(&port.core);

    pin_gate_open(&port.gate);
    retro_atomic_store_release_int(&port.state, PortFree);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortCreate(u32 granularity, u32 rate, s64 reserved,
                                      OrbisAudio3dPortId* port_id) {

    LOG_INFO(
        Lib_Audio3d,
        "called, granularity = %u, rate = %u, reserved = %" PRId64 ", port_id = 0x%llx",
        granularity, rate, reserved,
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(static_cast<void*>(port_id))));

    if (!port_id || reserved) {
        LOG_INFO(Lib_Audio3d, "!port_id || reserved");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    OrbisAudio3dOpenParameters local_params{};
    local_params.size_this = 0x10;
    local_params.granularity = granularity;
    local_params.rate = static_cast<OrbisAudio3dRate>(rate);
    return sceAudio3dPortOpen(static_cast<Libraries::UserService::OrbisUserServiceUserId>(0xFF),
                              &local_params, port_id);
}

s32 PS4_SYSV_ABI sceAudio3dPortDestroy() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortFlush(const OrbisAudio3dPortId port_id) {
    LOG_DEBUG(Lib_Audio3d, "called, port_id = %u", port_id);

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    Port& port = *pin.port;

    bool any_associated = false;
    for (auto& aout : port.audioout_ports) {
        any_associated |= retro_atomic_load_acquire_int(&aout.state) == AssociatedLive;
    }
    if (any_associated) {
        if (const s32 ret = DrainAssociatedPorts(port); ret < 0) {
            return ret;
        }
        for (auto& aout : port.audioout_ports) {
            if (retro_atomic_load_acquire_int(&aout.state) != AssociatedLive) {
                continue;
            }
            const s32 ret = AudioOut::sceAudioOutOutput(aout.handle, nullptr);
            if (ret < 0) {
                return ret;
            }
        }
        return ORBIS_OK;
    }

    // Only mix if there's actually something to mix.
    if (audio3d_ring_level(&port.core.mixed) == 0 && audio3d_port_has_input(&port.core)) {
        (void)audio3d_port_mix(&port.core);
    }

    if (audio3d_ring_level(&port.core.mixed) == 0) {
        return ORBIS_OK;
    }

    const s32 handle = PortOutputHandle(port);
    if (handle < 0) {
        return handle;
    }

    // Drain all queued mixed frames, blocking on each until consumed.
    for (;;) {
        bool sent = false;
        if (const s32 ret = SubmitMixed(port, handle, sent); ret < 0) {
            return ret;
        }
        if (!sent) {
            return ORBIS_OK;
        }
    }
}

s32 PS4_SYSV_ABI sceAudio3dPortFreeState() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetAttributesSupported(OrbisAudio3dPortId port_id,
                                                      OrbisAudio3dAttributeId* capabilities,
                                                      u32* num_capabilities) {
    LOG_DEBUG(Lib_Audio3d, "called");
    if (!num_capabilities) {
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (!PortPin{port_id}.port) {
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (capabilities) {
        // Writes up to num_capabilities supported capabilities,
        // then sets num_capabilities to how many were written.
        u32 caps_to_write = *num_capabilities;
        if (caps_to_write >= 1) {
            capabilities[0] = OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_PCM;
        }
        if (caps_to_write >= 2) {
            capabilities[1] = OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_GAIN;
        }
        if (caps_to_write >= 3) {
            capabilities[2] = OrbisAudio3dAttributeId::ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE;
        }
        *num_capabilities = std::min<u32>(caps_to_write, 3);
    } else {
        // If capabilities is null, then just report the number of supported capabilities.
        *num_capabilities = 3;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetList() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetParameters() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetQueueLevel(const OrbisAudio3dPortId port_id, u32* queue_level,
                                             u32* queue_available) {
    LOG_DEBUG(Lib_Audio3d, "called, port_id = %u, queue_level = 0x%llx, queue_available = 0x%llx",
              port_id,
              static_cast<unsigned long long>(
                  reinterpret_cast<uintptr_t>(static_cast<void*>(queue_level))),
              static_cast<unsigned long long>(
                  reinterpret_cast<uintptr_t>(static_cast<void*>(queue_available))));

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (!queue_level && !queue_available) {
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    const u32 size = audio3d_ring_level(&pin.port->core.mixed);

    if (queue_level) {
        *queue_level = size;
    }

    if (queue_available) {
        const u32 depth = pin.port->parameters.queue_depth;
        *queue_available = (size < depth) ? depth - size : 0u;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetState() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortGetStatus() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortOpen(const Libraries::UserService::OrbisUserServiceUserId user_id,
                                    const OrbisAudio3dOpenParameters* parameters,
                                    OrbisAudio3dPortId* port_id) {
    LOG_INFO(
        Lib_Audio3d, "called, user_id = %d, parameters = 0x%llx, port_id = 0x%llx", user_id,
        static_cast<unsigned long long>(
            reinterpret_cast<uintptr_t>(static_cast<const void*>(parameters))),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(static_cast<void*>(port_id))));

    if (user_id != 0xFF || !parameters || !port_id) {
        LOG_ERROR(Lib_Audio3d, "user_id != 0xFF || !parameters || !port_id");
        if (port_id)
            *port_id = ORBIS_AUDIO3D_PORT_INVALID;

        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    *port_id = ORBIS_AUDIO3D_PORT_INVALID;

    if (!state) {
        LOG_ERROR(Lib_Audio3d, "!initialized");
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }

    OrbisAudio3dOpenParameters effective{
        .size_this = 0x28,
        .granularity = parameters->granularity,
        .rate = parameters->rate,
        .max_objects = 512,
        .queue_depth = 2,
        .buffer_mode = OrbisAudio3dBufferMode::ORBIS_AUDIO3D_BUFFER_NO_ADVANCE,
        ._pad = 0,
        .num_beds = 2,
    };

    switch (parameters->size_this & ~0x7ull) {
    case 0x10:
        break;

    case 0x18:
        effective.max_objects = parameters->max_objects;
        effective.queue_depth = parameters->queue_depth;
        effective.buffer_mode = OrbisAudio3dBufferMode::ORBIS_AUDIO3D_BUFFER_ADVANCE_NO_PUSH;
        break;

    case 0x20:
        effective.max_objects = parameters->max_objects;
        effective.queue_depth = parameters->queue_depth;
        effective.buffer_mode = parameters->buffer_mode;
        break;

    case 0x28:
        effective.max_objects = parameters->max_objects;
        effective.queue_depth = parameters->queue_depth;
        effective.buffer_mode = parameters->buffer_mode;
        effective.num_beds = parameters->num_beds;
        break;

    default:
        LOG_ERROR(Lib_Audio3d, "invalid size_this");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.rate != OrbisAudio3dRate::ORBIS_AUDIO3D_RATE_48000) {
        LOG_ERROR(Lib_Audio3d, "unsupported rate");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.granularity < 0x100) {
        LOG_ERROR(Lib_Audio3d, "granularity < 0x100");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if ((effective.granularity & 0xFF) != 0) {
        LOG_ERROR(Lib_Audio3d, "granularity not aligned to 0x100");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.max_objects == 0) {
        LOG_ERROR(Lib_Audio3d, "max_objects == 0");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.queue_depth == 0) {
        LOG_ERROR(Lib_Audio3d, "queue_depth == 0");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.granularity == 0x100 && effective.queue_depth > 0x40) {
        LOG_ERROR(Lib_Audio3d, "queue_depth too large for 0x100 granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.granularity == 0x200 && effective.queue_depth > 0x1F) {
        LOG_ERROR(Lib_Audio3d, "queue_depth too large for 0x200 granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.granularity == 0x300 && effective.queue_depth > 0x14) {
        LOG_ERROR(Lib_Audio3d, "queue_depth too large for 0x300 granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.queue_depth > 0xF && effective.granularity > 0x3FF) {
        LOG_ERROR(Lib_Audio3d, "queue_depth invalid for large granularity");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (static_cast<u32>(effective.buffer_mode) > 2) {
        LOG_ERROR(Lib_Audio3d, "invalid buffer_mode");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if ((effective.num_beds & 0xfffffffe) != 2) {
        LOG_ERROR(Lib_Audio3d, "invalid num_beds");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    if (effective.max_objects > 0x200) {
        LOG_WARNING(Lib_Audio3d, "max_objects %u exceeds limit, clamping to 512",
                    effective.max_objects);
        effective.max_objects = 0x200;
    }

    // A free port, had by this thread from here; everything it holds made now
    for (OrbisAudio3dPortId id = 0; id < MaxPorts; id++) {
        Port& port = state->ports[id];
        if (!retro_atomic_cas_int(&port.state, PortFree, PortOpening)) {
            continue;
        }
        port.parameters = effective;
        retro_atomic_store_release_int(&port.audio_out_handle, -1);
        if (!audio3d_port_init(&port.core, effective.granularity, effective.queue_depth,
                               effective.max_objects, OutputChannels())) {
            retro_atomic_store_release_int(&port.state, PortFree);
            LOG_ERROR(Lib_Audio3d, "out of memory");
            return ORBIS_AUDIO3D_ERROR_OUT_OF_MEMORY;
        }
        retro_atomic_store_release_int(&port.state, PortOpen);
        *port_id = id;
        return ORBIS_OK;
    }

    LOG_ERROR(Lib_Audio3d, "no free ports");
    return ORBIS_AUDIO3D_ERROR_OUT_OF_RESOURCES;
}

s32 PS4_SYSV_ABI sceAudio3dPortPush(const OrbisAudio3dPortId port_id,
                                    const OrbisAudio3dBlocking blocking) {
    LOG_DEBUG(
        Lib_Audio3d, "called, port_id = %u, blocking = %s", port_id,
        enum_name(&enum_names_Libraries_Audio3d_OrbisAudio3dBlocking, static_cast<s64>(blocking)));

    PortPin pin{port_id};
    if (!pin.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }
    Port& port = *pin.port;

    if (port.parameters.buffer_mode !=
        OrbisAudio3dBufferMode::ORBIS_AUDIO3D_BUFFER_ADVANCE_AND_PUSH) {
        LOG_ERROR(Lib_Audio3d, "port doesn't have push capability");
        return ORBIS_AUDIO3D_ERROR_NOT_SUPPORTED;
    }

    const u32 depth = port.parameters.queue_depth;

    if (const s32 ret = DrainAssociatedPorts(port); ret < 0) {
        return ret;
    }

    const s32 handle = PortOutputHandle(port);
    if (handle < 0) {
        return handle;
    }

    if (audio3d_ring_level(&port.core.mixed) < depth) {
        return ORBIS_OK;
    }

    bool sent = false;
    if (const s32 ret = SubmitMixed(port, handle, sent); ret < 0) {
        return ret;
    }
    if (!sent || blocking == OrbisAudio3dBlocking::ORBIS_AUDIO3D_BLOCKING_ASYNC) {
        return ORBIS_OK;
    }

    while (audio3d_ring_level(&port.core.mixed) >= depth) {
        if (const s32 ret = SubmitMixed(port, handle, sent); ret < 0) {
            return ret;
        }
        if (!sent) {
            break;
        }
    }

    return ORBIS_OK;
}
s32 PS4_SYSV_ABI sceAudio3dPortQueryDebug() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dPortSetAttribute(const OrbisAudio3dPortId port_id,
                                            const OrbisAudio3dAttributeId attribute_id,
                                            void* attribute, const u64 attribute_size) {
    LOG_INFO(
        Lib_Audio3d,
        "called, port_id = %u, attribute_id = %u, attribute = 0x%llx, attribute_size = %" PRIu64,
        port_id, static_cast<u32>(attribute_id),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(attribute)), attribute_size);

    if (!PortPin{port_id}.port) {
        LOG_ERROR(Lib_Audio3d, "!state->ports.contains(port_id)");
        return ORBIS_AUDIO3D_ERROR_INVALID_PORT;
    }

    if (!attribute) {
        LOG_ERROR(Lib_Audio3d, "!attribute");
        return ORBIS_AUDIO3D_ERROR_INVALID_PARAMETER;
    }

    // TODO

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dReportRegisterHandler() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dReportUnregisterHandler() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dSetGpuRenderer() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dStrError() {
    LOG_ERROR(Lib_Audio3d, "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceAudio3dTerminate() {
    LOG_INFO(Lib_Audio3d, "called");
    if (!state) {
        return ORBIS_AUDIO3D_ERROR_NOT_READY;
    }

    for (OrbisAudio3dPortId id = 0; id < MaxPorts; id++) {
        if (retro_atomic_load_acquire_int(&state->ports[id].state) == PortOpen) {
            sceAudio3dPortClose(id);
        }
    }
    for (auto& port : state->ports) {
        pin_gate_free(&port.gate);
    }

    state.reset();
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("pZlOm1aF3aA", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutClose);
    LIB_FUNCTION("ucEsi62soTo", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutOpen);
    LIB_FUNCTION("7NYEzJ9SJbM", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutOutput);
    LIB_FUNCTION("HbxYY27lK6E", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dAudioOutOutputs);
    LIB_FUNCTION("9tEwE0GV0qo", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dBedWrite);
    LIB_FUNCTION("xH4Q9UILL3o", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dBedWrite2);
    LIB_FUNCTION("lvWMW6vEqFU", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dCreateSpeakerArray);
    LIB_FUNCTION("8hm6YdoQgwg", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dDeleteSpeakerArray);
    LIB_FUNCTION("Im+jOoa5WAI", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetDefaultOpenParameters);
    LIB_FUNCTION("kEqqyDkmgdI", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetSpeakerArrayMemorySize);
    LIB_FUNCTION("-R1DukFq7Dk", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetSpeakerArrayMixCoefficients);
    LIB_FUNCTION("-Re+pCWvwjQ", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dGetSpeakerArrayMixCoefficients2);
    LIB_FUNCTION("UmCvjSmuZIw", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dInitialize);
    LIB_FUNCTION("jO2tec4dJ2M", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectReserve);
    LIB_FUNCTION("V1FBFpNIAzk", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectSetAttribute);
    LIB_FUNCTION("4uyHN9q4ZeU", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectSetAttributes);
    LIB_FUNCTION("1HXxo-+1qCw", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dObjectUnreserve);
    LIB_FUNCTION("lw0qrdSjZt8", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortAdvance);
    LIB_FUNCTION("OyVqOeVNtSk", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortClose);
    LIB_FUNCTION("UHFOgVNz0kk", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortCreate);
    LIB_FUNCTION("Mw9mRQtWepY", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortDestroy);
    LIB_FUNCTION("ZOGrxWLgQzE", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortFlush);
    LIB_FUNCTION("uJ0VhGcxCTQ", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortFreeState);
    LIB_FUNCTION("9ZA23Ia46Po", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dPortGetAttributesSupported);
    LIB_FUNCTION("SEggctIeTcI", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetList);
    LIB_FUNCTION("flPcUaXVXcw", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetParameters);
    LIB_FUNCTION("YaaDbDwKpFM", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetQueueLevel);
    LIB_FUNCTION("CKHlRW2E9dA", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetState);
    LIB_FUNCTION("iRX6GJs9tvE", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortGetStatus);
    LIB_FUNCTION("XeDDK0xJWQA", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortOpen);
    LIB_FUNCTION("VEVhZ9qd4ZY", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortPush);
    LIB_FUNCTION("-pzYDZozm+M", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortQueryDebug);
    LIB_FUNCTION("Yq9bfUQ0uJg", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dPortSetAttribute);
    LIB_FUNCTION("QfNXBrKZeI0", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dReportRegisterHandler);
    LIB_FUNCTION("psv2gbihC1A", "libSceAudio3d", 1, "libSceAudio3d",
                 sceAudio3dReportUnregisterHandler);
    LIB_FUNCTION("yEYXcbAGK14", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dSetGpuRenderer);
    LIB_FUNCTION("Aacl5qkRU6U", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dStrError);
    LIB_FUNCTION("WW1TS2iz5yc", "libSceAudio3d", 1, "libSceAudio3d", sceAudio3dTerminate);
};

} // namespace Libraries::Audio3d
