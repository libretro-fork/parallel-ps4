/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>

#include "core/libraries/libc_internal/libc_internal_math.h"

double PS4_SYSV_ABI internal_sin(double x) {
    return sin(x);
}

float PS4_SYSV_ABI internal_sinf(float x) {
    return sinf(x);
}

double PS4_SYSV_ABI internal_cos(double x) {
    return cos(x);
}

float PS4_SYSV_ABI internal_cosf(float x) {
    return cosf(x);
}

void PS4_SYSV_ABI internal_sincos(double x, double* sinp, double* cosp) {
    *sinp = sin(x);
    *cosp = cos(x);
}

void PS4_SYSV_ABI internal_sincosf(float x, float* sinp, float* cosp) {
    *sinp = sinf(x);
    *cosp = cosf(x);
}

double PS4_SYSV_ABI internal_tan(double x) {
    return tan(x);
}

float PS4_SYSV_ABI internal_tanf(float x) {
    return tanf(x);
}

double PS4_SYSV_ABI internal_asin(double x) {
    return asin(x);
}

float PS4_SYSV_ABI internal_asinf(float x) {
    return asinf(x);
}

double PS4_SYSV_ABI internal_acos(double x) {
    return acos(x);
}

float PS4_SYSV_ABI internal_acosf(float x) {
    return acosf(x);
}

double PS4_SYSV_ABI internal_atan(double x) {
    return atan(x);
}

float PS4_SYSV_ABI internal_atanf(float x) {
    return atanf(x);
}

double PS4_SYSV_ABI internal_atan2(double y, double x) {
    return atan2(y, x);
}

float PS4_SYSV_ABI internal_atan2f(float y, float x) {
    return atan2f(y, x);
}

double PS4_SYSV_ABI internal_exp(double x) {
    return exp(x);
}

float PS4_SYSV_ABI internal_expf(float x) {
    return expf(x);
}

double PS4_SYSV_ABI internal_exp2(double x) {
    return exp2(x);
}

float PS4_SYSV_ABI internal_exp2f(float x) {
    return exp2f(x);
}

double PS4_SYSV_ABI internal_pow(double x, double y) {
    return pow(x, y);
}

float PS4_SYSV_ABI internal_powf(float x, float y) {
    return powf(x, y);
}

double PS4_SYSV_ABI internal_log(double x) {
    return log(x);
}

float PS4_SYSV_ABI internal_logf(float x) {
    return logf(x);
}

double PS4_SYSV_ABI internal_log10(double x) {
    return log10(x);
}

float PS4_SYSV_ABI internal_log10f(float x) {
    return log10f(x);
}
