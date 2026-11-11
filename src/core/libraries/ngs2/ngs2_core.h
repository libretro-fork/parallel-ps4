/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef NGS2_CORE_H
#define NGS2_CORE_H

/* libSceNgs2's sound engine: systems, racks and voices, and the rendering
 * of a grain of sound through them.
 *
 * Everything a system or a rack needs is laid out, when it is made, in the
 * buffer the game gives for it: nothing is allocated while a game plays. A
 * waveform block's data is read where the game has it. (An ATRAC9 decoder
 * is opened the first time a rack meets a configuration, and kept for the
 * next voice with it.)
 *
 * None of this takes a lock or is safe for two threads at once: the caller
 * holds the system's lock around every call that takes a system, a rack or
 * a voice.
 *
 * The structures named ngs2_*_param are the PS4's, as a game fills them. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NGS2_MAX_CHANNELS 8
#define NGS2_MAX_LEVELS (NGS2_MAX_CHANNELS * NGS2_MAX_CHANNELS)
#define NGS2_MAX_MODULES 24
#define NGS2_MAX_CUSTOM_PORTS 16
#define NGS2_MAX_BUFFERS 4
#define NGS2_MAX_OUTPUTS 8

#define NGS2_RACK_SAMPLER 0x1000u
#define NGS2_RACK_SUBMIXER 0x2000u
#define NGS2_RACK_REVERB 0x2001u
#define NGS2_RACK_MASTERING 0x3000u
#define NGS2_RACK_CUSTOM_SAMPLER 0x4001u
#define NGS2_RACK_CUSTOM_SUBMIXER 0x4002u
#define NGS2_RACK_CUSTOM_MASTERING 0x4003u

#define NGS2_WAVE_PCM_U8 0x11u
#define NGS2_WAVE_PCM_I16L 0x12u
#define NGS2_WAVE_PCM_I16B 0x13u
#define NGS2_WAVE_PCM_I24L 0x14u
#define NGS2_WAVE_PCM_I24B 0x15u
#define NGS2_WAVE_PCM_I32L 0x16u
#define NGS2_WAVE_PCM_I32B 0x17u
#define NGS2_WAVE_PCM_F32L 0x18u
#define NGS2_WAVE_PCM_F32B 0x19u
#define NGS2_WAVE_PCM_F64L 0x1au
#define NGS2_WAVE_PCM_F64B 0x1bu
#define NGS2_WAVE_VAG 0x1cu
#define NGS2_WAVE_ATRAC9 0x40u

#define NGS2_EVENT_PLAY 1u
#define NGS2_EVENT_STOP 2u
#define NGS2_EVENT_STOP_IMM 4u
#define NGS2_EVENT_KILL 8u
#define NGS2_EVENT_PAUSE 16u
#define NGS2_EVENT_RESUME 32u

#define NGS2_STATE_INUSE 1u
#define NGS2_STATE_PLAYING 2u
#define NGS2_STATE_PAUSED 4u
#define NGS2_STATE_STOPPED 8u
#define NGS2_STATE_EMPTY 0x20u

#define NGS2_BLOCKS_CONTINUE 1u
#define NGS2_BLOCKS_APPEND 2u
#define NGS2_BLOCKS_RESET 4u
#define NGS2_BLOCKS_SILENCE 0x10u

#define NGS2_CALLBACK_BLOCK_END 1u
#define NGS2_CALLBACK_BLOCK_REPEAT 2u

#define NGS2_REPEAT_ENDLESS 0xffffffffu

/* What the PS4's library answers */
#define NGS2_OK 0
#define NGS2_ERROR_INVALID_NUM_CHANNELS ((int)0x804A0052)
#define NGS2_ERROR_INVALID_NUM_MATRIX_LEVELS ((int)0x804A0150)
#define NGS2_ERROR_INVALID_NUM_WAVEFORM_BLOCKS ((int)0x804A0151)
#define NGS2_ERROR_INVALID_MATRIX_LEVEL_ADDRESS ((int)0x804A0153)
#define NGS2_ERROR_INVALID_WAVEFORM_BLOCK_ADDRESS ((int)0x804A0154)
#define NGS2_ERROR_INVALID_BUFFER_SIZE ((int)0x804A0209)
#define NGS2_ERROR_INVALID_VOICE_HANDLE ((int)0x804A0300)
#define NGS2_ERROR_UNINIT_VOICE ((int)0x804A0301)
#define NGS2_ERROR_INVALID_EVENT_TYPE ((int)0x804A0303)
#define NGS2_ERROR_INVALID_PORT_INDEX ((int)0x804A0304)
#define NGS2_ERROR_INVALID_PATCH ((int)0x804A0305)
#define NGS2_ERROR_EMPTY_CODEC_DECODER ((int)0x804A0307)
#define NGS2_ERROR_INVALID_VOICE_CONTROL_ID ((int)0x804A0308)
#define NGS2_ERROR_INVALID_VOICE_CONTROL_ADDRESS ((int)0x804A0309)
#define NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE ((int)0x804A030A)
#define NGS2_ERROR_DETECTED_CIRCULAR_VOICE_CONTROL ((int)0x804A030B)
#define NGS2_ERROR_INVALID_WAVEFORM_TYPE ((int)0x804A0402)
#define NGS2_ERROR_INVALID_WAVEFORM_CONFIG ((int)0x804A0403)
#define NGS2_ERROR_INVALID_WAVEFORM_SAMPLE_RATE ((int)0x804A0404)
#define NGS2_ERROR_INVALID_MATRIX_ID ((int)0x804A0450)

