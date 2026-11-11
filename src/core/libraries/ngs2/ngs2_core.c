/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <string.h>

#include <formats/ratrac9.h>

#include "ngs2_core.h"

#define MAGIC_SYSTEM 0x5332474eu /* "NG2S" */
#define MAGIC_RACK 0x5232474eu
#define MAGIC_VOICE 0x5632474eu

#define MAX_RACKS 64
#define MAX_CALLBACKS 256
#define MIN_GRAIN 64u
#define MAX_GRAIN 2048u
#define FRAC_BITS 16
#define FRAC_ONE (1u << FRAC_BITS)
#define MAX_STEP (8u << FRAC_BITS) /* the fastest a sampler reads its waveform */
#define HISTORY 4                  /* frames the resampler keeps from grain to grain */
#define AT9_UNIT 256               /* the most samples an ATRAC9 frame has */
#define VAG_SAMPLES 28
#define VAG_BYTES 16

#define MODULE_FILTER 0x13u
#define MODULE_GAIN 0x15u
#define MODULE_USER_FX 0x19u
#define MODULE_REVERB 0x1bu
#define MODULE_USER_FX2 0x1fu

#define PARAM_MATRIX_LEVELS 1u
#define PARAM_PORT_VOLUME 2u
#define PARAM_PORT_MATRIX 3u
#define PARAM_PORT_DELAY 4u
#define PARAM_PATCH 5u
#define PARAM_EVENT 6u
#define PARAM_CALLBACK 7u

enum { PLAY_EMPTY = 0, PLAY_PLAYING, PLAY_PAUSED, PLAY_STOPPED };
enum { RAMP_NONE = 0, RAMP_IN, RAMP_OUT };

typedef struct ngs2_block {
    const uint8_t* data;
    ngs2_waveform_block info;
    uint32_t repeats; /* as the game gave it; info.num_repeats counts down */
} ngs2_block;

typedef struct ngs2_port {
    ngs2_voice* dest;
    uint32_t dest_input;
    int32_t matrix;
    float volume;
    int mixed;                    /* gains holds the last grain's */
    float gains[NGS2_MAX_LEVELS]; /* [dest * source channels + source] */
} ngs2_port;

typedef struct ngs2_biquad {
    int enabled;
    uint32_t location;
    uint32_t channel_mask;
    float b0, b1, b2, a1, a2;
    float z[NGS2_MAX_CHANNELS][2];
} ngs2_biquad;

#define REVERB_COMBS 8
#define REVERB_ALLPASS 4
#define REVERB_TAPS 6

typedef struct ngs2_delay {
    float* line;
    uint32_t size;
    uint32_t pos;
} ngs2_delay;

typedef struct ngs2_reverb {
    ngs2_i3dl2 param;
    ngs2_delay pre;
    ngs2_delay comb[2][REVERB_COMBS];
    float comb_store[2][REVERB_COMBS];
    float comb_gain[2][REVERB_COMBS];
    ngs2_delay allpass[2][REVERB_ALLPASS];
    uint32_t tap[REVERB_TAPS];
    uint32_t late_tap;
    float damp;
    float early_level;
    float late_level;
    uint32_t rate;
    int active; /* there may be sound left in it */
} ngs2_reverb;

typedef struct ngs2_module_state {
    float gain[NGS2_MAX_CHANNELS];
    float gain_to[NGS2_MAX_CHANNELS];
    ngs2_biquad filter;
    ngs2_reverb* reverb;
    void* fx_handler;
    uintptr_t fx_data[3];
    uint32_t fx_flags;
    void* fx2_param;
    void* fx2_work;
    void* fx2_state;
    int fx2_active;
} ngs2_module_state;

typedef struct ngs2_decoder {
    ratrac9_t* at9;
    uint8_t config[4];
    int used;
} ngs2_decoder;

struct ngs2_voice {
    uint32_t magic;
    uint32_t index;
    ngs2_rack* rack;
    int play;
    int ramp;
    int setup;
    uint32_t flags; /* as the game reads them */
    uint32_t channels;
    uint32_t in_channels;
    uint32_t waiting; /* ports of voices not yet rendered this grain that feed it */
    int rendered;

    ngs2_port* ports;
    float* matrix_levels;    /* max_matrices of NGS2_MAX_LEVELS */
    uint32_t* matrix_counts; /* 0: the matrix was never set */
    ngs2_biquad* filters;
    float* buf[NGS2_MAX_BUFFERS]; /* a mixer's: channels of max_grain, one after another */
    uint64_t buf_stamp[NGS2_MAX_BUFFERS];
    ngs2_module_state* modules;
    ngs2_reverb* reverb;

    void* callback;
    uintptr_t callback_data;
    uint32_t callback_flags;
    void* fx_handler;
    uintptr_t fx_data[3];
    uint32_t fx_flags;
    float peak;

    /* mastering */
    uint32_t output_id;
    float fbw_level;
    float lfe_level;

    /* sampler */
    ngs2_waveform_format format;
    float pitch;
    uint32_t phase;
    uint32_t cached; /* frames of history that hold sound */
    float history[NGS2_MAX_CHANNELS][HISTORY];
    ngs2_block* blocks; /* a ring of max_waveform_blocks */
    ngs2_block* saved;  /* the blocks as given, to play again */
    uint32_t block_head;
    uint32_t block_count;
    uint32_t saved_count;
    int accepts_blocks;
    int starved;
    int block_started;
    uint32_t block_skip; /* frames of the block still to drop */
    uint32_t block_left; /* frames of the block still to give */
    uint32_t block_pos;  /* bytes of the block's data read */
    uint64_t decoded_samples;
    uint64_t decoded_bytes;
    const void* waveform_data;
    uintptr_t waveform_user_data;
    ngs2_decoder* decoder;
    float* window; /* ATRAC9: a frame, channel after channel; VAG the same */
    uint32_t window_units;
    uint32_t window_pos;
    uint32_t window_count;
    int32_t vag_history[NGS2_MAX_CHANNELS][2];
    float vag_window[NGS2_MAX_CHANNELS * VAG_SAMPLES];
};

struct ngs2_rack {
    uint32_t magic;
    uint32_t uid;
    ngs2_system* system;
    void* memory;
    size_t memory_size;
    uintptr_t user_data;
    ngs2_rack_desc desc;
    uint32_t buffers; /* a mixer voice's */
    ngs2_voice* voices;
    float* scratch; /* a sampler's waveform before resampling */
    uint32_t scratch_stride;
    float* out; /* a voice's grain when it has no buffer of its own */
    ngs2_decoder* decoders;
    uint32_t decoder_count;
    float* windows; /* units of AT9_UNIT samples */
    uint16_t* window_owner;
    uint32_t window_units;
    void* fx2_common[NGS2_MAX_MODULES];
};

typedef struct ngs2_pending_callback {
    void* handler;
    ngs2_callback_info info;
} ngs2_pending_callback;

struct ngs2_system {
    uint32_t magic;
    uint32_t uid;
    char name[16];
    void* memory;
    size_t memory_size;
    void* host_data;
    uintptr_t user_data;
    ngs2_host host;
    uint32_t max_grain;
    uint32_t grain;
    uint32_t sample_rate;
    uint32_t next_rack_uid;
    uint64_t render_count;
    uint32_t rack_count;
    ngs2_rack* racks[MAX_RACKS];
    float* mix; /* NGS2_MAX_OUTPUTS of max_grain * NGS2_MAX_CHANNELS, interleaved */
    int mix_used[NGS2_MAX_OUTPUTS];
    ngs2_pending_callback callbacks[MAX_CALLBACKS];
    uint32_t callback_count;
};

/* ------------------------------------------------------------------ */
/* A buffer handed out piece by piece                                  */

typedef struct arena {
    uint8_t* base;
    size_t used;
    size_t size; /* 0: only counting */
} arena;

static void* arena_take(arena* a, size_t bytes) {
    size_t at = (a->used + 15u) & ~(size_t)15u;
    a->used = at + bytes;
    if (!a->size)
        return NULL;
    if (a->used > a->size)
        return NULL;
    memset(a->base + at, 0, bytes);
    return a->base + at;
}

static int is_sampler(const ngs2_rack* rack) {
    return rack->desc.rack_id == NGS2_RACK_SAMPLER ||
           rack->desc.rack_id == NGS2_RACK_CUSTOM_SAMPLER;
}

static int is_mastering(const ngs2_rack* rack) {
    return rack->desc.rack_id == NGS2_RACK_MASTERING ||
           rack->desc.rack_id == NGS2_RACK_CUSTOM_MASTERING;
}

static int is_custom(const ngs2_rack* rack) {
    return (rack->desc.rack_id & 0xf000u) == 0x4000u;
}

/* ------------------------------------------------------------------ */
/* Default levels                                                      */

#define HALF_POWER 0.70710678f

/* Where a channel of a 1, 2, 6 or 8 channel layout sits */
enum { SPK_C, SPK_L, SPK_R, SPK_LFE, SPK_SL, SPK_SR, SPK_BL, SPK_BR };

static int speaker_of(uint32_t channels, uint32_t channel) {
    static const int l1[1] = {SPK_C};
    static const int l2[2] = {SPK_L, SPK_R};
    static const int l6[6] = {SPK_L, SPK_R, SPK_C, SPK_LFE, SPK_SL, SPK_SR};
    static const int l8[8] = {SPK_L, SPK_R, SPK_C, SPK_LFE, SPK_SL, SPK_SR, SPK_BL, SPK_BR};
    switch (channels) {
    case 1:
        return l1[0];
    case 2:
        return l2[channel];
    case 6:
        return l6[channel];
    case 8:
        return l8[channel];
    default:
        return -1;
    }
}

float ngs2_default_level(uint32_t source_channels, uint32_t source, uint32_t dest_channels,
                         uint32_t dest) {
    int from, to;
    if (source >= source_channels || dest >= dest_channels)
        return 0.0f;
    from = speaker_of(source_channels, source);
    to = speaker_of(dest_channels, dest);
    /* A layout with no name: each channel to the one of the same number */
    if (from < 0 || to < 0)
        return source == dest ? 1.0f : 0.0f;
    if (from == to)
        return 1.0f;
    switch (dest_channels) {
    case 1:
        switch (from) {
        case SPK_L:
        case SPK_R:
            return HALF_POWER;
        case SPK_LFE:
            return 0.0f;
        default:
            return 0.5f;
        }
    case 2:
        switch (from) {
        case SPK_C:
            return HALF_POWER;
        case SPK_SL:
        case SPK_BL:
            return to == SPK_L ? 0.5f : 0.0f;
        case SPK_SR:
        case SPK_BR:
            return to == SPK_R ? 0.5f : 0.0f;
        default:
            return 0.0f;
        }
    case 6:
        if (from == SPK_BL)
            return to == SPK_SL ? 1.0f : 0.0f;
        if (from == SPK_BR)
            return to == SPK_SR ? 1.0f : 0.0f;
        return 0.0f;
    default:
        return 0.0f;
    }
}

/* ------------------------------------------------------------------ */
/* Filters                                                             */

