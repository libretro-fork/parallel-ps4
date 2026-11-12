// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <mutex>
#include <optional>
#include <vector>
#include <queue>

#include "common/combiner.h"
#include "common/event_mutex.h"
#include "common/types.h"
#include "core/libraries/audio/audioout.h"

namespace Core::Loader {
class SymbolsResolver;
}

namespace Libraries::Audio3d {

using OrbisAudio3dPortId = u32;
using OrbisAudio3dObjectId = u32;
using OrbisAudio3dAmbisonics = u32;

constexpr int ORBIS_AUDIO3D_OBJECT_INVALID = 0xFFFFFFFF;
constexpr OrbisAudio3dPortId ORBIS_AUDIO3D_PORT_INVALID = 0xFFFFFFFFu;
constexpr OrbisAudio3dPortId MaxPorts = 4;

enum class OrbisAudio3dRate : u32 {
    ORBIS_AUDIO3D_RATE_48000 = 0,
};

enum class OrbisAudio3dBufferMode : u32 {
    ORBIS_AUDIO3D_BUFFER_NO_ADVANCE = 0,
    ORBIS_AUDIO3D_BUFFER_ADVANCE_NO_PUSH = 1,
    ORBIS_AUDIO3D_BUFFER_ADVANCE_AND_PUSH = 2,
};

struct OrbisAudio3dOpenParameters {
    u64 size_this;
    u32 granularity;
    OrbisAudio3dRate rate;
    u32 max_objects;
    u32 queue_depth;
    OrbisAudio3dBufferMode buffer_mode;
    u32 _pad;
    u32 num_beds;
};

enum class OrbisAudio3dFormat : u32 {
    ORBIS_AUDIO3D_FORMAT_S16 = 0,
    ORBIS_AUDIO3D_FORMAT_FLOAT = 1,
};

enum class OrbisAudio3dOutputRoute : u32 {
    ORBIS_AUDIO3D_OUTPUT_BOTH = 0,
    ORBIS_AUDIO3D_OUTPUT_HMU_ONLY = 1,
    ORBIS_AUDIO3D_OUTPUT_TV_ONLY = 2,
};

enum class OrbisAudio3dBlocking : u32 {
    ORBIS_AUDIO3D_BLOCKING_ASYNC = 0,
    ORBIS_AUDIO3D_BLOCKING_SYNC = 1,
};

struct OrbisAudio3dPcm {
    OrbisAudio3dFormat format;
    void* sample_buffer;
    u32 num_samples;
};

enum class OrbisAudio3dAttributeId : u32 {
    ORBIS_AUDIO3D_ATTRIBUTE_PCM = 1,
    ORBIS_AUDIO3D_ATTRIBUTE_POSITION = 2,
    ORBIS_AUDIO3D_ATTRIBUTE_GAIN = 3,
    ORBIS_AUDIO3D_ATTRIBUTE_SPREAD = 4,
    ORBIS_AUDIO3D_ATTRIBUTE_PRIORITY = 5,
    ORBIS_AUDIO3D_ATTRIBUTE_PASSTHROUGH = 6,
    ORBIS_AUDIO3D_ATTRIBUTE_AMBISONICS = 7,
    ORBIS_AUDIO3D_ATTRIBUTE_APPLICATION_SPECIFIC = 8,
    ORBIS_AUDIO3D_ATTRIBUTE_RESET_STATE = 9,
    ORBIS_AUDIO3D_ATTRIBUTE_RESTRICTED = 10,
    ORBIS_AUDIO3D_ATTRIBUTE_OUTPUT_ROUTE = 11,
};

struct OrbisAudio3dAttribute {
    OrbisAudio3dAttributeId attribute_id;
    int : 32;
    void* value;
    u64 value_size;
};

// Fixed-size blocks out of one allocation made when a port opens, given back and taken again:
// the sound the game hands over is never allocated for while it runs. Used under the port's
// mutex.
class BlockPool {
public:
    void Init(u32 block_bytes_, u32 count) {
        block_bytes = block_bytes_;
        storage.assign(static_cast<size_t>(block_bytes) * count, 0);
        free_blocks.clear();
        free_blocks.reserve(count);
        for (u32 i = 0; i < count; i++) {
            free_blocks.push_back(storage.data() + static_cast<size_t>(i) * block_bytes);
        }
    }
    u8* Take() {
        if (free_blocks.empty()) {
            return nullptr;
        }
        u8* const block = free_blocks.back();
        free_blocks.pop_back();
        return block;
    }
    // Within the capacity reserved at Init: never allocates
    void Give(u8* block) {
        free_blocks.push_back(block);
    }
    u32 BlockBytes() const {
        return block_bytes;
    }

private:
    std::vector<u8> storage;
    std::vector<u8*> free_blocks;
    u32 block_bytes{};
};

// A queue of at most the capacity given at Init, in storage made then: pushing and popping
// never allocate (a std::deque does, a chunk at a time, as it moves along). A popped slot is
// kept as it is, so a slot that owns memory (a vector) keeps it for the next push.
template <typename T>
class FixedQueue {
public:
    void Init(u32 capacity) {
        items.assign(capacity, T{});
        head = 0;
        count = 0;
    }
    u32 size() const {
        return count;
    }
    bool empty() const {
        return count == 0;
    }
    T& front() {
        return items[head];
    }
    // The next slot, counted in; the caller fills it. Only below capacity.
    T& push_slot() {
        T& slot = items[(head + count) % items.size()];
        ++count;
        return slot;
    }
    void push_back(const T& item) {
        push_slot() = item;
    }
    void pop_front() {
        head = (head + 1) % static_cast<u32>(items.size());
        --count;
    }
    void clear() {
        head = 0;
        count = 0;
    }
    bool full() const {
        return count >= items.size();
    }