typedef struct ngs2_system ngs2_system;
typedef struct ngs2_rack ngs2_rack;
typedef struct ngs2_voice ngs2_voice;

/* The PS4's structures */

typedef struct ngs2_param_header {
    uint16_t size;
    int16_t next; /* bytes from this parameter to the next, 0 at the last */
    uint32_t id;
} ngs2_param_header;

typedef struct ngs2_waveform_format {
    uint32_t type;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t config_data;
    uint32_t frame_offset;
    uint32_t frame_margin;
} ngs2_waveform_format;

typedef struct ngs2_waveform_block {
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t num_repeats;
    uint32_t num_skip_samples;
    uint32_t num_samples;
    uint32_t reserved;
    uintptr_t user_data;
} ngs2_waveform_block;

typedef struct ngs2_callback_info {
    uintptr_t callback_data;
    uintptr_t voice_handle;
    uint32_t flag;
    uint32_t reserved;
    uintptr_t user_data;
    const void* data;
    uint32_t data_size;
    uint32_t repeated_count;
    uint32_t attribute_flags;
    uint32_t reserved2;
} ngs2_callback_info;

typedef struct ngs2_user_fx_context {
    float** channel_data;
    uintptr_t user_data0;
    uintptr_t user_data1;
    uintptr_t user_data2;
    uint32_t flags;
    uint32_t channels;
    uint32_t grain_samples;
    uint32_t sample_rate;
} ngs2_user_fx_context;

typedef struct ngs2_user_fx2_setup_context {
    void* common;
    void* param;
    void* work;
    uintptr_t user_data;
    uint32_t max_voices;
    uint32_t voice_index;
    uint64_t reserved[4];
} ngs2_user_fx2_setup_context;

typedef struct ngs2_user_fx2_control_context {
    const void* data;
    size_t data_size;
    void* common;
    void* param;
    uintptr_t user_data;
    uint64_t reserved[4];
} ngs2_user_fx2_control_context;

typedef struct ngs2_user_fx2_process_context {
    float** channel_data;
    void* common;
    const void* param;
    void* work;
    void* state;
    uintptr_t user_data;
    uint32_t flags;
    uint32_t input_channels;
    uint32_t output_channels;
    uint32_t grain_samples;
    uint32_t sample_rate;
    uint32_t reserved;
    uint64_t reserved2[4];
} ngs2_user_fx2_process_context;

typedef struct ngs2_i3dl2 {
    float wet;
    float dry;
    int32_t room;
    int32_t room_hf;
    uint32_t reflection_pattern;
    float decay_time;
    float decay_hf_ratio;
    int32_t reflections;
    float reflections_delay;
    int32_t reverb;
    float reverb_delay;
    float diffusion;
    float density;
    float hf_reference;
    uint32_t reserve[8];
} ngs2_i3dl2;

typedef struct ngs2_render_buffer {
    void* buffer;
    size_t buffer_size;
    uint32_t waveform_type;
    uint32_t channels;
} ngs2_render_buffer;

typedef struct ngs2_port_info {
    int32_t matrix_id;
    float volume;
    uint32_t delay_samples;
    uint32_t dest_input_id;
    uintptr_t dest_handle;
} ngs2_port_info;

/* The state of a sampler voice, as the PS4's library reports it */
typedef struct ngs2_sampler_state {
    uint32_t state_flags;
    float envelope_height;
    float peak_height;
    uint32_t reserved;
    uint64_t decoded_samples;
    uint64_t decoded_bytes;
    uint64_t user_data;
    const void* waveform_data;
} ngs2_sampler_state;

/* What calls into the game's code go through: the game's functions have its
 * own calling convention, which only the caller of this engine knows. */
typedef struct ngs2_host {
    int (*user_fx)(void* handler, ngs2_user_fx_context* context);
    int (*user_fx2_setup)(void* handler, ngs2_user_fx2_setup_context* context);
    int (*user_fx2_cleanup)(void* handler, ngs2_user_fx2_setup_context* context);
    int (*user_fx2_control)(void* handler, ngs2_user_fx2_control_context* context);
    int (*user_fx2_process)(void* handler, ngs2_user_fx2_process_context* context);
} ngs2_host;

/* A module of a custom rack */
typedef struct ngs2_module_desc {
    uint32_t id;
    uint32_t source_buffer;
    uint32_t extra_buffer;
    uint32_t dest_buffer;
    uint32_t state_size;
    uint32_t reverb_size;
    /* user fx 2 */
    void* setup_handler;
    void* cleanup_handler;
    void* control_handler;
    void* process_handler;
    size_t common_size;
    size_t param_size;
    size_t work_size;
    uintptr_t user_data;
} ngs2_module_desc;

