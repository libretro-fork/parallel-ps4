/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <string.h>

#include "core/libraries/kernel/kernel_c.h"
#include "core/user_manager.h"
#include "input/controller.h"

const input_colour_t input_user_colours[4] = {
    {0, 0, 255},   /* blue */
    {255, 0, 0},   /* red */
    {0, 255, 0},   /* green */
    {255, 0, 255}, /* pink */
};

static const int32_t axis_defaults[INPUT_AXIS_COUNT] = {128, 128, 128, 128, 0, 0};
/* How long an axis takes to get to a new value, in microseconds */
#define AXIS_SMOOTHING_TIME 33000

static input_controller_t controllers[INPUT_CONTROLLERS];

/* A state as a pad starts: centred sticks, at rest, upright */
static void state_reset(input_state_t* s) {
    unsigned i;
    memset(s, 0, sizeof(*s));
    for (i = 0; i < INPUT_AXIS_COUNT; i++) {
        s->axes[i] = axis_defaults[i];
        s->axis_smoothing_start_values[i] = axis_defaults[i];
        s->axis_smoothing_end_values[i] = axis_defaults[i];
    }
    /* Only the first axis is smoothed until told otherwise */
    s->axis_smoothing_flags[0] = true;
    s->acceleration.y = -9.81f;
    s->orientation.w = 1.0f;
}

static void state_on_axis(input_state_t* s, enum input_axis axis, int value, uint64_t timestamp,
                          bool smooth) {
    uint32_t button = 0;
    s->axes[axis] = s->axis_smoothing_end_values[axis];
    s->axis_smoothing_start_times[axis] = timestamp;
    s->axis_smoothing_start_values[axis] = s->axes[axis];
    s->axis_smoothing_end_values[axis] = value;
    s->axis_smoothing_flags[axis] = smooth;
    if (axis == INPUT_AXIS_TRIGGER_LEFT)
        button = INPUT_BUTTON_L2;
    else if (axis == INPUT_AXIS_TRIGGER_RIGHT)
        button = INPUT_BUTTON_R2;
    if (button) {
        if (value > 0)
            s->buttons |= button;
        else
            s->buttons &= ~button;
    }
}

static void state_update_axis_smoothing(input_state_t* s, uint64_t timestamp) {
    unsigned i;
    for (i = 0; i < INPUT_AXIS_COUNT; i++) {
        float t;
        int diff = s->axes[i] - s->axis_smoothing_end_values[i];
        if (!s->axis_smoothing_flags[i] || (diff < 0 ? -diff : diff) < 16) {
            s->axes[i] = s->axis_smoothing_end_values[i];
            continue;
        }
        t = (float)(timestamp - s->axis_smoothing_start_times[i]) / (float)AXIS_SMOOTHING_TIME;
        if (t < 0.0f)
            t = 0.0f;
        else if (t > 1.0f)
            t = 1.0f;
        s->axes[i] = (int32_t)(s->axis_smoothing_start_values[i] * (1 - t) +
                               s->axis_smoothing_end_values[i] * t);
    }
}

static input_fquat_t calculate_orientation(input_fvec3_t angular_velocity, float delta_time,
                                           input_fquat_t q) {
    input_fquat_t q_omega;
    float norm;
    if (delta_time > 1.0f)
        return q;
    /* q * (angular_velocity, 0) */
    q_omega.x = q.w * angular_velocity.x + q.y * angular_velocity.z - q.z * angular_velocity.y;
    q_omega.y = q.w * angular_velocity.y + q.z * angular_velocity.x - q.x * angular_velocity.z;
    q_omega.z = q.w * angular_velocity.z + q.x * angular_velocity.y - q.y * angular_velocity.x;
    q_omega.w = -q.x * angular_velocity.x - q.y * angular_velocity.y - q.z * angular_velocity.z;

    q.x += 0.5f * q_omega.x * delta_time;
    q.y += 0.5f * q_omega.y * delta_time;
    q.z += 0.5f * q_omega.z * delta_time;
    q.w += 0.5f * q_omega.w * delta_time;

    norm = (float)sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    q.x /= norm;
    q.y /= norm;
    q.z /= norm;
    q.w /= norm;
    return q;
}

/* In the combiner */

static void update_orientation(input_controller_t* c, uint64_t timestamp) {
    if (c->last_orientation_update == 0 || timestamp <= c->last_orientation_update) {
        c->last_orientation_update = timestamp;
        return;
    }
    c->state.orientation = calculate_orientation(
        c->state.angular_velocity, (float)(timestamp - c->last_orientation_update) / 1000000.f,
        c->state.orientation);
    c->last_orientation_update = timestamp;
}