static void biquad_set(ngs2_biquad* f, uint32_t type, float fc, float q, float level,
                       const float* direct, float rate) {
    float w, s, c, alpha, a, t, a0, a1, a2, b0, b1, b2, inv;
    float pp, pm, mp, mm;
    if (type == 0) {
        f->enabled = 0;
        return;
    }
    if (type == 11) {
        f->b0 = direct[0];
        f->b1 = direct[1];
        f->b2 = direct[2];
        f->a1 = -direct[3];
        f->a2 = -direct[4];
        f->enabled = 1;
        return;
    }
    if (fc < 0.001f * rate)
        fc = 0.001f * rate;
    if (fc > 0.499f * rate)
        fc = 0.499f * rate;
    if (q < 0.0001f)
        q = 0.0001f;
    if (q > 32.0f)
        q = 32.0f;
    if (level < 0.0001f)
        level = 0.0001f;
    w = 6.2831853f * fc / rate;
    s = (float)sin(w);
    c = (float)cos(w);
    alpha = s / (2.0f * q);
    a = (float)sqrt(level);
    t = (float)sqrt(a) / q * s;
    pp = (a + 1.0f) + (a - 1.0f) * c;
    pm = (a + 1.0f) - (a - 1.0f) * c;
    mp = (a - 1.0f) + (a + 1.0f) * c;
    mm = (a - 1.0f) - (a + 1.0f) * c;
    a0 = 1.0f + alpha;
    a1 = -2.0f * c;
    a2 = 1.0f - alpha;
    b0 = b1 = b2 = 0.0f;
    switch (type) {
    case 1: /* low pass */
        b1 = 1.0f - c;
        b0 = b2 = b1 * 0.5f;
        break;
    case 2: /* high pass */
        b1 = -(1.0f + c);
        b0 = b2 = (1.0f + c) * 0.5f;
        break;
    case 3: /* band pass, peak gain q */
        b0 = s * 0.5f;
        b2 = -b0;
        break;
    case 4: /* band pass, peak gain 1 */
        b0 = alpha;
        b2 = -alpha;
        break;
    case 5: /* notch */
        b0 = b2 = 1.0f;
        b1 = -2.0f * c;
        break;
    case 6: /* low shelf */
        b0 = a * (pm + t);
        b1 = 2.0f * a * mm;
        b2 = a * (pm - t);
        a0 = pp + t;
        a1 = -2.0f * mp;
        a2 = pp - t;
        break;
    case 7: /* high shelf */
        b0 = a * (pp + t);
        b1 = -2.0f * a * mp;
        b2 = a * (pp - t);
        a0 = pm + t;
        a1 = 2.0f * mm;
        a2 = pm - t;
        break;
    case 9: /* one pole low pass */
        a0 = 1.0f + c + s;
        b0 = b1 = s;
        a1 = -(1.0f + c - s);
        a2 = 0.0f;
        break;
    case 10: /* one pole high pass */
        a0 = 1.0f + c + s;
        b0 = 1.0f + c;
        b1 = -b0;
        a1 = -(1.0f + c - s);
        a2 = 0.0f;
        break;
    default: /* peaking */
        b0 = 1.0f + alpha * a;
        b1 = -2.0f * c;
        b2 = 1.0f - alpha * a;
        a0 = 1.0f + alpha / a;
        a2 = 1.0f - alpha / a;
        break;
    }
    inv = 1.0f / a0;
    f->b0 = b0 * inv;
    f->b1 = b1 * inv;
    f->b2 = b2 * inv;
    f->a1 = a1 * inv;
    f->a2 = a2 * inv;
    f->enabled = 1;
}

/* @samples: @channels of @stride, the first @grain of each filtered */
static void biquad_run(ngs2_biquad* f, float* samples, uint32_t channels, uint32_t stride,
                       uint32_t grain) {
    uint32_t c, i;
    for (c = 0; c < channels; c++) {
        float* x = samples + (size_t)c * stride;
        float z0 = f->z[c][0], z1 = f->z[c][1];
        /* A channel the mask names goes by unfiltered */
        if (f->channel_mask & (1u << c))
            continue;
        for (i = 0; i < grain; i++) {
            float in = x[i];
            float out = f->b0 * in + z0;
            z0 = f->b1 * in - f->a1 * out + z1;
            z1 = f->b2 * in - f->a2 * out;
            x[i] = out;
        }
        /* What is left of a sound that ended is too small to hear and slow to compute */
        if (z0 > -1e-20f && z0 < 1e-20f)
            z0 = 0.0f;
        if (z1 > -1e-20f && z1 < 1e-20f)
            z1 = 0.0f;
        f->z[c][0] = z0;
        f->z[c][1] = z1;
    }
}

/* ------------------------------------------------------------------ */
/* Reverb: the I3DL2 parameters over a pre-delay, early taps, and eight */
/* damped combs into four allpasses on each side                        */

static const uint32_t comb_len[REVERB_COMBS] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static const uint32_t allpass_len[REVERB_ALLPASS] = {556, 441, 341, 225};
static const float tap_at[REVERB_TAPS] = {1.0f, 1.31f, 1.73f, 2.19f, 2.71f, 3.37f};
static const float tap_level[REVERB_TAPS] = {0.84f, -0.71f, 0.60f, -0.50f, 0.42f, -0.35f};
#define REVERB_SPREAD 23u
#define REVERB_PRE_SECONDS 0.5f
/* A rack's size is asked before its system is known: a reverb's delay lines are
 * as long as this rate needs, and no longer at a higher one */
#define REVERB_LAYOUT_RATE 48000u

static uint32_t reverb_scale(uint32_t length, uint32_t rate) {
    return (uint32_t)(((uint64_t)length * rate + 22050u) / 44100u) + 1u;
}

static void reverb_layout(ngs2_reverb* r, arena* a, uint32_t rate) {
    uint32_t side, i, n;
    n = (uint32_t)(REVERB_PRE_SECONDS * (float)rate) + 1u;
    if (r) {
        r->rate = rate;
        r->pre.size = n;
    }
    {
        float* line = (float*)arena_take(a, n * sizeof(float));
        if (r)
            r->pre.line = line;
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; i < REVERB_COMBS; i++) {
            float* line;
            n = reverb_scale(comb_len[i] + side * REVERB_SPREAD, rate);
            line = (float*)arena_take(a, n * sizeof(float));
            if (r) {
                r->comb[side][i].line = line;
                r->comb[side][i].size = n;
            }
        }
        for (i = 0; i < REVERB_ALLPASS; i++) {
            float* line;
            n = reverb_scale(allpass_len[i] + side * REVERB_SPREAD, rate);
            line = (float*)arena_take(a, n * sizeof(float));
            if (r) {
                r->allpass[side][i].line = line;
                r->allpass[side][i].size = n;
            }
        }
    }
}

static float millibel(int32_t mb) {
    if (mb <= -10000)
        return 0.0f;
    return (float)pow(10.0, (double)mb / 2000.0);
}

static void reverb_set(ngs2_reverb* r, const ngs2_i3dl2* p) {
    uint32_t side, i;
    float decay = p->decay_time, ratio = p->decay_hf_ratio, room;
    float pre = p->reflections_delay, late = p->reverb_delay;
    r->param = *p;
    if (decay < 0.1f)
        decay = 0.1f;
    if (decay > 20.0f)
        decay = 20.0f;
    if (ratio < 0.1f)
        ratio = 0.1f;
    if (ratio > 2.0f)
        ratio = 2.0f;
    if (pre < 0.0f)
        pre = 0.0f;
    if (late < 0.0f)
        late = 0.0f;
    for (side = 0; side < 2; side++)
        for (i = 0; i < REVERB_COMBS; i++)
            r->comb_gain[side][i] = (float)pow(10.0, -3.0 * (double)r->comb[side][i].size /
                                                         ((double)r->rate * (double)decay));
    /* High frequencies die @ratio times as fast: more of the comb's low pass */
    r->damp = ratio >= 1.0f ? 0.0f : (1.0f - ratio) * 0.6f;
    room = millibel(p->room);
    r->early_level = room * millibel(p->reflections);
    r->late_level = room * millibel(p->reverb) * 0.12f;
    for (i = 0; i < REVERB_TAPS; i++) {
        uint32_t at = (uint32_t)(pre * tap_at[i] * (float)r->rate);
        r->tap[i] = at < r->pre.size - 1u ? at : r->pre.size - 1u;
    }
    {
        uint32_t at = (uint32_t)((pre + late) * (float)r->rate);
        r->late_tap = at < r->pre.size - 1u ? at : r->pre.size - 1u;
    }
}

static void reverb_defaults(ngs2_reverb* r) {
    ngs2_i3dl2 p;
    memset(&p, 0, sizeof(p));
    p.wet = 1.0f;
    p.dry = 0.0f;
    p.room = -1000;
    p.room_hf = -100;
    p.decay_time = 1.49f;
    p.decay_hf_ratio = 0.83f;
    p.reflections = -2602;
    p.reflections_delay = 0.007f;
    p.reverb = 200;
    p.reverb_delay = 0.011f;
    p.diffusion = 100.0f;
    p.density = 100.0f;
    p.hf_reference = 5000.0f;
    reverb_set(r, &p);
}

static void reverb_clear(ngs2_reverb* r) {
    uint32_t side, i;
    memset(r->pre.line, 0, r->pre.size * sizeof(float));
    r->pre.pos = 0;
    for (side = 0; side < 2; side++) {
        for (i = 0; i < REVERB_COMBS; i++) {
            memset(r->comb[side][i].line, 0, r->comb[side][i].size * sizeof(float));
            r->comb[side][i].pos = 0;
            r->comb_store[side][i] = 0.0f;
        }
        for (i = 0; i < REVERB_ALLPASS; i++) {
            memset(r->allpass[side][i].line, 0, r->allpass[side][i].size * sizeof(float));
            r->allpass[side][i].pos = 0;
        }
    }
    r->active = 0;
}

/* @in: @in_ch channels of @stride; @out: @out_ch channels of @stride, which
 * may be @in itself only when the channel counts are equal. Returns whether
 * anything but silence came out. */
static int reverb_run(ngs2_reverb* r, const float* in, uint32_t in_ch, float* out, uint32_t out_ch,
                      uint32_t stride, uint32_t grain, int has_input) {
    uint32_t i, c, k, side;
    float energy = 0.0f;
    float wet = r->param.wet, dry = r->param.dry;
    float mono_gain = 1.0f / (float)in_ch;
    if (!has_input && !r->active) {
        if (out != in)
            for (c = 0; c < out_ch; c++)
                memset(out + (size_t)c * stride, 0, grain * sizeof(float));
        return 0;
    }
    for (i = 0; i < grain; i++) {
        float x = 0.0f, early = 0.0f, late_in, rev[2], dry_in[NGS2_MAX_CHANNELS];
        for (c = 0; c < in_ch; c++) {
            dry_in[c] = in[(size_t)c * stride + i];
            x += dry_in[c];
        }
        x *= mono_gain;
        r->pre.line[r->pre.pos] = x;
        for (k = 0; k < REVERB_TAPS; k++)
            early +=
                tap_level[k] * r->pre.line[(r->pre.pos + r->pre.size - r->tap[k]) % r->pre.size];
        late_in = r->pre.line[(r->pre.pos + r->pre.size - r->late_tap) % r->pre.size];
        if (++r->pre.pos == r->pre.size)
            r->pre.pos = 0;
        for (side = 0; side < 2; side++) {
            float sum = 0.0f;
            for (k = 0; k < REVERB_COMBS; k++) {
                ngs2_delay* d = &r->comb[side][k];
                float y = d->line[d->pos];
                float store = y * (1.0f - r->damp) + r->comb_store[side][k] * r->damp;
                r->comb_store[side][k] = store;
                d->line[d->pos] = late_in + store * r->comb_gain[side][k];
                if (++d->pos == d->size)
                    d->pos = 0;
                sum += y;
            }
            for (k = 0; k < REVERB_ALLPASS; k++) {
                ngs2_delay* d = &r->allpass[side][k];
                float buffered = d->line[d->pos];
                float y = buffered - sum;
                d->line[d->pos] = sum + buffered * 0.5f;
                if (++d->pos == d->size)
                    d->pos = 0;
                sum = y;
            }
            rev[side] = early * r->early_level + sum * r->late_level;
            energy += rev[side] * rev[side];
        }
        for (c = 0; c < out_ch; c++) {
            int spk = speaker_of(out_ch, c);
            float d = 0.0f, w;
            if (in_ch == out_ch)
                d = dry_in[c];
            else
                for (k = 0; k < in_ch; k++)
                    d += dry_in[k] * ngs2_default_level(in_ch, k, out_ch, c);
            switch (spk) {
            case SPK_C:
                w = out_ch == 1 ? (rev[0] + rev[1]) * 0.5f : 0.0f;
                break;
            case SPK_LFE:
                w = 0.0f;
                break;
            case SPK_L:
                w = rev[0];
                break;
            case SPK_R:
                w = rev[1];
                break;
            case SPK_SL:
            case SPK_BL:
                w = rev[0] * HALF_POWER;
                break;
            case SPK_SR:
            case SPK_BR:
                w = rev[1] * HALF_POWER;
                break;
            default:
                w = rev[c & 1u];
                break;
            }
            out[(size_t)c * stride + i] = d * dry + w * wet;
        }
    }
    /* Quiet for a whole grain with nothing coming in: it has rung out */
    r->active = has_input || energy > 1e-12f * (float)grain;
    if (!r->active)
        reverb_clear(r);
    return has_input || r->active;
}

/* ------------------------------------------------------------------ */
/* Systems                                                             */

static int valid_grain(uint32_t grain, uint32_t max) {
    return grain >= MIN_GRAIN && grain <= max && grain % MIN_GRAIN == 0;
}

static int valid_rate(uint32_t rate) {
    static const uint32_t rates[] = {11025, 12000, 22050, 24000,  44100,
                                     48000, 88200, 96000, 176400, 192000};
    size_t i;
    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++)
        if (rates[i] == rate)
            return 1;
    return 0;
}