/* What a rack is made with: the game's options, with the library's values
 * where the game gave none. */
typedef struct ngs2_rack_desc {
    uint32_t rack_id;
    char name[16];
    uint32_t max_grain_samples;
    uint32_t max_voices;
    uint32_t max_matrices;
    uint32_t max_ports;
    uint32_t max_channels;
    uint32_t max_inputs;
    uint32_t max_waveform_blocks;
    uint32_t max_filters;
    uint32_t max_atrac9_decoders;
    uint32_t reverb_size;
    uint32_t num_buffers;
    uint32_t num_modules;
    ngs2_module_desc modules[NGS2_MAX_MODULES];
    uint32_t port_buffers[NGS2_MAX_CUSTOM_PORTS];
} ngs2_rack_desc;

/* A system. ngs2_system_size() bytes at @mem, aligned to 16, hold all of it. */
size_t ngs2_system_size(uint32_t max_grain_samples);
ngs2_system* ngs2_system_init(void* mem, size_t size, const char* name, uint32_t max_grain_samples,
                              uint32_t grain_samples, uint32_t sample_rate, const ngs2_host* host);
/* Whether @handle is a system that is not destroyed */
ngs2_system* ngs2_system_from(uintptr_t handle);
void ngs2_system_destroy(ngs2_system* system);
void* ngs2_system_memory(const ngs2_system* system, size_t* size);
/* A word of the caller's the system carries (its lock) */
void ngs2_system_set_host_data(ngs2_system* system, void* data);
void* ngs2_system_host_data(const ngs2_system* system);
void ngs2_system_set_user_data(ngs2_system* system, uintptr_t data);
uintptr_t ngs2_system_user_data(const ngs2_system* system);
int ngs2_system_set_grain_samples(ngs2_system* system, uint32_t samples);
int ngs2_system_set_sample_rate(ngs2_system* system, uint32_t rate);
uint32_t ngs2_system_grain_samples(const ngs2_system* system);
uint32_t ngs2_system_sample_rate(const ngs2_system* system);
uint32_t ngs2_system_rack_count(const ngs2_system* system);
uint64_t ngs2_system_render_count(const ngs2_system* system);

ngs2_rack* ngs2_system_rack(ngs2_system* system, uint32_t index);

/* Renders one grain into @buffers: each mastering voice that plays goes to
 * the buffer its output names. A buffer no voice reaches is silence. */
int ngs2_system_render(ngs2_system* system, const ngs2_render_buffer* buffers, uint32_t count);
/* The game is to hear of each block that ended or went round in the grains
 * rendered: takes the oldest it has not, 0 when there is none. They are for
 * the caller to make, once it holds no lock the game's code could want. */
int ngs2_system_next_callback(ngs2_system* system, void** handler, ngs2_callback_info* info);

/* A rack. ngs2_rack_size() is 0 for a description no rack can be made of. */
size_t ngs2_rack_size(const ngs2_rack_desc* desc);
ngs2_rack* ngs2_rack_init(ngs2_system* system, void* mem, size_t size, const ngs2_rack_desc* desc);
ngs2_rack* ngs2_rack_from(uintptr_t handle);
/* Takes the rack out of its system; the voices patched into it lose it. */
void ngs2_rack_destroy(ngs2_rack* rack);
void* ngs2_rack_memory(const ngs2_rack* rack, size_t* size);
ngs2_system* ngs2_rack_system(const ngs2_rack* rack);
const ngs2_rack_desc* ngs2_rack_description(const ngs2_rack* rack);
void ngs2_rack_set_user_data(ngs2_rack* rack, uintptr_t data);
uintptr_t ngs2_rack_user_data(const ngs2_rack* rack);
ngs2_voice* ngs2_rack_voice(ngs2_rack* rack, uint32_t index);
uint32_t ngs2_rack_active_voices(const ngs2_rack* rack);

/* A voice */
ngs2_voice* ngs2_voice_from(uintptr_t handle);
ngs2_rack* ngs2_voice_rack(const ngs2_voice* voice);
uint32_t ngs2_voice_index(const ngs2_voice* voice);
/* Applies a list of parameters, in order; stops at the first refused. */
int ngs2_voice_control(ngs2_voice* voice, const ngs2_param_header* list);
uint32_t ngs2_voice_state_flags(const ngs2_voice* voice);
void ngs2_voice_sampler_state(const ngs2_voice* voice, ngs2_sampler_state* state);
float ngs2_voice_peak(const ngs2_voice* voice);
int ngs2_voice_port_info(const ngs2_voice* voice, uint32_t port, ngs2_port_info* info);
/* @levels has room for NGS2_MAX_LEVELS */
int ngs2_voice_matrix_info(const ngs2_voice* voice, uint32_t matrix, uint32_t* num_levels,
                           float* levels);

/* How loud channel @source of @source_channels is in channel @dest of
 * @dest_channels where a port names no matrix */
float ngs2_default_level(uint32_t source_channels, uint32_t source, uint32_t dest_channels,
                         uint32_t dest);

#ifdef __cplusplus
}
#endif

#endif