static void history_push(input_controller_t* c) {
    c->history[(c->history_begin + c->history_size) % INPUT_HISTORY] = c->state;
    if (c->history_size < INPUT_HISTORY)
        c->history_size++;
    else
        c->history_begin = (c->history_begin + 1) % INPUT_HISTORY;
}

static void push_state(input_controller_t* c, uint64_t timestamp) {
    if (timestamp == 0)
        timestamp = kernel_process_time_usec();
    state_update_axis_smoothing(&c->state, timestamp);
    c->state.angular_velocity.x = c->gyro_buf[0];
    c->state.angular_velocity.y = c->gyro_buf[1];
    c->state.angular_velocity.z = c->gyro_buf[2];
    c->state.acceleration.x = c->accel_buf[0];
    c->state.acceleration.y = c->accel_buf[1];
    c->state.acceleration.z = c->accel_buf[2];
    update_orientation(c, timestamp);
    c->state.time = timestamp;
    c->state.touch_time_since_held_down =
        c->touch_down_timestamp == 0 ? 0 : timestamp - c->touch_down_timestamp;
    history_push(c);
    retro_seqlock_write_begin(&c->published_seq);
    c->published = c->state;
    retro_seqlock_write_end(&c->published_seq);
}

void input_controllers_init(void) {
    unsigned i;
    for (i = 0; i < INPUT_CONTROLLERS; i++) {
        input_controller_t* c = &controllers[i];
        memset(c, 0, sizeof(*c));
        c->accel_buf[1] = 9.81f;
        c->user_id = -1;
        c->next_touch_id = 1;
        state_reset(&c->state);
        c->published = c->state;
        combiner_init(&c->combiner);
        retro_seqlock_init(&c->published_seq);
    }
}

input_controller_t* input_controller_get(unsigned index) {
    return index < INPUT_CONTROLLERS ? &controllers[index] : NULL;
}

input_state_t input_controller_read_state(input_controller_t* c) {
    input_state_t state;
    int seq;
    do {
        seq = retro_seqlock_read_begin(&c->published_seq);
        state = c->published;
    } while (retro_seqlock_read_retry(&c->published_seq, seq));
    return state;
}

typedef struct read_states_args {
    input_controller_t* c;
    input_state_t* states;
    int num;
    int read;
} read_states_args_t;

static void read_states_run(void* p) {
    read_states_args_t* a = (read_states_args_t*)p;
    input_controller_t* c = a->c;
    a->read = 0;
    if (a->num <= 0)
        return;
    /* Disconnected, or one state asked for: the current one. Retained history can make a later
     * multi-sample read return up to 64 stale reports, so mixed single- and multi-sample reads
     * require dedicated tests. */
    if (!c->state.connected || a->num == 1) {
        a->states[0] = c->state;
        a->read = 1;
        return;
    }
    while (a->read < a->num && c->history_size) {
        a->states[a->read++] = c->history[c->history_begin];
        c->history_begin = (c->history_begin + 1) % INPUT_HISTORY;
        c->history_size--;
    }
}

int input_controller_read_states(input_controller_t* c, input_state_t* states, int num) {
    read_states_args_t a;
    a.c = c;
    a.states = states;
    a.num = num;
    a.read = 0;
    combiner_run(&c->combiner, read_states_run, &a);
    return a.read;
}

typedef struct change_args {
    input_controller_t* c;
    uint32_t button;
    bool pressed;
    enum input_axis axis;
    int value;
    bool smooth;
    int touch_index;
    float x, y;
} change_args_t;

static void button_run(void* p) {
    change_args_t* a = (change_args_t*)p;
    if (a->pressed)
        a->c->state.buttons |= a->button;
    else
        a->c->state.buttons &= ~a->button;
    push_state(a->c, 0);
}

void input_controller_button(input_controller_t* c, uint32_t button, bool pressed) {
    change_args_t a;
    a.c = c;
    a.button = button;
    a.pressed = pressed;
    combiner_run(&c->combiner, button_run, &a);
}

static void axis_run(void* p) {
    change_args_t* a = (change_args_t*)p;
    const uint64_t timestamp = kernel_process_time_usec();
    state_on_axis(&a->c->state, a->axis, a->value, timestamp, a->smooth);
    push_state(a->c, timestamp);
}

void input_controller_axis(input_controller_t* c, enum input_axis axis, int value, bool smooth) {
    change_args_t a;
    a.c = c;
    a.axis = axis;
    a.value = value;
    a.smooth = smooth;
    combiner_run(&c->combiner, axis_run, &a);
}

static void poll_run(void* p) {
    push_state((input_controller_t*)p, 0);
}

void input_controller_poll(input_controller_t* c) {
    combiner_run(&c->combiner, poll_run, c);
}