static ngs2_system* system_layout(arena* a, uint32_t max_grain) {
    ngs2_system* s = (ngs2_system*)arena_take(a, sizeof(ngs2_system));
    float* mix = (float*)arena_take(a, (size_t)NGS2_MAX_OUTPUTS * max_grain * NGS2_MAX_CHANNELS *
                                           sizeof(float));
    if (s)
        s->mix = mix;
    return mix ? s : NULL;
}

size_t ngs2_system_size(uint32_t max_grain_samples) {
    arena a;
    if (!valid_grain(max_grain_samples, MAX_GRAIN))
        return 0;
    memset(&a, 0, sizeof(a));
    system_layout(&a, max_grain_samples);
    return (a.used + 15u) & ~(size_t)15u;
}

ngs2_system* ngs2_system_init(void* mem, size_t size, const char* name, uint32_t max_grain_samples,
                              uint32_t grain_samples, uint32_t sample_rate, const ngs2_host* host) {
    static uint32_t next_uid = 1;
    arena a;
    ngs2_system* s;
    if (!mem || ((uintptr_t)mem & 15u) || !valid_grain(max_grain_samples, MAX_GRAIN) ||
        !valid_grain(grain_samples, max_grain_samples) || !valid_rate(sample_rate))
        return NULL;
    a.base = (uint8_t*)mem;
    a.used = 0;
    a.size = size;
    s = system_layout(&a, max_grain_samples);
    if (!s)
        return NULL;
    s->magic = MAGIC_SYSTEM;
    s->uid = next_uid++;
    if (name)
        strncpy(s->name, name, sizeof(s->name) - 1);
    s->memory = mem;
    s->memory_size = size;
    s->host = *host;
    s->max_grain = max_grain_samples;
    s->grain = grain_samples;
    s->sample_rate = sample_rate;
    s->next_rack_uid = 1;
    return s;
}

ngs2_system* ngs2_system_from(uintptr_t handle) {
    ngs2_system* s = (ngs2_system*)handle;
    return s && !(handle & 15u) && s->magic == MAGIC_SYSTEM ? s : NULL;
}

void ngs2_system_destroy(ngs2_system* system) {
    while (system->rack_count)
        ngs2_rack_destroy(system->racks[system->rack_count - 1]);
    system->magic = 0;
}

void* ngs2_system_memory(const ngs2_system* system, size_t* size) {
    *size = system->memory_size;
    return system->memory;
}

void ngs2_system_set_host_data(ngs2_system* system, void* data) {
    system->host_data = data;
}
void* ngs2_system_host_data(const ngs2_system* system) {
    return system->host_data;
}
void ngs2_system_set_user_data(ngs2_system* system, uintptr_t data) {
    system->user_data = data;
}
uintptr_t ngs2_system_user_data(const ngs2_system* system) {
    return system->user_data;
}
uint32_t ngs2_system_grain_samples(const ngs2_system* system) {
    return system->grain;
}
uint32_t ngs2_system_sample_rate(const ngs2_system* system) {
    return system->sample_rate;
}
uint32_t ngs2_system_rack_count(const ngs2_system* system) {
    return system->rack_count;
}
uint64_t ngs2_system_render_count(const ngs2_system* system) {
    return system->render_count;
}

int ngs2_system_set_grain_samples(ngs2_system* system, uint32_t samples) {
    uint32_t i;
    if (!valid_grain(samples, system->max_grain))
        return 0;
    for (i = 0; i < system->rack_count; i++)
        if (samples > system->racks[i]->desc.max_grain_samples)
            return 0;
    system->grain = samples;
    return 1;
}

