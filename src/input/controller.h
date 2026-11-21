/* SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef PARALLEL_PS4_INPUT_CONTROLLER_H
#define PARALLEL_PS4_INPUT_CONTROLLER_H

/* The game's pads: what the frontend's ports drive, for the pad library to read.
 *
 * What changes a pad's state, and the reads of its history, run one at a time in its combiner;
 * input_controller_read_state takes the state as last pushed, under a seqlock, without
 * waiting on anything. */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <rthreads/retro_seqlock.h>

#include "common/combine.h"

RETRO_BEGIN_DECLS

enum input_axis {
    INPUT_AXIS_LEFT_X = 0,
    INPUT_AXIS_LEFT_Y,
    INPUT_AXIS_RIGHT_X,
    INPUT_AXIS_RIGHT_Y,
    INPUT_AXIS_TRIGGER_LEFT,
    INPUT_AXIS_TRIGGER_RIGHT,

    INPUT_AXIS_COUNT
};

/* The pad's buttons, as the pad library reports them (OrbisPadButtonDataOffset) */
#define INPUT_BUTTON_L2 0x100u
#define INPUT_BUTTON_R2 0x200u

/* The controllers there are */
#define INPUT_CONTROLLERS 5
/* The states a controller keeps for a read of several */
#define INPUT_HISTORY 64

typedef struct input_touchpad_entry {
    uint8_t id;
    bool state;
    uint16_t x;
    uint16_t y;
} input_touchpad_entry_t;

typedef struct input_colour {
    uint8_t r, g, b;
} input_colour_t;

/* The light bar colours of the users, by player */
extern const input_colour_t input_user_colours[4];

typedef struct input_fvec3 {
    float x, y, z;
} input_fvec3_t;

typedef struct input_fquat {
    float x, y, z, w;
} input_fquat_t;

typedef struct input_state {
    /* Held buttons: OrbisPadButtonDataOffset bits */
    uint32_t buttons;
    uint64_t time;
    int32_t axes[INPUT_AXIS_COUNT];
    input_touchpad_entry_t touchpad[2];
    input_fvec3_t acceleration;
    input_fvec3_t angular_velocity;
    input_fquat_t orientation;
    uint64_t touch_time_since_held_down;
    bool connected;
    uint8_t connected_count;

    /* An axis moving to a new value moves there over 33 ms */
    bool axis_smoothing_flags[INPUT_AXIS_COUNT];
    uint64_t axis_smoothing_start_times[INPUT_AXIS_COUNT];
    int axis_smoothing_start_values[INPUT_AXIS_COUNT];
    int axis_smoothing_end_values[INPUT_AXIS_COUNT];
} input_state_t;

typedef struct input_controller {
    float gyro_buf[3];
    float accel_buf[3];
    /* The user it is logged in as, ORBIS_USER_SERVICE_USER_ID_INVALID (-1) for none */
    int32_t user_id;

    /* The rest is the controller's own */
    uint8_t next_touch_id;
    uint64_t touch_down_timestamp;
    uint64_t last_orientation_update;
    input_colour_t colour;

    input_state_t state;
    combiner_t combiner;
    input_state_t published;
    retro_seqlock_t published_seq;
    input_state_t history[INPUT_HISTORY];
    uint32_t history_begin;
    uint32_t history_size;
} input_controller_t;

/* Makes the controllers, all disconnected; before any other call */
void input_controllers_init(void);

/* Controller @index (0 to INPUT_CONTROLLERS - 1) */
input_controller_t* input_controller_get(unsigned index);

/* The first controller, connected and its user logged in when the game starts the pad
 * library: the one the frontend's first port drives (libretro/host.cpp connects the others) */
void input_connect_first_controller(void);

/* The controller of @user_id's player, as its index; -1 for none */
int input_controller_index_from_user_id(int32_t user_id);
/* The controller of @controller_id (1 to 5), as its index; -1 for none */
int input_controller_index_from_controller_id(int32_t controller_id);

void input_controller_connect(input_controller_t* c);
void input_controller_disconnect(input_controller_t* c);

/* The state as last pushed */
input_state_t input_controller_read_state(input_controller_t* c);
/* Up to @num states into @states: the history for more than one; their count */
int input_controller_read_states(input_controller_t* c, input_state_t* states, int num);

void input_controller_button(input_controller_t* c, uint32_t button, bool pressed);
void input_controller_axis(input_controller_t* c, enum input_axis axis, int value, bool smooth);
void input_controller_poll(input_controller_t* c);
void input_controller_reset_orientation(input_controller_t* c);
void input_controller_set_touchpad(input_controller_t* c, int touch_index, bool down, float x,
                                   float y);

void input_controller_set_light_bar(input_controller_t* c, input_colour_t colour);
input_colour_t input_controller_get_light_bar(const input_controller_t* c);
bool input_controller_set_vibration(input_controller_t* c, uint8_t small_motor,
                                    uint8_t large_motor);

/* What the controller module calls in the C++ parts of the core */

/* Rumble on the frontend's port @pad */
void libretro_set_rumble(uint32_t pad, uint8_t small_motor, uint8_t large_motor);
/* Tells the game @user_id logged in */
void userservice_push_login(int32_t user_id);

RETRO_END_DECLS

#endif