    class Iterator {
    public:
        Iterator(FixedQueue* q_, u32 i_) : q{q_}, i{i_} {}
        T& operator*() const {
            return q->items[(q->head + i) % q->items.size()];
        }
        Iterator& operator++() {
            ++i;
            return *this;
        }
        bool operator!=(const Iterator& other) const {
            return i != other.i;
        }

    private:
        FixedQueue* q;
        u32 i;
    };
    Iterator begin() {
        return Iterator{this, 0};
    }
    Iterator end() {
        return Iterator{this, count};
    }

private:
    std::vector<T> items;
    u32 head{};
    u32 count{};
};

struct AudioData {
    u8* sample_buffer;
    BlockPool* pool{}; // where sample_buffer goes back to
    u32 num_samples;
    u32 num_channels{1}; // channels in sample_buffer
    OrbisAudio3dFormat format{
        OrbisAudio3dFormat::ORBIS_AUDIO3D_FORMAT_S16}; // format of sample_buffer
};

struct ObjectState {
    FixedQueue<AudioData> pcm_queue;
    std::unordered_map<u32, std::vector<u8>> persistent_attributes;
    bool unreserved{false};
};

// An AudioOut port opened by the game through sceAudio3dAudioOutOpen
struct AssociatedAudioOutPort {
    s32 handle{-1};
    u32 buffer_bytes{0};
    u32 samples_per_buffer{0};
    bool is_float{false};
    // Buffers of buffer_bytes each, made when the port is opened
    FixedQueue<std::vector<u8>> pending;
};

struct Port {
    mutable Common::RecursiveEventMutex mutex;
    OrbisAudio3dOpenParameters parameters{};
    // Opened lazily on the first sceAudio3dPortPush call.
    s32 audio_out_handle{-1};
    // AudioOut ports explicitly opened by the game via sceAudio3dAudioOutOpen.
    std::vector<AssociatedAudioOutPort> audioout_ports;
    // Reserved objects and their state.
    std::unordered_map<OrbisAudio3dObjectId, ObjectState> objects;
    // increasing counter for generating unique object IDs within this port.
    OrbisAudio3dObjectId next_object_id{0};
    // Bed audio queue
    FixedQueue<AudioData> bed_queue;
    // Mixed frames ready to be consumed by sceAudio3dPortPush: stereo, or 7.1 with surround
    FixedQueue<AudioData> mixed_queue;
    // The blocks queued sound is kept in, made when the port opens: a bed's, up to 8
    // channels, objects' mono sound, and the mix's
    BlockPool bed_pool;
    BlockPool sample_pool;
    BlockPool mix_pool;
};

struct Audio3dState {
    Common::Combiner ports_mutex;
    std::unordered_map<OrbisAudio3dPortId, Port> ports;
};

s32 PS4_SYSV_ABI sceAudio3dAudioOutClose(s32 handle);
s32 PS4_SYSV_ABI sceAudio3dAudioOutOpen(OrbisAudio3dPortId port_id,
                                        Libraries::UserService::OrbisUserServiceUserId user_id,
                                        s32 type, s32 index, u32 len, u32 freq,
                                        AudioOut::OrbisAudioOutParamExtendedInformation param);
s32 PS4_SYSV_ABI sceAudio3dAudioOutOutput(s32 handle, void* ptr);
s32 PS4_SYSV_ABI sceAudio3dAudioOutOutputs(AudioOut::OrbisAudioOutOutputParam* param, u32 num);
s32 PS4_SYSV_ABI sceAudio3dBedWrite(OrbisAudio3dPortId port_id, u32 num_channels,
                                    OrbisAudio3dFormat format, void* buffer, u32 num_samples);
s32 PS4_SYSV_ABI sceAudio3dBedWrite2(OrbisAudio3dPortId port_id, u32 num_channels,
                                     OrbisAudio3dFormat format, void* buffer, u32 num_samples,
                                     OrbisAudio3dOutputRoute output_route, bool restricted);
s32 PS4_SYSV_ABI sceAudio3dCreateSpeakerArray();
s32 PS4_SYSV_ABI sceAudio3dDeleteSpeakerArray();
s32 PS4_SYSV_ABI sceAudio3dGetDefaultOpenParameters(OrbisAudio3dOpenParameters* params);
s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMemorySize();
s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients();
s32 PS4_SYSV_ABI sceAudio3dGetSpeakerArrayMixCoefficients2();
s32 PS4_SYSV_ABI sceAudio3dInitialize(s64 reserved);
s32 PS4_SYSV_ABI sceAudio3dObjectReserve(OrbisAudio3dPortId port_id,
                                         OrbisAudio3dObjectId* object_id);
s32 PS4_SYSV_ABI sceAudio3dObjectSetAttribute(OrbisAudio3dPortId port_id,
                                              OrbisAudio3dObjectId object_id,
                                              OrbisAudio3dAttributeId attribute_id,
                                              const void* attribute, u64 attribute_size);
s32 PS4_SYSV_ABI sceAudio3dObjectSetAttributes(OrbisAudio3dPortId port_id,
                                               OrbisAudio3dObjectId object_id, u64 num_attributes,
                                               const OrbisAudio3dAttribute* attribute_array);
s32 PS4_SYSV_ABI sceAudio3dObjectUnreserve(OrbisAudio3dPortId port_id,
                                           OrbisAudio3dObjectId object_id);
s32 PS4_SYSV_ABI sceAudio3dPortAdvance(OrbisAudio3dPortId port_id);
s32 PS4_SYSV_ABI sceAudio3dPortClose(OrbisAudio3dPortId port_id);
s32 PS4_SYSV_ABI sceAudio3dPortCreate(u32 granularity, u32 rate, s64 reserved,
                                      OrbisAudio3dPortId* port_id);
s32 PS4_SYSV_ABI sceAudio3dPortDestroy();
s32 PS4_SYSV_ABI sceAudio3dPortFlush(OrbisAudio3dPortId port_id);
s32 PS4_SYSV_ABI sceAudio3dPortFreeState();
s32 PS4_SYSV_ABI sceAudio3dPortGetAttributesSupported(OrbisAudio3dPortId port_id,
                                                      OrbisAudio3dAttributeId* capabilities,
                                                      u32* num_capabilities);
s32 PS4_SYSV_ABI sceAudio3dPortGetList();
s32 PS4_SYSV_ABI sceAudio3dPortGetParameters();
s32 PS4_SYSV_ABI sceAudio3dPortGetQueueLevel(OrbisAudio3dPortId port_id, u32* queue_level,
                                             u32* queue_available);
s32 PS4_SYSV_ABI sceAudio3dPortGetState();
s32 PS4_SYSV_ABI sceAudio3dPortGetStatus();
s32 PS4_SYSV_ABI sceAudio3dPortOpen(Libraries::UserService::OrbisUserServiceUserId user_id,
                                    const OrbisAudio3dOpenParameters* parameters,
                                    OrbisAudio3dPortId* port_id);
s32 PS4_SYSV_ABI sceAudio3dPortPush(OrbisAudio3dPortId port_id, OrbisAudio3dBlocking blocking);
s32 PS4_SYSV_ABI sceAudio3dPortQueryDebug();
s32 PS4_SYSV_ABI sceAudio3dPortSetAttribute(OrbisAudio3dPortId port_id,
                                            OrbisAudio3dAttributeId attribute_id, void* attribute,
                                            u64 attribute_size);
s32 PS4_SYSV_ABI sceAudio3dReportRegisterHandler();
s32 PS4_SYSV_ABI sceAudio3dReportUnregisterHandler();
s32 PS4_SYSV_ABI sceAudio3dSetGpuRenderer();
s32 PS4_SYSV_ABI sceAudio3dStrError();
s32 PS4_SYSV_ABI sceAudio3dTerminate();

void RegisterLib(Core::Loader::SymbolsResolver* sym);
} // namespace Libraries::Audio3d