int ngs2_system_set_sample_rate(ngs2_system* system, uint32_t rate) {
    if (!valid_rate(rate))
        return 0;
    system->sample_rate = rate;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Racks                                                               */

static uint32_t rack_buffers(const ngs2_rack_desc* d) {
    uint32_t n = d->num_buffers > d->max_inputs ? d->num_buffers : d->max_inputs;
    return n ? n : 1u;
}

static int desc_sampler(const ngs2_rack_desc* d) {
    return d->rack_id == NGS2_RACK_SAMPLER || d->rack_id == NGS2_RACK_CUSTOM_SAMPLER;
}

static int desc_valid(const ngs2_rack_desc* d) {
    uint32_t i;
    switch (d->rack_id) {
    case NGS2_RACK_SAMPLER:
    case NGS2_RACK_SUBMIXER:
    case NGS2_RACK_REVERB:
    case NGS2_RACK_MASTERING:
    case NGS2_RACK_CUSTOM_SAMPLER:
    case NGS2_RACK_CUSTOM_SUBMIXER:
    case NGS2_RACK_CUSTOM_MASTERING:
        break;
    default:
        return 0;
    }
    if (!valid_grain(d->max_grain_samples, MAX_GRAIN) || !d->max_voices || d->max_voices > 0xffffu)
        return 0;
    if (!d->max_channels || d->max_channels > NGS2_MAX_CHANNELS)
        return 0;
    if (d->max_ports > 64 || d->max_matrices > 64 || d->max_filters > 32)
        return 0;
    if (rack_buffers(d) > NGS2_MAX_BUFFERS || d->num_modules > NGS2_MAX_MODULES)
        return 0;
    if (desc_sampler(d) && !d->max_waveform_blocks)
        return 0;
    for (i = 0; i < d->num_modules; i++)
        if (d->modules[i].source_buffer >= rack_buffers(d) ||
            d->modules[i].dest_buffer >= rack_buffers(d))
            return 0;
    for (i = 0; i < NGS2_MAX_CUSTOM_PORTS; i++)
        if (d->port_buffers[i] >= rack_buffers(d))
            return 0;
    return 1;
}

/* Lays a rack out in @a. With @a only counting, returns NULL. */
static ngs2_rack* rack_layout(arena* a, const ngs2_rack_desc* d, uint32_t rate) {
    ngs2_rack* r = (ngs2_rack*)arena_take(a, sizeof(ngs2_rack));
    ngs2_voice* voices = (ngs2_voice*)arena_take(a, (size_t)d->max_voices * sizeof(ngs2_voice));
    uint32_t buffers = rack_buffers(d);
    uint32_t v, b, m;
    size_t grain_floats = (size_t)d->max_channels * d->max_grain_samples * sizeof(float);
    float* out =
        (float*)arena_take(a, (size_t)NGS2_MAX_CHANNELS * d->max_grain_samples * sizeof(float));
    if (r) {
        r->voices = voices;
        r->buffers = buffers;
        r->out = out;
    }
    if (desc_sampler(d)) {
        uint32_t stride = d->max_grain_samples * (MAX_STEP >> FRAC_BITS) + HISTORY + 1u;
        float* scratch = (float*)arena_take(a, (size_t)stride * d->max_channels * sizeof(float));
        uint32_t units = d->max_atrac9_decoders * 2u;
        ngs2_decoder* decoders;
        float* windows;
        uint16_t* owner;
        if (units < NGS2_MAX_CHANNELS)
            units = NGS2_MAX_CHANNELS;
        decoders =
            (ngs2_decoder*)arena_take(a, (size_t)d->max_atrac9_decoders * sizeof(ngs2_decoder));
        windows = (float*)arena_take(a, (size_t)units * AT9_UNIT * sizeof(float));
        owner = (uint16_t*)arena_take(a, (size_t)units * sizeof(uint16_t));
        if (r) {
            r->scratch = scratch;
            r->scratch_stride = stride;
            r->decoders = decoders;
            r->decoder_count = d->max_atrac9_decoders;
            r->windows = windows;
            r->window_owner = owner;
            r->window_units = units;
        }
    }
    for (m = 0; m < d->num_modules; m++) {
        void* common = NULL;
        if (d->modules[m].id == MODULE_USER_FX2 && d->modules[m].common_size)
            common = arena_take(a, d->modules[m].common_size);
        if (r)
            r->fx2_common[m] = common;
    }
    for (v = 0; v < d->max_voices; v++) {
        ngs2_voice* voice = voices ? &voices[v] : NULL;
        ngs2_port* ports = (ngs2_port*)arena_take(a, (size_t)d->max_ports * sizeof(ngs2_port));
        float* levels =
            (float*)arena_take(a, (size_t)d->max_matrices * NGS2_MAX_LEVELS * sizeof(float));
        uint32_t* counts = (uint32_t*)arena_take(a, (size_t)d->max_matrices * sizeof(uint32_t));
        ngs2_biquad* filters =
            (ngs2_biquad*)arena_take(a, (size_t)d->max_filters * sizeof(ngs2_biquad));
        if (voice) {
            voice->ports = ports;
            voice->matrix_levels = levels;
            voice->matrix_counts = counts;
            voice->filters = filters;
        }
        if (desc_sampler(d)) {
            ngs2_block* blocks =
                (ngs2_block*)arena_take(a, (size_t)d->max_waveform_blocks * sizeof(ngs2_block));
            ngs2_block* saved =
                (ngs2_block*)arena_take(a, (size_t)d->max_waveform_blocks * sizeof(ngs2_block));
            if (voice) {
                voice->blocks = blocks;
                voice->saved = saved;
            }
        } else {
            for (b = 0; b < buffers; b++) {
                float* buf = (float*)arena_take(a, grain_floats);
                if (voice)
                    voice->buf[b] = buf;
            }
        }
        if (d->rack_id == NGS2_RACK_REVERB) {
            ngs2_reverb* rv = (ngs2_reverb*)arena_take(a, sizeof(ngs2_reverb));
            reverb_layout(rv, a, rate);
            if (voice)
                voice->reverb = rv;
        }
        if (d->num_modules) {
            ngs2_module_state* modules = (ngs2_module_state*)arena_take(
                a, (size_t)d->num_modules * sizeof(ngs2_module_state));
            if (voice)
                voice->modules = modules;
            for (m = 0; m < d->num_modules; m++) {
                const ngs2_module_desc* md = &d->modules[m];
                if (md->id == MODULE_REVERB) {
                    ngs2_reverb* rv = (ngs2_reverb*)arena_take(a, sizeof(ngs2_reverb));
                    reverb_layout(rv, a, rate);
                    if (modules)
                        modules[m].reverb = rv;
                } else if (md->id == MODULE_USER_FX2) {
                    void* param = md->param_size ? arena_take(a, md->param_size) : NULL;
                    void* work = md->work_size ? arena_take(a, md->work_size) : NULL;
                    void* state = md->state_size ? arena_take(a, md->state_size) : NULL;
                    if (modules) {
                        modules[m].fx2_param = param;
                        modules[m].fx2_work = work;
                        modules[m].fx2_state = state;
                    }
                }
            }
        }
    }
    if (a->size && a->used > a->size)
        return NULL;
    return r;
}

size_t ngs2_rack_size(const ngs2_rack_desc* desc) {
    arena a;
    if (!desc_valid(desc))
        return 0;
    memset(&a, 0, sizeof(a));
    rack_layout(&a, desc, REVERB_LAYOUT_RATE);
    return (a.used + 15u) & ~(size_t)15u;
}

static void voice_reset(ngs2_voice* v);

ngs2_rack* ngs2_rack_init(ngs2_system* system, void* mem, size_t size, const ngs2_rack_desc* desc) {
    arena a;
    ngs2_rack* r;
    uint32_t v, m;
    if (!mem || ((uintptr_t)mem & 15u) || !desc_valid(desc) || system->rack_count == MAX_RACKS ||
        desc->max_grain_samples < system->grain)
        return NULL;
    a.base = (uint8_t*)mem;
    a.used = 0;
    a.size = size;
    r = rack_layout(&a, desc,
                    system->sample_rate < REVERB_LAYOUT_RATE ? system->sample_rate
                                                             : REVERB_LAYOUT_RATE);
    if (!r)
        return NULL;
    r->magic = MAGIC_RACK;
    r->uid = system->next_rack_uid++;
    r->system = system;
    r->memory = mem;
    r->memory_size = size;
    r->desc = *desc;
    for (v = 0; v < desc->max_voices; v++) {
        ngs2_voice* voice = &r->voices[v];
        voice->magic = MAGIC_VOICE;
        voice->index = v;
        voice->rack = r;
        if (voice->reverb)
            reverb_defaults(voice->reverb);
        for (m = 0; m < desc->num_modules; m++) {
            const ngs2_module_desc* md = &desc->modules[m];
            if (voice->modules[m].reverb)
                reverb_defaults(voice->modules[m].reverb);
            if (md->id == MODULE_USER_FX2 && md->setup_handler && system->host.user_fx2_setup) {
                ngs2_user_fx2_setup_context ctx;
                memset(&ctx, 0, sizeof(ctx));
                ctx.common = r->fx2_common[m];
                ctx.param = voice->modules[m].fx2_param;
                ctx.work = voice->modules[m].fx2_work;
                ctx.user_data = md->user_data;
                ctx.max_voices = desc->max_voices;
                ctx.voice_index = v;
                system->host.user_fx2_setup(md->setup_handler, &ctx);
            }
        }
        voice_reset(voice);
    }
    system->racks[system->rack_count++] = r;
    return r;
}

ngs2_rack* ngs2_rack_from(uintptr_t handle) {
    ngs2_rack* r = (ngs2_rack*)handle;
    return r && !(handle & 15u) && r->magic == MAGIC_RACK ? r : NULL;
}

void ngs2_rack_destroy(ngs2_rack* rack) {
    ngs2_system* s = rack->system;
    uint32_t i, v, p, m;
    /* No voice of another rack goes on sending into this one */
    for (i = 0; i < s->rack_count; i++) {
        ngs2_rack* other = s->racks[i];
        if (other == rack)
            continue;
        for (v = 0; v < other->desc.max_voices; v++)
            for (p = 0; p < other->desc.max_ports; p++) {
                ngs2_port* port = &other->voices[v].ports[p];
                if (port->dest && port->dest->rack == rack)
                    port->dest = NULL;
            }
    }
    for (v = 0; v < rack->desc.max_voices; v++) {
        ngs2_voice* voice = &rack->voices[v];
        for (m = 0; m < rack->desc.num_modules; m++) {
            const ngs2_module_desc* md = &rack->desc.modules[m];
            if (md->id == MODULE_USER_FX2 && md->cleanup_handler && s->host.user_fx2_cleanup) {
                ngs2_user_fx2_setup_context ctx;
                memset(&ctx, 0, sizeof(ctx));
                ctx.common = rack->fx2_common[m];
                ctx.param = voice->modules[m].fx2_param;
                ctx.work = voice->modules[m].fx2_work;
                ctx.user_data = md->user_data;
                ctx.max_voices = rack->desc.max_voices;
                ctx.voice_index = v;
                s->host.user_fx2_cleanup(md->cleanup_handler, &ctx);
            }
        }
        voice->magic = 0;
    }
    for (i = 0; i < rack->decoder_count; i++)
        if (rack->decoders[i].at9)
            ratrac9_close(rack->decoders[i].at9);
    /* A block's callback still waiting names a voice that is gone */
    for (i = 0; i < s->callback_count;) {
        ngs2_voice* voice = (ngs2_voice*)s->callbacks[i].info.voice_handle;
        if (voice >= rack->voices && voice < rack->voices + rack->desc.max_voices)
            s->callbacks[i] = s->callbacks[--s->callback_count];
        else
            i++;
    }
    for (i = 0; i < s->rack_count; i++)
        if (s->racks[i] == rack) {
            memmove(&s->racks[i], &s->racks[i + 1], (s->rack_count - i - 1) * sizeof(s->racks[0]));
            s->rack_count--;
            break;
        }
    rack->magic = 0;
}

void* ngs2_rack_memory(const ngs2_rack* rack, size_t* size) {
    *size = rack->memory_size;
    return rack->memory;
}
ngs2_system* ngs2_rack_system(const ngs2_rack* rack) {
    return rack->system;
}
const ngs2_rack_desc* ngs2_rack_description(const ngs2_rack* rack) {
    return &rack->desc;
}
void ngs2_rack_set_user_data(ngs2_rack* rack, uintptr_t data) {
    rack->user_data = data;
}
uintptr_t ngs2_rack_user_data(const ngs2_rack* rack) {
    return rack->user_data;
}
ngs2_voice* ngs2_rack_voice(ngs2_rack* rack, uint32_t index) {
    return index < rack->desc.max_voices ? &rack->voices[index] : NULL;
}
uint32_t ngs2_rack_active_voices(const ngs2_rack* rack) {
    uint32_t v, n = 0;
    for (v = 0; v < rack->desc.max_voices; v++)
        if (rack->voices[v].flags & NGS2_STATE_INUSE)
            n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* Voices: what a game sets                                            */

static void window_release(ngs2_voice* v) {
    ngs2_rack* r = v->rack;
    uint32_t i;
    if (v->window && v->window != v->vag_window)
        for (i = 0; i < r->window_units; i++)
            if (r->window_owner[i] == (uint16_t)(v->index + 1u))
                r->window_owner[i] = 0;
    v->window = NULL;
    v->window_units = 0;
    if (v->decoder) {
        v->decoder->used = 0;
        v->decoder = NULL;
    }
}

/* A run of @units free units for @v, or NULL */
static float* window_claim(ngs2_voice* v, uint32_t units) {
    ngs2_rack* r = v->rack;
    uint32_t i, run = 0;
    for (i = 0; i < r->window_units; i++) {
        run = r->window_owner[i] ? 0 : run + 1u;
        if (run == units) {
            uint32_t first = i + 1u - units, k;
            for (k = first; k <= i; k++)
                r->window_owner[k] = (uint16_t)(v->index + 1u);
            return r->windows + (size_t)first * AT9_UNIT;
        }
    }
    return NULL;
}

static void sampler_restart(ngs2_voice* v) {
    v->phase = 0;
    v->cached = 0;
    memset(v->history, 0, sizeof(v->history));
    memset(v->vag_history, 0, sizeof(v->vag_history));
    v->starved = 0;
    v->block_started = 0;
    v->window_pos = v->window_count = 0;
    v->decoded_samples = 0;
    v->decoded_bytes = 0;
    v->waveform_data = NULL;
    v->waveform_user_data = 0;
    if (v->decoder && v->decoder->at9)
        ratrac9_reset(v->decoder->at9);
}

/* The state a voice is made in, and that a setup returns it to */
static void voice_reset(ngs2_voice* v) {
    ngs2_rack* r = v->rack;
    uint32_t i, c;
    v->play = PLAY_EMPTY;
    v->ramp = RAMP_NONE;
    v->setup = 0;
    v->flags = 0;
    v->channels = v->in_channels = 0;
    for (i = 0; i < r->desc.max_ports; i++) {
        memset(&v->ports[i], 0, sizeof(v->ports[i]));
        v->ports[i].matrix = -1;
        v->ports[i].volume = 1.0f;
    }
    for (i = 0; i < r->desc.max_matrices; i++)
        v->matrix_counts[i] = 0;
    for (i = 0; i < r->desc.max_filters; i++)
        memset(&v->filters[i], 0, sizeof(v->filters[i]));
    for (i = 0; i < r->desc.num_modules; i++) {
        ngs2_module_state* m = &v->modules[i];
        for (c = 0; c < NGS2_MAX_CHANNELS; c++)
            m->gain[c] = m->gain_to[c] = 1.0f;
        memset(&m->filter, 0, sizeof(m->filter));
        m->fx_handler = NULL;
        m->fx_flags = 0;
        m->fx2_active = 0;
        if (m->reverb)
            reverb_clear(m->reverb);
    }
    if (v->reverb)
        reverb_clear(v->reverb);
    v->callback = NULL;
    v->callback_data = 0;
    v->callback_flags = 0;
    v->fx_handler = NULL;
    v->fx_flags = 0;
    v->peak = 0.0f;
    v->output_id = 0;
    v->fbw_level = v->lfe_level = 1.0f;
    if (is_sampler(r)) {
        window_release(v);
        memset(&v->format, 0, sizeof(v->format));
        v->pitch = 1.0f;
        v->block_head = v->block_count = v->saved_count = 0;
        v->accepts_blocks = 1;
        sampler_restart(v);
    }
}

static uint32_t pcm_bytes(uint32_t type) {
    switch (type) {
    case NGS2_WAVE_PCM_U8:
        return 1;
    case NGS2_WAVE_PCM_I16L:
    case NGS2_WAVE_PCM_I16B:
        return 2;
    case NGS2_WAVE_PCM_I24L:
    case NGS2_WAVE_PCM_I24B:
        return 3;
    case NGS2_WAVE_PCM_I32L:
    case NGS2_WAVE_PCM_I32B:
    case NGS2_WAVE_PCM_F32L:
    case NGS2_WAVE_PCM_F32B:
        return 4;
    case NGS2_WAVE_PCM_F64L:
    case NGS2_WAVE_PCM_F64B:
        return 8;
    default:
        return 0;
    }
}

static int sampler_setup(ngs2_voice* v, const ngs2_waveform_format* f) {
    ngs2_rack* r = v->rack;
    if (!f->channels || f->channels > r->desc.max_channels)
        return NGS2_ERROR_INVALID_NUM_CHANNELS;
    if (!f->sample_rate || f->sample_rate > 192000)
        return NGS2_ERROR_INVALID_WAVEFORM_SAMPLE_RATE;
    if (!pcm_bytes(f->type) && f->type != NGS2_WAVE_VAG && f->type != NGS2_WAVE_ATRAC9)
        return NGS2_ERROR_INVALID_WAVEFORM_TYPE;
    voice_reset(v);
    if (f->type == NGS2_WAVE_ATRAC9) {
        uint8_t config[4];
        ngs2_decoder *slot = NULL, *unopened = NULL, *other = NULL;
        uint32_t i, order;
        /* A decoder that had this configuration before is ready as it is */
        for (order = 0; order < 2 && !slot; order++) {
            for (i = 0; i < 4; i++)
                config[i] = (uint8_t)(f->config_data >> (order ? 8u * i : 24u - 8u * i));
            for (i = 0; i < r->decoder_count; i++) {
                ngs2_decoder* d = &r->decoders[i];
                if (d->used)
                    continue;
                if (d->at9 && !memcmp(d->config, config, 4)) {
                    slot = d;
                    break;
                }
                if (!d->at9 && !unopened)
                    unopened = d;
                if (d->at9 && !other)
                    other = d;
            }
            if (!slot && (unopened || other)) {
                ngs2_decoder* d = unopened ? unopened : other;
                ratrac9_t* at9 = ratrac9_open(config);
                /* The configuration word is in the file's byte order or the other */
                if (at9 && (ratrac9_channels(at9) != f->channels ||
                            ratrac9_sample_rate(at9) != f->sample_rate)) {
                    ratrac9_close(at9);
                    at9 = NULL;
                }
                if (at9) {
                    if (d->at9)
                        ratrac9_close(d->at9);
                    d->at9 = at9;
                    memcpy(d->config, config, 4);
                    slot = d;
                }
            }
        }
        if (!slot)
            return unopened || other ? NGS2_ERROR_INVALID_WAVEFORM_CONFIG
                                     : NGS2_ERROR_EMPTY_CODEC_DECODER;
        if (ratrac9_frame_samples(slot->at9) > AT9_UNIT)
            return NGS2_ERROR_INVALID_WAVEFORM_CONFIG;
        v->window = window_claim(v, f->channels);
        if (!v->window)
            return NGS2_ERROR_EMPTY_CODEC_DECODER;
        v->window_units = f->channels;
        slot->used = 1;
        ratrac9_reset(slot->at9);
        v->decoder = slot;
    } else if (f->type == NGS2_WAVE_VAG) {
        v->window = v->vag_window;
    }
    v->format = *f;
    v->channels = v->in_channels = f->channels;
    v->setup = 1;
    v->flags = NGS2_STATE_INUSE;
    return NGS2_OK;
}

static int mixer_setup(ngs2_voice* v, uint32_t in, uint32_t out) {
    if (!in || in > v->rack->desc.max_channels || !out || out > v->rack->desc.max_channels)
        return NGS2_ERROR_INVALID_NUM_CHANNELS;
    voice_reset(v);
    v->in_channels = in;
    v->channels = out;
    v->setup = 1;
    v->flags = NGS2_STATE_INUSE;
    return NGS2_OK;
}

static void voice_event(ngs2_voice* v, uint32_t event) {
    uint32_t i;
    switch (event) {
    case NGS2_EVENT_PLAY:
        if (!v->setup)
            break;
        if (v->play == PLAY_EMPTY || v->play == PLAY_STOPPED) {
            if (is_sampler(v->rack)) {
                /* The blocks it was given, from the start */
                for (i = 0; i < v->saved_count; i++) {
                    v->blocks[i] = v->saved[i];
                    v->blocks[i].info.num_repeats = v->blocks[i].repeats;
                }
                if (v->saved_count) {
                    v->block_head = 0;
                    v->block_count = v->saved_count;
                }
                sampler_restart(v);
            }
            for (i = 0; i < v->rack->desc.max_filters; i++)
                memset(v->filters[i].z, 0, sizeof(v->filters[i].z));
            for (i = 0; i < v->rack->desc.max_ports; i++)
                v->ports[i].mixed = 0;
            for (i = 0; i < v->rack->desc.num_modules; i++) {
                v->modules[i].fx_flags |= 3u;
                memset(v->modules[i].filter.z, 0, sizeof(v->modules[i].filter.z));
            }
            v->fx_flags |= 3u;
            v->play = PLAY_PLAYING;
            v->ramp = RAMP_NONE;
            v->flags |= NGS2_STATE_INUSE;
        }
        break;
    case NGS2_EVENT_STOP:
        if (v->play == PLAY_PLAYING || v->play == PLAY_PAUSED) {
            /* A paused voice gave its last grain when it paused */
            v->ramp = v->play == PLAY_PLAYING ? RAMP_OUT : RAMP_NONE;
            v->play = PLAY_STOPPED;
        }
        break;
    case NGS2_EVENT_STOP_IMM:
    case NGS2_EVENT_KILL:
        v->play = PLAY_EMPTY;
        v->ramp = RAMP_NONE;
        v->flags = event == NGS2_EVENT_KILL || !v->setup ? 0 : NGS2_STATE_INUSE;
        if (v->reverb)
            reverb_clear(v->reverb);
        for (i = 0; i < v->rack->desc.num_modules; i++)
            if (v->modules[i].reverb)
                reverb_clear(v->modules[i].reverb);
        if (event == NGS2_EVENT_KILL) {
            if (is_sampler(v->rack))
                window_release(v);
            v->setup = 0;
        }
        break;
    case NGS2_EVENT_PAUSE:
        if (v->play == PLAY_PLAYING) {
            v->play = PLAY_PAUSED;
            v->ramp = RAMP_OUT;
        }
        break;
    case NGS2_EVENT_RESUME:
        if (v->play == PLAY_PAUSED) {
            /* Paused and resumed with no grain between: it never went quiet */
            v->ramp = v->ramp == RAMP_OUT ? RAMP_NONE : RAMP_IN;
            v->play = PLAY_PLAYING;
        }
        break;
    default:
        break;
    }
}

static int add_blocks(ngs2_voice* v, const void* data, uint32_t flags, uint32_t count,
                      const ngs2_waveform_block* blocks) {
    ngs2_rack* r = v->rack;
    uint32_t cap = r->desc.max_waveform_blocks, i;
    if (!v->setup)
        return NGS2_ERROR_UNINIT_VOICE;
    if (count && !blocks)
        return NGS2_ERROR_INVALID_WAVEFORM_BLOCK_ADDRESS;
    if (flags & NGS2_BLOCKS_RESET) {
        v->block_head = v->block_count = 0;
        v->saved_count = 0;
        v->block_started = 0;
        v->window_pos = v->window_count = 0;
        memset(v->vag_history, 0, sizeof(v->vag_history));
        if (v->decoder && v->decoder->at9)
            ratrac9_reset(v->decoder->at9);
    }
    /* Blocks given while it plays what it had follow those; a new set replaces the
     * set a play starts from */
    if (!(flags & NGS2_BLOCKS_APPEND) && !v->block_count)
        v->saved_count = 0;
    for (i = 0; i < count; i++) {
        ngs2_block b;
        if (!blocks[i].num_samples && !blocks[i].data_size)
            continue;
        if (v->block_count == cap)
            return NGS2_ERROR_INVALID_NUM_WAVEFORM_BLOCKS;
        b.info = blocks[i];
        b.repeats = blocks[i].num_repeats;
        b.data = data && blocks[i].data_size ? (const uint8_t*)data + blocks[i].data_offset : NULL;
        v->blocks[(v->block_head + v->block_count) % cap] = b;
        v->block_count++;
        if (v->saved_count < cap)
            v->saved[v->saved_count++] = b;
    }
    v->accepts_blocks = (flags & NGS2_BLOCKS_CONTINUE) != 0;
    v->starved = 0;
    return NGS2_OK;
}

static int set_filter(ngs2_voice* v, ngs2_biquad* f, uint32_t location, uint32_t type,
                      uint32_t channel_mask, const float* values) {
    f->location = location;
    f->channel_mask = channel_mask;
    biquad_set(f, type, values[0], values[1], values[2], values,
               (float)v->rack->system->sample_rate);
    return NGS2_OK;
}

static uint32_t rd32(const ngs2_param_header* p, size_t offset) {
    uint32_t value;
    memcpy(&value, (const uint8_t*)p + offset, sizeof(value));
    return value;
}

static float rdf(const ngs2_param_header* p, size_t offset) {
    float value;
    memcpy(&value, (const uint8_t*)p + offset, sizeof(value));
    return value;
}

static uintptr_t rdp(const ngs2_param_header* p, size_t offset) {
    uintptr_t value;
    memcpy(&value, (const uint8_t*)p + offset, sizeof(value));
    return value;
}

static int common_param(ngs2_voice* v, const ngs2_param_header* p) {
    ngs2_rack* r = v->rack;
    switch (p->id) {
    case PARAM_MATRIX_LEVELS: {
        uint32_t matrix, count, i;
        const float* levels;
        if (p->size < 24)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        matrix = rd32(p, 8);
        count = rd32(p, 12);
        levels = (const float*)rdp(p, 16);
        if (matrix >= r->desc.max_matrices)
            return NGS2_ERROR_INVALID_MATRIX_ID;
        if (count > NGS2_MAX_LEVELS)
            return NGS2_ERROR_INVALID_NUM_MATRIX_LEVELS;
        if (count && !levels)
            return NGS2_ERROR_INVALID_MATRIX_LEVEL_ADDRESS;
        for (i = 0; i < count; i++)
            memcpy(&v->matrix_levels[(size_t)matrix * NGS2_MAX_LEVELS + i], &levels[i],
                   sizeof(float));
        v->matrix_counts[matrix] = count;
        return NGS2_OK;
    }
    case PARAM_PORT_VOLUME: {
        uint32_t port;
        float level;
        if (p->size < 16)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        port = rd32(p, 8);
        level = rdf(p, 12);
        if (port >= r->desc.max_ports)
            return NGS2_ERROR_INVALID_PORT_INDEX;
        if (!(level >= 0.0f))
            level = 0.0f;
        v->ports[port].volume = level;
        return NGS2_OK;
    }
    case PARAM_PORT_MATRIX: {
        uint32_t port;
        int32_t matrix;
        if (p->size < 16)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        port = rd32(p, 8);
        matrix = (int32_t)rd32(p, 12);
        if (port >= r->desc.max_ports)
            return NGS2_ERROR_INVALID_PORT_INDEX;
        if (matrix >= (int32_t)r->desc.max_matrices)
            return NGS2_ERROR_INVALID_MATRIX_ID;
        v->ports[port].matrix = matrix < 0 ? -1 : matrix;
        return NGS2_OK;
    }
    case PARAM_PORT_DELAY:
        return NGS2_OK;
    case PARAM_PATCH: {
        uint32_t port, input;
        uintptr_t handle;
        ngs2_voice* dest;
        if (p->size < 24)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        port = rd32(p, 8);
        input = rd32(p, 12);
        handle = rdp(p, 16);
        if (port >= r->desc.max_ports)
            return NGS2_ERROR_INVALID_PORT_INDEX;
        dest = handle ? ngs2_voice_from(handle) : NULL;
        if (handle && (!dest || dest == v || dest->rack->system != r->system ||
                       is_sampler(dest->rack) || input >= dest->rack->buffers))
            return NGS2_ERROR_INVALID_PATCH;
        v->ports[port].dest = dest;
        v->ports[port].dest_input = input;
        v->ports[port].mixed = 0;
        return NGS2_OK;
    }
    case PARAM_EVENT: {
        uint32_t event;
        if (p->size < 12)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        event = rd32(p, 8);
        if (event != NGS2_EVENT_PLAY && event != NGS2_EVENT_STOP && event != NGS2_EVENT_STOP_IMM &&
            event != NGS2_EVENT_KILL && event != NGS2_EVENT_PAUSE && event != NGS2_EVENT_RESUME)
            return NGS2_ERROR_INVALID_EVENT_TYPE;
        voice_event(v, event);
        return NGS2_OK;
    }
    case PARAM_CALLBACK:
        if (p->size < 32)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        v->callback = (void*)rdp(p, 8);
        v->callback_data = rdp(p, 16);
        v->callback_flags = rd32(p, 24);
        return NGS2_OK;
    default:
        return NGS2_ERROR_INVALID_VOICE_CONTROL_ID;
    }
}

static int custom_param(ngs2_voice* v, const ngs2_param_header* p) {
    ngs2_rack* r = v->rack;
    uint32_t module = (p->id >> 8) & 0xffu;
    uint32_t index = p->id & 0xffu;
    ngs2_module_state* m;
    uint32_t c;
    /* A custom sampler's modules are numbered after the sampler itself */
    if (r->desc.rack_id == NGS2_RACK_CUSTOM_SAMPLER && index)
        index--;
    /* A parameter for a module the rack has not, or has another kind of there, is
     * left unused: the sound goes on without the effect */
    if (index >= r->desc.num_modules || r->desc.modules[index].id != module)
        return NGS2_OK;
    m = &v->modules[index];
    switch (module) {
    case MODULE_GAIN:
        for (c = 0; c < NGS2_MAX_CHANNELS && 8u + 4u * c + 4u <= p->size; c++)
            m->gain_to[c] = rdf(p, 8u + 4u * c);
        /* Not playing: there is nothing to slide from */
        if (v->play != PLAY_PLAYING)
            memcpy(m->gain, m->gain_to, sizeof(m->gain));
        return NGS2_OK;
    case MODULE_FILTER: {
        float values[5];
        if (p->size < 36)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        for (c = 0; c < 5; c++)
            values[c] = rdf(p, 16u + 4u * c);
        return set_filter(v, &m->filter, 0, rd32(p, 8), rd32(p, 12), values);
    }
    case MODULE_REVERB:
        if (p->size < 8 + sizeof(ngs2_i3dl2))
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        if (m->reverb) {
            ngs2_i3dl2 param;
            memcpy(&param, (const uint8_t*)p + 8, sizeof(param));
            reverb_set(m->reverb, &param);
        }
        return NGS2_OK;
    case MODULE_USER_FX:
        if (p->size < 40)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        m->fx_handler = (void*)rdp(p, 8);
        m->fx_data[0] = rdp(p, 16);
        m->fx_data[1] = rdp(p, 24);
        m->fx_data[2] = rdp(p, 32);
        m->fx_flags |= 2u;
        return NGS2_OK;
    case MODULE_USER_FX2: {
        const ngs2_module_desc* md = &r->desc.modules[index];
        const void* data;
        size_t size;
        if (p->size < 24)
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        data = (const void*)rdp(p, 8);
        size = (size_t)rdp(p, 16);
        if (md->control_handler && r->system->host.user_fx2_control) {
            ngs2_user_fx2_control_context ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.data = data;
            ctx.data_size = size;
            ctx.common = r->fx2_common[index];
            ctx.param = m->fx2_param;
            ctx.user_data = md->user_data;
            r->system->host.user_fx2_control(md->control_handler, &ctx);
        } else if (data && size && size <= md->param_size) {
            memcpy(m->fx2_param, data, size);
        }
        m->fx2_active = 1;
        m->fx_flags |= 2u;
        return NGS2_OK;
    }
    default:
        return NGS2_OK;
    }
}

static int rack_param(ngs2_voice* v, const ngs2_param_header* p) {
    ngs2_rack* r = v->rack;
    uint32_t id = p->id & 0xffffu;
    float values[5];
    uint32_t c;
    switch (r->desc.rack_id) {
    case NGS2_RACK_SAMPLER:
    case NGS2_RACK_CUSTOM_SAMPLER:
        switch (id) {
        case 0: {
            ngs2_waveform_format format;
            if (p->size < 8 + sizeof(format))
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            memcpy(&format, (const uint8_t*)p + 8, sizeof(format));
            return sampler_setup(v, &format);
        }
        case 1:
            if (p->size < 32)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            return add_blocks(v, (const void*)rdp(p, 8), rd32(p, 16), rd32(p, 20),
                              (const ngs2_waveform_block*)rdp(p, 24));
        case 2: { /* the waveform was moved */
            const uint8_t *from, *to;
            if (p->size < 24)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            from = (const uint8_t*)rdp(p, 8);
            to = (const uint8_t*)rdp(p, 16);
            for (c = 0; c < r->desc.max_waveform_blocks; c++) {
                if (v->blocks[c].data)
                    v->blocks[c].data = to + (v->blocks[c].data - from);
                if (v->saved[c].data)
                    v->saved[c].data = to + (v->saved[c].data - from);
            }
            return NGS2_OK;
        }
        case 4: /* leave the loop: the block it is in ends where it ends */
            for (c = 0; c < v->block_count; c++) {
                ngs2_block* b = &v->blocks[(v->block_head + c) % r->desc.max_waveform_blocks];
                if (b->info.num_repeats) {
                    b->info.num_repeats = 0;
                    break;
                }
            }
            return NGS2_OK;
        case 5: {
            float ratio;
            if (p->size < 12)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            ratio = rdf(p, 8);
            if (!(ratio >= 0.0f))
                ratio = 0.0f;
            v->pitch = ratio > 4.0f ? 4.0f : ratio;
            return NGS2_OK;
        }
        case 8:
            if (p->size < 40)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            v->fx_handler = (void*)rdp(p, 8);
            v->fx_data[0] = rdp(p, 16);
            v->fx_data[1] = rdp(p, 24);
            v->fx_data[2] = rdp(p, 32);
            v->fx_flags |= 2u;
            return NGS2_OK;
        case 0xa: {
            uint32_t index;
            if (p->size < 44)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            index = rd32(p, 8);
            if (index >= r->desc.max_filters)
                return NGS2_OK;
            for (c = 0; c < 5; c++)
                values[c] = rdf(p, 24u + 4u * c);
            return set_filter(v, &v->filters[index], rd32(p, 12), rd32(p, 16), rd32(p, 20), values);
        }
        default: /* frame offset, envelope, distortion, peak meter */
            return NGS2_OK;
        }
    case NGS2_RACK_SUBMIXER:
        switch (id) {
        case 0:
            if (p->size < 12)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            return mixer_setup(v, rd32(p, 8), rd32(p, 8));
        case 4:
            if (p->size < 40)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            v->fx_handler = (void*)rdp(p, 8);
            v->fx_data[0] = rdp(p, 16);
            v->fx_data[1] = rdp(p, 24);
            v->fx_data[2] = rdp(p, 32);
            v->fx_flags |= 2u;
            return NGS2_OK;
        case 6: {
            uint32_t index;
            if (p->size < 44)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            index = rd32(p, 8);
            if (index >= r->desc.max_filters)
                return NGS2_OK;
            for (c = 0; c < 5; c++)
                values[c] = rdf(p, 24u + 4u * c);
            return set_filter(v, &v->filters[index], rd32(p, 12), rd32(p, 16), rd32(p, 20), values);
        }
        default: /* envelope, compressor, distortion, peak meter */
            return NGS2_OK;
        }
    case NGS2_RACK_REVERB:
        switch (id) {
        case 0:
            if (p->size < 16)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            return mixer_setup(v, rd32(p, 8), rd32(p, 12));
        case 1: {
            ngs2_i3dl2 param;
            if (p->size < 8 + sizeof(param))
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            memcpy(&param, (const uint8_t*)p + 8, sizeof(param));
            reverb_set(v->reverb, &param);
            return NGS2_OK;
        }
        default:
            return NGS2_OK;
        }
    case NGS2_RACK_MASTERING:
    case NGS2_RACK_CUSTOM_MASTERING:
        switch (id) {
        case 0:
            if (p->size < 12)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            return mixer_setup(v, rd32(p, 8), rd32(p, 8));
        case 4:
            if (p->size < 16)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            v->fbw_level = rdf(p, 8);
            v->lfe_level = rdf(p, 12);
            return NGS2_OK;
        case 5:
            if (p->size < 12)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            v->output_id = rd32(p, 8);
            return NGS2_OK;
        default: /* matrix, lfe, limiter, peak meter */
            return NGS2_OK;
        }
    case NGS2_RACK_CUSTOM_SUBMIXER:
        if (id == 0) {
            if (p->size < 16)
                return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
            return mixer_setup(v, rd32(p, 8), rd32(p, 12));
        }
        return NGS2_OK;
    default:
        return NGS2_ERROR_INVALID_VOICE_CONTROL_ID;
    }
}

ngs2_voice* ngs2_voice_from(uintptr_t handle) {
    ngs2_voice* v = (ngs2_voice*)handle;
    return v && !(handle & 7u) && v->magic == MAGIC_VOICE ? v : NULL;
}

ngs2_rack* ngs2_voice_rack(const ngs2_voice* voice) {
    return voice->rack;
}

uint32_t ngs2_voice_index(const ngs2_voice* voice) {
    return voice->index;
}

int ngs2_voice_control(ngs2_voice* voice, const ngs2_param_header* list) {
    const ngs2_param_header* p = list;
    uint32_t guard = 0;
    if (!p)
        return NGS2_ERROR_INVALID_VOICE_CONTROL_ADDRESS;
    for (;;) {
        ngs2_param_header h;
        uint32_t group;
        int result;
        memcpy(&h, p, sizeof(h));
        if (h.size < sizeof(h))
            return NGS2_ERROR_INVALID_VOICE_CONTROL_SIZE;
        group = h.id >> 16;
        if (group == 0)
            result = common_param(voice, p);
        else if (group == 0x4000u)
            result = is_custom(voice->rack) ? custom_param(voice, p)
                                            : NGS2_ERROR_INVALID_VOICE_CONTROL_ID;
        else if (group == voice->rack->desc.rack_id)
            result = rack_param(voice, p);
        else
            result = NGS2_ERROR_INVALID_VOICE_CONTROL_ID;
        if (result != NGS2_OK)
            return result;
        if (!h.next)
            return NGS2_OK;
        if (++guard > 4096u)
            return NGS2_ERROR_DETECTED_CIRCULAR_VOICE_CONTROL;
        p = (const ngs2_param_header*)((const uint8_t*)p + h.next);
    }
}

uint32_t ngs2_voice_state_flags(const ngs2_voice* voice) {
    return voice->flags;
}

void ngs2_voice_sampler_state(const ngs2_voice* voice, ngs2_sampler_state* state) {
    memset(state, 0, sizeof(*state));
    state->state_flags = voice->flags;
    if (voice->play == PLAY_EMPTY)
        return;
    state->envelope_height = 1.0f;
    state->peak_height = voice->peak;
    state->decoded_samples = voice->decoded_samples;
    state->decoded_bytes = voice->decoded_bytes;
    state->user_data = voice->waveform_user_data;
    state->waveform_data = voice->waveform_data;
}

float ngs2_voice_peak(const ngs2_voice* voice) {
    return voice->peak;
}

int ngs2_voice_port_info(const ngs2_voice* voice, uint32_t port, ngs2_port_info* info) {
    if (port >= voice->rack->desc.max_ports)
        return NGS2_ERROR_INVALID_PORT_INDEX;
    memset(info, 0, sizeof(*info));
    info->matrix_id = voice->ports[port].matrix;
    info->volume = voice->ports[port].volume;
    info->dest_input_id = voice->ports[port].dest_input;
    info->dest_handle = (uintptr_t)voice->ports[port].dest;
    return NGS2_OK;
}

int ngs2_voice_matrix_info(const ngs2_voice* voice, uint32_t matrix, uint32_t* num_levels,
                           float* levels) {
    if (matrix >= voice->rack->desc.max_matrices)
        return NGS2_ERROR_INVALID_MATRIX_ID;
    *num_levels = voice->matrix_counts[matrix];
    memcpy(levels, &voice->matrix_levels[(size_t)matrix * NGS2_MAX_LEVELS],
           voice->matrix_counts[matrix] * sizeof(float));
    return NGS2_OK;
}

/* ------------------------------------------------------------------ */
/* A sampler's waveform                                                */

static float pcm_sample(uint32_t type, const uint8_t* s) {
    switch (type) {
    case NGS2_WAVE_PCM_U8:
        return ((int)s[0] - 128) / 128.0f;
    case NGS2_WAVE_PCM_I16L:
        return (int16_t)(s[0] | (s[1] << 8)) / 32768.0f;
    case NGS2_WAVE_PCM_I16B:
        return (int16_t)(s[1] | (s[0] << 8)) / 32768.0f;
    case NGS2_WAVE_PCM_I24L:
        return (int32_t)(((uint32_t)s[0] << 8) | ((uint32_t)s[1] << 16) | ((uint32_t)s[2] << 24)) /
               2147483648.0f;
    case NGS2_WAVE_PCM_I24B:
        return (int32_t)(((uint32_t)s[2] << 8) | ((uint32_t)s[1] << 16) | ((uint32_t)s[0] << 24)) /
               2147483648.0f;
    case NGS2_WAVE_PCM_I32L:
        return (int32_t)((uint32_t)s[0] | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16) |
                         ((uint32_t)s[3] << 24)) /
               2147483648.0f;
    case NGS2_WAVE_PCM_I32B:
        return (int32_t)((uint32_t)s[3] | ((uint32_t)s[2] << 8) | ((uint32_t)s[1] << 16) |
                         ((uint32_t)s[0] << 24)) /
               2147483648.0f;
    case NGS2_WAVE_PCM_F32L:
    case NGS2_WAVE_PCM_F32B: {
        uint8_t b[4];
        float f;
        int big = type == NGS2_WAVE_PCM_F32B;
        b[0] = s[big ? 3 : 0];
        b[1] = s[big ? 2 : 1];
        b[2] = s[big ? 1 : 2];
        b[3] = s[big ? 0 : 3];
        memcpy(&f, b, 4);
        return f == f ? f : 0.0f;
    }
    default: {
        uint8_t b[8];
        double d;
        int big = type == NGS2_WAVE_PCM_F64B, i;
        for (i = 0; i < 8; i++)
            b[i] = s[big ? 7 - i : i];
        memcpy(&d, b, 8);
        return d == d ? (float)d : 0.0f;
    }
    }
}

static ngs2_block* block_front(ngs2_voice* v) {
    return v->block_count ? &v->blocks[v->block_head] : NULL;
}

static void block_begin(ngs2_voice* v, ngs2_block* b) {
    uint32_t frames;
    v->block_started = 1;
    v->block_pos = 0;
    v->block_skip = b->info.num_skip_samples;
    v->window_pos = v->window_count = 0;
    v->waveform_data = b->data;
    v->waveform_user_data = b->info.user_data;
    if (pcm_bytes(v->format.type)) {
        frames = b->info.data_size / (pcm_bytes(v->format.type) * v->format.channels);
        if (!b->data)
            frames = b->info.num_skip_samples + b->info.num_samples;
        frames = frames > v->block_skip ? frames - v->block_skip : 0;
        v->block_left =
            b->info.num_samples && b->info.num_samples < frames ? b->info.num_samples : frames;
        v->block_pos = b->data ? v->block_skip * pcm_bytes(v->format.type) * v->format.channels : 0;
        v->block_skip = 0;
    } else {
        /* As many as it says, or all its data holds */
        v->block_left = b->info.num_samples ? b->info.num_samples : 0xffffffffu;
        if (!b->data && !b->info.num_samples)
            v->block_left = 0;
        if (v->format.type == NGS2_WAVE_ATRAC9 && v->decoder)
            ratrac9_reset(v->decoder->at9);
        if (v->format.type == NGS2_WAVE_VAG)
            memset(v->vag_history, 0, sizeof(v->vag_history));
    }
}

/* The block in front has given all it has: it goes round again or gives way to
 * the next, and the game is told. */
static void block_end(ngs2_voice* v) {
    ngs2_system* s = v->rack->system;
    ngs2_block* b = block_front(v);
    int repeat = b->info.num_repeats > 1u;
    uint32_t flag = repeat ? NGS2_CALLBACK_BLOCK_REPEAT : NGS2_CALLBACK_BLOCK_END;
    if (b->data && b->info.data_size > v->block_pos)
        v->decoded_bytes += b->info.data_size - v->block_pos;
    if (v->callback && (v->callback_flags & flag) && s->callback_count < MAX_CALLBACKS) {
        ngs2_pending_callback* cb = &s->callbacks[s->callback_count++];
        memset(cb, 0, sizeof(*cb));
        cb->handler = v->callback;
        cb->info.callback_data = v->callback_data;
        cb->info.voice_handle = (uintptr_t)v;
        cb->info.flag = flag;
        cb->info.user_data = b->info.user_data;
        cb->info.data = b->data;
        cb->info.data_size = b->info.data_size;
    }
    if (repeat) {
        if (b->info.num_repeats != NGS2_REPEAT_ENDLESS)
            b->info.num_repeats--;
    } else {
        v->block_head = (v->block_head + 1u) % v->rack->desc.max_waveform_blocks;
        v->block_count--;
    }
    v->block_started = 0;
}

static void vag_frame(ngs2_voice* v, const uint8_t* frame, uint32_t channel) {
    static const int coef[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    int predict = frame[0] >> 4, shift = frame[0] & 15, i;
    int32_t h0 = v->vag_history[channel][0], h1 = v->vag_history[channel][1];
    float* out = v->window + (size_t)channel * VAG_SAMPLES;
    if (predict > 4)
        predict = 0;
    if (shift > 12)
        shift = 9;
    for (i = 0; i < VAG_SAMPLES; i++) {
        int nibble = (frame[2 + i / 2] >> ((i & 1) * 4)) & 15;
        int32_t s = (int32_t)((int16_t)(nibble << 12)) >> shift;
        s += (h0 * coef[predict][0] + h1 * coef[predict][1] + 32) >> 6;
        if (s > 32767)
            s = 32767;
        if (s < -32768)
            s = -32768;
        h1 = h0;
        h0 = s;
        out[i] = (float)s / 32768.0f;
    }
    v->vag_history[channel][0] = h0;
    v->vag_history[channel][1] = h1;
}

/* Fills the window with the block's next frame. 0 when its data is at an end. */
static int window_fill(ngs2_voice* v, ngs2_block* b) {
    uint32_t channels = v->format.channels, c;
    v->window_pos = 0;
    if (v->format.type == NGS2_WAVE_VAG) {
        if (!b->data || v->block_pos + VAG_BYTES * channels > b->info.data_size)
            return 0;
        for (c = 0; c < channels; c++)
            vag_frame(v, b->data + v->block_pos + (size_t)c * VAG_BYTES, c);
        v->block_pos += VAG_BYTES * channels;
        v->window_count = VAG_SAMPLES;
        return 1;
    } else {
        ratrac9_t* at9 = v->decoder->at9;
        uint32_t frame = ratrac9_frame_bytes(at9), samples = ratrac9_frame_samples(at9);
        size_t used = 0;
        if (!b->data || v->block_pos + frame > b->info.data_size)
            return 0;
        /* Channel after channel, each of the frame's samples, as the window is read */
        if (ratrac9_decode_f32(at9, b->data + v->block_pos, frame, v->window, true, &used) < 0)
            memset(v->window, 0, sizeof(float) * samples * channels);
        v->block_pos += frame;
        v->window_count = samples;
        return 1;
    }
}

/* Up to @count frames of the voice's waveform into @dst, a channel every
 * @stride. Returns how many there were. */
static uint32_t sampler_read(ngs2_voice* v, float* dst, uint32_t stride, uint32_t count) {
    uint32_t channels = v->format.channels, done = 0, c, i;
    uint32_t type = v->format.type, bytes = pcm_bytes(type);
    uint32_t unit = type == NGS2_WAVE_VAG ? VAG_SAMPLES
                    : v->decoder          ? ratrac9_frame_samples(v->decoder->at9)
                                          : AT9_UNIT;
    /* Blocks that hold nothing follow one another without end only if every one
     * of them is such a block */
    uint32_t spins = 0;
    while (done < count && spins < 2u * v->rack->desc.max_waveform_blocks + 2u) {
        ngs2_block* b = block_front(v);
        uint32_t n;
        if (!b)
            break;
        if (!v->block_started)
            block_begin(v, b);
        if (bytes) {
            n = count - done < v->block_left ? count - done : v->block_left;
            if (b->data) {
                const uint8_t* src = b->data + v->block_pos;
                for (c = 0; c < channels; c++) {
                    float* out = dst + (size_t)c * stride + done;
                    const uint8_t* s = src + (size_t)c * bytes;
                    size_t step = (size_t)bytes * channels;
                    if (type == NGS2_WAVE_PCM_I16L)
                        for (i = 0; i < n; i++, s += step)
                            out[i] = (int16_t)(s[0] | (s[1] << 8)) * (1.0f / 32768.0f);
                    else
                        for (i = 0; i < n; i++, s += step)
                            out[i] = pcm_sample(type, s);
                }
                v->block_pos += n * bytes * channels;
                v->decoded_bytes += (uint64_t)n * bytes * channels;
            } else {
                for (c = 0; c < channels; c++)
                    memset(dst + (size_t)c * stride + done, 0, n * sizeof(float));
            }
        } else {
            if (v->window_pos == v->window_count && v->block_left) {
                uint32_t before = v->block_pos;
                if (!b->data) {
                    /* A block of silence */
                    v->window_pos = 0;
                    v->window_count = unit;
                    for (c = 0; c < channels; c++)
                        memset(v->window + (size_t)c * unit, 0, unit * sizeof(float));
                } else if (!window_fill(v, b)) {
                    v->block_left = 0;
                }
                v->decoded_bytes += v->block_pos - before;
            }
            n = v->window_count - v->window_pos;
            if (v->block_skip && n) {
                uint32_t drop = n < v->block_skip ? n : v->block_skip;
                v->window_pos += drop;
                v->block_skip -= drop;
                continue;
            }
            if (n > count - done)
                n = count - done;
            if (n > v->block_left)
                n = v->block_left;
            for (c = 0; c < channels; c++)
                memcpy(dst + (size_t)c * stride + done,
                       v->window + (size_t)c * unit + v->window_pos, n * sizeof(float));
            v->window_pos += n;
        }
        v->block_left -= n;
        done += n;
        v->decoded_samples += n;
        spins = n ? 0 : spins + 1u;
        if (!v->block_left)
            block_end(v);
    }
    return done;
}

/* One grain of the voice at the system's rate into @out, a channel every
 * @stride. Returns whether there was any waveform in it. */
static int sampler_grain(ngs2_voice* v, float* out, uint32_t stride, uint32_t grain) {
    ngs2_rack* r = v->rack;
    uint32_t channels = v->format.channels, c, i;
    uint32_t ss = r->scratch_stride;
    double ratio =
        (double)v->format.sample_rate / (double)r->system->sample_rate * (double)v->pitch;
    uint32_t step = ratio * FRAC_ONE >= (double)MAX_STEP ? MAX_STEP : (uint32_t)(ratio * FRAC_ONE);
    uint64_t end = (uint64_t)v->phase + (uint64_t)step * grain;
    uint32_t consumed = (uint32_t)(end >> FRAC_BITS);
    uint32_t got;
    for (c = 0; c < channels; c++) {
        float* s = r->scratch + (size_t)c * ss;
        memcpy(s, v->history[c], sizeof(v->history[c]));
        memset(s + HISTORY, 0, (consumed + 1u) * sizeof(float));
    }
    got = consumed ? sampler_read(v, r->scratch + HISTORY, ss, consumed) : 0;
    for (c = 0; c < channels; c++) {
        const float* s = r->scratch + (size_t)c * ss;
        float* o = out + (size_t)c * stride;
        uint32_t phase = v->phase;
        if (step == FRAC_ONE && phase == 0) {
            memcpy(o, s + 1, grain * sizeof(float));
        } else {
            for (i = 0; i < grain; i++) {
                /* Between the second and third of four frames, a cubic through them */
                const float* p = s + (phase >> FRAC_BITS);
                float t = (float)(phase & (FRAC_ONE - 1u)) * (1.0f / FRAC_ONE);
                float a = p[1];
                float b = 0.5f * (p[2] - p[0]);
                float d = p[0] - 2.5f * p[1] + 2.0f * p[2] - 0.5f * p[3];
                float e = 1.5f * (p[1] - p[2]) + 0.5f * (p[3] - p[0]);
                o[i] = a + t * (b + t * (d + t * e));
                phase += step;
            }
        }
        memcpy(v->history[c], s + consumed, sizeof(v->history[c]));
    }
    v->phase = (uint32_t)(end & (FRAC_ONE - 1u));
    v->starved = got < consumed;
    /* What it had is in the history a little longer than in the waveform */
    if (got)
        v->cached = HISTORY;
    else if (v->cached)
        v->cached = v->cached > consumed ? v->cached - consumed : 0;
    return got || v->cached;
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */

static float* mixer_input(ngs2_voice* v, uint32_t buffer) {
    ngs2_system* s = v->rack->system;
    uint32_t c;
    if (v->buf_stamp[buffer] != s->render_count + 1u) {
        for (c = 0; c < v->rack->desc.max_channels; c++)
            memset(v->buf[buffer] + (size_t)c * v->rack->desc.max_grain_samples, 0,
                   s->grain * sizeof(float));
        v->buf_stamp[buffer] = s->render_count + 1u;
    }
    return v->buf[buffer];
}

static void apply_ramp(float* samples, uint32_t channels, uint32_t stride, uint32_t grain,
                       int ramp) {
    uint32_t c, i;
    float step = 1.0f / (float)grain;
    for (c = 0; c < channels; c++) {
        float* x = samples + (size_t)c * stride;
        if (ramp == RAMP_IN)
            for (i = 0; i < grain; i++)
                x[i] *= (float)i * step;
        else
            for (i = 0; i < grain; i++)
                x[i] *= (float)(grain - i) * step;
    }
}

static int renders(const ngs2_voice* v) {
    return v->play == PLAY_PLAYING || v->ramp == RAMP_OUT;
}

/* Sends what @v made this grain where its ports lead. @src: for each buffer
 * a port may read, @v's channels, one every @stride; NULL where that buffer
 * holds silence. */
static void send(ngs2_voice* v, float* const* src, uint32_t stride, uint32_t grain) {
    ngs2_rack* r = v->rack;
    uint32_t p, d, c, i;
    for (p = 0; p < r->desc.max_ports; p++) {
        ngs2_port* port = &v->ports[p];
        ngs2_voice* dest = port->dest;
        const float* from;
        float* to;
        uint32_t dest_ch, dstride;
        const float* matrix = NULL;
        uint32_t matrix_dests = 0;
        if (!dest)
            continue;
        if (dest->waiting)
            dest->waiting--;
        from = src[is_custom(r) && p < NGS2_MAX_CUSTOM_PORTS ? r->desc.port_buffers[p] : 0];
        if (!from || !renders(dest) || dest->rendered) {
            port->mixed = 0;
            continue;
        }
        dest_ch = dest->in_channels;
        dstride = dest->rack->desc.max_grain_samples;
        if (port->matrix >= 0 && v->matrix_counts[port->matrix]) {
            matrix = &v->matrix_levels[(size_t)port->matrix * NGS2_MAX_LEVELS];
            matrix_dests = v->matrix_counts[port->matrix] / v->channels;
        }
        to = NULL;
        for (d = 0; d < dest_ch; d++) {
            for (c = 0; c < v->channels; c++) {
                uint32_t at = d * v->channels + c;
                float now, before;
                if (matrix)
                    now = d < matrix_dests ? matrix[at] : 0.0f;
                else
                    now = ngs2_default_level(v->channels, c, dest_ch, d);
                now *= port->volume;
                before = port->mixed ? port->gains[at] : now;
                port->gains[at] = now;
                if (now == 0.0f && before == 0.0f)
                    continue;
                if (!to)
                    to = mixer_input(dest, port->dest_input);
                {
                    const float* in = from + (size_t)c * stride;
                    float* out = to + (size_t)d * dstride;
                    if (now == before) {
                        for (i = 0; i < grain; i++)
                            out[i] += in[i] * now;
                    } else {
                        /* A level that changed slides there over the grain */
                        float delta = (now - before) / (float)grain, g = before;
                        for (i = 0; i < grain; i++, g += delta)
                            out[i] += in[i] * g;
                    }
                }
            }
        }
        port->mixed = 1;
    }
}

static int run_user_fx(ngs2_system* s, void* handler, const uintptr_t* data, uint32_t* flags,
                       float* samples, uint32_t channels, uint32_t stride) {
    float* pointers[NGS2_MAX_CHANNELS];
    ngs2_user_fx_context ctx;
    uint32_t c;
    if (!handler || !s->host.user_fx)
        return 0;
    for (c = 0; c < channels; c++)
        pointers[c] = samples + (size_t)c * stride;
    ctx.channel_data = pointers;
    ctx.user_data0 = data[0];
    ctx.user_data1 = data[1];
    ctx.user_data2 = data[2];
    ctx.flags = *flags;
    ctx.channels = channels;
    ctx.grain_samples = s->grain;
    ctx.sample_rate = s->sample_rate;
    s->host.user_fx(handler, &ctx);
    *flags = 0;
    return 1;
}

static void measure_peak(ngs2_voice* v, const float* samples, uint32_t channels, uint32_t stride,
                         uint32_t grain) {
    uint32_t c, i;
    float peak = 0.0f;
    for (c = 0; c < channels; c++) {
        const float* x = samples + (size_t)c * stride;
        for (i = 0; i < grain; i++) {
            float a = x[i] < 0.0f ? -x[i] : x[i];
            if (a > peak)
                peak = a;
        }
    }
    v->peak = peak;
}

static void render_sampler(ngs2_voice* v) {
    ngs2_rack* r = v->rack;
    ngs2_system* s = r->system;
    uint32_t grain = s->grain, stride = r->desc.max_grain_samples, i;
    float* bufs[NGS2_MAX_BUFFERS];
    int sound = sampler_grain(v, r->out, stride, grain);
    for (i = 0; i < r->desc.max_filters; i++)
        if (v->filters[i].enabled)
            biquad_run(&v->filters[i], r->out, v->channels, stride, grain);
    run_user_fx(s, v->fx_handler, v->fx_data, &v->fx_flags, r->out, v->channels, stride);
    if (v->ramp != RAMP_NONE)
        apply_ramp(r->out, v->channels, stride, grain, v->ramp);
    measure_peak(v, r->out, v->channels, stride, grain);
    for (i = 0; i < NGS2_MAX_BUFFERS; i++)
        bufs[i] = sound || v->fx_handler ? r->out : NULL;
    send(v, bufs, stride, grain);
    /* Nothing left of its waveform and none to come */
    if (v->starved && !v->block_count && !v->accepts_blocks && !v->cached &&
        v->play == PLAY_PLAYING)
        v->play = PLAY_STOPPED;
}

static void render_modules(ngs2_voice* v, int* sound) {
    ngs2_rack* r = v->rack;
    ngs2_system* s = r->system;
    uint32_t grain = s->grain, stride = r->desc.max_grain_samples, m, c, i;
    for (m = 0; m < r->desc.num_modules; m++) {
        const ngs2_module_desc* md = &r->desc.modules[m];
        ngs2_module_state* st = &v->modules[m];
        float* src = mixer_input(v, md->source_buffer);
        float* dst = mixer_input(v, md->dest_buffer);
        int has = sound[md->source_buffer];
        if (md->id == MODULE_REVERB && st->reverb) {
            /* Into the rack's grain and back: the reverb reads what it writes over */
            has = reverb_run(st->reverb, src, v->channels, r->out, v->channels, stride, grain, has);
            for (c = 0; c < v->channels; c++)
                memcpy(dst + (size_t)c * stride, r->out + (size_t)c * stride,
                       grain * sizeof(float));
            sound[md->dest_buffer] = has;
            continue;
        }
        if (src != dst)
            for (c = 0; c < v->channels; c++)
                memcpy(dst + (size_t)c * stride, src + (size_t)c * stride, grain * sizeof(float));
        switch (md->id) {
        case MODULE_GAIN:
            for (c = 0; c < v->channels; c++) {
                float* x = dst + (size_t)c * stride;
                float from = st->gain[c], to = st->gain_to[c];
                if (from == to) {
                    if (to != 1.0f)
                        for (i = 0; i < grain; i++)
                            x[i] *= to;
                } else {
                    float delta = (to - from) / (float)grain;
                    for (i = 0; i < grain; i++, from += delta)
                        x[i] *= from;
                }
                st->gain[c] = to;
            }
            break;
        case MODULE_FILTER:
            if (st->filter.enabled)
                biquad_run(&st->filter, dst, v->channels, stride, grain);
            break;
        case MODULE_USER_FX:
            if (run_user_fx(s, st->fx_handler, st->fx_data, &st->fx_flags, dst, v->channels,
                            stride))
                has = 1;
            break;
        case MODULE_USER_FX2:
            if (st->fx2_active && md->process_handler && s->host.user_fx2_process) {
                float* pointers[NGS2_MAX_CHANNELS];
                ngs2_user_fx2_process_context ctx;
                memset(&ctx, 0, sizeof(ctx));
                for (c = 0; c < v->channels; c++)
                    pointers[c] = dst + (size_t)c * stride;
                ctx.channel_data = pointers;
                ctx.common = r->fx2_common[m];
                ctx.param = st->fx2_param;
                ctx.work = st->fx2_work;
                ctx.state = st->fx2_state;
                ctx.user_data = md->user_data;
                ctx.flags = st->fx_flags;
                ctx.input_channels = ctx.output_channels = v->channels;
                ctx.grain_samples = grain;
                ctx.sample_rate = s->sample_rate;
                s->host.user_fx2_process(md->process_handler, &ctx);
                st->fx_flags = 0;
                has = 1;
            }
            break;
        default:
            break;
        }
        sound[md->dest_buffer] = has;
    }
}

static void render_mixer(ngs2_voice* v, const ngs2_render_buffer* outputs, uint32_t output_count) {
    ngs2_rack* r = v->rack;
    ngs2_system* s = r->system;
    uint32_t grain = s->grain, stride = r->desc.max_grain_samples, i, c, d;
    int sound[NGS2_MAX_BUFFERS];
    float* bufs[NGS2_MAX_BUFFERS];
    float* main_buf;
    for (i = 0; i < NGS2_MAX_BUFFERS; i++)
        sound[i] = i < r->buffers && v->buf_stamp[i] == s->render_count + 1u;
    main_buf = mixer_input(v, 0);
    if (r->desc.rack_id == NGS2_RACK_REVERB) {
        sound[0] = reverb_run(v->reverb, main_buf, v->in_channels, r->out, v->channels, stride,
                              grain, sound[0]);
        main_buf = r->out;
    } else {
        for (i = 0; i < r->desc.max_filters; i++)
            if (v->filters[i].enabled && sound[0])
                biquad_run(&v->filters[i], main_buf, v->channels, stride, grain);
        if (run_user_fx(s, v->fx_handler, v->fx_data, &v->fx_flags, main_buf, v->channels, stride))
            sound[0] = 1;
        if (r->desc.num_modules)
            render_modules(v, sound);
    }
    for (i = 0; i < NGS2_MAX_BUFFERS; i++) {
        bufs[i] = NULL;
        if (i < r->buffers && sound[i])
            bufs[i] = i == 0 ? main_buf : v->buf[i];
        if (bufs[i] && v->ramp != RAMP_NONE)
            apply_ramp(bufs[i], v->channels, stride, grain, v->ramp);
    }
    if (sound[0])
        measure_peak(v, main_buf, v->channels, stride, grain);
    else
        v->peak = 0.0f;
    if (!is_mastering(r)) {
        send(v, bufs, stride, grain);
        return;
    }
    if (!sound[0] || v->output_id >= output_count || v->output_id >= NGS2_MAX_OUTPUTS ||
        !outputs[v->output_id].buffer)
        return;
    {
        uint32_t out_ch = outputs[v->output_id].channels;
        float* mix = s->mix + (size_t)v->output_id * s->max_grain * NGS2_MAX_CHANNELS;
        if (!s->mix_used[v->output_id]) {
            memset(mix, 0, (size_t)grain * out_ch * sizeof(float));
            s->mix_used[v->output_id] = 1;
        }
        for (c = 0; c < v->channels; c++) {
            const float* in = main_buf + (size_t)c * stride;
            float level = speaker_of(v->channels, c) == SPK_LFE ? v->lfe_level : v->fbw_level;
            for (d = 0; d < out_ch; d++) {
                float g =
                    level * (v->channels == out_ch ? (c == d ? 1.0f : 0.0f)
                                                   : ngs2_default_level(v->channels, c, out_ch, d));
                float* o = mix + d;
                if (g == 0.0f)
                    continue;
                for (i = 0; i < grain; i++, o += out_ch)
                    *o += in[i] * g;
            }
        }
    }
}

/* What a voice is after the grain, and what the game reads of it */
static void voice_settle(ngs2_voice* v) {
    if (v->ramp == RAMP_OUT || v->ramp == RAMP_IN)
        v->ramp = RAMP_NONE;
    if (v->play == PLAY_STOPPED)
        v->play = PLAY_EMPTY;
    switch (v->play) {
    case PLAY_PLAYING:
        v->flags = NGS2_STATE_INUSE | NGS2_STATE_PLAYING;
        if (is_sampler(v->rack) && v->starved && v->accepts_blocks)
            v->flags |= NGS2_STATE_EMPTY;
        break;
    case PLAY_PAUSED:
        v->flags = NGS2_STATE_INUSE | NGS2_STATE_PAUSED;
        break;
    default:
        v->flags = v->setup ? NGS2_STATE_INUSE : 0;
        break;
    }
}

int ngs2_system_render(ngs2_system* system, const ngs2_render_buffer* buffers, uint32_t count) {
    ngs2_system* s = system;
    uint32_t grain = s->grain, i, v, p, progress;
    for (i = 0; i < count; i++) {
        uint32_t bytes;
        if (!buffers[i].buffer)
            continue;
        if (buffers[i].waveform_type != NGS2_WAVE_PCM_I16L &&
            buffers[i].waveform_type != NGS2_WAVE_PCM_F32L)
            return NGS2_ERROR_INVALID_WAVEFORM_TYPE;
        if (!buffers[i].channels || buffers[i].channels > NGS2_MAX_CHANNELS)
            return NGS2_ERROR_INVALID_NUM_CHANNELS;
        bytes = grain * buffers[i].channels *
                (buffers[i].waveform_type == NGS2_WAVE_PCM_I16L ? 2u : 4u);
        if (buffers[i].buffer_size < bytes)
            return NGS2_ERROR_INVALID_BUFFER_SIZE;
    }
    memset(s->mix_used, 0, sizeof(s->mix_used));
    /* Who waits on whom */
    for (i = 0; i < s->rack_count; i++)
        for (v = 0; v < s->racks[i]->desc.max_voices; v++) {
            s->racks[i]->voices[v].waiting = 0;
            s->racks[i]->voices[v].rendered = 0;
        }
    for (i = 0; i < s->rack_count; i++) {
        ngs2_rack* r = s->racks[i];
        for (v = 0; v < r->desc.max_voices; v++) {
            ngs2_voice* voice = &r->voices[v];
            if (!renders(voice))
                continue;
            for (p = 0; p < r->desc.max_ports; p++)
                if (voice->ports[p].dest)
                    voice->ports[p].dest->waiting++;
        }
    }
    for (i = 0; i < s->rack_count; i++) {
        ngs2_rack* r = s->racks[i];
        if (!is_sampler(r))
            continue;
        for (v = 0; v < r->desc.max_voices; v++)
            if (renders(&r->voices[v])) {
                render_sampler(&r->voices[v]);
                r->voices[v].rendered = 1;
            }
    }
    /* A mixer once all that feeds it is in. Voices in a ring never are: the
     * first of them in the order they stand goes without waiting. */
    for (;;) {
        ngs2_voice* held = NULL;
        progress = 0;
        for (i = 0; i < s->rack_count; i++) {
            ngs2_rack* r = s->racks[i];
            if (is_sampler(r))
                continue;
            for (v = 0; v < r->desc.max_voices; v++) {
                ngs2_voice* voice = &r->voices[v];
                if (voice->rendered || !renders(voice))
                    continue;
                if (voice->waiting) {
                    if (!held)
                        held = voice;
                    continue;
                }
                render_mixer(voice, buffers, count);
                voice->rendered = 1;
                progress = 1;
            }
        }
        if (progress)
            continue;
        if (!held)
            break;
        render_mixer(held, buffers, count);
        held->rendered = 1;
    }
    for (i = 0; i < count; i++) {
        uint32_t n = grain * buffers[i].channels, k;
        const float* mix = s->mix + (size_t)i * s->max_grain * NGS2_MAX_CHANNELS;
        if (!buffers[i].buffer)
            continue;
        if (i >= NGS2_MAX_OUTPUTS || !s->mix_used[i]) {
            memset(buffers[i].buffer, 0,
                   (size_t)n * (buffers[i].waveform_type == NGS2_WAVE_PCM_I16L ? 2u : 4u));
        } else if (buffers[i].waveform_type == NGS2_WAVE_PCM_F32L) {
            memcpy(buffers[i].buffer, mix, (size_t)n * sizeof(float));
        } else {
            int16_t* pcm = (int16_t*)buffers[i].buffer;
            for (k = 0; k < n; k++) {
                float x = mix[k] * 32767.0f;
                pcm[k] = (int16_t)(x != x          ? 0.0f
                                   : x > 32767.0f  ? 32767.0f
                                   : x < -32767.0f ? -32767.0f
                                                   : x);
            }
        }
    }
    for (i = 0; i < s->rack_count; i++)
        for (v = 0; v < s->racks[i]->desc.max_voices; v++)
            voice_settle(&s->racks[i]->voices[v]);
    s->render_count++;
    return NGS2_OK;
}

int ngs2_system_next_callback(ngs2_system* system, void** handler, ngs2_callback_info* info) {
    while (system->callback_count) {
        ngs2_pending_callback cb = system->callbacks[0];
        system->callback_count--;
        memmove(&system->callbacks[0], &system->callbacks[1],
                system->callback_count * sizeof(system->callbacks[0]));
        if (!ngs2_voice_from(cb.info.voice_handle))
            continue;
        *handler = cb.handler;
        *info = cb.info;
        return 1;
    }
    return 0;
}

ngs2_rack* ngs2_system_rack(ngs2_system* system, uint32_t index) {
    return index < system->rack_count ? system->racks[index] : NULL;
}