static void reset_orientation_run(void* p) {
    input_controller_t* c = (input_controller_t*)p;
    c->state.orientation.x = 0.0f;
    c->state.orientation.y = 0.0f;
    c->state.orientation.z = 0.0f;
    c->state.orientation.w = 1.0f;
    c->last_orientation_update = 0;
    push_state(c, 0);
}

void input_controller_reset_orientation(input_controller_t* c) {
    combiner_run(&c->combiner, reset_orientation_run, c);
}

static void touchpad_run(void* p) {
    change_args_t* a = (change_args_t*)p;
    input_controller_t* c = a->c;
    const uint64_t timestamp = kernel_process_time_usec();
    const bool was_pressed = c->state.touchpad[0].state || c->state.touchpad[1].state;
    input_touchpad_entry_t* touch = &c->state.touchpad[a->touch_index];
    bool is_pressed;
    if (a->pressed && !touch->state) {
        touch->id = c->next_touch_id;
        c->next_touch_id = c->next_touch_id == 127 ? 1 : (uint8_t)(c->next_touch_id + 1);
    }
    touch->state = a->pressed;
    touch->x = (uint16_t)(a->x * 1920);
    touch->y = (uint16_t)(a->y * 941);
    is_pressed = c->state.touchpad[0].state || c->state.touchpad[1].state;
    if (!was_pressed && is_pressed)
        c->touch_down_timestamp = timestamp;
    else if (was_pressed && !is_pressed)
        c->touch_down_timestamp = 0;
    push_state(c, timestamp);
}

void input_controller_set_touchpad(input_controller_t* c, int touch_index, bool down, float x,
                                   float y) {
    change_args_t a;
    if (touch_index < 0 || touch_index >= 2)
        return;
    a.c = c;
    a.touch_index = touch_index;
    a.pressed = down;
    a.x = x;
    a.y = y;
    combiner_run(&c->combiner, touchpad_run, &a);
}

static void connect_run(void* p) {
    input_controller_t* c = (input_controller_t*)p;
    c->history_begin = 0;
    c->history_size = 0;
    if (!c->state.connected) {
        c->state.connected_count++;
        if (c->state.connected_count == 0)
            c->state.connected_count = 1;
    }
    c->state.connected = true;
    c->last_orientation_update = 0;
    push_state(c, 0);
}

void input_controller_connect(input_controller_t* c) {
    combiner_run(&c->combiner, connect_run, c);
}

static void disconnect_run(void* p) {
    input_controller_t* c = (input_controller_t*)p;
    const uint8_t connected_count = c->state.connected_count;
    c->history_begin = 0;
    c->history_size = 0;
    state_reset(&c->state);
    c->state.connected_count = connected_count;
    c->gyro_buf[0] = 0.0f;
    c->gyro_buf[1] = 0.0f;
    c->gyro_buf[2] = 0.0f;
    c->accel_buf[0] = 0.0f;
    c->accel_buf[1] = 9.81f;
    c->accel_buf[2] = 0.0f;
    c->next_touch_id = 1;
    c->touch_down_timestamp = 0;
    c->state.connected = false;
    c->last_orientation_update = 0;
    push_state(c, 0);
}

void input_controller_disconnect(input_controller_t* c) {
    combiner_run(&c->combiner, disconnect_run, c);
}

/* The light bar's colour, as the game set it, for the game to read back: the frontend has no
 * light bar to show it on. */
void input_controller_set_light_bar(input_controller_t* c, input_colour_t colour) {
    c->colour = colour;
}

input_colour_t input_controller_get_light_bar(const input_controller_t* c) {
    return c->colour;
}

bool input_controller_set_vibration(input_controller_t* c, uint8_t small_motor,
                                    uint8_t large_motor) {
    /* The core's pads are the frontend's ports, in the controllers' order */
    ptrdiff_t index = c - controllers;
    if (index >= 0 && index < 4)
        libretro_set_rumble((uint32_t)index, small_motor, large_motor);
    return true;
}

void input_connect_first_controller(void) {
    static bool connected = false;
    struct user* u;
    if (connected)
        return;
    connected = true;
    u = user_manager_get_by_player_index(1);
    controllers[0].user_id = u->user_id;
    input_controller_connect(&controllers[0]);
    if (user_manager_login(u, 1))
        userservice_push_login(u->user_id);
}

int input_controller_index_from_user_id(int32_t user_id) {
    const struct user* u = user_manager_get_by_id(user_id);
    return u ? u->player_index - 1 : -1;
}

int input_controller_index_from_controller_id(int32_t controller_id) {
    if (controller_id < 1 || controller_id > 5)
        return -1;
    return controller_id - 1;
}
