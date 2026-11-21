/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/camera/camera.h"
#include "core/libraries/camera/camera_backend.h"
#include "core/libraries/camera/camera_helpers.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"

/* The configurations of types 1 to 5, for the two devices: formats and frame rate; the
 * resolution 1280x800 (0), no size of its own */
static const OrbisCameraConfigExtention camera_config_types[5][ORBIS_CAMERA_MAX_DEVICE_NUM] = {
    {
        {{ORBIS_CAMERA_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_Y8, ORBIS_CAMERA_SCALE_FORMAT_Y8,
          ORBIS_CAMERA_SCALE_FORMAT_Y8},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
        {{ORBIS_CAMERA_FORMAT_RAW16, ORBIS_CAMERA_SCALE_FORMAT_Y8, ORBIS_CAMERA_SCALE_FORMAT_Y8,
          ORBIS_CAMERA_SCALE_FORMAT_Y8},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
    },
    {
        {{ORBIS_CAMERA_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422,
          ORBIS_CAMERA_SCALE_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
        {{ORBIS_CAMERA_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422,
          ORBIS_CAMERA_SCALE_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
    },
    {
        {{ORBIS_CAMERA_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_Y8, ORBIS_CAMERA_SCALE_FORMAT_Y8,
          ORBIS_CAMERA_SCALE_FORMAT_Y8},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
        {{ORBIS_CAMERA_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_Y8, ORBIS_CAMERA_SCALE_FORMAT_Y8,
          ORBIS_CAMERA_SCALE_FORMAT_Y8},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
    },
    {
        {{ORBIS_CAMERA_FORMAT_RAW16, ORBIS_CAMERA_SCALE_FORMAT_YUV422,
          ORBIS_CAMERA_SCALE_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
        {{ORBIS_CAMERA_FORMAT_RAW16, ORBIS_CAMERA_SCALE_FORMAT_YUV422,
          ORBIS_CAMERA_SCALE_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
    },
    {
        {{ORBIS_CAMERA_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422,
          ORBIS_CAMERA_SCALE_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
        {{ORBIS_CAMERA_FORMAT_RAW16, ORBIS_CAMERA_SCALE_FORMAT_YUV422,
          ORBIS_CAMERA_SCALE_FORMAT_YUV422, ORBIS_CAMERA_SCALE_FORMAT_YUV422},
         ORBIS_CAMERA_RESOLUTION_1280X800,
         ORBIS_CAMERA_FRAMERATE_60,
         0,
         0,
         0,
         NULL},
    },
};

s32 PS4_SYSV_ABI sceCameraGetAttribute(s32 handle, OrbisCameraAttribute* attribute) {
    OrbisCameraChannel channel;
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetAttribute", "called");
    if (handle < 1 || attribute == NULL || attribute->sizeThis != sizeof(OrbisCameraAttribute) ||
        attribute->channel >= ORBIS_CAMERA_CHANNEL_BOTH ||
        attribute->channel < ORBIS_CAMERA_CHANNEL_0) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    channel = attribute->channel;

    /* Set default attributes */
    memset(attribute, 0, sizeof(OrbisCameraAttribute));
    attribute->sizeThis = sizeof(OrbisCameraAttribute);
    attribute->channel = channel;
    attribute->exposureGain.exposure = 83;
    attribute->exposureGain.gain = 100;
    attribute->whiteBalance.gainRed = 768;
    attribute->whiteBalance.gainBlue = 768;
    attribute->whiteBalance.gainGreen = 512;
    attribute->gamma.value = 4;
    attribute->saturation = 64;
    attribute->contrast = 32;
    attribute->sharpness = 1;
    attribute->hue = 1;

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetAutoExposureGain(s32 handle, OrbisCameraChannel channel, u32* enable,
                                              OrbisCameraAutoExposureGainTarget* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetAutoExposureGain", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (option != NULL && (g_firmware_version < KERNEL_FW_300 ||
                           option->sizeThis != sizeof(OrbisCameraAutoExposureGainTarget))) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *enable = 0;
    if (option != NULL) {
        option->sizeThis = 0;
        option->target = ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_DEF;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetAutoWhiteBalance(s32 handle, OrbisCameraChannel channel, u32* enable,
                                              void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetAutoWhiteBalance", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *enable = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetConfig(s32 handle, OrbisCameraConfig* config) {
    OrbisCameraConfigExtention default_extension;
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetConfig", "called");
    if (handle < 1 || config == NULL || config->sizeThis != sizeof(OrbisCameraConfig)) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    /* Set default config */
    config->configType = ORBIS_CAMERA_CONFIG_TYPE1;

    default_extension.format.formatLevel0 = ORBIS_CAMERA_FORMAT_YUV422;
    default_extension.format.formatLevel1 = ORBIS_CAMERA_SCALE_FORMAT_Y8;
    default_extension.format.formatLevel2 = ORBIS_CAMERA_SCALE_FORMAT_Y8;
    default_extension.format.formatLevel3 = ORBIS_CAMERA_SCALE_FORMAT_Y8;
    default_extension.resolution = ORBIS_CAMERA_RESOLUTION_1280X800;
    default_extension.framerate = ORBIS_CAMERA_FRAMERATE_60;
    default_extension.width = 0;
    default_extension.height = 0;
    default_extension.reserved1 = 0;

    memcpy(&config->configExtention[0], &default_extension, sizeof(OrbisCameraConfigExtention));
    memcpy(&config->configExtention[1], &default_extension, sizeof(OrbisCameraConfigExtention));

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetContrast(s32 handle, OrbisCameraChannel channel, u32* contrast,
                                      void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetContrast", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        contrast == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *contrast = 32;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetDefectivePixelCancellation(s32 handle, OrbisCameraChannel channel,
                                                        u32* enable, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetDefectivePixelCancellation", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *enable = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetExposureGain(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraExposureGain* exposure_gain, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetExposureGain", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        exposure_gain == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    /* Return default parameters */
    exposure_gain->exposureControl = 0;
    exposure_gain->exposure = 20;
    exposure_gain->gain = 100;
    exposure_gain->mode = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetGamma(s32 handle, OrbisCameraChannel channel, OrbisCameraGamma* gamma,
                                   void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetGamma", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        gamma == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    /* Return default parameters */
    memset(gamma, 0, sizeof(OrbisCameraGamma));
    gamma->value = 4;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetHue(s32 handle, OrbisCameraChannel channel, s32* hue, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetHue", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        hue == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *hue = 1;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetLensCorrection(s32 handle, OrbisCameraChannel channel, u32* enable,
                                            void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetLensCorrection", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *enable = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetMmapConnectedCount(u32* count) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraGetMmapConnectedCount",
          "(STUBBED) called");
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }
    if (count == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }

    *count = 0;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetProductInfo(void* product_info) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetProductInfo", "(STUBBED) called");
    if (product_info == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_INIT;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraGetRegister(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraGetRegister", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetRegistryInfo(void* registry_info) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetRegistryInfo", "(STUBBED) called");
    if (registry_info == NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_INIT;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraGetSaturation(s32 handle, OrbisCameraChannel channel, u32* saturation,
                                        void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetSaturation", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        saturation == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *saturation = 64;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetSharpness(s32 handle, OrbisCameraChannel channel, u32* sharpness,
                                       void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetSharpness", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        sharpness == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    *sharpness = 1;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetVrCaptureInfo(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraGetVrCaptureInfo", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraGetWhiteBalance(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraWhiteBalance* white_balance, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraGetWhiteBalance", "called");
    if (handle < 1 || channel >= ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        white_balance == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    /* Set default parameters */
    white_balance->whiteBalanceControl = 0;
    white_balance->gainRed = 768;
    white_balance->gainBlue = 768;
    white_balance->gainGreen = 512;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetAttribute(s32 handle, OrbisCameraAttribute* attribute) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetAttribute", "called");
    if (handle < 1 || attribute == NULL || attribute->sizeThis != sizeof(OrbisCameraAttribute) ||
        attribute->channel > ORBIS_CAMERA_CHANNEL_BOTH ||
        attribute->channel < ORBIS_CAMERA_CHANNEL_0) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetAttributeInternal(void) {
    /* Stubbed on real hardware */
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetAutoExposureGain(s32 handle, OrbisCameraChannel channel, u32 enable,
                                              OrbisCameraAutoExposureGainTarget* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetAutoExposureGain", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (option != NULL) {
        if (g_firmware_version < KERNEL_FW_300 ||
            option->sizeThis != sizeof(OrbisCameraAutoExposureGainTarget)) {
            return ORBIS_CAMERA_ERROR_PARAM;
        }
        if (option->target % 2 == 1 || option->target < ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_DEF ||
            option->target > ORBIS_CAMERA_ATTRIBUTE_AECAGC_TARGET_2_0) {
            return ORBIS_CAMERA_ERROR_PARAM;
        }
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetAutoWhiteBalance(s32 handle, OrbisCameraChannel channel, u32 enable,
                                              void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetAutoWhiteBalance", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetCalibData(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetCalibData", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetConfig(s32 handle, OrbisCameraConfig* config) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_INFO, "sceCameraSetConfig", "called");
    if (handle < 1 || config == NULL || config->sizeThis != sizeof(OrbisCameraConfig)) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetConfig",
              "ORBIS_CAMERA_ERROR_PARAM");
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetConfig",
              "ORBIS_CAMERA_ERROR_NOT_OPEN");
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }
    if (!camera_get_backend()->is_attached()) {
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetConfig",
              "ORBIS_CAMERA_ERROR_NOT_CONNECTED");
        return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
    }

    switch (config->configType) {
    case ORBIS_CAMERA_CONFIG_TYPE1:
    case ORBIS_CAMERA_CONFIG_TYPE2:
    case ORBIS_CAMERA_CONFIG_TYPE3:
    case ORBIS_CAMERA_CONFIG_TYPE4:
        output_config0 = camera_config_types[config->configType - 1][0];
        output_config1 = camera_config_types[config->configType - 1][1];
        break;
    case ORBIS_CAMERA_CONFIG_TYPE5:
        if (g_firmware_version < KERNEL_FW_450) {
            c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetConfig",
                  "ORBIS_CAMERA_ERROR_UNKNOWN_CONFIG");
            return ORBIS_CAMERA_ERROR_UNKNOWN_CONFIG;
        }
        output_config0 = camera_config_types[config->configType - 1][0];
        output_config1 = camera_config_types[config->configType - 1][1];
        break;
    case ORBIS_CAMERA_CONFIG_EXTENTION:
        output_config0 = config->configExtention[0];
        output_config1 = config->configExtention[1];
        break;
    default:
        c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetConfig", "Invalid config type %u",
              (unsigned)config->configType);
        return ORBIS_CAMERA_ERROR_PARAM;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetConfigInternal(s32 handle, OrbisCameraConfig* config) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetConfigInternal", "called");
    if (handle < 1 || config == NULL || config->sizeThis != sizeof(OrbisCameraConfig)) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetContrast(s32 handle, OrbisCameraChannel channel, u32 contrast,
                                      void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetContrast", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetDebugStop(u32 debug_stop_enable) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetDebugStop", "(STUBBED) called");
    if (debug_stop_enable > 1) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetDefectivePixelCancellation(s32 handle, OrbisCameraChannel channel,
                                                        u32 enable, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetDefectivePixelCancellation", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable > 1 || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetDefectivePixelCancellationInternal(s32 handle,
                                                                OrbisCameraChannel channel,
                                                                u32 enable, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetDefectivePixelCancellationInternal",
          "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable > 2 || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetExposureGain(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraExposureGain* exposure_gain, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetExposureGain", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        exposure_gain != NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }
    if (!camera_get_backend()->is_attached()) {
        return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetForceActivate(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetForceActivate", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetGamma(s32 handle, OrbisCameraChannel channel, OrbisCameraGamma* gamma,
                                   void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetGamma", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        gamma != NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetHue(s32 handle, OrbisCameraChannel channel, s32 hue, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetHue", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetLensCorrection(s32 handle, OrbisCameraChannel channel, u32 enable,
                                            void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetLensCorrection", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable > 1 || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetLensCorrectionInternal(s32 handle, OrbisCameraChannel channel,
                                                    u32 enable, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetLensCorrectionInternal", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        enable > 2 || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetProcessFocus(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetProcessFocus", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetProcessFocusByHandle(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetProcessFocusByHandle",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetRegister(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetRegister", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetSaturation(s32 handle, OrbisCameraChannel channel, u32 saturation,
                                        void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetSaturation", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetSharpness(s32 handle, OrbisCameraChannel channel, u32 sharpness,
                                       void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetSharpness", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (g_firmware_version >= KERNEL_FW_350 && sharpness > 10) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}

s32 PS4_SYSV_ABI sceCameraSetTrackerMode(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetTrackerMode", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetUacModeInternal(void) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetUacModeInternal", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetVideoSync(s32 handle, OrbisCameraVideoSyncParameter* video_sync) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetVideoSync", "(STUBBED) called");
    if (handle < 1 || video_sync == NULL ||
        video_sync->sizeThis != sizeof(OrbisCameraVideoSyncParameter) ||
        video_sync->videoSyncMode > 1 || video_sync->pModeOption != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetVideoSyncInternal(s32 handle,
                                               OrbisCameraVideoSyncParameter* video_sync) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_ERROR, "sceCameraSetVideoSyncInternal", "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceCameraSetWhiteBalance(s32 handle, OrbisCameraChannel channel,
                                          OrbisCameraWhiteBalance* white_balance, void* option) {
    c_log(C_LOG_LIB_CAMERA, C_LOG_LEVEL_DEBUG, "sceCameraSetWhiteBalance", "called");
    if (handle < 1 || channel > ORBIS_CAMERA_CHANNEL_BOTH || channel < ORBIS_CAMERA_CHANNEL_0 ||
        white_balance == NULL || option != NULL) {
        return ORBIS_CAMERA_ERROR_PARAM;
    }
    if (!g_library_opened) {
        return ORBIS_CAMERA_ERROR_NOT_OPEN;
    }

    return ORBIS_CAMERA_ERROR_NOT_CONNECTED;
}
