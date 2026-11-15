#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include "libretro.h"

/*
 ********************************
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/
/* RETRO_LANGUAGE_AR */

#define CATEGORY_SYSTEM_LABEL_AR NULL
#define CATEGORY_SYSTEM_INFO_0_AR NULL
#define CATEGORY_VIDEO_LABEL_AR NULL
#define CATEGORY_VIDEO_INFO_0_AR NULL
#define CATEGORY_AUDIO_LABEL_AR NULL
#define CATEGORY_AUDIO_INFO_0_AR NULL
#define CATEGORY_TROPHIES_LABEL_AR NULL
#define CATEGORY_TROPHIES_INFO_0_AR NULL
#define CATEGORY_LOGGING_LABEL_AR NULL
#define CATEGORY_LOGGING_INFO_0_AR NULL
#define SHADPS4_NEO_MODE_LABEL_AR NULL
#define SHADPS4_NEO_MODE_INFO_0_AR NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_AR NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_AR NULL
#define OPTION_VAL_1_AR NULL
#define OPTION_VAL_18_AR NULL
#define OPTION_VAL_21_AR NULL
#define OPTION_VAL_23_AR NULL
#define OPTION_VAL_14_AR NULL
#define OPTION_VAL_6_AR NULL
#define OPTION_VAL_12_AR NULL
#define OPTION_VAL_22_AR NULL
#define OPTION_VAL_2_AR NULL
#define OPTION_VAL_4_AR NULL
#define OPTION_VAL_25_AR NULL
#define OPTION_VAL_24_AR NULL
#define OPTION_VAL_29_AR NULL
#define OPTION_VAL_5_AR NULL
#define OPTION_VAL_0_AR NULL
#define OPTION_VAL_9_AR NULL
#define OPTION_VAL_15_AR NULL
#define OPTION_VAL_16_AR NULL
#define OPTION_VAL_17_AR NULL
#define OPTION_VAL_7_AR NULL
#define OPTION_VAL_26_AR NULL
#define OPTION_VAL_8_AR NULL
#define OPTION_VAL_11_AR NULL
#define OPTION_VAL_20_AR NULL
#define OPTION_VAL_3_AR NULL
#define OPTION_VAL_13_AR NULL
#define OPTION_VAL_27_AR NULL
#define OPTION_VAL_10_AR NULL
#define OPTION_VAL_19_AR NULL
#define OPTION_VAL_30_AR NULL
#define OPTION_VAL_28_AR NULL
#define SHADPS4_DEV_KIT_LABEL_AR NULL
#define SHADPS4_DEV_KIT_INFO_0_AR NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_AR NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_AR NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_AR NULL
#define OPTION_VAL_512_AR NULL
#define OPTION_VAL_1024_AR NULL
#define OPTION_VAL_2048_AR NULL
#define OPTION_VAL_3072_AR NULL
#define OPTION_VAL_4096_AR NULL
#define OPTION_VAL_6144_AR NULL
#define OPTION_VAL_8192_AR NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_AR NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_AR NULL
#define OPTION_VAL_CROSS_AR NULL
#define OPTION_VAL_CIRCLE_AR NULL
#define SHADPS4_NETWORK_LABEL_AR NULL
#define SHADPS4_NETWORK_INFO_0_AR NULL
#define SHADPS4_CONTROLLERS_LABEL_AR NULL
#define SHADPS4_CONTROLLERS_INFO_0_AR NULL
#define SHADPS4_SHOW_SPLASH_LABEL_AR NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_AR NULL
#define SHADPS4_RESOLUTION_LABEL_AR NULL
#define SHADPS4_RESOLUTION_INFO_0_AR NULL
#define OPTION_VAL_1280X720_AR NULL
#define OPTION_VAL_1920X1080_AR NULL
#define OPTION_VAL_2560X1440_AR NULL
#define OPTION_VAL_3840X2160_AR NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_AR NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_AR NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_AR NULL
#define OPTION_VAL_60_AR NULL
#define OPTION_VAL_90_AR NULL
#define OPTION_VAL_120_AR NULL
#define OPTION_VAL_144_AR NULL
#define OPTION_VAL_240_AR NULL
#define SHADPS4_READBACKS_LABEL_AR NULL
#define SHADPS4_READBACKS_INFO_0_AR NULL
#define OPTION_VAL_RELAXED_AR NULL
#define OPTION_VAL_PRECISE_AR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_AR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_AR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_AR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_AR NULL
#define SHADPS4_SHADER_CACHE_LABEL_AR NULL
#define SHADPS4_SHADER_CACHE_INFO_0_AR NULL
#define SHADPS4_SHOW_FPS_LABEL_AR NULL
#define SHADPS4_SHOW_FPS_INFO_0_AR NULL
#define SHADPS4_VOLUME_LABEL_AR NULL
#define SHADPS4_VOLUME_INFO_0_AR NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_AR NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_AR NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_AR NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_AR NULL
#define OPTION_VAL_40_AR NULL
#define OPTION_VAL_50_AR NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_AR NULL
#define OPTION_VAL_70_AR NULL
#define OPTION_VAL_80_AR NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_AR NULL
#define OPTION_VAL_100_AR NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_AR NULL
#define OPTION_VAL_150_AR NULL
#define OPTION_VAL_200_AR NULL
#define OPTION_VAL_300_AR NULL
#define OPTION_VAL_400_AR NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_AR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_AR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_AR NULL
#define SHADPS4_TROPHY_POSITION_LABEL_AR NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_AR NULL
#define OPTION_VAL_LEFT_AR NULL
#define OPTION_VAL_RIGHT_AR NULL
#define OPTION_VAL_TOP_AR NULL
#define OPTION_VAL_BOTTOM_AR NULL
#define SHADPS4_TROPHY_DURATION_LABEL_AR NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_AR NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_AR NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_AR NULL
#define SHADPS4_LOG_INFO_0_AR NULL
#define SHADPS4_LOG_SEPARATE_LABEL_AR NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_AR NULL
#define SHADPS4_LOG_SYNC_LABEL_AR NULL
#define SHADPS4_LOG_SYNC_INFO_0_AR NULL

struct retro_core_option_v2_category option_cats_ar[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_AR,
        CATEGORY_SYSTEM_INFO_0_AR
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_AR,
        CATEGORY_VIDEO_INFO_0_AR
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_AR,
        CATEGORY_AUDIO_INFO_0_AR
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_AR,
        CATEGORY_TROPHIES_INFO_0_AR
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_AR,
        CATEGORY_LOGGING_INFO_0_AR
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ar[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_AR,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_AR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_AR,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_AR,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_AR },
            { "18", OPTION_VAL_18_AR },
            { "21", OPTION_VAL_21_AR },
            { "23", OPTION_VAL_23_AR },
            { "14", OPTION_VAL_14_AR },
            { "6", OPTION_VAL_6_AR },
            { "12", OPTION_VAL_12_AR },
            { "22", OPTION_VAL_22_AR },
            { "2", OPTION_VAL_2_AR },
            { "4", OPTION_VAL_4_AR },
            { "25", OPTION_VAL_25_AR },
            { "24", OPTION_VAL_24_AR },
            { "29", OPTION_VAL_29_AR },
            { "5", OPTION_VAL_5_AR },
            { "0", OPTION_VAL_0_AR },
            { "9", OPTION_VAL_9_AR },
            { "15", OPTION_VAL_15_AR },
            { "16", OPTION_VAL_16_AR },
            { "17", OPTION_VAL_17_AR },
            { "7", OPTION_VAL_7_AR },
            { "26", OPTION_VAL_26_AR },
            { "8", OPTION_VAL_8_AR },
            { "11", OPTION_VAL_11_AR },
            { "20", OPTION_VAL_20_AR },
            { "3", OPTION_VAL_3_AR },
            { "13", OPTION_VAL_13_AR },
            { "27", OPTION_VAL_27_AR },
            { "10", OPTION_VAL_10_AR },
            { "19", OPTION_VAL_19_AR },
            { "30", OPTION_VAL_30_AR },
            { "28", OPTION_VAL_28_AR },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_AR,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_AR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_AR,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_AR,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_AR },
            { "512", OPTION_VAL_512_AR },
            { "1024", OPTION_VAL_1024_AR },
            { "2048", OPTION_VAL_2048_AR },
            { "3072", OPTION_VAL_3072_AR },
            { "4096", OPTION_VAL_4096_AR },
            { "6144", OPTION_VAL_6144_AR },
            { "8192", OPTION_VAL_8192_AR },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_AR,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_AR,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_AR },
            { "circle", OPTION_VAL_CIRCLE_AR },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_AR,
        NULL,
        SHADPS4_NETWORK_INFO_0_AR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_AR,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_AR,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_AR,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_AR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_AR,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_AR,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_AR },
            { "1920x1080", OPTION_VAL_1920X1080_AR },
            { "2560x1440", OPTION_VAL_2560X1440_AR },
            { "3840x2160", OPTION_VAL_3840X2160_AR },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_AR,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_AR,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_AR },
            { "60",  OPTION_VAL_60_AR },
            { "90",  OPTION_VAL_90_AR },
            { "120", OPTION_VAL_120_AR },
            { "144", OPTION_VAL_144_AR },
            { "240", OPTION_VAL_240_AR },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_AR,
        NULL,
        SHADPS4_READBACKS_INFO_0_AR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_AR },
            { "precise",  OPTION_VAL_PRECISE_AR },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_AR,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_AR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_AR,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_AR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_AR,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_AR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_AR,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_AR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_AR,
        NULL,
        SHADPS4_VOLUME_INFO_0_AR,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_AR },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_AR },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_AR },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_AR },
            { "40",  OPTION_VAL_40_AR },
            { "50",  OPTION_VAL_50_AR },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_AR },
            { "70",  OPTION_VAL_70_AR },
            { "80",  OPTION_VAL_80_AR },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_AR },
            { "100", OPTION_VAL_100_AR },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_AR },
            { "150", OPTION_VAL_150_AR },
            { "200", OPTION_VAL_200_AR },
            { "300", OPTION_VAL_300_AR },
            { "400", OPTION_VAL_400_AR },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_AR },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_AR,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_AR,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_AR,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_AR,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_AR },
            { "right", OPTION_VAL_RIGHT_AR },
            { "top", OPTION_VAL_TOP_AR },
            { "bottom", OPTION_VAL_BOTTOM_AR },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_AR,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_AR,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_AR },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_AR },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_AR },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_AR },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_AR },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_AR },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_AR },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_AR },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_AR },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_AR },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_AR,
        NULL,
        SHADPS4_LOG_INFO_0_AR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_AR,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_AR,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_AR,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_AR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_ar = {
   option_cats_ar,
   option_defs_ar
};

/* RETRO_LANGUAGE_AST */

#define CATEGORY_SYSTEM_LABEL_AST NULL
#define CATEGORY_SYSTEM_INFO_0_AST NULL
#define CATEGORY_VIDEO_LABEL_AST NULL
#define CATEGORY_VIDEO_INFO_0_AST NULL
#define CATEGORY_AUDIO_LABEL_AST NULL
#define CATEGORY_AUDIO_INFO_0_AST NULL
#define CATEGORY_TROPHIES_LABEL_AST NULL
#define CATEGORY_TROPHIES_INFO_0_AST NULL
#define CATEGORY_LOGGING_LABEL_AST NULL
#define CATEGORY_LOGGING_INFO_0_AST NULL
#define SHADPS4_NEO_MODE_LABEL_AST NULL
#define SHADPS4_NEO_MODE_INFO_0_AST NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_AST NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_AST NULL
#define OPTION_VAL_1_AST NULL
#define OPTION_VAL_18_AST NULL
#define OPTION_VAL_21_AST NULL
#define OPTION_VAL_23_AST NULL
#define OPTION_VAL_14_AST NULL
#define OPTION_VAL_6_AST NULL
#define OPTION_VAL_12_AST NULL
#define OPTION_VAL_22_AST NULL
#define OPTION_VAL_2_AST NULL
#define OPTION_VAL_4_AST NULL
#define OPTION_VAL_25_AST NULL
#define OPTION_VAL_24_AST NULL
#define OPTION_VAL_29_AST NULL
#define OPTION_VAL_5_AST NULL
#define OPTION_VAL_0_AST NULL
#define OPTION_VAL_9_AST NULL
#define OPTION_VAL_15_AST NULL
#define OPTION_VAL_16_AST NULL
#define OPTION_VAL_17_AST NULL
#define OPTION_VAL_7_AST NULL
#define OPTION_VAL_26_AST NULL
#define OPTION_VAL_8_AST NULL
#define OPTION_VAL_11_AST NULL
#define OPTION_VAL_20_AST NULL
#define OPTION_VAL_3_AST NULL
#define OPTION_VAL_13_AST NULL
#define OPTION_VAL_27_AST NULL
#define OPTION_VAL_10_AST NULL
#define OPTION_VAL_19_AST NULL
#define OPTION_VAL_30_AST NULL
#define OPTION_VAL_28_AST NULL
#define SHADPS4_DEV_KIT_LABEL_AST NULL
#define SHADPS4_DEV_KIT_INFO_0_AST NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_AST NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_AST NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_AST NULL
#define OPTION_VAL_512_AST NULL
#define OPTION_VAL_1024_AST NULL
#define OPTION_VAL_2048_AST NULL
#define OPTION_VAL_3072_AST NULL
#define OPTION_VAL_4096_AST NULL
#define OPTION_VAL_6144_AST NULL
#define OPTION_VAL_8192_AST NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_AST NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_AST NULL
#define OPTION_VAL_CROSS_AST NULL
#define OPTION_VAL_CIRCLE_AST NULL
#define SHADPS4_NETWORK_LABEL_AST NULL
#define SHADPS4_NETWORK_INFO_0_AST NULL
#define SHADPS4_CONTROLLERS_LABEL_AST NULL
#define SHADPS4_CONTROLLERS_INFO_0_AST NULL
#define SHADPS4_SHOW_SPLASH_LABEL_AST NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_AST NULL
#define SHADPS4_RESOLUTION_LABEL_AST NULL
#define SHADPS4_RESOLUTION_INFO_0_AST NULL
#define OPTION_VAL_1280X720_AST NULL
#define OPTION_VAL_1920X1080_AST NULL
#define OPTION_VAL_2560X1440_AST NULL
#define OPTION_VAL_3840X2160_AST NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_AST NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_AST NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_AST NULL
#define OPTION_VAL_60_AST NULL
#define OPTION_VAL_90_AST NULL
#define OPTION_VAL_120_AST NULL
#define OPTION_VAL_144_AST NULL
#define OPTION_VAL_240_AST NULL
#define SHADPS4_READBACKS_LABEL_AST NULL
#define SHADPS4_READBACKS_INFO_0_AST NULL
#define OPTION_VAL_RELAXED_AST NULL
#define OPTION_VAL_PRECISE_AST NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_AST NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_AST NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_AST NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_AST NULL
#define SHADPS4_SHADER_CACHE_LABEL_AST NULL
#define SHADPS4_SHADER_CACHE_INFO_0_AST NULL
#define SHADPS4_SHOW_FPS_LABEL_AST NULL
#define SHADPS4_SHOW_FPS_INFO_0_AST NULL
#define SHADPS4_VOLUME_LABEL_AST NULL
#define SHADPS4_VOLUME_INFO_0_AST NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_AST NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_AST NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_AST NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_AST NULL
#define OPTION_VAL_40_AST NULL
#define OPTION_VAL_50_AST NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_AST NULL
#define OPTION_VAL_70_AST NULL
#define OPTION_VAL_80_AST NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_AST NULL
#define OPTION_VAL_100_AST NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_AST NULL
#define OPTION_VAL_150_AST NULL
#define OPTION_VAL_200_AST NULL
#define OPTION_VAL_300_AST NULL
#define OPTION_VAL_400_AST NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_AST NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_AST NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_AST NULL
#define SHADPS4_TROPHY_POSITION_LABEL_AST NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_AST NULL
#define OPTION_VAL_LEFT_AST NULL
#define OPTION_VAL_RIGHT_AST NULL
#define OPTION_VAL_TOP_AST NULL
#define OPTION_VAL_BOTTOM_AST NULL
#define SHADPS4_TROPHY_DURATION_LABEL_AST NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_AST NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_AST NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_AST NULL
#define SHADPS4_LOG_INFO_0_AST NULL
#define SHADPS4_LOG_SEPARATE_LABEL_AST NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_AST NULL
#define SHADPS4_LOG_SYNC_LABEL_AST NULL
#define SHADPS4_LOG_SYNC_INFO_0_AST NULL

struct retro_core_option_v2_category option_cats_ast[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_AST,
        CATEGORY_SYSTEM_INFO_0_AST
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_AST,
        CATEGORY_VIDEO_INFO_0_AST
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_AST,
        CATEGORY_AUDIO_INFO_0_AST
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_AST,
        CATEGORY_TROPHIES_INFO_0_AST
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_AST,
        CATEGORY_LOGGING_INFO_0_AST
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ast[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_AST,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_AST,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_AST,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_AST,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_AST },
            { "18", OPTION_VAL_18_AST },
            { "21", OPTION_VAL_21_AST },
            { "23", OPTION_VAL_23_AST },
            { "14", OPTION_VAL_14_AST },
            { "6", OPTION_VAL_6_AST },
            { "12", OPTION_VAL_12_AST },
            { "22", OPTION_VAL_22_AST },
            { "2", OPTION_VAL_2_AST },
            { "4", OPTION_VAL_4_AST },
            { "25", OPTION_VAL_25_AST },
            { "24", OPTION_VAL_24_AST },
            { "29", OPTION_VAL_29_AST },
            { "5", OPTION_VAL_5_AST },
            { "0", OPTION_VAL_0_AST },
            { "9", OPTION_VAL_9_AST },
            { "15", OPTION_VAL_15_AST },
            { "16", OPTION_VAL_16_AST },
            { "17", OPTION_VAL_17_AST },
            { "7", OPTION_VAL_7_AST },
            { "26", OPTION_VAL_26_AST },
            { "8", OPTION_VAL_8_AST },
            { "11", OPTION_VAL_11_AST },
            { "20", OPTION_VAL_20_AST },
            { "3", OPTION_VAL_3_AST },
            { "13", OPTION_VAL_13_AST },
            { "27", OPTION_VAL_27_AST },
            { "10", OPTION_VAL_10_AST },
            { "19", OPTION_VAL_19_AST },
            { "30", OPTION_VAL_30_AST },
            { "28", OPTION_VAL_28_AST },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_AST,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_AST,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_AST,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_AST,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_AST },
            { "512", OPTION_VAL_512_AST },
            { "1024", OPTION_VAL_1024_AST },
            { "2048", OPTION_VAL_2048_AST },
            { "3072", OPTION_VAL_3072_AST },
            { "4096", OPTION_VAL_4096_AST },
            { "6144", OPTION_VAL_6144_AST },
            { "8192", OPTION_VAL_8192_AST },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_AST,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_AST,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_AST },
            { "circle", OPTION_VAL_CIRCLE_AST },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_AST,
        NULL,
        SHADPS4_NETWORK_INFO_0_AST,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_AST,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_AST,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_AST,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_AST,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_AST,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_AST,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_AST },
            { "1920x1080", OPTION_VAL_1920X1080_AST },
            { "2560x1440", OPTION_VAL_2560X1440_AST },
            { "3840x2160", OPTION_VAL_3840X2160_AST },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_AST,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_AST,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_AST },
            { "60",  OPTION_VAL_60_AST },
            { "90",  OPTION_VAL_90_AST },
            { "120", OPTION_VAL_120_AST },
            { "144", OPTION_VAL_144_AST },
            { "240", OPTION_VAL_240_AST },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_AST,
        NULL,
        SHADPS4_READBACKS_INFO_0_AST,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_AST },
            { "precise",  OPTION_VAL_PRECISE_AST },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_AST,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_AST,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_AST,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_AST,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_AST,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_AST,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_AST,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_AST,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_AST,
        NULL,
        SHADPS4_VOLUME_INFO_0_AST,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_AST },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_AST },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_AST },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_AST },
            { "40",  OPTION_VAL_40_AST },
            { "50",  OPTION_VAL_50_AST },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_AST },
            { "70",  OPTION_VAL_70_AST },
            { "80",  OPTION_VAL_80_AST },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_AST },
            { "100", OPTION_VAL_100_AST },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_AST },
            { "150", OPTION_VAL_150_AST },
            { "200", OPTION_VAL_200_AST },
            { "300", OPTION_VAL_300_AST },
            { "400", OPTION_VAL_400_AST },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_AST },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_AST,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_AST,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_AST,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_AST,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_AST },
            { "right", OPTION_VAL_RIGHT_AST },
            { "top", OPTION_VAL_TOP_AST },
            { "bottom", OPTION_VAL_BOTTOM_AST },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_AST,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_AST,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_AST },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_AST },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_AST },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_AST },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_AST },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_AST },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_AST },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_AST },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_AST },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_AST },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_AST,
        NULL,
        SHADPS4_LOG_INFO_0_AST,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_AST,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_AST,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_AST,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_AST,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_ast = {
   option_cats_ast,
   option_defs_ast
};

/* RETRO_LANGUAGE_BE */

#define CATEGORY_SYSTEM_LABEL_BE NULL
#define CATEGORY_SYSTEM_INFO_0_BE NULL
#define CATEGORY_VIDEO_LABEL_BE NULL
#define CATEGORY_VIDEO_INFO_0_BE NULL
#define CATEGORY_AUDIO_LABEL_BE NULL
#define CATEGORY_AUDIO_INFO_0_BE NULL
#define CATEGORY_TROPHIES_LABEL_BE NULL
#define CATEGORY_TROPHIES_INFO_0_BE NULL
#define CATEGORY_LOGGING_LABEL_BE NULL
#define CATEGORY_LOGGING_INFO_0_BE NULL
#define SHADPS4_NEO_MODE_LABEL_BE NULL
#define SHADPS4_NEO_MODE_INFO_0_BE NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_BE NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_BE NULL
#define OPTION_VAL_1_BE NULL
#define OPTION_VAL_18_BE NULL
#define OPTION_VAL_21_BE NULL
#define OPTION_VAL_23_BE NULL
#define OPTION_VAL_14_BE NULL
#define OPTION_VAL_6_BE NULL
#define OPTION_VAL_12_BE NULL
#define OPTION_VAL_22_BE NULL
#define OPTION_VAL_2_BE NULL
#define OPTION_VAL_4_BE NULL
#define OPTION_VAL_25_BE NULL
#define OPTION_VAL_24_BE NULL
#define OPTION_VAL_29_BE NULL
#define OPTION_VAL_5_BE NULL
#define OPTION_VAL_0_BE NULL
#define OPTION_VAL_9_BE NULL
#define OPTION_VAL_15_BE NULL
#define OPTION_VAL_16_BE NULL
#define OPTION_VAL_17_BE NULL
#define OPTION_VAL_7_BE NULL
#define OPTION_VAL_26_BE NULL
#define OPTION_VAL_8_BE NULL
#define OPTION_VAL_11_BE NULL
#define OPTION_VAL_20_BE NULL
#define OPTION_VAL_3_BE NULL
#define OPTION_VAL_13_BE NULL
#define OPTION_VAL_27_BE NULL
#define OPTION_VAL_10_BE NULL
#define OPTION_VAL_19_BE NULL
#define OPTION_VAL_30_BE NULL
#define OPTION_VAL_28_BE NULL
#define SHADPS4_DEV_KIT_LABEL_BE NULL
#define SHADPS4_DEV_KIT_INFO_0_BE NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_BE NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_BE NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_BE NULL
#define OPTION_VAL_512_BE NULL
#define OPTION_VAL_1024_BE NULL
#define OPTION_VAL_2048_BE NULL
#define OPTION_VAL_3072_BE NULL
#define OPTION_VAL_4096_BE NULL
#define OPTION_VAL_6144_BE NULL
#define OPTION_VAL_8192_BE NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_BE NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_BE NULL
#define OPTION_VAL_CROSS_BE NULL
#define OPTION_VAL_CIRCLE_BE NULL
#define SHADPS4_NETWORK_LABEL_BE NULL
#define SHADPS4_NETWORK_INFO_0_BE NULL
#define SHADPS4_CONTROLLERS_LABEL_BE NULL
#define SHADPS4_CONTROLLERS_INFO_0_BE NULL
#define SHADPS4_SHOW_SPLASH_LABEL_BE NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_BE NULL
#define SHADPS4_RESOLUTION_LABEL_BE NULL
#define SHADPS4_RESOLUTION_INFO_0_BE NULL
#define OPTION_VAL_1280X720_BE NULL
#define OPTION_VAL_1920X1080_BE NULL
#define OPTION_VAL_2560X1440_BE NULL
#define OPTION_VAL_3840X2160_BE NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_BE NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_BE NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_BE NULL
#define OPTION_VAL_60_BE NULL
#define OPTION_VAL_90_BE NULL
#define OPTION_VAL_120_BE NULL
#define OPTION_VAL_144_BE NULL
#define OPTION_VAL_240_BE NULL
#define SHADPS4_READBACKS_LABEL_BE NULL
#define SHADPS4_READBACKS_INFO_0_BE NULL
#define OPTION_VAL_RELAXED_BE NULL
#define OPTION_VAL_PRECISE_BE NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_BE NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_BE NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_BE NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_BE NULL
#define SHADPS4_SHADER_CACHE_LABEL_BE NULL
#define SHADPS4_SHADER_CACHE_INFO_0_BE NULL
#define SHADPS4_SHOW_FPS_LABEL_BE NULL
#define SHADPS4_SHOW_FPS_INFO_0_BE NULL
#define SHADPS4_VOLUME_LABEL_BE NULL
#define SHADPS4_VOLUME_INFO_0_BE NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_BE NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_BE NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_BE NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_BE NULL
#define OPTION_VAL_40_BE NULL
#define OPTION_VAL_50_BE NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_BE NULL
#define OPTION_VAL_70_BE NULL
#define OPTION_VAL_80_BE NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_BE NULL
#define OPTION_VAL_100_BE NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_BE NULL
#define OPTION_VAL_150_BE NULL
#define OPTION_VAL_200_BE NULL
#define OPTION_VAL_300_BE NULL
#define OPTION_VAL_400_BE NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_BE NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_BE NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_BE NULL
#define SHADPS4_TROPHY_POSITION_LABEL_BE NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_BE NULL
#define OPTION_VAL_LEFT_BE NULL
#define OPTION_VAL_RIGHT_BE NULL
#define OPTION_VAL_TOP_BE NULL
#define OPTION_VAL_BOTTOM_BE NULL
#define SHADPS4_TROPHY_DURATION_LABEL_BE NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_BE NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_BE NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_BE NULL
#define SHADPS4_LOG_INFO_0_BE NULL
#define SHADPS4_LOG_SEPARATE_LABEL_BE NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_BE NULL
#define SHADPS4_LOG_SYNC_LABEL_BE NULL
#define SHADPS4_LOG_SYNC_INFO_0_BE NULL

struct retro_core_option_v2_category option_cats_be[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_BE,
        CATEGORY_SYSTEM_INFO_0_BE
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_BE,
        CATEGORY_VIDEO_INFO_0_BE
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_BE,
        CATEGORY_AUDIO_INFO_0_BE
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_BE,
        CATEGORY_TROPHIES_INFO_0_BE
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_BE,
        CATEGORY_LOGGING_INFO_0_BE
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_be[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_BE,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_BE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_BE,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_BE,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_BE },
            { "18", OPTION_VAL_18_BE },
            { "21", OPTION_VAL_21_BE },
            { "23", OPTION_VAL_23_BE },
            { "14", OPTION_VAL_14_BE },
            { "6", OPTION_VAL_6_BE },
            { "12", OPTION_VAL_12_BE },
            { "22", OPTION_VAL_22_BE },
            { "2", OPTION_VAL_2_BE },
            { "4", OPTION_VAL_4_BE },
            { "25", OPTION_VAL_25_BE },
            { "24", OPTION_VAL_24_BE },
            { "29", OPTION_VAL_29_BE },
            { "5", OPTION_VAL_5_BE },
            { "0", OPTION_VAL_0_BE },
            { "9", OPTION_VAL_9_BE },
            { "15", OPTION_VAL_15_BE },
            { "16", OPTION_VAL_16_BE },
            { "17", OPTION_VAL_17_BE },
            { "7", OPTION_VAL_7_BE },
            { "26", OPTION_VAL_26_BE },
            { "8", OPTION_VAL_8_BE },
            { "11", OPTION_VAL_11_BE },
            { "20", OPTION_VAL_20_BE },
            { "3", OPTION_VAL_3_BE },
            { "13", OPTION_VAL_13_BE },
            { "27", OPTION_VAL_27_BE },
            { "10", OPTION_VAL_10_BE },
            { "19", OPTION_VAL_19_BE },
            { "30", OPTION_VAL_30_BE },
            { "28", OPTION_VAL_28_BE },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_BE,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_BE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_BE,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_BE,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_BE },
            { "512", OPTION_VAL_512_BE },
            { "1024", OPTION_VAL_1024_BE },
            { "2048", OPTION_VAL_2048_BE },
            { "3072", OPTION_VAL_3072_BE },
            { "4096", OPTION_VAL_4096_BE },
            { "6144", OPTION_VAL_6144_BE },
            { "8192", OPTION_VAL_8192_BE },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_BE,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_BE,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_BE },
            { "circle", OPTION_VAL_CIRCLE_BE },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_BE,
        NULL,
        SHADPS4_NETWORK_INFO_0_BE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_BE,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_BE,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_BE,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_BE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_BE,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_BE,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_BE },
            { "1920x1080", OPTION_VAL_1920X1080_BE },
            { "2560x1440", OPTION_VAL_2560X1440_BE },
            { "3840x2160", OPTION_VAL_3840X2160_BE },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_BE,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_BE,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_BE },
            { "60",  OPTION_VAL_60_BE },
            { "90",  OPTION_VAL_90_BE },
            { "120", OPTION_VAL_120_BE },
            { "144", OPTION_VAL_144_BE },
            { "240", OPTION_VAL_240_BE },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_BE,
        NULL,
        SHADPS4_READBACKS_INFO_0_BE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_BE },
            { "precise",  OPTION_VAL_PRECISE_BE },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_BE,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_BE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_BE,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_BE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_BE,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_BE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_BE,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_BE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_BE,
        NULL,
        SHADPS4_VOLUME_INFO_0_BE,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_BE },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_BE },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_BE },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_BE },
            { "40",  OPTION_VAL_40_BE },
            { "50",  OPTION_VAL_50_BE },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_BE },
            { "70",  OPTION_VAL_70_BE },
            { "80",  OPTION_VAL_80_BE },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_BE },
            { "100", OPTION_VAL_100_BE },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_BE },
            { "150", OPTION_VAL_150_BE },
            { "200", OPTION_VAL_200_BE },
            { "300", OPTION_VAL_300_BE },
            { "400", OPTION_VAL_400_BE },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_BE },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_BE,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_BE,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_BE,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_BE,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_BE },
            { "right", OPTION_VAL_RIGHT_BE },
            { "top", OPTION_VAL_TOP_BE },
            { "bottom", OPTION_VAL_BOTTOM_BE },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_BE,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_BE,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_BE },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_BE },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_BE },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_BE },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_BE },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_BE },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_BE },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_BE },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_BE },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_BE },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_BE,
        NULL,
        SHADPS4_LOG_INFO_0_BE,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_BE,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_BE,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_BE,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_BE,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_be = {
   option_cats_be,
   option_defs_be
};

/* RETRO_LANGUAGE_CA */

#define CATEGORY_SYSTEM_LABEL_CA NULL
#define CATEGORY_SYSTEM_INFO_0_CA NULL
#define CATEGORY_VIDEO_LABEL_CA NULL
#define CATEGORY_VIDEO_INFO_0_CA NULL
#define CATEGORY_AUDIO_LABEL_CA NULL
#define CATEGORY_AUDIO_INFO_0_CA NULL
#define CATEGORY_TROPHIES_LABEL_CA NULL
#define CATEGORY_TROPHIES_INFO_0_CA NULL
#define CATEGORY_LOGGING_LABEL_CA NULL
#define CATEGORY_LOGGING_INFO_0_CA NULL
#define SHADPS4_NEO_MODE_LABEL_CA NULL
#define SHADPS4_NEO_MODE_INFO_0_CA NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_CA NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_CA NULL
#define OPTION_VAL_1_CA NULL
#define OPTION_VAL_18_CA NULL
#define OPTION_VAL_21_CA NULL
#define OPTION_VAL_23_CA NULL
#define OPTION_VAL_14_CA NULL
#define OPTION_VAL_6_CA NULL
#define OPTION_VAL_12_CA NULL
#define OPTION_VAL_22_CA NULL
#define OPTION_VAL_2_CA NULL
#define OPTION_VAL_4_CA NULL
#define OPTION_VAL_25_CA NULL
#define OPTION_VAL_24_CA NULL
#define OPTION_VAL_29_CA NULL
#define OPTION_VAL_5_CA NULL
#define OPTION_VAL_0_CA NULL
#define OPTION_VAL_9_CA NULL
#define OPTION_VAL_15_CA NULL
#define OPTION_VAL_16_CA NULL
#define OPTION_VAL_17_CA NULL
#define OPTION_VAL_7_CA NULL
#define OPTION_VAL_26_CA NULL
#define OPTION_VAL_8_CA NULL
#define OPTION_VAL_11_CA NULL
#define OPTION_VAL_20_CA NULL
#define OPTION_VAL_3_CA NULL
#define OPTION_VAL_13_CA NULL
#define OPTION_VAL_27_CA NULL
#define OPTION_VAL_10_CA NULL
#define OPTION_VAL_19_CA NULL
#define OPTION_VAL_30_CA NULL
#define OPTION_VAL_28_CA NULL
#define SHADPS4_DEV_KIT_LABEL_CA NULL
#define SHADPS4_DEV_KIT_INFO_0_CA NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_CA NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_CA NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CA NULL
#define OPTION_VAL_512_CA NULL
#define OPTION_VAL_1024_CA NULL
#define OPTION_VAL_2048_CA NULL
#define OPTION_VAL_3072_CA NULL
#define OPTION_VAL_4096_CA NULL
#define OPTION_VAL_6144_CA NULL
#define OPTION_VAL_8192_CA NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_CA NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_CA NULL
#define OPTION_VAL_CROSS_CA NULL
#define OPTION_VAL_CIRCLE_CA NULL
#define SHADPS4_NETWORK_LABEL_CA NULL
#define SHADPS4_NETWORK_INFO_0_CA NULL
#define SHADPS4_CONTROLLERS_LABEL_CA NULL
#define SHADPS4_CONTROLLERS_INFO_0_CA NULL
#define SHADPS4_SHOW_SPLASH_LABEL_CA NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_CA NULL
#define SHADPS4_RESOLUTION_LABEL_CA NULL
#define SHADPS4_RESOLUTION_INFO_0_CA NULL
#define OPTION_VAL_1280X720_CA NULL
#define OPTION_VAL_1920X1080_CA NULL
#define OPTION_VAL_2560X1440_CA NULL
#define OPTION_VAL_3840X2160_CA NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_CA NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_CA NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CA NULL
#define OPTION_VAL_60_CA NULL
#define OPTION_VAL_90_CA NULL
#define OPTION_VAL_120_CA NULL
#define OPTION_VAL_144_CA NULL
#define OPTION_VAL_240_CA NULL
#define SHADPS4_READBACKS_LABEL_CA NULL
#define SHADPS4_READBACKS_INFO_0_CA NULL
#define OPTION_VAL_RELAXED_CA NULL
#define OPTION_VAL_PRECISE_CA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CA NULL
#define SHADPS4_SHADER_CACHE_LABEL_CA NULL
#define SHADPS4_SHADER_CACHE_INFO_0_CA NULL
#define SHADPS4_SHOW_FPS_LABEL_CA NULL
#define SHADPS4_SHOW_FPS_INFO_0_CA NULL
#define SHADPS4_VOLUME_LABEL_CA NULL
#define SHADPS4_VOLUME_INFO_0_CA NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_CA NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_CA NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_CA NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_CA NULL
#define OPTION_VAL_40_CA NULL
#define OPTION_VAL_50_CA NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_CA NULL
#define OPTION_VAL_70_CA NULL
#define OPTION_VAL_80_CA NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_CA NULL
#define OPTION_VAL_100_CA NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_CA NULL
#define OPTION_VAL_150_CA NULL
#define OPTION_VAL_200_CA NULL
#define OPTION_VAL_300_CA NULL
#define OPTION_VAL_400_CA NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_CA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CA NULL
#define SHADPS4_TROPHY_POSITION_LABEL_CA NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_CA NULL
#define OPTION_VAL_LEFT_CA NULL
#define OPTION_VAL_RIGHT_CA NULL
#define OPTION_VAL_TOP_CA NULL
#define OPTION_VAL_BOTTOM_CA NULL
#define SHADPS4_TROPHY_DURATION_LABEL_CA NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_CA NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CA NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CA NULL
#define SHADPS4_LOG_INFO_0_CA NULL
#define SHADPS4_LOG_SEPARATE_LABEL_CA NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_CA NULL
#define SHADPS4_LOG_SYNC_LABEL_CA NULL
#define SHADPS4_LOG_SYNC_INFO_0_CA NULL

struct retro_core_option_v2_category option_cats_ca[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_CA,
        CATEGORY_SYSTEM_INFO_0_CA
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_CA,
        CATEGORY_VIDEO_INFO_0_CA
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_CA,
        CATEGORY_AUDIO_INFO_0_CA
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_CA,
        CATEGORY_TROPHIES_INFO_0_CA
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_CA,
        CATEGORY_LOGGING_INFO_0_CA
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ca[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_CA,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_CA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_CA,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_CA,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_CA },
            { "18", OPTION_VAL_18_CA },
            { "21", OPTION_VAL_21_CA },
            { "23", OPTION_VAL_23_CA },
            { "14", OPTION_VAL_14_CA },
            { "6", OPTION_VAL_6_CA },
            { "12", OPTION_VAL_12_CA },
            { "22", OPTION_VAL_22_CA },
            { "2", OPTION_VAL_2_CA },
            { "4", OPTION_VAL_4_CA },
            { "25", OPTION_VAL_25_CA },
            { "24", OPTION_VAL_24_CA },
            { "29", OPTION_VAL_29_CA },
            { "5", OPTION_VAL_5_CA },
            { "0", OPTION_VAL_0_CA },
            { "9", OPTION_VAL_9_CA },
            { "15", OPTION_VAL_15_CA },
            { "16", OPTION_VAL_16_CA },
            { "17", OPTION_VAL_17_CA },
            { "7", OPTION_VAL_7_CA },
            { "26", OPTION_VAL_26_CA },
            { "8", OPTION_VAL_8_CA },
            { "11", OPTION_VAL_11_CA },
            { "20", OPTION_VAL_20_CA },
            { "3", OPTION_VAL_3_CA },
            { "13", OPTION_VAL_13_CA },
            { "27", OPTION_VAL_27_CA },
            { "10", OPTION_VAL_10_CA },
            { "19", OPTION_VAL_19_CA },
            { "30", OPTION_VAL_30_CA },
            { "28", OPTION_VAL_28_CA },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_CA,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_CA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_CA,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_CA,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CA },
            { "512", OPTION_VAL_512_CA },
            { "1024", OPTION_VAL_1024_CA },
            { "2048", OPTION_VAL_2048_CA },
            { "3072", OPTION_VAL_3072_CA },
            { "4096", OPTION_VAL_4096_CA },
            { "6144", OPTION_VAL_6144_CA },
            { "8192", OPTION_VAL_8192_CA },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_CA,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_CA,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_CA },
            { "circle", OPTION_VAL_CIRCLE_CA },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_CA,
        NULL,
        SHADPS4_NETWORK_INFO_0_CA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_CA,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_CA,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_CA,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_CA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_CA,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_CA,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_CA },
            { "1920x1080", OPTION_VAL_1920X1080_CA },
            { "2560x1440", OPTION_VAL_2560X1440_CA },
            { "3840x2160", OPTION_VAL_3840X2160_CA },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_CA,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_CA,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CA },
            { "60",  OPTION_VAL_60_CA },
            { "90",  OPTION_VAL_90_CA },
            { "120", OPTION_VAL_120_CA },
            { "144", OPTION_VAL_144_CA },
            { "240", OPTION_VAL_240_CA },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_CA,
        NULL,
        SHADPS4_READBACKS_INFO_0_CA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_CA },
            { "precise",  OPTION_VAL_PRECISE_CA },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CA,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CA,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_CA,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_CA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_CA,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_CA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_CA,
        NULL,
        SHADPS4_VOLUME_INFO_0_CA,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_CA },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_CA },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_CA },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_CA },
            { "40",  OPTION_VAL_40_CA },
            { "50",  OPTION_VAL_50_CA },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_CA },
            { "70",  OPTION_VAL_70_CA },
            { "80",  OPTION_VAL_80_CA },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_CA },
            { "100", OPTION_VAL_100_CA },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_CA },
            { "150", OPTION_VAL_150_CA },
            { "200", OPTION_VAL_200_CA },
            { "300", OPTION_VAL_300_CA },
            { "400", OPTION_VAL_400_CA },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_CA },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CA,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CA,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_CA,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_CA,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_CA },
            { "right", OPTION_VAL_RIGHT_CA },
            { "top", OPTION_VAL_TOP_CA },
            { "bottom", OPTION_VAL_BOTTOM_CA },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_CA,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_CA,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CA },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CA },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CA },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CA },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CA },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CA },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CA },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CA },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CA },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CA },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_CA,
        NULL,
        SHADPS4_LOG_INFO_0_CA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_CA,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_CA,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_CA,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_CA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_ca = {
   option_cats_ca,
   option_defs_ca
};

/* RETRO_LANGUAGE_CHS */

#define CATEGORY_SYSTEM_LABEL_CHS NULL
#define CATEGORY_SYSTEM_INFO_0_CHS NULL
#define CATEGORY_VIDEO_LABEL_CHS NULL
#define CATEGORY_VIDEO_INFO_0_CHS NULL
#define CATEGORY_AUDIO_LABEL_CHS NULL
#define CATEGORY_AUDIO_INFO_0_CHS NULL
#define CATEGORY_TROPHIES_LABEL_CHS NULL
#define CATEGORY_TROPHIES_INFO_0_CHS NULL
#define CATEGORY_LOGGING_LABEL_CHS NULL
#define CATEGORY_LOGGING_INFO_0_CHS NULL
#define SHADPS4_NEO_MODE_LABEL_CHS NULL
#define SHADPS4_NEO_MODE_INFO_0_CHS NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_CHS NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_CHS NULL
#define OPTION_VAL_1_CHS NULL
#define OPTION_VAL_18_CHS NULL
#define OPTION_VAL_21_CHS NULL
#define OPTION_VAL_23_CHS NULL
#define OPTION_VAL_14_CHS NULL
#define OPTION_VAL_6_CHS NULL
#define OPTION_VAL_12_CHS NULL
#define OPTION_VAL_22_CHS NULL
#define OPTION_VAL_2_CHS NULL
#define OPTION_VAL_4_CHS NULL
#define OPTION_VAL_25_CHS NULL
#define OPTION_VAL_24_CHS NULL
#define OPTION_VAL_29_CHS NULL
#define OPTION_VAL_5_CHS NULL
#define OPTION_VAL_0_CHS NULL
#define OPTION_VAL_9_CHS NULL
#define OPTION_VAL_15_CHS NULL
#define OPTION_VAL_16_CHS NULL
#define OPTION_VAL_17_CHS NULL
#define OPTION_VAL_7_CHS NULL
#define OPTION_VAL_26_CHS NULL
#define OPTION_VAL_8_CHS NULL
#define OPTION_VAL_11_CHS NULL
#define OPTION_VAL_20_CHS NULL
#define OPTION_VAL_3_CHS NULL
#define OPTION_VAL_13_CHS NULL
#define OPTION_VAL_27_CHS NULL
#define OPTION_VAL_10_CHS NULL
#define OPTION_VAL_19_CHS NULL
#define OPTION_VAL_30_CHS NULL
#define OPTION_VAL_28_CHS NULL
#define SHADPS4_DEV_KIT_LABEL_CHS NULL
#define SHADPS4_DEV_KIT_INFO_0_CHS NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_CHS NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_CHS NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CHS NULL
#define OPTION_VAL_512_CHS NULL
#define OPTION_VAL_1024_CHS NULL
#define OPTION_VAL_2048_CHS NULL
#define OPTION_VAL_3072_CHS NULL
#define OPTION_VAL_4096_CHS NULL
#define OPTION_VAL_6144_CHS NULL
#define OPTION_VAL_8192_CHS NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_CHS NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_CHS NULL
#define OPTION_VAL_CROSS_CHS NULL
#define OPTION_VAL_CIRCLE_CHS NULL
#define SHADPS4_NETWORK_LABEL_CHS NULL
#define SHADPS4_NETWORK_INFO_0_CHS NULL
#define SHADPS4_CONTROLLERS_LABEL_CHS NULL
#define SHADPS4_CONTROLLERS_INFO_0_CHS NULL
#define SHADPS4_SHOW_SPLASH_LABEL_CHS NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_CHS NULL
#define SHADPS4_RESOLUTION_LABEL_CHS NULL
#define SHADPS4_RESOLUTION_INFO_0_CHS NULL
#define OPTION_VAL_1280X720_CHS NULL
#define OPTION_VAL_1920X1080_CHS NULL
#define OPTION_VAL_2560X1440_CHS NULL
#define OPTION_VAL_3840X2160_CHS NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_CHS NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_CHS NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CHS NULL
#define OPTION_VAL_60_CHS NULL
#define OPTION_VAL_90_CHS NULL
#define OPTION_VAL_120_CHS NULL
#define OPTION_VAL_144_CHS NULL
#define OPTION_VAL_240_CHS NULL
#define SHADPS4_READBACKS_LABEL_CHS NULL
#define SHADPS4_READBACKS_INFO_0_CHS NULL
#define OPTION_VAL_RELAXED_CHS NULL
#define OPTION_VAL_PRECISE_CHS NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CHS NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CHS NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CHS NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CHS NULL
#define SHADPS4_SHADER_CACHE_LABEL_CHS NULL
#define SHADPS4_SHADER_CACHE_INFO_0_CHS NULL
#define SHADPS4_SHOW_FPS_LABEL_CHS NULL
#define SHADPS4_SHOW_FPS_INFO_0_CHS NULL
#define SHADPS4_VOLUME_LABEL_CHS NULL
#define SHADPS4_VOLUME_INFO_0_CHS NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_CHS NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_CHS NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_CHS NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_CHS NULL
#define OPTION_VAL_40_CHS NULL
#define OPTION_VAL_50_CHS NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_CHS NULL
#define OPTION_VAL_70_CHS NULL
#define OPTION_VAL_80_CHS NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_CHS NULL
#define OPTION_VAL_100_CHS NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_CHS NULL
#define OPTION_VAL_150_CHS NULL
#define OPTION_VAL_200_CHS NULL
#define OPTION_VAL_300_CHS NULL
#define OPTION_VAL_400_CHS NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_CHS NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CHS NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CHS NULL
#define SHADPS4_TROPHY_POSITION_LABEL_CHS NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_CHS NULL
#define OPTION_VAL_LEFT_CHS NULL
#define OPTION_VAL_RIGHT_CHS NULL
#define OPTION_VAL_TOP_CHS NULL
#define OPTION_VAL_BOTTOM_CHS NULL
#define SHADPS4_TROPHY_DURATION_LABEL_CHS NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_CHS NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CHS NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CHS NULL
#define SHADPS4_LOG_INFO_0_CHS NULL
#define SHADPS4_LOG_SEPARATE_LABEL_CHS NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_CHS NULL
#define SHADPS4_LOG_SYNC_LABEL_CHS NULL
#define SHADPS4_LOG_SYNC_INFO_0_CHS NULL

struct retro_core_option_v2_category option_cats_chs[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_CHS,
        CATEGORY_SYSTEM_INFO_0_CHS
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_CHS,
        CATEGORY_VIDEO_INFO_0_CHS
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_CHS,
        CATEGORY_AUDIO_INFO_0_CHS
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_CHS,
        CATEGORY_TROPHIES_INFO_0_CHS
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_CHS,
        CATEGORY_LOGGING_INFO_0_CHS
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_chs[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_CHS,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_CHS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_CHS,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_CHS,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_CHS },
            { "18", OPTION_VAL_18_CHS },
            { "21", OPTION_VAL_21_CHS },
            { "23", OPTION_VAL_23_CHS },
            { "14", OPTION_VAL_14_CHS },
            { "6", OPTION_VAL_6_CHS },
            { "12", OPTION_VAL_12_CHS },
            { "22", OPTION_VAL_22_CHS },
            { "2", OPTION_VAL_2_CHS },
            { "4", OPTION_VAL_4_CHS },
            { "25", OPTION_VAL_25_CHS },
            { "24", OPTION_VAL_24_CHS },
            { "29", OPTION_VAL_29_CHS },
            { "5", OPTION_VAL_5_CHS },
            { "0", OPTION_VAL_0_CHS },
            { "9", OPTION_VAL_9_CHS },
            { "15", OPTION_VAL_15_CHS },
            { "16", OPTION_VAL_16_CHS },
            { "17", OPTION_VAL_17_CHS },
            { "7", OPTION_VAL_7_CHS },
            { "26", OPTION_VAL_26_CHS },
            { "8", OPTION_VAL_8_CHS },
            { "11", OPTION_VAL_11_CHS },
            { "20", OPTION_VAL_20_CHS },
            { "3", OPTION_VAL_3_CHS },
            { "13", OPTION_VAL_13_CHS },
            { "27", OPTION_VAL_27_CHS },
            { "10", OPTION_VAL_10_CHS },
            { "19", OPTION_VAL_19_CHS },
            { "30", OPTION_VAL_30_CHS },
            { "28", OPTION_VAL_28_CHS },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_CHS,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_CHS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_CHS,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_CHS,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CHS },
            { "512", OPTION_VAL_512_CHS },
            { "1024", OPTION_VAL_1024_CHS },
            { "2048", OPTION_VAL_2048_CHS },
            { "3072", OPTION_VAL_3072_CHS },
            { "4096", OPTION_VAL_4096_CHS },
            { "6144", OPTION_VAL_6144_CHS },
            { "8192", OPTION_VAL_8192_CHS },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_CHS,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_CHS,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_CHS },
            { "circle", OPTION_VAL_CIRCLE_CHS },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_CHS,
        NULL,
        SHADPS4_NETWORK_INFO_0_CHS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_CHS,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_CHS,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_CHS,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_CHS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_CHS,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_CHS,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_CHS },
            { "1920x1080", OPTION_VAL_1920X1080_CHS },
            { "2560x1440", OPTION_VAL_2560X1440_CHS },
            { "3840x2160", OPTION_VAL_3840X2160_CHS },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_CHS,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_CHS,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CHS },
            { "60",  OPTION_VAL_60_CHS },
            { "90",  OPTION_VAL_90_CHS },
            { "120", OPTION_VAL_120_CHS },
            { "144", OPTION_VAL_144_CHS },
            { "240", OPTION_VAL_240_CHS },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_CHS,
        NULL,
        SHADPS4_READBACKS_INFO_0_CHS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_CHS },
            { "precise",  OPTION_VAL_PRECISE_CHS },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CHS,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CHS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CHS,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CHS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_CHS,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_CHS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_CHS,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_CHS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_CHS,
        NULL,
        SHADPS4_VOLUME_INFO_0_CHS,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_CHS },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_CHS },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_CHS },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_CHS },
            { "40",  OPTION_VAL_40_CHS },
            { "50",  OPTION_VAL_50_CHS },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_CHS },
            { "70",  OPTION_VAL_70_CHS },
            { "80",  OPTION_VAL_80_CHS },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_CHS },
            { "100", OPTION_VAL_100_CHS },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_CHS },
            { "150", OPTION_VAL_150_CHS },
            { "200", OPTION_VAL_200_CHS },
            { "300", OPTION_VAL_300_CHS },
            { "400", OPTION_VAL_400_CHS },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_CHS },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CHS,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CHS,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_CHS,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_CHS,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_CHS },
            { "right", OPTION_VAL_RIGHT_CHS },
            { "top", OPTION_VAL_TOP_CHS },
            { "bottom", OPTION_VAL_BOTTOM_CHS },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_CHS,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_CHS,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CHS },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CHS },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CHS },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CHS },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CHS },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CHS },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CHS },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CHS },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CHS },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CHS },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_CHS,
        NULL,
        SHADPS4_LOG_INFO_0_CHS,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_CHS,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_CHS,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_CHS,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_CHS,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_chs = {
   option_cats_chs,
   option_defs_chs
};

/* RETRO_LANGUAGE_CHT */

#define CATEGORY_SYSTEM_LABEL_CHT NULL
#define CATEGORY_SYSTEM_INFO_0_CHT NULL
#define CATEGORY_VIDEO_LABEL_CHT NULL
#define CATEGORY_VIDEO_INFO_0_CHT NULL
#define CATEGORY_AUDIO_LABEL_CHT NULL
#define CATEGORY_AUDIO_INFO_0_CHT NULL
#define CATEGORY_TROPHIES_LABEL_CHT NULL
#define CATEGORY_TROPHIES_INFO_0_CHT NULL
#define CATEGORY_LOGGING_LABEL_CHT NULL
#define CATEGORY_LOGGING_INFO_0_CHT NULL
#define SHADPS4_NEO_MODE_LABEL_CHT NULL
#define SHADPS4_NEO_MODE_INFO_0_CHT NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_CHT NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_CHT NULL
#define OPTION_VAL_1_CHT NULL
#define OPTION_VAL_18_CHT NULL
#define OPTION_VAL_21_CHT NULL
#define OPTION_VAL_23_CHT NULL
#define OPTION_VAL_14_CHT NULL
#define OPTION_VAL_6_CHT NULL
#define OPTION_VAL_12_CHT NULL
#define OPTION_VAL_22_CHT NULL
#define OPTION_VAL_2_CHT NULL
#define OPTION_VAL_4_CHT NULL
#define OPTION_VAL_25_CHT NULL
#define OPTION_VAL_24_CHT NULL
#define OPTION_VAL_29_CHT NULL
#define OPTION_VAL_5_CHT NULL
#define OPTION_VAL_0_CHT NULL
#define OPTION_VAL_9_CHT NULL
#define OPTION_VAL_15_CHT NULL
#define OPTION_VAL_16_CHT NULL
#define OPTION_VAL_17_CHT NULL
#define OPTION_VAL_7_CHT NULL
#define OPTION_VAL_26_CHT NULL
#define OPTION_VAL_8_CHT NULL
#define OPTION_VAL_11_CHT NULL
#define OPTION_VAL_20_CHT NULL
#define OPTION_VAL_3_CHT NULL
#define OPTION_VAL_13_CHT NULL
#define OPTION_VAL_27_CHT NULL
#define OPTION_VAL_10_CHT NULL
#define OPTION_VAL_19_CHT NULL
#define OPTION_VAL_30_CHT NULL
#define OPTION_VAL_28_CHT NULL
#define SHADPS4_DEV_KIT_LABEL_CHT NULL
#define SHADPS4_DEV_KIT_INFO_0_CHT NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_CHT NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_CHT NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CHT NULL
#define OPTION_VAL_512_CHT NULL
#define OPTION_VAL_1024_CHT NULL
#define OPTION_VAL_2048_CHT NULL
#define OPTION_VAL_3072_CHT NULL
#define OPTION_VAL_4096_CHT NULL
#define OPTION_VAL_6144_CHT NULL
#define OPTION_VAL_8192_CHT NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_CHT NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_CHT NULL
#define OPTION_VAL_CROSS_CHT NULL
#define OPTION_VAL_CIRCLE_CHT NULL
#define SHADPS4_NETWORK_LABEL_CHT NULL
#define SHADPS4_NETWORK_INFO_0_CHT NULL
#define SHADPS4_CONTROLLERS_LABEL_CHT NULL
#define SHADPS4_CONTROLLERS_INFO_0_CHT NULL
#define SHADPS4_SHOW_SPLASH_LABEL_CHT NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_CHT NULL
#define SHADPS4_RESOLUTION_LABEL_CHT NULL
#define SHADPS4_RESOLUTION_INFO_0_CHT NULL
#define OPTION_VAL_1280X720_CHT NULL
#define OPTION_VAL_1920X1080_CHT NULL
#define OPTION_VAL_2560X1440_CHT NULL
#define OPTION_VAL_3840X2160_CHT NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_CHT NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_CHT NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CHT NULL
#define OPTION_VAL_60_CHT NULL
#define OPTION_VAL_90_CHT NULL
#define OPTION_VAL_120_CHT NULL
#define OPTION_VAL_144_CHT NULL
#define OPTION_VAL_240_CHT NULL
#define SHADPS4_READBACKS_LABEL_CHT NULL
#define SHADPS4_READBACKS_INFO_0_CHT NULL
#define OPTION_VAL_RELAXED_CHT NULL
#define OPTION_VAL_PRECISE_CHT NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CHT NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CHT NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CHT NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CHT NULL
#define SHADPS4_SHADER_CACHE_LABEL_CHT NULL
#define SHADPS4_SHADER_CACHE_INFO_0_CHT NULL
#define SHADPS4_SHOW_FPS_LABEL_CHT NULL
#define SHADPS4_SHOW_FPS_INFO_0_CHT NULL
#define SHADPS4_VOLUME_LABEL_CHT NULL
#define SHADPS4_VOLUME_INFO_0_CHT NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_CHT NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_CHT NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_CHT NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_CHT NULL
#define OPTION_VAL_40_CHT NULL
#define OPTION_VAL_50_CHT NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_CHT NULL
#define OPTION_VAL_70_CHT NULL
#define OPTION_VAL_80_CHT NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_CHT NULL
#define OPTION_VAL_100_CHT NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_CHT NULL
#define OPTION_VAL_150_CHT NULL
#define OPTION_VAL_200_CHT NULL
#define OPTION_VAL_300_CHT NULL
#define OPTION_VAL_400_CHT NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_CHT NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CHT NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CHT NULL
#define SHADPS4_TROPHY_POSITION_LABEL_CHT NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_CHT NULL
#define OPTION_VAL_LEFT_CHT NULL
#define OPTION_VAL_RIGHT_CHT NULL
#define OPTION_VAL_TOP_CHT NULL
#define OPTION_VAL_BOTTOM_CHT NULL
#define SHADPS4_TROPHY_DURATION_LABEL_CHT NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_CHT NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CHT NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CHT NULL
#define SHADPS4_LOG_INFO_0_CHT NULL
#define SHADPS4_LOG_SEPARATE_LABEL_CHT NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_CHT NULL
#define SHADPS4_LOG_SYNC_LABEL_CHT NULL
#define SHADPS4_LOG_SYNC_INFO_0_CHT NULL

struct retro_core_option_v2_category option_cats_cht[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_CHT,
        CATEGORY_SYSTEM_INFO_0_CHT
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_CHT,
        CATEGORY_VIDEO_INFO_0_CHT
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_CHT,
        CATEGORY_AUDIO_INFO_0_CHT
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_CHT,
        CATEGORY_TROPHIES_INFO_0_CHT
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_CHT,
        CATEGORY_LOGGING_INFO_0_CHT
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cht[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_CHT,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_CHT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_CHT,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_CHT,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_CHT },
            { "18", OPTION_VAL_18_CHT },
            { "21", OPTION_VAL_21_CHT },
            { "23", OPTION_VAL_23_CHT },
            { "14", OPTION_VAL_14_CHT },
            { "6", OPTION_VAL_6_CHT },
            { "12", OPTION_VAL_12_CHT },
            { "22", OPTION_VAL_22_CHT },
            { "2", OPTION_VAL_2_CHT },
            { "4", OPTION_VAL_4_CHT },
            { "25", OPTION_VAL_25_CHT },
            { "24", OPTION_VAL_24_CHT },
            { "29", OPTION_VAL_29_CHT },
            { "5", OPTION_VAL_5_CHT },
            { "0", OPTION_VAL_0_CHT },
            { "9", OPTION_VAL_9_CHT },
            { "15", OPTION_VAL_15_CHT },
            { "16", OPTION_VAL_16_CHT },
            { "17", OPTION_VAL_17_CHT },
            { "7", OPTION_VAL_7_CHT },
            { "26", OPTION_VAL_26_CHT },
            { "8", OPTION_VAL_8_CHT },
            { "11", OPTION_VAL_11_CHT },
            { "20", OPTION_VAL_20_CHT },
            { "3", OPTION_VAL_3_CHT },
            { "13", OPTION_VAL_13_CHT },
            { "27", OPTION_VAL_27_CHT },
            { "10", OPTION_VAL_10_CHT },
            { "19", OPTION_VAL_19_CHT },
            { "30", OPTION_VAL_30_CHT },
            { "28", OPTION_VAL_28_CHT },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_CHT,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_CHT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_CHT,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_CHT,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CHT },
            { "512", OPTION_VAL_512_CHT },
            { "1024", OPTION_VAL_1024_CHT },
            { "2048", OPTION_VAL_2048_CHT },
            { "3072", OPTION_VAL_3072_CHT },
            { "4096", OPTION_VAL_4096_CHT },
            { "6144", OPTION_VAL_6144_CHT },
            { "8192", OPTION_VAL_8192_CHT },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_CHT,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_CHT,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_CHT },
            { "circle", OPTION_VAL_CIRCLE_CHT },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_CHT,
        NULL,
        SHADPS4_NETWORK_INFO_0_CHT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_CHT,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_CHT,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_CHT,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_CHT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_CHT,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_CHT,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_CHT },
            { "1920x1080", OPTION_VAL_1920X1080_CHT },
            { "2560x1440", OPTION_VAL_2560X1440_CHT },
            { "3840x2160", OPTION_VAL_3840X2160_CHT },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_CHT,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_CHT,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CHT },
            { "60",  OPTION_VAL_60_CHT },
            { "90",  OPTION_VAL_90_CHT },
            { "120", OPTION_VAL_120_CHT },
            { "144", OPTION_VAL_144_CHT },
            { "240", OPTION_VAL_240_CHT },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_CHT,
        NULL,
        SHADPS4_READBACKS_INFO_0_CHT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_CHT },
            { "precise",  OPTION_VAL_PRECISE_CHT },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CHT,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CHT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CHT,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CHT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_CHT,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_CHT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_CHT,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_CHT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_CHT,
        NULL,
        SHADPS4_VOLUME_INFO_0_CHT,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_CHT },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_CHT },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_CHT },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_CHT },
            { "40",  OPTION_VAL_40_CHT },
            { "50",  OPTION_VAL_50_CHT },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_CHT },
            { "70",  OPTION_VAL_70_CHT },
            { "80",  OPTION_VAL_80_CHT },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_CHT },
            { "100", OPTION_VAL_100_CHT },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_CHT },
            { "150", OPTION_VAL_150_CHT },
            { "200", OPTION_VAL_200_CHT },
            { "300", OPTION_VAL_300_CHT },
            { "400", OPTION_VAL_400_CHT },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_CHT },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CHT,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CHT,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_CHT,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_CHT,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_CHT },
            { "right", OPTION_VAL_RIGHT_CHT },
            { "top", OPTION_VAL_TOP_CHT },
            { "bottom", OPTION_VAL_BOTTOM_CHT },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_CHT,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_CHT,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CHT },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CHT },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CHT },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CHT },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CHT },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CHT },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CHT },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CHT },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CHT },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CHT },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_CHT,
        NULL,
        SHADPS4_LOG_INFO_0_CHT,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_CHT,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_CHT,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_CHT,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_CHT,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_cht = {
   option_cats_cht,
   option_defs_cht
};

/* RETRO_LANGUAGE_CS */

#define CATEGORY_SYSTEM_LABEL_CS NULL
#define CATEGORY_SYSTEM_INFO_0_CS NULL
#define CATEGORY_VIDEO_LABEL_CS NULL
#define CATEGORY_VIDEO_INFO_0_CS NULL
#define CATEGORY_AUDIO_LABEL_CS NULL
#define CATEGORY_AUDIO_INFO_0_CS NULL
#define CATEGORY_TROPHIES_LABEL_CS NULL
#define CATEGORY_TROPHIES_INFO_0_CS NULL
#define CATEGORY_LOGGING_LABEL_CS NULL
#define CATEGORY_LOGGING_INFO_0_CS NULL
#define SHADPS4_NEO_MODE_LABEL_CS NULL
#define SHADPS4_NEO_MODE_INFO_0_CS NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_CS NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_CS NULL
#define OPTION_VAL_1_CS NULL
#define OPTION_VAL_18_CS NULL
#define OPTION_VAL_21_CS NULL
#define OPTION_VAL_23_CS NULL
#define OPTION_VAL_14_CS NULL
#define OPTION_VAL_6_CS NULL
#define OPTION_VAL_12_CS NULL
#define OPTION_VAL_22_CS NULL
#define OPTION_VAL_2_CS NULL
#define OPTION_VAL_4_CS NULL
#define OPTION_VAL_25_CS NULL
#define OPTION_VAL_24_CS NULL
#define OPTION_VAL_29_CS NULL
#define OPTION_VAL_5_CS NULL
#define OPTION_VAL_0_CS NULL
#define OPTION_VAL_9_CS NULL
#define OPTION_VAL_15_CS NULL
#define OPTION_VAL_16_CS NULL
#define OPTION_VAL_17_CS NULL
#define OPTION_VAL_7_CS NULL
#define OPTION_VAL_26_CS NULL
#define OPTION_VAL_8_CS NULL
#define OPTION_VAL_11_CS NULL
#define OPTION_VAL_20_CS NULL
#define OPTION_VAL_3_CS NULL
#define OPTION_VAL_13_CS NULL
#define OPTION_VAL_27_CS NULL
#define OPTION_VAL_10_CS NULL
#define OPTION_VAL_19_CS NULL
#define OPTION_VAL_30_CS NULL
#define OPTION_VAL_28_CS NULL
#define SHADPS4_DEV_KIT_LABEL_CS NULL
#define SHADPS4_DEV_KIT_INFO_0_CS NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_CS NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_CS NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CS NULL
#define OPTION_VAL_512_CS NULL
#define OPTION_VAL_1024_CS NULL
#define OPTION_VAL_2048_CS NULL
#define OPTION_VAL_3072_CS NULL
#define OPTION_VAL_4096_CS NULL
#define OPTION_VAL_6144_CS NULL
#define OPTION_VAL_8192_CS NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_CS NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_CS NULL
#define OPTION_VAL_CROSS_CS NULL
#define OPTION_VAL_CIRCLE_CS NULL
#define SHADPS4_NETWORK_LABEL_CS NULL
#define SHADPS4_NETWORK_INFO_0_CS NULL
#define SHADPS4_CONTROLLERS_LABEL_CS NULL
#define SHADPS4_CONTROLLERS_INFO_0_CS NULL
#define SHADPS4_SHOW_SPLASH_LABEL_CS NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_CS NULL
#define SHADPS4_RESOLUTION_LABEL_CS NULL
#define SHADPS4_RESOLUTION_INFO_0_CS NULL
#define OPTION_VAL_1280X720_CS NULL
#define OPTION_VAL_1920X1080_CS NULL
#define OPTION_VAL_2560X1440_CS NULL
#define OPTION_VAL_3840X2160_CS NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_CS NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_CS NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CS NULL
#define OPTION_VAL_60_CS NULL
#define OPTION_VAL_90_CS NULL
#define OPTION_VAL_120_CS NULL
#define OPTION_VAL_144_CS NULL
#define OPTION_VAL_240_CS NULL
#define SHADPS4_READBACKS_LABEL_CS NULL
#define SHADPS4_READBACKS_INFO_0_CS NULL
#define OPTION_VAL_RELAXED_CS NULL
#define OPTION_VAL_PRECISE_CS NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CS NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CS NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CS NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CS NULL
#define SHADPS4_SHADER_CACHE_LABEL_CS NULL
#define SHADPS4_SHADER_CACHE_INFO_0_CS NULL
#define SHADPS4_SHOW_FPS_LABEL_CS NULL
#define SHADPS4_SHOW_FPS_INFO_0_CS NULL
#define SHADPS4_VOLUME_LABEL_CS NULL
#define SHADPS4_VOLUME_INFO_0_CS NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_CS NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_CS NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_CS NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_CS NULL
#define OPTION_VAL_40_CS NULL
#define OPTION_VAL_50_CS NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_CS NULL
#define OPTION_VAL_70_CS NULL
#define OPTION_VAL_80_CS NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_CS NULL
#define OPTION_VAL_100_CS NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_CS NULL
#define OPTION_VAL_150_CS NULL
#define OPTION_VAL_200_CS NULL
#define OPTION_VAL_300_CS NULL
#define OPTION_VAL_400_CS NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_CS NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CS NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CS NULL
#define SHADPS4_TROPHY_POSITION_LABEL_CS NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_CS NULL
#define OPTION_VAL_LEFT_CS NULL
#define OPTION_VAL_RIGHT_CS NULL
#define OPTION_VAL_TOP_CS NULL
#define OPTION_VAL_BOTTOM_CS NULL
#define SHADPS4_TROPHY_DURATION_LABEL_CS NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_CS NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CS NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CS NULL
#define SHADPS4_LOG_INFO_0_CS NULL
#define SHADPS4_LOG_SEPARATE_LABEL_CS NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_CS NULL
#define SHADPS4_LOG_SYNC_LABEL_CS NULL
#define SHADPS4_LOG_SYNC_INFO_0_CS NULL

struct retro_core_option_v2_category option_cats_cs[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_CS,
        CATEGORY_SYSTEM_INFO_0_CS
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_CS,
        CATEGORY_VIDEO_INFO_0_CS
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_CS,
        CATEGORY_AUDIO_INFO_0_CS
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_CS,
        CATEGORY_TROPHIES_INFO_0_CS
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_CS,
        CATEGORY_LOGGING_INFO_0_CS
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_cs[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_CS,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_CS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_CS,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_CS,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_CS },
            { "18", OPTION_VAL_18_CS },
            { "21", OPTION_VAL_21_CS },
            { "23", OPTION_VAL_23_CS },
            { "14", OPTION_VAL_14_CS },
            { "6", OPTION_VAL_6_CS },
            { "12", OPTION_VAL_12_CS },
            { "22", OPTION_VAL_22_CS },
            { "2", OPTION_VAL_2_CS },
            { "4", OPTION_VAL_4_CS },
            { "25", OPTION_VAL_25_CS },
            { "24", OPTION_VAL_24_CS },
            { "29", OPTION_VAL_29_CS },
            { "5", OPTION_VAL_5_CS },
            { "0", OPTION_VAL_0_CS },
            { "9", OPTION_VAL_9_CS },
            { "15", OPTION_VAL_15_CS },
            { "16", OPTION_VAL_16_CS },
            { "17", OPTION_VAL_17_CS },
            { "7", OPTION_VAL_7_CS },
            { "26", OPTION_VAL_26_CS },
            { "8", OPTION_VAL_8_CS },
            { "11", OPTION_VAL_11_CS },
            { "20", OPTION_VAL_20_CS },
            { "3", OPTION_VAL_3_CS },
            { "13", OPTION_VAL_13_CS },
            { "27", OPTION_VAL_27_CS },
            { "10", OPTION_VAL_10_CS },
            { "19", OPTION_VAL_19_CS },
            { "30", OPTION_VAL_30_CS },
            { "28", OPTION_VAL_28_CS },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_CS,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_CS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_CS,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_CS,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_CS },
            { "512", OPTION_VAL_512_CS },
            { "1024", OPTION_VAL_1024_CS },
            { "2048", OPTION_VAL_2048_CS },
            { "3072", OPTION_VAL_3072_CS },
            { "4096", OPTION_VAL_4096_CS },
            { "6144", OPTION_VAL_6144_CS },
            { "8192", OPTION_VAL_8192_CS },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_CS,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_CS,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_CS },
            { "circle", OPTION_VAL_CIRCLE_CS },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_CS,
        NULL,
        SHADPS4_NETWORK_INFO_0_CS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_CS,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_CS,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_CS,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_CS,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_CS,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_CS,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_CS },
            { "1920x1080", OPTION_VAL_1920X1080_CS },
            { "2560x1440", OPTION_VAL_2560X1440_CS },
            { "3840x2160", OPTION_VAL_3840X2160_CS },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_CS,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_CS,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_CS },
            { "60",  OPTION_VAL_60_CS },
            { "90",  OPTION_VAL_90_CS },
            { "120", OPTION_VAL_120_CS },
            { "144", OPTION_VAL_144_CS },
            { "240", OPTION_VAL_240_CS },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_CS,
        NULL,
        SHADPS4_READBACKS_INFO_0_CS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_CS },
            { "precise",  OPTION_VAL_PRECISE_CS },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_CS,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_CS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_CS,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_CS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_CS,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_CS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_CS,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_CS,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_CS,
        NULL,
        SHADPS4_VOLUME_INFO_0_CS,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_CS },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_CS },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_CS },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_CS },
            { "40",  OPTION_VAL_40_CS },
            { "50",  OPTION_VAL_50_CS },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_CS },
            { "70",  OPTION_VAL_70_CS },
            { "80",  OPTION_VAL_80_CS },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_CS },
            { "100", OPTION_VAL_100_CS },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_CS },
            { "150", OPTION_VAL_150_CS },
            { "200", OPTION_VAL_200_CS },
            { "300", OPTION_VAL_300_CS },
            { "400", OPTION_VAL_400_CS },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_CS },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_CS,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_CS,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_CS,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_CS,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_CS },
            { "right", OPTION_VAL_RIGHT_CS },
            { "top", OPTION_VAL_TOP_CS },
            { "bottom", OPTION_VAL_BOTTOM_CS },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_CS,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_CS,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_CS },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_CS },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_CS },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_CS },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_CS },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_CS },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_CS },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_CS },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_CS },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_CS },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_CS,
        NULL,
        SHADPS4_LOG_INFO_0_CS,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_CS,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_CS,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_CS,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_CS,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_cs = {
   option_cats_cs,
   option_defs_cs
};

/* RETRO_LANGUAGE_DE */

#define CATEGORY_SYSTEM_LABEL_DE NULL
#define CATEGORY_SYSTEM_INFO_0_DE NULL
#define CATEGORY_VIDEO_LABEL_DE NULL
#define CATEGORY_VIDEO_INFO_0_DE NULL
#define CATEGORY_AUDIO_LABEL_DE NULL
#define CATEGORY_AUDIO_INFO_0_DE NULL
#define CATEGORY_TROPHIES_LABEL_DE NULL
#define CATEGORY_TROPHIES_INFO_0_DE NULL
#define CATEGORY_LOGGING_LABEL_DE NULL
#define CATEGORY_LOGGING_INFO_0_DE NULL
#define SHADPS4_NEO_MODE_LABEL_DE NULL
#define SHADPS4_NEO_MODE_INFO_0_DE NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_DE NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_DE NULL
#define OPTION_VAL_1_DE NULL
#define OPTION_VAL_18_DE NULL
#define OPTION_VAL_21_DE NULL
#define OPTION_VAL_23_DE NULL
#define OPTION_VAL_14_DE NULL
#define OPTION_VAL_6_DE NULL
#define OPTION_VAL_12_DE NULL
#define OPTION_VAL_22_DE NULL
#define OPTION_VAL_2_DE NULL
#define OPTION_VAL_4_DE NULL
#define OPTION_VAL_25_DE NULL
#define OPTION_VAL_24_DE NULL
#define OPTION_VAL_29_DE NULL
#define OPTION_VAL_5_DE NULL
#define OPTION_VAL_0_DE NULL
#define OPTION_VAL_9_DE NULL
#define OPTION_VAL_15_DE NULL
#define OPTION_VAL_16_DE NULL
#define OPTION_VAL_17_DE NULL
#define OPTION_VAL_7_DE NULL
#define OPTION_VAL_26_DE NULL
#define OPTION_VAL_8_DE NULL
#define OPTION_VAL_11_DE NULL
#define OPTION_VAL_20_DE NULL
#define OPTION_VAL_3_DE NULL
#define OPTION_VAL_13_DE NULL
#define OPTION_VAL_27_DE NULL
#define OPTION_VAL_10_DE NULL
#define OPTION_VAL_19_DE NULL
#define OPTION_VAL_30_DE NULL
#define OPTION_VAL_28_DE NULL
#define SHADPS4_DEV_KIT_LABEL_DE NULL
#define SHADPS4_DEV_KIT_INFO_0_DE NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_DE NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_DE NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_DE NULL
#define OPTION_VAL_512_DE NULL
#define OPTION_VAL_1024_DE NULL
#define OPTION_VAL_2048_DE NULL
#define OPTION_VAL_3072_DE NULL
#define OPTION_VAL_4096_DE NULL
#define OPTION_VAL_6144_DE NULL
#define OPTION_VAL_8192_DE NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_DE NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_DE NULL
#define OPTION_VAL_CROSS_DE NULL
#define OPTION_VAL_CIRCLE_DE NULL
#define SHADPS4_NETWORK_LABEL_DE NULL
#define SHADPS4_NETWORK_INFO_0_DE NULL
#define SHADPS4_CONTROLLERS_LABEL_DE NULL
#define SHADPS4_CONTROLLERS_INFO_0_DE NULL
#define SHADPS4_SHOW_SPLASH_LABEL_DE NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_DE NULL
#define SHADPS4_RESOLUTION_LABEL_DE NULL
#define SHADPS4_RESOLUTION_INFO_0_DE NULL
#define OPTION_VAL_1280X720_DE NULL
#define OPTION_VAL_1920X1080_DE NULL
#define OPTION_VAL_2560X1440_DE NULL
#define OPTION_VAL_3840X2160_DE NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_DE NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_DE NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_DE NULL
#define OPTION_VAL_60_DE NULL
#define OPTION_VAL_90_DE NULL
#define OPTION_VAL_120_DE NULL
#define OPTION_VAL_144_DE NULL
#define OPTION_VAL_240_DE NULL
#define SHADPS4_READBACKS_LABEL_DE NULL
#define SHADPS4_READBACKS_INFO_0_DE NULL
#define OPTION_VAL_RELAXED_DE NULL
#define OPTION_VAL_PRECISE_DE NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_DE NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_DE NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_DE NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_DE NULL
#define SHADPS4_SHADER_CACHE_LABEL_DE NULL
#define SHADPS4_SHADER_CACHE_INFO_0_DE NULL
#define SHADPS4_SHOW_FPS_LABEL_DE NULL
#define SHADPS4_SHOW_FPS_INFO_0_DE NULL
#define SHADPS4_VOLUME_LABEL_DE NULL
#define SHADPS4_VOLUME_INFO_0_DE NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_DE NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_DE NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_DE NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_DE NULL
#define OPTION_VAL_40_DE NULL
#define OPTION_VAL_50_DE NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_DE NULL
#define OPTION_VAL_70_DE NULL
#define OPTION_VAL_80_DE NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_DE NULL
#define OPTION_VAL_100_DE NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_DE NULL
#define OPTION_VAL_150_DE NULL
#define OPTION_VAL_200_DE NULL
#define OPTION_VAL_300_DE NULL
#define OPTION_VAL_400_DE NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_DE NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_DE NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_DE NULL
#define SHADPS4_TROPHY_POSITION_LABEL_DE NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_DE NULL
#define OPTION_VAL_LEFT_DE NULL
#define OPTION_VAL_RIGHT_DE NULL
#define OPTION_VAL_TOP_DE NULL
#define OPTION_VAL_BOTTOM_DE NULL
#define SHADPS4_TROPHY_DURATION_LABEL_DE NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_DE NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_DE NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_DE NULL
#define SHADPS4_LOG_INFO_0_DE NULL
#define SHADPS4_LOG_SEPARATE_LABEL_DE NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_DE NULL
#define SHADPS4_LOG_SYNC_LABEL_DE NULL
#define SHADPS4_LOG_SYNC_INFO_0_DE NULL

struct retro_core_option_v2_category option_cats_de[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_DE,
        CATEGORY_SYSTEM_INFO_0_DE
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_DE,
        CATEGORY_VIDEO_INFO_0_DE
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_DE,
        CATEGORY_AUDIO_INFO_0_DE
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_DE,
        CATEGORY_TROPHIES_INFO_0_DE
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_DE,
        CATEGORY_LOGGING_INFO_0_DE
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_de[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_DE,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_DE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_DE,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_DE,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_DE },
            { "18", OPTION_VAL_18_DE },
            { "21", OPTION_VAL_21_DE },
            { "23", OPTION_VAL_23_DE },
            { "14", OPTION_VAL_14_DE },
            { "6", OPTION_VAL_6_DE },
            { "12", OPTION_VAL_12_DE },
            { "22", OPTION_VAL_22_DE },
            { "2", OPTION_VAL_2_DE },
            { "4", OPTION_VAL_4_DE },
            { "25", OPTION_VAL_25_DE },
            { "24", OPTION_VAL_24_DE },
            { "29", OPTION_VAL_29_DE },
            { "5", OPTION_VAL_5_DE },
            { "0", OPTION_VAL_0_DE },
            { "9", OPTION_VAL_9_DE },
            { "15", OPTION_VAL_15_DE },
            { "16", OPTION_VAL_16_DE },
            { "17", OPTION_VAL_17_DE },
            { "7", OPTION_VAL_7_DE },
            { "26", OPTION_VAL_26_DE },
            { "8", OPTION_VAL_8_DE },
            { "11", OPTION_VAL_11_DE },
            { "20", OPTION_VAL_20_DE },
            { "3", OPTION_VAL_3_DE },
            { "13", OPTION_VAL_13_DE },
            { "27", OPTION_VAL_27_DE },
            { "10", OPTION_VAL_10_DE },
            { "19", OPTION_VAL_19_DE },
            { "30", OPTION_VAL_30_DE },
            { "28", OPTION_VAL_28_DE },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_DE,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_DE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_DE,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_DE,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_DE },
            { "512", OPTION_VAL_512_DE },
            { "1024", OPTION_VAL_1024_DE },
            { "2048", OPTION_VAL_2048_DE },
            { "3072", OPTION_VAL_3072_DE },
            { "4096", OPTION_VAL_4096_DE },
            { "6144", OPTION_VAL_6144_DE },
            { "8192", OPTION_VAL_8192_DE },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_DE,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_DE,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_DE },
            { "circle", OPTION_VAL_CIRCLE_DE },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_DE,
        NULL,
        SHADPS4_NETWORK_INFO_0_DE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_DE,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_DE,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_DE,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_DE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_DE,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_DE,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_DE },
            { "1920x1080", OPTION_VAL_1920X1080_DE },
            { "2560x1440", OPTION_VAL_2560X1440_DE },
            { "3840x2160", OPTION_VAL_3840X2160_DE },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_DE,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_DE,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_DE },
            { "60",  OPTION_VAL_60_DE },
            { "90",  OPTION_VAL_90_DE },
            { "120", OPTION_VAL_120_DE },
            { "144", OPTION_VAL_144_DE },
            { "240", OPTION_VAL_240_DE },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_DE,
        NULL,
        SHADPS4_READBACKS_INFO_0_DE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_DE },
            { "precise",  OPTION_VAL_PRECISE_DE },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_DE,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_DE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_DE,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_DE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_DE,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_DE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_DE,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_DE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_DE,
        NULL,
        SHADPS4_VOLUME_INFO_0_DE,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_DE },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_DE },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_DE },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_DE },
            { "40",  OPTION_VAL_40_DE },
            { "50",  OPTION_VAL_50_DE },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_DE },
            { "70",  OPTION_VAL_70_DE },
            { "80",  OPTION_VAL_80_DE },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_DE },
            { "100", OPTION_VAL_100_DE },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_DE },
            { "150", OPTION_VAL_150_DE },
            { "200", OPTION_VAL_200_DE },
            { "300", OPTION_VAL_300_DE },
            { "400", OPTION_VAL_400_DE },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_DE },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_DE,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_DE,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_DE,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_DE,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_DE },
            { "right", OPTION_VAL_RIGHT_DE },
            { "top", OPTION_VAL_TOP_DE },
            { "bottom", OPTION_VAL_BOTTOM_DE },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_DE,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_DE,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_DE },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_DE },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_DE },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_DE },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_DE },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_DE },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_DE },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_DE },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_DE },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_DE },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_DE,
        NULL,
        SHADPS4_LOG_INFO_0_DE,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_DE,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_DE,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_DE,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_DE,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_de = {
   option_cats_de,
   option_defs_de
};

/* RETRO_LANGUAGE_EL */

#define CATEGORY_SYSTEM_LABEL_EL NULL
#define CATEGORY_SYSTEM_INFO_0_EL NULL
#define CATEGORY_VIDEO_LABEL_EL NULL
#define CATEGORY_VIDEO_INFO_0_EL NULL
#define CATEGORY_AUDIO_LABEL_EL NULL
#define CATEGORY_AUDIO_INFO_0_EL NULL
#define CATEGORY_TROPHIES_LABEL_EL NULL
#define CATEGORY_TROPHIES_INFO_0_EL NULL
#define CATEGORY_LOGGING_LABEL_EL NULL
#define CATEGORY_LOGGING_INFO_0_EL NULL
#define SHADPS4_NEO_MODE_LABEL_EL NULL
#define SHADPS4_NEO_MODE_INFO_0_EL NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_EL NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_EL NULL
#define OPTION_VAL_1_EL NULL
#define OPTION_VAL_18_EL NULL
#define OPTION_VAL_21_EL NULL
#define OPTION_VAL_23_EL NULL
#define OPTION_VAL_14_EL NULL
#define OPTION_VAL_6_EL NULL
#define OPTION_VAL_12_EL NULL
#define OPTION_VAL_22_EL NULL
#define OPTION_VAL_2_EL NULL
#define OPTION_VAL_4_EL NULL
#define OPTION_VAL_25_EL NULL
#define OPTION_VAL_24_EL NULL
#define OPTION_VAL_29_EL NULL
#define OPTION_VAL_5_EL NULL
#define OPTION_VAL_0_EL NULL
#define OPTION_VAL_9_EL NULL
#define OPTION_VAL_15_EL NULL
#define OPTION_VAL_16_EL NULL
#define OPTION_VAL_17_EL NULL
#define OPTION_VAL_7_EL NULL
#define OPTION_VAL_26_EL NULL
#define OPTION_VAL_8_EL NULL
#define OPTION_VAL_11_EL NULL
#define OPTION_VAL_20_EL NULL
#define OPTION_VAL_3_EL NULL
#define OPTION_VAL_13_EL NULL
#define OPTION_VAL_27_EL NULL
#define OPTION_VAL_10_EL NULL
#define OPTION_VAL_19_EL NULL
#define OPTION_VAL_30_EL NULL
#define OPTION_VAL_28_EL NULL
#define SHADPS4_DEV_KIT_LABEL_EL NULL
#define SHADPS4_DEV_KIT_INFO_0_EL NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_EL NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_EL NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_EL NULL
#define OPTION_VAL_512_EL NULL
#define OPTION_VAL_1024_EL NULL
#define OPTION_VAL_2048_EL NULL
#define OPTION_VAL_3072_EL NULL
#define OPTION_VAL_4096_EL NULL
#define OPTION_VAL_6144_EL NULL
#define OPTION_VAL_8192_EL NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_EL NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_EL NULL
#define OPTION_VAL_CROSS_EL NULL
#define OPTION_VAL_CIRCLE_EL NULL
#define SHADPS4_NETWORK_LABEL_EL NULL
#define SHADPS4_NETWORK_INFO_0_EL NULL
#define SHADPS4_CONTROLLERS_LABEL_EL NULL
#define SHADPS4_CONTROLLERS_INFO_0_EL NULL
#define SHADPS4_SHOW_SPLASH_LABEL_EL NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_EL NULL
#define SHADPS4_RESOLUTION_LABEL_EL NULL
#define SHADPS4_RESOLUTION_INFO_0_EL NULL
#define OPTION_VAL_1280X720_EL NULL
#define OPTION_VAL_1920X1080_EL NULL
#define OPTION_VAL_2560X1440_EL NULL
#define OPTION_VAL_3840X2160_EL NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_EL NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_EL NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_EL NULL
#define OPTION_VAL_60_EL NULL
#define OPTION_VAL_90_EL NULL
#define OPTION_VAL_120_EL NULL
#define OPTION_VAL_144_EL NULL
#define OPTION_VAL_240_EL NULL
#define SHADPS4_READBACKS_LABEL_EL NULL
#define SHADPS4_READBACKS_INFO_0_EL NULL
#define OPTION_VAL_RELAXED_EL NULL
#define OPTION_VAL_PRECISE_EL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_EL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_EL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_EL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_EL NULL
#define SHADPS4_SHADER_CACHE_LABEL_EL NULL
#define SHADPS4_SHADER_CACHE_INFO_0_EL NULL
#define SHADPS4_SHOW_FPS_LABEL_EL NULL
#define SHADPS4_SHOW_FPS_INFO_0_EL NULL
#define SHADPS4_VOLUME_LABEL_EL NULL
#define SHADPS4_VOLUME_INFO_0_EL NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_EL NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_EL NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_EL NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_EL NULL
#define OPTION_VAL_40_EL NULL
#define OPTION_VAL_50_EL NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_EL NULL
#define OPTION_VAL_70_EL NULL
#define OPTION_VAL_80_EL NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_EL NULL
#define OPTION_VAL_100_EL NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_EL NULL
#define OPTION_VAL_150_EL NULL
#define OPTION_VAL_200_EL NULL
#define OPTION_VAL_300_EL NULL
#define OPTION_VAL_400_EL NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_EL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_EL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_EL NULL
#define SHADPS4_TROPHY_POSITION_LABEL_EL NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_EL NULL
#define OPTION_VAL_LEFT_EL NULL
#define OPTION_VAL_RIGHT_EL NULL
#define OPTION_VAL_TOP_EL NULL
#define OPTION_VAL_BOTTOM_EL NULL
#define SHADPS4_TROPHY_DURATION_LABEL_EL NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_EL NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_EL NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_EL NULL
#define SHADPS4_LOG_INFO_0_EL NULL
#define SHADPS4_LOG_SEPARATE_LABEL_EL NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_EL NULL
#define SHADPS4_LOG_SYNC_LABEL_EL NULL
#define SHADPS4_LOG_SYNC_INFO_0_EL NULL

struct retro_core_option_v2_category option_cats_el[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_EL,
        CATEGORY_SYSTEM_INFO_0_EL
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_EL,
        CATEGORY_VIDEO_INFO_0_EL
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_EL,
        CATEGORY_AUDIO_INFO_0_EL
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_EL,
        CATEGORY_TROPHIES_INFO_0_EL
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_EL,
        CATEGORY_LOGGING_INFO_0_EL
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_el[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_EL,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_EL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_EL,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_EL,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_EL },
            { "18", OPTION_VAL_18_EL },
            { "21", OPTION_VAL_21_EL },
            { "23", OPTION_VAL_23_EL },
            { "14", OPTION_VAL_14_EL },
            { "6", OPTION_VAL_6_EL },
            { "12", OPTION_VAL_12_EL },
            { "22", OPTION_VAL_22_EL },
            { "2", OPTION_VAL_2_EL },
            { "4", OPTION_VAL_4_EL },
            { "25", OPTION_VAL_25_EL },
            { "24", OPTION_VAL_24_EL },
            { "29", OPTION_VAL_29_EL },
            { "5", OPTION_VAL_5_EL },
            { "0", OPTION_VAL_0_EL },
            { "9", OPTION_VAL_9_EL },
            { "15", OPTION_VAL_15_EL },
            { "16", OPTION_VAL_16_EL },
            { "17", OPTION_VAL_17_EL },
            { "7", OPTION_VAL_7_EL },
            { "26", OPTION_VAL_26_EL },
            { "8", OPTION_VAL_8_EL },
            { "11", OPTION_VAL_11_EL },
            { "20", OPTION_VAL_20_EL },
            { "3", OPTION_VAL_3_EL },
            { "13", OPTION_VAL_13_EL },
            { "27", OPTION_VAL_27_EL },
            { "10", OPTION_VAL_10_EL },
            { "19", OPTION_VAL_19_EL },
            { "30", OPTION_VAL_30_EL },
            { "28", OPTION_VAL_28_EL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_EL,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_EL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_EL,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_EL,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_EL },
            { "512", OPTION_VAL_512_EL },
            { "1024", OPTION_VAL_1024_EL },
            { "2048", OPTION_VAL_2048_EL },
            { "3072", OPTION_VAL_3072_EL },
            { "4096", OPTION_VAL_4096_EL },
            { "6144", OPTION_VAL_6144_EL },
            { "8192", OPTION_VAL_8192_EL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_EL,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_EL,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_EL },
            { "circle", OPTION_VAL_CIRCLE_EL },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_EL,
        NULL,
        SHADPS4_NETWORK_INFO_0_EL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_EL,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_EL,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_EL,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_EL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_EL,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_EL,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_EL },
            { "1920x1080", OPTION_VAL_1920X1080_EL },
            { "2560x1440", OPTION_VAL_2560X1440_EL },
            { "3840x2160", OPTION_VAL_3840X2160_EL },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_EL,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_EL,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_EL },
            { "60",  OPTION_VAL_60_EL },
            { "90",  OPTION_VAL_90_EL },
            { "120", OPTION_VAL_120_EL },
            { "144", OPTION_VAL_144_EL },
            { "240", OPTION_VAL_240_EL },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_EL,
        NULL,
        SHADPS4_READBACKS_INFO_0_EL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_EL },
            { "precise",  OPTION_VAL_PRECISE_EL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_EL,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_EL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_EL,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_EL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_EL,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_EL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_EL,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_EL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_EL,
        NULL,
        SHADPS4_VOLUME_INFO_0_EL,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_EL },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_EL },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_EL },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_EL },
            { "40",  OPTION_VAL_40_EL },
            { "50",  OPTION_VAL_50_EL },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_EL },
            { "70",  OPTION_VAL_70_EL },
            { "80",  OPTION_VAL_80_EL },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_EL },
            { "100", OPTION_VAL_100_EL },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_EL },
            { "150", OPTION_VAL_150_EL },
            { "200", OPTION_VAL_200_EL },
            { "300", OPTION_VAL_300_EL },
            { "400", OPTION_VAL_400_EL },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_EL },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_EL,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_EL,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_EL,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_EL,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_EL },
            { "right", OPTION_VAL_RIGHT_EL },
            { "top", OPTION_VAL_TOP_EL },
            { "bottom", OPTION_VAL_BOTTOM_EL },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_EL,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_EL,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_EL },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_EL },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_EL },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_EL },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_EL },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_EL },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_EL },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_EL },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_EL },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_EL },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_EL,
        NULL,
        SHADPS4_LOG_INFO_0_EL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_EL,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_EL,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_EL,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_EL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_el = {
   option_cats_el,
   option_defs_el
};

/* RETRO_LANGUAGE_EN */

#define CATEGORY_SYSTEM_LABEL_EN NULL
#define CATEGORY_SYSTEM_INFO_0_EN NULL
#define CATEGORY_VIDEO_LABEL_EN NULL
#define CATEGORY_VIDEO_INFO_0_EN NULL
#define CATEGORY_AUDIO_LABEL_EN NULL
#define CATEGORY_AUDIO_INFO_0_EN NULL
#define CATEGORY_TROPHIES_LABEL_EN NULL
#define CATEGORY_TROPHIES_INFO_0_EN NULL
#define CATEGORY_LOGGING_LABEL_EN NULL
#define CATEGORY_LOGGING_INFO_0_EN NULL
#define SHADPS4_NEO_MODE_LABEL_EN NULL
#define SHADPS4_NEO_MODE_INFO_0_EN NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_EN NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_EN NULL
#define OPTION_VAL_1_EN NULL
#define OPTION_VAL_18_EN NULL
#define OPTION_VAL_21_EN NULL
#define OPTION_VAL_23_EN NULL
#define OPTION_VAL_14_EN NULL
#define OPTION_VAL_6_EN NULL
#define OPTION_VAL_12_EN NULL
#define OPTION_VAL_22_EN NULL
#define OPTION_VAL_2_EN NULL
#define OPTION_VAL_4_EN NULL
#define OPTION_VAL_25_EN NULL
#define OPTION_VAL_24_EN NULL
#define OPTION_VAL_29_EN NULL
#define OPTION_VAL_5_EN NULL
#define OPTION_VAL_0_EN NULL
#define OPTION_VAL_9_EN NULL
#define OPTION_VAL_15_EN NULL
#define OPTION_VAL_16_EN NULL
#define OPTION_VAL_17_EN NULL
#define OPTION_VAL_7_EN NULL
#define OPTION_VAL_26_EN NULL
#define OPTION_VAL_8_EN NULL
#define OPTION_VAL_11_EN NULL
#define OPTION_VAL_20_EN NULL
#define OPTION_VAL_3_EN NULL
#define OPTION_VAL_13_EN NULL
#define OPTION_VAL_27_EN NULL
#define OPTION_VAL_10_EN NULL
#define OPTION_VAL_19_EN NULL
#define OPTION_VAL_30_EN NULL
#define OPTION_VAL_28_EN NULL
#define SHADPS4_DEV_KIT_LABEL_EN NULL
#define SHADPS4_DEV_KIT_INFO_0_EN NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_EN NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_EN NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_EN NULL
#define OPTION_VAL_512_EN NULL
#define OPTION_VAL_1024_EN NULL
#define OPTION_VAL_2048_EN NULL
#define OPTION_VAL_3072_EN NULL
#define OPTION_VAL_4096_EN NULL
#define OPTION_VAL_6144_EN NULL
#define OPTION_VAL_8192_EN NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_EN NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_EN NULL
#define OPTION_VAL_CROSS_EN NULL
#define OPTION_VAL_CIRCLE_EN NULL
#define SHADPS4_NETWORK_LABEL_EN NULL
#define SHADPS4_NETWORK_INFO_0_EN NULL
#define SHADPS4_CONTROLLERS_LABEL_EN NULL
#define SHADPS4_CONTROLLERS_INFO_0_EN NULL
#define SHADPS4_SHOW_SPLASH_LABEL_EN NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_EN NULL
#define SHADPS4_RESOLUTION_LABEL_EN NULL
#define SHADPS4_RESOLUTION_INFO_0_EN NULL
#define OPTION_VAL_1280X720_EN NULL
#define OPTION_VAL_1920X1080_EN NULL
#define OPTION_VAL_2560X1440_EN NULL
#define OPTION_VAL_3840X2160_EN NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_EN NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_EN NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_EN NULL
#define OPTION_VAL_60_EN NULL
#define OPTION_VAL_90_EN NULL
#define OPTION_VAL_120_EN NULL
#define OPTION_VAL_144_EN NULL
#define OPTION_VAL_240_EN NULL
#define SHADPS4_READBACKS_LABEL_EN NULL
#define SHADPS4_READBACKS_INFO_0_EN NULL
#define OPTION_VAL_RELAXED_EN NULL
#define OPTION_VAL_PRECISE_EN NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_EN NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_EN NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_EN NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_EN NULL
#define SHADPS4_SHADER_CACHE_LABEL_EN NULL
#define SHADPS4_SHADER_CACHE_INFO_0_EN NULL
#define SHADPS4_SHOW_FPS_LABEL_EN NULL
#define SHADPS4_SHOW_FPS_INFO_0_EN NULL
#define SHADPS4_VOLUME_LABEL_EN NULL
#define SHADPS4_VOLUME_INFO_0_EN NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_EN NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_EN NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_EN NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_EN NULL
#define OPTION_VAL_40_EN NULL
#define OPTION_VAL_50_EN NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_EN NULL
#define OPTION_VAL_70_EN NULL
#define OPTION_VAL_80_EN NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_EN NULL
#define OPTION_VAL_100_EN NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_EN NULL
#define OPTION_VAL_150_EN NULL
#define OPTION_VAL_200_EN NULL
#define OPTION_VAL_300_EN NULL
#define OPTION_VAL_400_EN NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_EN NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_EN NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_EN NULL
#define SHADPS4_TROPHY_POSITION_LABEL_EN NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_EN NULL
#define OPTION_VAL_LEFT_EN NULL
#define OPTION_VAL_RIGHT_EN NULL
#define OPTION_VAL_TOP_EN NULL
#define OPTION_VAL_BOTTOM_EN NULL
#define SHADPS4_TROPHY_DURATION_LABEL_EN NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_EN NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_EN NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_EN NULL
#define SHADPS4_LOG_INFO_0_EN NULL
#define SHADPS4_LOG_SEPARATE_LABEL_EN NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_EN NULL
#define SHADPS4_LOG_SYNC_LABEL_EN NULL
#define SHADPS4_LOG_SYNC_INFO_0_EN NULL

struct retro_core_option_v2_category option_cats_en[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_EN,
        CATEGORY_SYSTEM_INFO_0_EN
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_EN,
        CATEGORY_VIDEO_INFO_0_EN
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_EN,
        CATEGORY_AUDIO_INFO_0_EN
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_EN,
        CATEGORY_TROPHIES_INFO_0_EN
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_EN,
        CATEGORY_LOGGING_INFO_0_EN
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_en[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_EN,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_EN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_EN,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_EN,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_EN },
            { "18", OPTION_VAL_18_EN },
            { "21", OPTION_VAL_21_EN },
            { "23", OPTION_VAL_23_EN },
            { "14", OPTION_VAL_14_EN },
            { "6", OPTION_VAL_6_EN },
            { "12", OPTION_VAL_12_EN },
            { "22", OPTION_VAL_22_EN },
            { "2", OPTION_VAL_2_EN },
            { "4", OPTION_VAL_4_EN },
            { "25", OPTION_VAL_25_EN },
            { "24", OPTION_VAL_24_EN },
            { "29", OPTION_VAL_29_EN },
            { "5", OPTION_VAL_5_EN },
            { "0", OPTION_VAL_0_EN },
            { "9", OPTION_VAL_9_EN },
            { "15", OPTION_VAL_15_EN },
            { "16", OPTION_VAL_16_EN },
            { "17", OPTION_VAL_17_EN },
            { "7", OPTION_VAL_7_EN },
            { "26", OPTION_VAL_26_EN },
            { "8", OPTION_VAL_8_EN },
            { "11", OPTION_VAL_11_EN },
            { "20", OPTION_VAL_20_EN },
            { "3", OPTION_VAL_3_EN },
            { "13", OPTION_VAL_13_EN },
            { "27", OPTION_VAL_27_EN },
            { "10", OPTION_VAL_10_EN },
            { "19", OPTION_VAL_19_EN },
            { "30", OPTION_VAL_30_EN },
            { "28", OPTION_VAL_28_EN },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_EN,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_EN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_EN,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_EN,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_EN },
            { "512", OPTION_VAL_512_EN },
            { "1024", OPTION_VAL_1024_EN },
            { "2048", OPTION_VAL_2048_EN },
            { "3072", OPTION_VAL_3072_EN },
            { "4096", OPTION_VAL_4096_EN },
            { "6144", OPTION_VAL_6144_EN },
            { "8192", OPTION_VAL_8192_EN },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_EN,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_EN,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_EN },
            { "circle", OPTION_VAL_CIRCLE_EN },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_EN,
        NULL,
        SHADPS4_NETWORK_INFO_0_EN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_EN,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_EN,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_EN,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_EN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_EN,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_EN,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_EN },
            { "1920x1080", OPTION_VAL_1920X1080_EN },
            { "2560x1440", OPTION_VAL_2560X1440_EN },
            { "3840x2160", OPTION_VAL_3840X2160_EN },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_EN,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_EN,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_EN },
            { "60",  OPTION_VAL_60_EN },
            { "90",  OPTION_VAL_90_EN },
            { "120", OPTION_VAL_120_EN },
            { "144", OPTION_VAL_144_EN },
            { "240", OPTION_VAL_240_EN },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_EN,
        NULL,
        SHADPS4_READBACKS_INFO_0_EN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_EN },
            { "precise",  OPTION_VAL_PRECISE_EN },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_EN,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_EN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_EN,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_EN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_EN,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_EN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_EN,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_EN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_EN,
        NULL,
        SHADPS4_VOLUME_INFO_0_EN,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_EN },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_EN },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_EN },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_EN },
            { "40",  OPTION_VAL_40_EN },
            { "50",  OPTION_VAL_50_EN },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_EN },
            { "70",  OPTION_VAL_70_EN },
            { "80",  OPTION_VAL_80_EN },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_EN },
            { "100", OPTION_VAL_100_EN },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_EN },
            { "150", OPTION_VAL_150_EN },
            { "200", OPTION_VAL_200_EN },
            { "300", OPTION_VAL_300_EN },
            { "400", OPTION_VAL_400_EN },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_EN },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_EN,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_EN,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_EN,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_EN,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_EN },
            { "right", OPTION_VAL_RIGHT_EN },
            { "top", OPTION_VAL_TOP_EN },
            { "bottom", OPTION_VAL_BOTTOM_EN },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_EN,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_EN,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_EN },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_EN },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_EN },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_EN },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_EN },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_EN },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_EN },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_EN },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_EN },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_EN },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_EN,
        NULL,
        SHADPS4_LOG_INFO_0_EN,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_EN,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_EN,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_EN,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_EN,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_en = {
   option_cats_en,
   option_defs_en
};

/* RETRO_LANGUAGE_EO */

#define CATEGORY_SYSTEM_LABEL_EO NULL
#define CATEGORY_SYSTEM_INFO_0_EO NULL
#define CATEGORY_VIDEO_LABEL_EO NULL
#define CATEGORY_VIDEO_INFO_0_EO NULL
#define CATEGORY_AUDIO_LABEL_EO NULL
#define CATEGORY_AUDIO_INFO_0_EO NULL
#define CATEGORY_TROPHIES_LABEL_EO NULL
#define CATEGORY_TROPHIES_INFO_0_EO NULL
#define CATEGORY_LOGGING_LABEL_EO NULL
#define CATEGORY_LOGGING_INFO_0_EO NULL
#define SHADPS4_NEO_MODE_LABEL_EO NULL
#define SHADPS4_NEO_MODE_INFO_0_EO NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_EO NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_EO NULL
#define OPTION_VAL_1_EO NULL
#define OPTION_VAL_18_EO NULL
#define OPTION_VAL_21_EO NULL
#define OPTION_VAL_23_EO NULL
#define OPTION_VAL_14_EO NULL
#define OPTION_VAL_6_EO NULL
#define OPTION_VAL_12_EO NULL
#define OPTION_VAL_22_EO NULL
#define OPTION_VAL_2_EO NULL
#define OPTION_VAL_4_EO NULL
#define OPTION_VAL_25_EO NULL
#define OPTION_VAL_24_EO NULL
#define OPTION_VAL_29_EO NULL
#define OPTION_VAL_5_EO NULL
#define OPTION_VAL_0_EO NULL
#define OPTION_VAL_9_EO NULL
#define OPTION_VAL_15_EO NULL
#define OPTION_VAL_16_EO NULL
#define OPTION_VAL_17_EO NULL
#define OPTION_VAL_7_EO NULL
#define OPTION_VAL_26_EO NULL
#define OPTION_VAL_8_EO NULL
#define OPTION_VAL_11_EO NULL
#define OPTION_VAL_20_EO NULL
#define OPTION_VAL_3_EO NULL
#define OPTION_VAL_13_EO NULL
#define OPTION_VAL_27_EO NULL
#define OPTION_VAL_10_EO NULL
#define OPTION_VAL_19_EO NULL
#define OPTION_VAL_30_EO NULL
#define OPTION_VAL_28_EO NULL
#define SHADPS4_DEV_KIT_LABEL_EO NULL
#define SHADPS4_DEV_KIT_INFO_0_EO NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_EO NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_EO NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_EO NULL
#define OPTION_VAL_512_EO NULL
#define OPTION_VAL_1024_EO NULL
#define OPTION_VAL_2048_EO NULL
#define OPTION_VAL_3072_EO NULL
#define OPTION_VAL_4096_EO NULL
#define OPTION_VAL_6144_EO NULL
#define OPTION_VAL_8192_EO NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_EO NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_EO NULL
#define OPTION_VAL_CROSS_EO NULL
#define OPTION_VAL_CIRCLE_EO NULL
#define SHADPS4_NETWORK_LABEL_EO NULL
#define SHADPS4_NETWORK_INFO_0_EO NULL
#define SHADPS4_CONTROLLERS_LABEL_EO NULL
#define SHADPS4_CONTROLLERS_INFO_0_EO NULL
#define SHADPS4_SHOW_SPLASH_LABEL_EO NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_EO NULL
#define SHADPS4_RESOLUTION_LABEL_EO NULL
#define SHADPS4_RESOLUTION_INFO_0_EO NULL
#define OPTION_VAL_1280X720_EO NULL
#define OPTION_VAL_1920X1080_EO NULL
#define OPTION_VAL_2560X1440_EO NULL
#define OPTION_VAL_3840X2160_EO NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_EO NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_EO NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_EO NULL
#define OPTION_VAL_60_EO NULL
#define OPTION_VAL_90_EO NULL
#define OPTION_VAL_120_EO NULL
#define OPTION_VAL_144_EO NULL
#define OPTION_VAL_240_EO NULL
#define SHADPS4_READBACKS_LABEL_EO NULL
#define SHADPS4_READBACKS_INFO_0_EO NULL
#define OPTION_VAL_RELAXED_EO NULL
#define OPTION_VAL_PRECISE_EO NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_EO NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_EO NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_EO NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_EO NULL
#define SHADPS4_SHADER_CACHE_LABEL_EO NULL
#define SHADPS4_SHADER_CACHE_INFO_0_EO NULL
#define SHADPS4_SHOW_FPS_LABEL_EO NULL
#define SHADPS4_SHOW_FPS_INFO_0_EO NULL
#define SHADPS4_VOLUME_LABEL_EO NULL
#define SHADPS4_VOLUME_INFO_0_EO NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_EO NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_EO NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_EO NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_EO NULL
#define OPTION_VAL_40_EO NULL
#define OPTION_VAL_50_EO NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_EO NULL
#define OPTION_VAL_70_EO NULL
#define OPTION_VAL_80_EO NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_EO NULL
#define OPTION_VAL_100_EO NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_EO NULL
#define OPTION_VAL_150_EO NULL
#define OPTION_VAL_200_EO NULL
#define OPTION_VAL_300_EO NULL
#define OPTION_VAL_400_EO NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_EO NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_EO NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_EO NULL
#define SHADPS4_TROPHY_POSITION_LABEL_EO NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_EO NULL
#define OPTION_VAL_LEFT_EO NULL
#define OPTION_VAL_RIGHT_EO NULL
#define OPTION_VAL_TOP_EO NULL
#define OPTION_VAL_BOTTOM_EO NULL
#define SHADPS4_TROPHY_DURATION_LABEL_EO NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_EO NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_EO NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_EO NULL
#define SHADPS4_LOG_INFO_0_EO NULL
#define SHADPS4_LOG_SEPARATE_LABEL_EO NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_EO NULL
#define SHADPS4_LOG_SYNC_LABEL_EO NULL
#define SHADPS4_LOG_SYNC_INFO_0_EO NULL

struct retro_core_option_v2_category option_cats_eo[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_EO,
        CATEGORY_SYSTEM_INFO_0_EO
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_EO,
        CATEGORY_VIDEO_INFO_0_EO
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_EO,
        CATEGORY_AUDIO_INFO_0_EO
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_EO,
        CATEGORY_TROPHIES_INFO_0_EO
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_EO,
        CATEGORY_LOGGING_INFO_0_EO
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_eo[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_EO,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_EO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_EO,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_EO,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_EO },
            { "18", OPTION_VAL_18_EO },
            { "21", OPTION_VAL_21_EO },
            { "23", OPTION_VAL_23_EO },
            { "14", OPTION_VAL_14_EO },
            { "6", OPTION_VAL_6_EO },
            { "12", OPTION_VAL_12_EO },
            { "22", OPTION_VAL_22_EO },
            { "2", OPTION_VAL_2_EO },
            { "4", OPTION_VAL_4_EO },
            { "25", OPTION_VAL_25_EO },
            { "24", OPTION_VAL_24_EO },
            { "29", OPTION_VAL_29_EO },
            { "5", OPTION_VAL_5_EO },
            { "0", OPTION_VAL_0_EO },
            { "9", OPTION_VAL_9_EO },
            { "15", OPTION_VAL_15_EO },
            { "16", OPTION_VAL_16_EO },
            { "17", OPTION_VAL_17_EO },
            { "7", OPTION_VAL_7_EO },
            { "26", OPTION_VAL_26_EO },
            { "8", OPTION_VAL_8_EO },
            { "11", OPTION_VAL_11_EO },
            { "20", OPTION_VAL_20_EO },
            { "3", OPTION_VAL_3_EO },
            { "13", OPTION_VAL_13_EO },
            { "27", OPTION_VAL_27_EO },
            { "10", OPTION_VAL_10_EO },
            { "19", OPTION_VAL_19_EO },
            { "30", OPTION_VAL_30_EO },
            { "28", OPTION_VAL_28_EO },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_EO,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_EO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_EO,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_EO,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_EO },
            { "512", OPTION_VAL_512_EO },
            { "1024", OPTION_VAL_1024_EO },
            { "2048", OPTION_VAL_2048_EO },
            { "3072", OPTION_VAL_3072_EO },
            { "4096", OPTION_VAL_4096_EO },
            { "6144", OPTION_VAL_6144_EO },
            { "8192", OPTION_VAL_8192_EO },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_EO,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_EO,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_EO },
            { "circle", OPTION_VAL_CIRCLE_EO },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_EO,
        NULL,
        SHADPS4_NETWORK_INFO_0_EO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_EO,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_EO,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_EO,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_EO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_EO,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_EO,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_EO },
            { "1920x1080", OPTION_VAL_1920X1080_EO },
            { "2560x1440", OPTION_VAL_2560X1440_EO },
            { "3840x2160", OPTION_VAL_3840X2160_EO },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_EO,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_EO,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_EO },
            { "60",  OPTION_VAL_60_EO },
            { "90",  OPTION_VAL_90_EO },
            { "120", OPTION_VAL_120_EO },
            { "144", OPTION_VAL_144_EO },
            { "240", OPTION_VAL_240_EO },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_EO,
        NULL,
        SHADPS4_READBACKS_INFO_0_EO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_EO },
            { "precise",  OPTION_VAL_PRECISE_EO },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_EO,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_EO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_EO,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_EO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_EO,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_EO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_EO,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_EO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_EO,
        NULL,
        SHADPS4_VOLUME_INFO_0_EO,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_EO },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_EO },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_EO },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_EO },
            { "40",  OPTION_VAL_40_EO },
            { "50",  OPTION_VAL_50_EO },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_EO },
            { "70",  OPTION_VAL_70_EO },
            { "80",  OPTION_VAL_80_EO },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_EO },
            { "100", OPTION_VAL_100_EO },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_EO },
            { "150", OPTION_VAL_150_EO },
            { "200", OPTION_VAL_200_EO },
            { "300", OPTION_VAL_300_EO },
            { "400", OPTION_VAL_400_EO },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_EO },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_EO,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_EO,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_EO,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_EO,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_EO },
            { "right", OPTION_VAL_RIGHT_EO },
            { "top", OPTION_VAL_TOP_EO },
            { "bottom", OPTION_VAL_BOTTOM_EO },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_EO,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_EO,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_EO },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_EO },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_EO },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_EO },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_EO },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_EO },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_EO },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_EO },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_EO },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_EO },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_EO,
        NULL,
        SHADPS4_LOG_INFO_0_EO,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_EO,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_EO,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_EO,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_EO,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_eo = {
   option_cats_eo,
   option_defs_eo
};

/* RETRO_LANGUAGE_ES */

#define CATEGORY_SYSTEM_LABEL_ES NULL
#define CATEGORY_SYSTEM_INFO_0_ES NULL
#define CATEGORY_VIDEO_LABEL_ES NULL
#define CATEGORY_VIDEO_INFO_0_ES NULL
#define CATEGORY_AUDIO_LABEL_ES NULL
#define CATEGORY_AUDIO_INFO_0_ES NULL
#define CATEGORY_TROPHIES_LABEL_ES NULL
#define CATEGORY_TROPHIES_INFO_0_ES NULL
#define CATEGORY_LOGGING_LABEL_ES NULL
#define CATEGORY_LOGGING_INFO_0_ES NULL
#define SHADPS4_NEO_MODE_LABEL_ES NULL
#define SHADPS4_NEO_MODE_INFO_0_ES NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_ES NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_ES NULL
#define OPTION_VAL_1_ES NULL
#define OPTION_VAL_18_ES NULL
#define OPTION_VAL_21_ES NULL
#define OPTION_VAL_23_ES NULL
#define OPTION_VAL_14_ES NULL
#define OPTION_VAL_6_ES NULL
#define OPTION_VAL_12_ES NULL
#define OPTION_VAL_22_ES NULL
#define OPTION_VAL_2_ES NULL
#define OPTION_VAL_4_ES NULL
#define OPTION_VAL_25_ES NULL
#define OPTION_VAL_24_ES NULL
#define OPTION_VAL_29_ES NULL
#define OPTION_VAL_5_ES NULL
#define OPTION_VAL_0_ES NULL
#define OPTION_VAL_9_ES NULL
#define OPTION_VAL_15_ES NULL
#define OPTION_VAL_16_ES NULL
#define OPTION_VAL_17_ES NULL
#define OPTION_VAL_7_ES NULL
#define OPTION_VAL_26_ES NULL
#define OPTION_VAL_8_ES NULL
#define OPTION_VAL_11_ES NULL
#define OPTION_VAL_20_ES NULL
#define OPTION_VAL_3_ES NULL
#define OPTION_VAL_13_ES NULL
#define OPTION_VAL_27_ES NULL
#define OPTION_VAL_10_ES NULL
#define OPTION_VAL_19_ES NULL
#define OPTION_VAL_30_ES NULL
#define OPTION_VAL_28_ES NULL
#define SHADPS4_DEV_KIT_LABEL_ES NULL
#define SHADPS4_DEV_KIT_INFO_0_ES NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_ES NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_ES NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_ES NULL
#define OPTION_VAL_512_ES NULL
#define OPTION_VAL_1024_ES NULL
#define OPTION_VAL_2048_ES NULL
#define OPTION_VAL_3072_ES NULL
#define OPTION_VAL_4096_ES NULL
#define OPTION_VAL_6144_ES NULL
#define OPTION_VAL_8192_ES NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_ES NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_ES NULL
#define OPTION_VAL_CROSS_ES NULL
#define OPTION_VAL_CIRCLE_ES NULL
#define SHADPS4_NETWORK_LABEL_ES NULL
#define SHADPS4_NETWORK_INFO_0_ES NULL
#define SHADPS4_CONTROLLERS_LABEL_ES NULL
#define SHADPS4_CONTROLLERS_INFO_0_ES NULL
#define SHADPS4_SHOW_SPLASH_LABEL_ES NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_ES NULL
#define SHADPS4_RESOLUTION_LABEL_ES NULL
#define SHADPS4_RESOLUTION_INFO_0_ES NULL
#define OPTION_VAL_1280X720_ES NULL
#define OPTION_VAL_1920X1080_ES NULL
#define OPTION_VAL_2560X1440_ES NULL
#define OPTION_VAL_3840X2160_ES NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_ES NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_ES NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_ES NULL
#define OPTION_VAL_60_ES NULL
#define OPTION_VAL_90_ES NULL
#define OPTION_VAL_120_ES NULL
#define OPTION_VAL_144_ES NULL
#define OPTION_VAL_240_ES NULL
#define SHADPS4_READBACKS_LABEL_ES NULL
#define SHADPS4_READBACKS_INFO_0_ES NULL
#define OPTION_VAL_RELAXED_ES NULL
#define OPTION_VAL_PRECISE_ES NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_ES NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_ES NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_ES NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_ES NULL
#define SHADPS4_SHADER_CACHE_LABEL_ES NULL
#define SHADPS4_SHADER_CACHE_INFO_0_ES NULL
#define SHADPS4_SHOW_FPS_LABEL_ES NULL
#define SHADPS4_SHOW_FPS_INFO_0_ES NULL
#define SHADPS4_VOLUME_LABEL_ES NULL
#define SHADPS4_VOLUME_INFO_0_ES NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_ES NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_ES NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_ES NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_ES NULL
#define OPTION_VAL_40_ES NULL
#define OPTION_VAL_50_ES NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_ES NULL
#define OPTION_VAL_70_ES NULL
#define OPTION_VAL_80_ES NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_ES NULL
#define OPTION_VAL_100_ES NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_ES NULL
#define OPTION_VAL_150_ES NULL
#define OPTION_VAL_200_ES NULL
#define OPTION_VAL_300_ES NULL
#define OPTION_VAL_400_ES NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_ES NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_ES NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_ES NULL
#define SHADPS4_TROPHY_POSITION_LABEL_ES NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_ES NULL
#define OPTION_VAL_LEFT_ES NULL
#define OPTION_VAL_RIGHT_ES NULL
#define OPTION_VAL_TOP_ES NULL
#define OPTION_VAL_BOTTOM_ES NULL
#define SHADPS4_TROPHY_DURATION_LABEL_ES NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_ES NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_ES NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_ES NULL
#define SHADPS4_LOG_INFO_0_ES NULL
#define SHADPS4_LOG_SEPARATE_LABEL_ES NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_ES NULL
#define SHADPS4_LOG_SYNC_LABEL_ES NULL
#define SHADPS4_LOG_SYNC_INFO_0_ES NULL

struct retro_core_option_v2_category option_cats_es[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_ES,
        CATEGORY_SYSTEM_INFO_0_ES
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_ES,
        CATEGORY_VIDEO_INFO_0_ES
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_ES,
        CATEGORY_AUDIO_INFO_0_ES
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_ES,
        CATEGORY_TROPHIES_INFO_0_ES
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_ES,
        CATEGORY_LOGGING_INFO_0_ES
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_es[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_ES,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_ES,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_ES,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_ES,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_ES },
            { "18", OPTION_VAL_18_ES },
            { "21", OPTION_VAL_21_ES },
            { "23", OPTION_VAL_23_ES },
            { "14", OPTION_VAL_14_ES },
            { "6", OPTION_VAL_6_ES },
            { "12", OPTION_VAL_12_ES },
            { "22", OPTION_VAL_22_ES },
            { "2", OPTION_VAL_2_ES },
            { "4", OPTION_VAL_4_ES },
            { "25", OPTION_VAL_25_ES },
            { "24", OPTION_VAL_24_ES },
            { "29", OPTION_VAL_29_ES },
            { "5", OPTION_VAL_5_ES },
            { "0", OPTION_VAL_0_ES },
            { "9", OPTION_VAL_9_ES },
            { "15", OPTION_VAL_15_ES },
            { "16", OPTION_VAL_16_ES },
            { "17", OPTION_VAL_17_ES },
            { "7", OPTION_VAL_7_ES },
            { "26", OPTION_VAL_26_ES },
            { "8", OPTION_VAL_8_ES },
            { "11", OPTION_VAL_11_ES },
            { "20", OPTION_VAL_20_ES },
            { "3", OPTION_VAL_3_ES },
            { "13", OPTION_VAL_13_ES },
            { "27", OPTION_VAL_27_ES },
            { "10", OPTION_VAL_10_ES },
            { "19", OPTION_VAL_19_ES },
            { "30", OPTION_VAL_30_ES },
            { "28", OPTION_VAL_28_ES },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_ES,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_ES,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_ES,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_ES,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_ES },
            { "512", OPTION_VAL_512_ES },
            { "1024", OPTION_VAL_1024_ES },
            { "2048", OPTION_VAL_2048_ES },
            { "3072", OPTION_VAL_3072_ES },
            { "4096", OPTION_VAL_4096_ES },
            { "6144", OPTION_VAL_6144_ES },
            { "8192", OPTION_VAL_8192_ES },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_ES,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_ES,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_ES },
            { "circle", OPTION_VAL_CIRCLE_ES },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_ES,
        NULL,
        SHADPS4_NETWORK_INFO_0_ES,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_ES,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_ES,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_ES,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_ES,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_ES,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_ES,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_ES },
            { "1920x1080", OPTION_VAL_1920X1080_ES },
            { "2560x1440", OPTION_VAL_2560X1440_ES },
            { "3840x2160", OPTION_VAL_3840X2160_ES },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_ES,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_ES,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_ES },
            { "60",  OPTION_VAL_60_ES },
            { "90",  OPTION_VAL_90_ES },
            { "120", OPTION_VAL_120_ES },
            { "144", OPTION_VAL_144_ES },
            { "240", OPTION_VAL_240_ES },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_ES,
        NULL,
        SHADPS4_READBACKS_INFO_0_ES,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_ES },
            { "precise",  OPTION_VAL_PRECISE_ES },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_ES,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_ES,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_ES,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_ES,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_ES,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_ES,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_ES,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_ES,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_ES,
        NULL,
        SHADPS4_VOLUME_INFO_0_ES,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_ES },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_ES },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_ES },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_ES },
            { "40",  OPTION_VAL_40_ES },
            { "50",  OPTION_VAL_50_ES },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_ES },
            { "70",  OPTION_VAL_70_ES },
            { "80",  OPTION_VAL_80_ES },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_ES },
            { "100", OPTION_VAL_100_ES },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_ES },
            { "150", OPTION_VAL_150_ES },
            { "200", OPTION_VAL_200_ES },
            { "300", OPTION_VAL_300_ES },
            { "400", OPTION_VAL_400_ES },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_ES },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_ES,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_ES,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_ES,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_ES,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_ES },
            { "right", OPTION_VAL_RIGHT_ES },
            { "top", OPTION_VAL_TOP_ES },
            { "bottom", OPTION_VAL_BOTTOM_ES },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_ES,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_ES,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_ES },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_ES },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_ES },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_ES },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_ES },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_ES },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_ES },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_ES },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_ES },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_ES },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_ES,
        NULL,
        SHADPS4_LOG_INFO_0_ES,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_ES,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_ES,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_ES,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_ES,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_es = {
   option_cats_es,
   option_defs_es
};

/* RETRO_LANGUAGE_FA */

#define CATEGORY_SYSTEM_LABEL_FA NULL
#define CATEGORY_SYSTEM_INFO_0_FA NULL
#define CATEGORY_VIDEO_LABEL_FA NULL
#define CATEGORY_VIDEO_INFO_0_FA NULL
#define CATEGORY_AUDIO_LABEL_FA NULL
#define CATEGORY_AUDIO_INFO_0_FA NULL
#define CATEGORY_TROPHIES_LABEL_FA NULL
#define CATEGORY_TROPHIES_INFO_0_FA NULL
#define CATEGORY_LOGGING_LABEL_FA NULL
#define CATEGORY_LOGGING_INFO_0_FA NULL
#define SHADPS4_NEO_MODE_LABEL_FA NULL
#define SHADPS4_NEO_MODE_INFO_0_FA NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_FA NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_FA NULL
#define OPTION_VAL_1_FA NULL
#define OPTION_VAL_18_FA NULL
#define OPTION_VAL_21_FA NULL
#define OPTION_VAL_23_FA NULL
#define OPTION_VAL_14_FA NULL
#define OPTION_VAL_6_FA NULL
#define OPTION_VAL_12_FA NULL
#define OPTION_VAL_22_FA NULL
#define OPTION_VAL_2_FA NULL
#define OPTION_VAL_4_FA NULL
#define OPTION_VAL_25_FA NULL
#define OPTION_VAL_24_FA NULL
#define OPTION_VAL_29_FA NULL
#define OPTION_VAL_5_FA NULL
#define OPTION_VAL_0_FA NULL
#define OPTION_VAL_9_FA NULL
#define OPTION_VAL_15_FA NULL
#define OPTION_VAL_16_FA NULL
#define OPTION_VAL_17_FA NULL
#define OPTION_VAL_7_FA NULL
#define OPTION_VAL_26_FA NULL
#define OPTION_VAL_8_FA NULL
#define OPTION_VAL_11_FA NULL
#define OPTION_VAL_20_FA NULL
#define OPTION_VAL_3_FA NULL
#define OPTION_VAL_13_FA NULL
#define OPTION_VAL_27_FA NULL
#define OPTION_VAL_10_FA NULL
#define OPTION_VAL_19_FA NULL
#define OPTION_VAL_30_FA NULL
#define OPTION_VAL_28_FA NULL
#define SHADPS4_DEV_KIT_LABEL_FA NULL
#define SHADPS4_DEV_KIT_INFO_0_FA NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_FA NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_FA NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_FA NULL
#define OPTION_VAL_512_FA NULL
#define OPTION_VAL_1024_FA NULL
#define OPTION_VAL_2048_FA NULL
#define OPTION_VAL_3072_FA NULL
#define OPTION_VAL_4096_FA NULL
#define OPTION_VAL_6144_FA NULL
#define OPTION_VAL_8192_FA NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_FA NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_FA NULL
#define OPTION_VAL_CROSS_FA NULL
#define OPTION_VAL_CIRCLE_FA NULL
#define SHADPS4_NETWORK_LABEL_FA NULL
#define SHADPS4_NETWORK_INFO_0_FA NULL
#define SHADPS4_CONTROLLERS_LABEL_FA NULL
#define SHADPS4_CONTROLLERS_INFO_0_FA NULL
#define SHADPS4_SHOW_SPLASH_LABEL_FA NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_FA NULL
#define SHADPS4_RESOLUTION_LABEL_FA NULL
#define SHADPS4_RESOLUTION_INFO_0_FA NULL
#define OPTION_VAL_1280X720_FA NULL
#define OPTION_VAL_1920X1080_FA NULL
#define OPTION_VAL_2560X1440_FA NULL
#define OPTION_VAL_3840X2160_FA NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_FA NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_FA NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_FA NULL
#define OPTION_VAL_60_FA NULL
#define OPTION_VAL_90_FA NULL
#define OPTION_VAL_120_FA NULL
#define OPTION_VAL_144_FA NULL
#define OPTION_VAL_240_FA NULL
#define SHADPS4_READBACKS_LABEL_FA NULL
#define SHADPS4_READBACKS_INFO_0_FA NULL
#define OPTION_VAL_RELAXED_FA NULL
#define OPTION_VAL_PRECISE_FA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_FA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_FA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_FA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_FA NULL
#define SHADPS4_SHADER_CACHE_LABEL_FA NULL
#define SHADPS4_SHADER_CACHE_INFO_0_FA NULL
#define SHADPS4_SHOW_FPS_LABEL_FA NULL
#define SHADPS4_SHOW_FPS_INFO_0_FA NULL
#define SHADPS4_VOLUME_LABEL_FA NULL
#define SHADPS4_VOLUME_INFO_0_FA NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_FA NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_FA NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_FA NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_FA NULL
#define OPTION_VAL_40_FA NULL
#define OPTION_VAL_50_FA NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_FA NULL
#define OPTION_VAL_70_FA NULL
#define OPTION_VAL_80_FA NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_FA NULL
#define OPTION_VAL_100_FA NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_FA NULL
#define OPTION_VAL_150_FA NULL
#define OPTION_VAL_200_FA NULL
#define OPTION_VAL_300_FA NULL
#define OPTION_VAL_400_FA NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_FA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_FA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_FA NULL
#define SHADPS4_TROPHY_POSITION_LABEL_FA NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_FA NULL
#define OPTION_VAL_LEFT_FA NULL
#define OPTION_VAL_RIGHT_FA NULL
#define OPTION_VAL_TOP_FA NULL
#define OPTION_VAL_BOTTOM_FA NULL
#define SHADPS4_TROPHY_DURATION_LABEL_FA NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_FA NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_FA NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_FA NULL
#define SHADPS4_LOG_INFO_0_FA NULL
#define SHADPS4_LOG_SEPARATE_LABEL_FA NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_FA NULL
#define SHADPS4_LOG_SYNC_LABEL_FA NULL
#define SHADPS4_LOG_SYNC_INFO_0_FA NULL

struct retro_core_option_v2_category option_cats_fa[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_FA,
        CATEGORY_SYSTEM_INFO_0_FA
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_FA,
        CATEGORY_VIDEO_INFO_0_FA
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_FA,
        CATEGORY_AUDIO_INFO_0_FA
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_FA,
        CATEGORY_TROPHIES_INFO_0_FA
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_FA,
        CATEGORY_LOGGING_INFO_0_FA
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fa[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_FA,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_FA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_FA,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_FA,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_FA },
            { "18", OPTION_VAL_18_FA },
            { "21", OPTION_VAL_21_FA },
            { "23", OPTION_VAL_23_FA },
            { "14", OPTION_VAL_14_FA },
            { "6", OPTION_VAL_6_FA },
            { "12", OPTION_VAL_12_FA },
            { "22", OPTION_VAL_22_FA },
            { "2", OPTION_VAL_2_FA },
            { "4", OPTION_VAL_4_FA },
            { "25", OPTION_VAL_25_FA },
            { "24", OPTION_VAL_24_FA },
            { "29", OPTION_VAL_29_FA },
            { "5", OPTION_VAL_5_FA },
            { "0", OPTION_VAL_0_FA },
            { "9", OPTION_VAL_9_FA },
            { "15", OPTION_VAL_15_FA },
            { "16", OPTION_VAL_16_FA },
            { "17", OPTION_VAL_17_FA },
            { "7", OPTION_VAL_7_FA },
            { "26", OPTION_VAL_26_FA },
            { "8", OPTION_VAL_8_FA },
            { "11", OPTION_VAL_11_FA },
            { "20", OPTION_VAL_20_FA },
            { "3", OPTION_VAL_3_FA },
            { "13", OPTION_VAL_13_FA },
            { "27", OPTION_VAL_27_FA },
            { "10", OPTION_VAL_10_FA },
            { "19", OPTION_VAL_19_FA },
            { "30", OPTION_VAL_30_FA },
            { "28", OPTION_VAL_28_FA },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_FA,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_FA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_FA,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_FA,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_FA },
            { "512", OPTION_VAL_512_FA },
            { "1024", OPTION_VAL_1024_FA },
            { "2048", OPTION_VAL_2048_FA },
            { "3072", OPTION_VAL_3072_FA },
            { "4096", OPTION_VAL_4096_FA },
            { "6144", OPTION_VAL_6144_FA },
            { "8192", OPTION_VAL_8192_FA },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_FA,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_FA,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_FA },
            { "circle", OPTION_VAL_CIRCLE_FA },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_FA,
        NULL,
        SHADPS4_NETWORK_INFO_0_FA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_FA,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_FA,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_FA,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_FA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_FA,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_FA,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_FA },
            { "1920x1080", OPTION_VAL_1920X1080_FA },
            { "2560x1440", OPTION_VAL_2560X1440_FA },
            { "3840x2160", OPTION_VAL_3840X2160_FA },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_FA,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_FA,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_FA },
            { "60",  OPTION_VAL_60_FA },
            { "90",  OPTION_VAL_90_FA },
            { "120", OPTION_VAL_120_FA },
            { "144", OPTION_VAL_144_FA },
            { "240", OPTION_VAL_240_FA },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_FA,
        NULL,
        SHADPS4_READBACKS_INFO_0_FA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_FA },
            { "precise",  OPTION_VAL_PRECISE_FA },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_FA,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_FA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_FA,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_FA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_FA,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_FA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_FA,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_FA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_FA,
        NULL,
        SHADPS4_VOLUME_INFO_0_FA,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_FA },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_FA },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_FA },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_FA },
            { "40",  OPTION_VAL_40_FA },
            { "50",  OPTION_VAL_50_FA },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_FA },
            { "70",  OPTION_VAL_70_FA },
            { "80",  OPTION_VAL_80_FA },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_FA },
            { "100", OPTION_VAL_100_FA },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_FA },
            { "150", OPTION_VAL_150_FA },
            { "200", OPTION_VAL_200_FA },
            { "300", OPTION_VAL_300_FA },
            { "400", OPTION_VAL_400_FA },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_FA },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_FA,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_FA,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_FA,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_FA,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_FA },
            { "right", OPTION_VAL_RIGHT_FA },
            { "top", OPTION_VAL_TOP_FA },
            { "bottom", OPTION_VAL_BOTTOM_FA },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_FA,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_FA,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_FA },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_FA },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_FA },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_FA },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_FA },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_FA },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_FA },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_FA },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_FA },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_FA },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_FA,
        NULL,
        SHADPS4_LOG_INFO_0_FA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_FA,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_FA,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_FA,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_FA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_fa = {
   option_cats_fa,
   option_defs_fa
};

/* RETRO_LANGUAGE_FI */

#define CATEGORY_SYSTEM_LABEL_FI NULL
#define CATEGORY_SYSTEM_INFO_0_FI NULL
#define CATEGORY_VIDEO_LABEL_FI NULL
#define CATEGORY_VIDEO_INFO_0_FI NULL
#define CATEGORY_AUDIO_LABEL_FI NULL
#define CATEGORY_AUDIO_INFO_0_FI NULL
#define CATEGORY_TROPHIES_LABEL_FI NULL
#define CATEGORY_TROPHIES_INFO_0_FI NULL
#define CATEGORY_LOGGING_LABEL_FI NULL
#define CATEGORY_LOGGING_INFO_0_FI NULL
#define SHADPS4_NEO_MODE_LABEL_FI NULL
#define SHADPS4_NEO_MODE_INFO_0_FI NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_FI NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_FI NULL
#define OPTION_VAL_1_FI NULL
#define OPTION_VAL_18_FI NULL
#define OPTION_VAL_21_FI NULL
#define OPTION_VAL_23_FI NULL
#define OPTION_VAL_14_FI NULL
#define OPTION_VAL_6_FI NULL
#define OPTION_VAL_12_FI NULL
#define OPTION_VAL_22_FI NULL
#define OPTION_VAL_2_FI NULL
#define OPTION_VAL_4_FI NULL
#define OPTION_VAL_25_FI NULL
#define OPTION_VAL_24_FI NULL
#define OPTION_VAL_29_FI NULL
#define OPTION_VAL_5_FI NULL
#define OPTION_VAL_0_FI NULL
#define OPTION_VAL_9_FI NULL
#define OPTION_VAL_15_FI NULL
#define OPTION_VAL_16_FI NULL
#define OPTION_VAL_17_FI NULL
#define OPTION_VAL_7_FI NULL
#define OPTION_VAL_26_FI NULL
#define OPTION_VAL_8_FI NULL
#define OPTION_VAL_11_FI NULL
#define OPTION_VAL_20_FI NULL
#define OPTION_VAL_3_FI NULL
#define OPTION_VAL_13_FI NULL
#define OPTION_VAL_27_FI NULL
#define OPTION_VAL_10_FI NULL
#define OPTION_VAL_19_FI NULL
#define OPTION_VAL_30_FI NULL
#define OPTION_VAL_28_FI NULL
#define SHADPS4_DEV_KIT_LABEL_FI NULL
#define SHADPS4_DEV_KIT_INFO_0_FI NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_FI NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_FI NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_FI NULL
#define OPTION_VAL_512_FI NULL
#define OPTION_VAL_1024_FI NULL
#define OPTION_VAL_2048_FI NULL
#define OPTION_VAL_3072_FI NULL
#define OPTION_VAL_4096_FI NULL
#define OPTION_VAL_6144_FI NULL
#define OPTION_VAL_8192_FI NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_FI NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_FI NULL
#define OPTION_VAL_CROSS_FI NULL
#define OPTION_VAL_CIRCLE_FI NULL
#define SHADPS4_NETWORK_LABEL_FI NULL
#define SHADPS4_NETWORK_INFO_0_FI NULL
#define SHADPS4_CONTROLLERS_LABEL_FI NULL
#define SHADPS4_CONTROLLERS_INFO_0_FI NULL
#define SHADPS4_SHOW_SPLASH_LABEL_FI NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_FI NULL
#define SHADPS4_RESOLUTION_LABEL_FI NULL
#define SHADPS4_RESOLUTION_INFO_0_FI NULL
#define OPTION_VAL_1280X720_FI NULL
#define OPTION_VAL_1920X1080_FI NULL
#define OPTION_VAL_2560X1440_FI NULL
#define OPTION_VAL_3840X2160_FI NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_FI NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_FI NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_FI NULL
#define OPTION_VAL_60_FI NULL
#define OPTION_VAL_90_FI NULL
#define OPTION_VAL_120_FI NULL
#define OPTION_VAL_144_FI NULL
#define OPTION_VAL_240_FI NULL
#define SHADPS4_READBACKS_LABEL_FI NULL
#define SHADPS4_READBACKS_INFO_0_FI NULL
#define OPTION_VAL_RELAXED_FI NULL
#define OPTION_VAL_PRECISE_FI NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_FI NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_FI NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_FI NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_FI NULL
#define SHADPS4_SHADER_CACHE_LABEL_FI NULL
#define SHADPS4_SHADER_CACHE_INFO_0_FI NULL
#define SHADPS4_SHOW_FPS_LABEL_FI NULL
#define SHADPS4_SHOW_FPS_INFO_0_FI NULL
#define SHADPS4_VOLUME_LABEL_FI NULL
#define SHADPS4_VOLUME_INFO_0_FI NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_FI NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_FI NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_FI NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_FI NULL
#define OPTION_VAL_40_FI NULL
#define OPTION_VAL_50_FI NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_FI NULL
#define OPTION_VAL_70_FI NULL
#define OPTION_VAL_80_FI NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_FI NULL
#define OPTION_VAL_100_FI NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_FI NULL
#define OPTION_VAL_150_FI NULL
#define OPTION_VAL_200_FI NULL
#define OPTION_VAL_300_FI NULL
#define OPTION_VAL_400_FI NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_FI NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_FI NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_FI NULL
#define SHADPS4_TROPHY_POSITION_LABEL_FI NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_FI NULL
#define OPTION_VAL_LEFT_FI NULL
#define OPTION_VAL_RIGHT_FI NULL
#define OPTION_VAL_TOP_FI NULL
#define OPTION_VAL_BOTTOM_FI NULL
#define SHADPS4_TROPHY_DURATION_LABEL_FI NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_FI NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_FI NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_FI NULL
#define SHADPS4_LOG_INFO_0_FI NULL
#define SHADPS4_LOG_SEPARATE_LABEL_FI NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_FI NULL
#define SHADPS4_LOG_SYNC_LABEL_FI NULL
#define SHADPS4_LOG_SYNC_INFO_0_FI NULL

struct retro_core_option_v2_category option_cats_fi[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_FI,
        CATEGORY_SYSTEM_INFO_0_FI
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_FI,
        CATEGORY_VIDEO_INFO_0_FI
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_FI,
        CATEGORY_AUDIO_INFO_0_FI
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_FI,
        CATEGORY_TROPHIES_INFO_0_FI
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_FI,
        CATEGORY_LOGGING_INFO_0_FI
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fi[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_FI,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_FI,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_FI,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_FI,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_FI },
            { "18", OPTION_VAL_18_FI },
            { "21", OPTION_VAL_21_FI },
            { "23", OPTION_VAL_23_FI },
            { "14", OPTION_VAL_14_FI },
            { "6", OPTION_VAL_6_FI },
            { "12", OPTION_VAL_12_FI },
            { "22", OPTION_VAL_22_FI },
            { "2", OPTION_VAL_2_FI },
            { "4", OPTION_VAL_4_FI },
            { "25", OPTION_VAL_25_FI },
            { "24", OPTION_VAL_24_FI },
            { "29", OPTION_VAL_29_FI },
            { "5", OPTION_VAL_5_FI },
            { "0", OPTION_VAL_0_FI },
            { "9", OPTION_VAL_9_FI },
            { "15", OPTION_VAL_15_FI },
            { "16", OPTION_VAL_16_FI },
            { "17", OPTION_VAL_17_FI },
            { "7", OPTION_VAL_7_FI },
            { "26", OPTION_VAL_26_FI },
            { "8", OPTION_VAL_8_FI },
            { "11", OPTION_VAL_11_FI },
            { "20", OPTION_VAL_20_FI },
            { "3", OPTION_VAL_3_FI },
            { "13", OPTION_VAL_13_FI },
            { "27", OPTION_VAL_27_FI },
            { "10", OPTION_VAL_10_FI },
            { "19", OPTION_VAL_19_FI },
            { "30", OPTION_VAL_30_FI },
            { "28", OPTION_VAL_28_FI },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_FI,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_FI,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_FI,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_FI,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_FI },
            { "512", OPTION_VAL_512_FI },
            { "1024", OPTION_VAL_1024_FI },
            { "2048", OPTION_VAL_2048_FI },
            { "3072", OPTION_VAL_3072_FI },
            { "4096", OPTION_VAL_4096_FI },
            { "6144", OPTION_VAL_6144_FI },
            { "8192", OPTION_VAL_8192_FI },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_FI,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_FI,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_FI },
            { "circle", OPTION_VAL_CIRCLE_FI },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_FI,
        NULL,
        SHADPS4_NETWORK_INFO_0_FI,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_FI,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_FI,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_FI,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_FI,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_FI,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_FI,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_FI },
            { "1920x1080", OPTION_VAL_1920X1080_FI },
            { "2560x1440", OPTION_VAL_2560X1440_FI },
            { "3840x2160", OPTION_VAL_3840X2160_FI },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_FI,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_FI,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_FI },
            { "60",  OPTION_VAL_60_FI },
            { "90",  OPTION_VAL_90_FI },
            { "120", OPTION_VAL_120_FI },
            { "144", OPTION_VAL_144_FI },
            { "240", OPTION_VAL_240_FI },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_FI,
        NULL,
        SHADPS4_READBACKS_INFO_0_FI,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_FI },
            { "precise",  OPTION_VAL_PRECISE_FI },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_FI,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_FI,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_FI,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_FI,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_FI,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_FI,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_FI,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_FI,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_FI,
        NULL,
        SHADPS4_VOLUME_INFO_0_FI,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_FI },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_FI },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_FI },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_FI },
            { "40",  OPTION_VAL_40_FI },
            { "50",  OPTION_VAL_50_FI },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_FI },
            { "70",  OPTION_VAL_70_FI },
            { "80",  OPTION_VAL_80_FI },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_FI },
            { "100", OPTION_VAL_100_FI },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_FI },
            { "150", OPTION_VAL_150_FI },
            { "200", OPTION_VAL_200_FI },
            { "300", OPTION_VAL_300_FI },
            { "400", OPTION_VAL_400_FI },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_FI },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_FI,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_FI,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_FI,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_FI,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_FI },
            { "right", OPTION_VAL_RIGHT_FI },
            { "top", OPTION_VAL_TOP_FI },
            { "bottom", OPTION_VAL_BOTTOM_FI },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_FI,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_FI,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_FI },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_FI },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_FI },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_FI },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_FI },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_FI },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_FI },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_FI },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_FI },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_FI },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_FI,
        NULL,
        SHADPS4_LOG_INFO_0_FI,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_FI,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_FI,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_FI,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_FI,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_fi = {
   option_cats_fi,
   option_defs_fi
};

/* RETRO_LANGUAGE_FR */

#define CATEGORY_SYSTEM_LABEL_FR NULL
#define CATEGORY_SYSTEM_INFO_0_FR NULL
#define CATEGORY_VIDEO_LABEL_FR NULL
#define CATEGORY_VIDEO_INFO_0_FR NULL
#define CATEGORY_AUDIO_LABEL_FR NULL
#define CATEGORY_AUDIO_INFO_0_FR NULL
#define CATEGORY_TROPHIES_LABEL_FR NULL
#define CATEGORY_TROPHIES_INFO_0_FR NULL
#define CATEGORY_LOGGING_LABEL_FR NULL
#define CATEGORY_LOGGING_INFO_0_FR NULL
#define SHADPS4_NEO_MODE_LABEL_FR NULL
#define SHADPS4_NEO_MODE_INFO_0_FR NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_FR NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_FR NULL
#define OPTION_VAL_1_FR NULL
#define OPTION_VAL_18_FR NULL
#define OPTION_VAL_21_FR NULL
#define OPTION_VAL_23_FR NULL
#define OPTION_VAL_14_FR NULL
#define OPTION_VAL_6_FR NULL
#define OPTION_VAL_12_FR NULL
#define OPTION_VAL_22_FR NULL
#define OPTION_VAL_2_FR NULL
#define OPTION_VAL_4_FR NULL
#define OPTION_VAL_25_FR NULL
#define OPTION_VAL_24_FR NULL
#define OPTION_VAL_29_FR NULL
#define OPTION_VAL_5_FR NULL
#define OPTION_VAL_0_FR NULL
#define OPTION_VAL_9_FR NULL
#define OPTION_VAL_15_FR NULL
#define OPTION_VAL_16_FR NULL
#define OPTION_VAL_17_FR NULL
#define OPTION_VAL_7_FR NULL
#define OPTION_VAL_26_FR NULL
#define OPTION_VAL_8_FR NULL
#define OPTION_VAL_11_FR NULL
#define OPTION_VAL_20_FR NULL
#define OPTION_VAL_3_FR NULL
#define OPTION_VAL_13_FR NULL
#define OPTION_VAL_27_FR NULL
#define OPTION_VAL_10_FR NULL
#define OPTION_VAL_19_FR NULL
#define OPTION_VAL_30_FR NULL
#define OPTION_VAL_28_FR NULL
#define SHADPS4_DEV_KIT_LABEL_FR NULL
#define SHADPS4_DEV_KIT_INFO_0_FR NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_FR NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_FR NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_FR NULL
#define OPTION_VAL_512_FR NULL
#define OPTION_VAL_1024_FR NULL
#define OPTION_VAL_2048_FR NULL
#define OPTION_VAL_3072_FR NULL
#define OPTION_VAL_4096_FR NULL
#define OPTION_VAL_6144_FR NULL
#define OPTION_VAL_8192_FR NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_FR NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_FR NULL
#define OPTION_VAL_CROSS_FR NULL
#define OPTION_VAL_CIRCLE_FR NULL
#define SHADPS4_NETWORK_LABEL_FR NULL
#define SHADPS4_NETWORK_INFO_0_FR NULL
#define SHADPS4_CONTROLLERS_LABEL_FR NULL
#define SHADPS4_CONTROLLERS_INFO_0_FR NULL
#define SHADPS4_SHOW_SPLASH_LABEL_FR NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_FR NULL
#define SHADPS4_RESOLUTION_LABEL_FR NULL
#define SHADPS4_RESOLUTION_INFO_0_FR NULL
#define OPTION_VAL_1280X720_FR NULL
#define OPTION_VAL_1920X1080_FR NULL
#define OPTION_VAL_2560X1440_FR NULL
#define OPTION_VAL_3840X2160_FR NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_FR NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_FR NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_FR NULL
#define OPTION_VAL_60_FR NULL
#define OPTION_VAL_90_FR NULL
#define OPTION_VAL_120_FR NULL
#define OPTION_VAL_144_FR NULL
#define OPTION_VAL_240_FR NULL
#define SHADPS4_READBACKS_LABEL_FR NULL
#define SHADPS4_READBACKS_INFO_0_FR NULL
#define OPTION_VAL_RELAXED_FR NULL
#define OPTION_VAL_PRECISE_FR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_FR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_FR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_FR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_FR NULL
#define SHADPS4_SHADER_CACHE_LABEL_FR NULL
#define SHADPS4_SHADER_CACHE_INFO_0_FR NULL
#define SHADPS4_SHOW_FPS_LABEL_FR NULL
#define SHADPS4_SHOW_FPS_INFO_0_FR NULL
#define SHADPS4_VOLUME_LABEL_FR NULL
#define SHADPS4_VOLUME_INFO_0_FR NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_FR NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_FR NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_FR NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_FR NULL
#define OPTION_VAL_40_FR NULL
#define OPTION_VAL_50_FR NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_FR NULL
#define OPTION_VAL_70_FR NULL
#define OPTION_VAL_80_FR NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_FR NULL
#define OPTION_VAL_100_FR NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_FR NULL
#define OPTION_VAL_150_FR NULL
#define OPTION_VAL_200_FR NULL
#define OPTION_VAL_300_FR NULL
#define OPTION_VAL_400_FR NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_FR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_FR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_FR NULL
#define SHADPS4_TROPHY_POSITION_LABEL_FR NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_FR NULL
#define OPTION_VAL_LEFT_FR NULL
#define OPTION_VAL_RIGHT_FR NULL
#define OPTION_VAL_TOP_FR NULL
#define OPTION_VAL_BOTTOM_FR NULL
#define SHADPS4_TROPHY_DURATION_LABEL_FR NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_FR NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_FR NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_FR NULL
#define SHADPS4_LOG_INFO_0_FR NULL
#define SHADPS4_LOG_SEPARATE_LABEL_FR NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_FR NULL
#define SHADPS4_LOG_SYNC_LABEL_FR NULL
#define SHADPS4_LOG_SYNC_INFO_0_FR NULL

struct retro_core_option_v2_category option_cats_fr[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_FR,
        CATEGORY_SYSTEM_INFO_0_FR
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_FR,
        CATEGORY_VIDEO_INFO_0_FR
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_FR,
        CATEGORY_AUDIO_INFO_0_FR
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_FR,
        CATEGORY_TROPHIES_INFO_0_FR
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_FR,
        CATEGORY_LOGGING_INFO_0_FR
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_fr[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_FR,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_FR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_FR,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_FR,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_FR },
            { "18", OPTION_VAL_18_FR },
            { "21", OPTION_VAL_21_FR },
            { "23", OPTION_VAL_23_FR },
            { "14", OPTION_VAL_14_FR },
            { "6", OPTION_VAL_6_FR },
            { "12", OPTION_VAL_12_FR },
            { "22", OPTION_VAL_22_FR },
            { "2", OPTION_VAL_2_FR },
            { "4", OPTION_VAL_4_FR },
            { "25", OPTION_VAL_25_FR },
            { "24", OPTION_VAL_24_FR },
            { "29", OPTION_VAL_29_FR },
            { "5", OPTION_VAL_5_FR },
            { "0", OPTION_VAL_0_FR },
            { "9", OPTION_VAL_9_FR },
            { "15", OPTION_VAL_15_FR },
            { "16", OPTION_VAL_16_FR },
            { "17", OPTION_VAL_17_FR },
            { "7", OPTION_VAL_7_FR },
            { "26", OPTION_VAL_26_FR },
            { "8", OPTION_VAL_8_FR },
            { "11", OPTION_VAL_11_FR },
            { "20", OPTION_VAL_20_FR },
            { "3", OPTION_VAL_3_FR },
            { "13", OPTION_VAL_13_FR },
            { "27", OPTION_VAL_27_FR },
            { "10", OPTION_VAL_10_FR },
            { "19", OPTION_VAL_19_FR },
            { "30", OPTION_VAL_30_FR },
            { "28", OPTION_VAL_28_FR },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_FR,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_FR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_FR,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_FR,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_FR },
            { "512", OPTION_VAL_512_FR },
            { "1024", OPTION_VAL_1024_FR },
            { "2048", OPTION_VAL_2048_FR },
            { "3072", OPTION_VAL_3072_FR },
            { "4096", OPTION_VAL_4096_FR },
            { "6144", OPTION_VAL_6144_FR },
            { "8192", OPTION_VAL_8192_FR },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_FR,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_FR,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_FR },
            { "circle", OPTION_VAL_CIRCLE_FR },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_FR,
        NULL,
        SHADPS4_NETWORK_INFO_0_FR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_FR,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_FR,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_FR,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_FR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_FR,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_FR,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_FR },
            { "1920x1080", OPTION_VAL_1920X1080_FR },
            { "2560x1440", OPTION_VAL_2560X1440_FR },
            { "3840x2160", OPTION_VAL_3840X2160_FR },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_FR,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_FR,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_FR },
            { "60",  OPTION_VAL_60_FR },
            { "90",  OPTION_VAL_90_FR },
            { "120", OPTION_VAL_120_FR },
            { "144", OPTION_VAL_144_FR },
            { "240", OPTION_VAL_240_FR },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_FR,
        NULL,
        SHADPS4_READBACKS_INFO_0_FR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_FR },
            { "precise",  OPTION_VAL_PRECISE_FR },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_FR,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_FR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_FR,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_FR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_FR,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_FR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_FR,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_FR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_FR,
        NULL,
        SHADPS4_VOLUME_INFO_0_FR,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_FR },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_FR },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_FR },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_FR },
            { "40",  OPTION_VAL_40_FR },
            { "50",  OPTION_VAL_50_FR },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_FR },
            { "70",  OPTION_VAL_70_FR },
            { "80",  OPTION_VAL_80_FR },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_FR },
            { "100", OPTION_VAL_100_FR },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_FR },
            { "150", OPTION_VAL_150_FR },
            { "200", OPTION_VAL_200_FR },
            { "300", OPTION_VAL_300_FR },
            { "400", OPTION_VAL_400_FR },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_FR },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_FR,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_FR,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_FR,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_FR,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_FR },
            { "right", OPTION_VAL_RIGHT_FR },
            { "top", OPTION_VAL_TOP_FR },
            { "bottom", OPTION_VAL_BOTTOM_FR },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_FR,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_FR,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_FR },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_FR },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_FR },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_FR },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_FR },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_FR },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_FR },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_FR },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_FR },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_FR },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_FR,
        NULL,
        SHADPS4_LOG_INFO_0_FR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_FR,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_FR,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_FR,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_FR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_fr = {
   option_cats_fr,
   option_defs_fr
};

/* RETRO_LANGUAGE_GA */

#define CATEGORY_SYSTEM_LABEL_GA NULL
#define CATEGORY_SYSTEM_INFO_0_GA NULL
#define CATEGORY_VIDEO_LABEL_GA NULL
#define CATEGORY_VIDEO_INFO_0_GA NULL
#define CATEGORY_AUDIO_LABEL_GA NULL
#define CATEGORY_AUDIO_INFO_0_GA NULL
#define CATEGORY_TROPHIES_LABEL_GA NULL
#define CATEGORY_TROPHIES_INFO_0_GA NULL
#define CATEGORY_LOGGING_LABEL_GA NULL
#define CATEGORY_LOGGING_INFO_0_GA NULL
#define SHADPS4_NEO_MODE_LABEL_GA NULL
#define SHADPS4_NEO_MODE_INFO_0_GA NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_GA NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_GA NULL
#define OPTION_VAL_1_GA NULL
#define OPTION_VAL_18_GA NULL
#define OPTION_VAL_21_GA NULL
#define OPTION_VAL_23_GA NULL
#define OPTION_VAL_14_GA NULL
#define OPTION_VAL_6_GA NULL
#define OPTION_VAL_12_GA NULL
#define OPTION_VAL_22_GA NULL
#define OPTION_VAL_2_GA NULL
#define OPTION_VAL_4_GA NULL
#define OPTION_VAL_25_GA NULL
#define OPTION_VAL_24_GA NULL
#define OPTION_VAL_29_GA NULL
#define OPTION_VAL_5_GA NULL
#define OPTION_VAL_0_GA NULL
#define OPTION_VAL_9_GA NULL
#define OPTION_VAL_15_GA NULL
#define OPTION_VAL_16_GA NULL
#define OPTION_VAL_17_GA NULL
#define OPTION_VAL_7_GA NULL
#define OPTION_VAL_26_GA NULL
#define OPTION_VAL_8_GA NULL
#define OPTION_VAL_11_GA NULL
#define OPTION_VAL_20_GA NULL
#define OPTION_VAL_3_GA NULL
#define OPTION_VAL_13_GA NULL
#define OPTION_VAL_27_GA NULL
#define OPTION_VAL_10_GA NULL
#define OPTION_VAL_19_GA NULL
#define OPTION_VAL_30_GA NULL
#define OPTION_VAL_28_GA NULL
#define SHADPS4_DEV_KIT_LABEL_GA NULL
#define SHADPS4_DEV_KIT_INFO_0_GA NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_GA NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_GA NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_GA NULL
#define OPTION_VAL_512_GA NULL
#define OPTION_VAL_1024_GA NULL
#define OPTION_VAL_2048_GA NULL
#define OPTION_VAL_3072_GA NULL
#define OPTION_VAL_4096_GA NULL
#define OPTION_VAL_6144_GA NULL
#define OPTION_VAL_8192_GA NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_GA NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_GA NULL
#define OPTION_VAL_CROSS_GA NULL
#define OPTION_VAL_CIRCLE_GA NULL
#define SHADPS4_NETWORK_LABEL_GA NULL
#define SHADPS4_NETWORK_INFO_0_GA NULL
#define SHADPS4_CONTROLLERS_LABEL_GA NULL
#define SHADPS4_CONTROLLERS_INFO_0_GA NULL
#define SHADPS4_SHOW_SPLASH_LABEL_GA NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_GA NULL
#define SHADPS4_RESOLUTION_LABEL_GA NULL
#define SHADPS4_RESOLUTION_INFO_0_GA NULL
#define OPTION_VAL_1280X720_GA NULL
#define OPTION_VAL_1920X1080_GA NULL
#define OPTION_VAL_2560X1440_GA NULL
#define OPTION_VAL_3840X2160_GA NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_GA NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_GA NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_GA NULL
#define OPTION_VAL_60_GA NULL
#define OPTION_VAL_90_GA NULL
#define OPTION_VAL_120_GA NULL
#define OPTION_VAL_144_GA NULL
#define OPTION_VAL_240_GA NULL
#define SHADPS4_READBACKS_LABEL_GA NULL
#define SHADPS4_READBACKS_INFO_0_GA NULL
#define OPTION_VAL_RELAXED_GA NULL
#define OPTION_VAL_PRECISE_GA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_GA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_GA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_GA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_GA NULL
#define SHADPS4_SHADER_CACHE_LABEL_GA NULL
#define SHADPS4_SHADER_CACHE_INFO_0_GA NULL
#define SHADPS4_SHOW_FPS_LABEL_GA NULL
#define SHADPS4_SHOW_FPS_INFO_0_GA NULL
#define SHADPS4_VOLUME_LABEL_GA NULL
#define SHADPS4_VOLUME_INFO_0_GA NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_GA NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_GA NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_GA NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_GA NULL
#define OPTION_VAL_40_GA NULL
#define OPTION_VAL_50_GA NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_GA NULL
#define OPTION_VAL_70_GA NULL
#define OPTION_VAL_80_GA NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_GA NULL
#define OPTION_VAL_100_GA NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_GA NULL
#define OPTION_VAL_150_GA NULL
#define OPTION_VAL_200_GA NULL
#define OPTION_VAL_300_GA NULL
#define OPTION_VAL_400_GA NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_GA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_GA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_GA NULL
#define SHADPS4_TROPHY_POSITION_LABEL_GA NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_GA NULL
#define OPTION_VAL_LEFT_GA NULL
#define OPTION_VAL_RIGHT_GA NULL
#define OPTION_VAL_TOP_GA NULL
#define OPTION_VAL_BOTTOM_GA NULL
#define SHADPS4_TROPHY_DURATION_LABEL_GA NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_GA NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_GA NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_GA NULL
#define SHADPS4_LOG_INFO_0_GA NULL
#define SHADPS4_LOG_SEPARATE_LABEL_GA NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_GA NULL
#define SHADPS4_LOG_SYNC_LABEL_GA NULL
#define SHADPS4_LOG_SYNC_INFO_0_GA NULL

struct retro_core_option_v2_category option_cats_ga[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_GA,
        CATEGORY_SYSTEM_INFO_0_GA
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_GA,
        CATEGORY_VIDEO_INFO_0_GA
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_GA,
        CATEGORY_AUDIO_INFO_0_GA
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_GA,
        CATEGORY_TROPHIES_INFO_0_GA
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_GA,
        CATEGORY_LOGGING_INFO_0_GA
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ga[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_GA,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_GA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_GA,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_GA,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_GA },
            { "18", OPTION_VAL_18_GA },
            { "21", OPTION_VAL_21_GA },
            { "23", OPTION_VAL_23_GA },
            { "14", OPTION_VAL_14_GA },
            { "6", OPTION_VAL_6_GA },
            { "12", OPTION_VAL_12_GA },
            { "22", OPTION_VAL_22_GA },
            { "2", OPTION_VAL_2_GA },
            { "4", OPTION_VAL_4_GA },
            { "25", OPTION_VAL_25_GA },
            { "24", OPTION_VAL_24_GA },
            { "29", OPTION_VAL_29_GA },
            { "5", OPTION_VAL_5_GA },
            { "0", OPTION_VAL_0_GA },
            { "9", OPTION_VAL_9_GA },
            { "15", OPTION_VAL_15_GA },
            { "16", OPTION_VAL_16_GA },
            { "17", OPTION_VAL_17_GA },
            { "7", OPTION_VAL_7_GA },
            { "26", OPTION_VAL_26_GA },
            { "8", OPTION_VAL_8_GA },
            { "11", OPTION_VAL_11_GA },
            { "20", OPTION_VAL_20_GA },
            { "3", OPTION_VAL_3_GA },
            { "13", OPTION_VAL_13_GA },
            { "27", OPTION_VAL_27_GA },
            { "10", OPTION_VAL_10_GA },
            { "19", OPTION_VAL_19_GA },
            { "30", OPTION_VAL_30_GA },
            { "28", OPTION_VAL_28_GA },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_GA,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_GA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_GA,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_GA,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_GA },
            { "512", OPTION_VAL_512_GA },
            { "1024", OPTION_VAL_1024_GA },
            { "2048", OPTION_VAL_2048_GA },
            { "3072", OPTION_VAL_3072_GA },
            { "4096", OPTION_VAL_4096_GA },
            { "6144", OPTION_VAL_6144_GA },
            { "8192", OPTION_VAL_8192_GA },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_GA,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_GA,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_GA },
            { "circle", OPTION_VAL_CIRCLE_GA },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_GA,
        NULL,
        SHADPS4_NETWORK_INFO_0_GA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_GA,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_GA,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_GA,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_GA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_GA,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_GA,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_GA },
            { "1920x1080", OPTION_VAL_1920X1080_GA },
            { "2560x1440", OPTION_VAL_2560X1440_GA },
            { "3840x2160", OPTION_VAL_3840X2160_GA },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_GA,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_GA,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_GA },
            { "60",  OPTION_VAL_60_GA },
            { "90",  OPTION_VAL_90_GA },
            { "120", OPTION_VAL_120_GA },
            { "144", OPTION_VAL_144_GA },
            { "240", OPTION_VAL_240_GA },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_GA,
        NULL,
        SHADPS4_READBACKS_INFO_0_GA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_GA },
            { "precise",  OPTION_VAL_PRECISE_GA },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_GA,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_GA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_GA,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_GA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_GA,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_GA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_GA,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_GA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_GA,
        NULL,
        SHADPS4_VOLUME_INFO_0_GA,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_GA },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_GA },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_GA },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_GA },
            { "40",  OPTION_VAL_40_GA },
            { "50",  OPTION_VAL_50_GA },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_GA },
            { "70",  OPTION_VAL_70_GA },
            { "80",  OPTION_VAL_80_GA },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_GA },
            { "100", OPTION_VAL_100_GA },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_GA },
            { "150", OPTION_VAL_150_GA },
            { "200", OPTION_VAL_200_GA },
            { "300", OPTION_VAL_300_GA },
            { "400", OPTION_VAL_400_GA },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_GA },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_GA,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_GA,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_GA,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_GA,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_GA },
            { "right", OPTION_VAL_RIGHT_GA },
            { "top", OPTION_VAL_TOP_GA },
            { "bottom", OPTION_VAL_BOTTOM_GA },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_GA,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_GA,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_GA },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_GA },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_GA },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_GA },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_GA },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_GA },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_GA },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_GA },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_GA },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_GA },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_GA,
        NULL,
        SHADPS4_LOG_INFO_0_GA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_GA,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_GA,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_GA,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_GA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_ga = {
   option_cats_ga,
   option_defs_ga
};

/* RETRO_LANGUAGE_GL */

#define CATEGORY_SYSTEM_LABEL_GL NULL
#define CATEGORY_SYSTEM_INFO_0_GL NULL
#define CATEGORY_VIDEO_LABEL_GL NULL
#define CATEGORY_VIDEO_INFO_0_GL NULL
#define CATEGORY_AUDIO_LABEL_GL NULL
#define CATEGORY_AUDIO_INFO_0_GL NULL
#define CATEGORY_TROPHIES_LABEL_GL NULL
#define CATEGORY_TROPHIES_INFO_0_GL NULL
#define CATEGORY_LOGGING_LABEL_GL NULL
#define CATEGORY_LOGGING_INFO_0_GL NULL
#define SHADPS4_NEO_MODE_LABEL_GL NULL
#define SHADPS4_NEO_MODE_INFO_0_GL NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_GL NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_GL NULL
#define OPTION_VAL_1_GL NULL
#define OPTION_VAL_18_GL NULL
#define OPTION_VAL_21_GL NULL
#define OPTION_VAL_23_GL NULL
#define OPTION_VAL_14_GL NULL
#define OPTION_VAL_6_GL NULL
#define OPTION_VAL_12_GL NULL
#define OPTION_VAL_22_GL NULL
#define OPTION_VAL_2_GL NULL
#define OPTION_VAL_4_GL NULL
#define OPTION_VAL_25_GL NULL
#define OPTION_VAL_24_GL NULL
#define OPTION_VAL_29_GL NULL
#define OPTION_VAL_5_GL NULL
#define OPTION_VAL_0_GL NULL
#define OPTION_VAL_9_GL NULL
#define OPTION_VAL_15_GL NULL
#define OPTION_VAL_16_GL NULL
#define OPTION_VAL_17_GL NULL
#define OPTION_VAL_7_GL NULL
#define OPTION_VAL_26_GL NULL
#define OPTION_VAL_8_GL NULL
#define OPTION_VAL_11_GL NULL
#define OPTION_VAL_20_GL NULL
#define OPTION_VAL_3_GL NULL
#define OPTION_VAL_13_GL NULL
#define OPTION_VAL_27_GL NULL
#define OPTION_VAL_10_GL NULL
#define OPTION_VAL_19_GL NULL
#define OPTION_VAL_30_GL NULL
#define OPTION_VAL_28_GL NULL
#define SHADPS4_DEV_KIT_LABEL_GL NULL
#define SHADPS4_DEV_KIT_INFO_0_GL NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_GL NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_GL NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_GL NULL
#define OPTION_VAL_512_GL NULL
#define OPTION_VAL_1024_GL NULL
#define OPTION_VAL_2048_GL NULL
#define OPTION_VAL_3072_GL NULL
#define OPTION_VAL_4096_GL NULL
#define OPTION_VAL_6144_GL NULL
#define OPTION_VAL_8192_GL NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_GL NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_GL NULL
#define OPTION_VAL_CROSS_GL NULL
#define OPTION_VAL_CIRCLE_GL NULL
#define SHADPS4_NETWORK_LABEL_GL NULL
#define SHADPS4_NETWORK_INFO_0_GL NULL
#define SHADPS4_CONTROLLERS_LABEL_GL NULL
#define SHADPS4_CONTROLLERS_INFO_0_GL NULL
#define SHADPS4_SHOW_SPLASH_LABEL_GL NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_GL NULL
#define SHADPS4_RESOLUTION_LABEL_GL NULL
#define SHADPS4_RESOLUTION_INFO_0_GL NULL
#define OPTION_VAL_1280X720_GL NULL
#define OPTION_VAL_1920X1080_GL NULL
#define OPTION_VAL_2560X1440_GL NULL
#define OPTION_VAL_3840X2160_GL NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_GL NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_GL NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_GL NULL
#define OPTION_VAL_60_GL NULL
#define OPTION_VAL_90_GL NULL
#define OPTION_VAL_120_GL NULL
#define OPTION_VAL_144_GL NULL
#define OPTION_VAL_240_GL NULL
#define SHADPS4_READBACKS_LABEL_GL NULL
#define SHADPS4_READBACKS_INFO_0_GL NULL
#define OPTION_VAL_RELAXED_GL NULL
#define OPTION_VAL_PRECISE_GL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_GL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_GL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_GL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_GL NULL
#define SHADPS4_SHADER_CACHE_LABEL_GL NULL
#define SHADPS4_SHADER_CACHE_INFO_0_GL NULL
#define SHADPS4_SHOW_FPS_LABEL_GL NULL
#define SHADPS4_SHOW_FPS_INFO_0_GL NULL
#define SHADPS4_VOLUME_LABEL_GL NULL
#define SHADPS4_VOLUME_INFO_0_GL NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_GL NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_GL NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_GL NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_GL NULL
#define OPTION_VAL_40_GL NULL
#define OPTION_VAL_50_GL NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_GL NULL
#define OPTION_VAL_70_GL NULL
#define OPTION_VAL_80_GL NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_GL NULL
#define OPTION_VAL_100_GL NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_GL NULL
#define OPTION_VAL_150_GL NULL
#define OPTION_VAL_200_GL NULL
#define OPTION_VAL_300_GL NULL
#define OPTION_VAL_400_GL NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_GL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_GL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_GL NULL
#define SHADPS4_TROPHY_POSITION_LABEL_GL NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_GL NULL
#define OPTION_VAL_LEFT_GL NULL
#define OPTION_VAL_RIGHT_GL NULL
#define OPTION_VAL_TOP_GL NULL
#define OPTION_VAL_BOTTOM_GL NULL
#define SHADPS4_TROPHY_DURATION_LABEL_GL NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_GL NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_GL NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_GL NULL
#define SHADPS4_LOG_INFO_0_GL NULL
#define SHADPS4_LOG_SEPARATE_LABEL_GL NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_GL NULL
#define SHADPS4_LOG_SYNC_LABEL_GL NULL
#define SHADPS4_LOG_SYNC_INFO_0_GL NULL

struct retro_core_option_v2_category option_cats_gl[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_GL,
        CATEGORY_SYSTEM_INFO_0_GL
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_GL,
        CATEGORY_VIDEO_INFO_0_GL
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_GL,
        CATEGORY_AUDIO_INFO_0_GL
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_GL,
        CATEGORY_TROPHIES_INFO_0_GL
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_GL,
        CATEGORY_LOGGING_INFO_0_GL
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_gl[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_GL,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_GL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_GL,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_GL,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_GL },
            { "18", OPTION_VAL_18_GL },
            { "21", OPTION_VAL_21_GL },
            { "23", OPTION_VAL_23_GL },
            { "14", OPTION_VAL_14_GL },
            { "6", OPTION_VAL_6_GL },
            { "12", OPTION_VAL_12_GL },
            { "22", OPTION_VAL_22_GL },
            { "2", OPTION_VAL_2_GL },
            { "4", OPTION_VAL_4_GL },
            { "25", OPTION_VAL_25_GL },
            { "24", OPTION_VAL_24_GL },
            { "29", OPTION_VAL_29_GL },
            { "5", OPTION_VAL_5_GL },
            { "0", OPTION_VAL_0_GL },
            { "9", OPTION_VAL_9_GL },
            { "15", OPTION_VAL_15_GL },
            { "16", OPTION_VAL_16_GL },
            { "17", OPTION_VAL_17_GL },
            { "7", OPTION_VAL_7_GL },
            { "26", OPTION_VAL_26_GL },
            { "8", OPTION_VAL_8_GL },
            { "11", OPTION_VAL_11_GL },
            { "20", OPTION_VAL_20_GL },
            { "3", OPTION_VAL_3_GL },
            { "13", OPTION_VAL_13_GL },
            { "27", OPTION_VAL_27_GL },
            { "10", OPTION_VAL_10_GL },
            { "19", OPTION_VAL_19_GL },
            { "30", OPTION_VAL_30_GL },
            { "28", OPTION_VAL_28_GL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_GL,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_GL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_GL,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_GL,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_GL },
            { "512", OPTION_VAL_512_GL },
            { "1024", OPTION_VAL_1024_GL },
            { "2048", OPTION_VAL_2048_GL },
            { "3072", OPTION_VAL_3072_GL },
            { "4096", OPTION_VAL_4096_GL },
            { "6144", OPTION_VAL_6144_GL },
            { "8192", OPTION_VAL_8192_GL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_GL,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_GL,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_GL },
            { "circle", OPTION_VAL_CIRCLE_GL },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_GL,
        NULL,
        SHADPS4_NETWORK_INFO_0_GL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_GL,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_GL,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_GL,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_GL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_GL,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_GL,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_GL },
            { "1920x1080", OPTION_VAL_1920X1080_GL },
            { "2560x1440", OPTION_VAL_2560X1440_GL },
            { "3840x2160", OPTION_VAL_3840X2160_GL },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_GL,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_GL,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_GL },
            { "60",  OPTION_VAL_60_GL },
            { "90",  OPTION_VAL_90_GL },
            { "120", OPTION_VAL_120_GL },
            { "144", OPTION_VAL_144_GL },
            { "240", OPTION_VAL_240_GL },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_GL,
        NULL,
        SHADPS4_READBACKS_INFO_0_GL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_GL },
            { "precise",  OPTION_VAL_PRECISE_GL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_GL,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_GL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_GL,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_GL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_GL,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_GL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_GL,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_GL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_GL,
        NULL,
        SHADPS4_VOLUME_INFO_0_GL,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_GL },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_GL },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_GL },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_GL },
            { "40",  OPTION_VAL_40_GL },
            { "50",  OPTION_VAL_50_GL },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_GL },
            { "70",  OPTION_VAL_70_GL },
            { "80",  OPTION_VAL_80_GL },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_GL },
            { "100", OPTION_VAL_100_GL },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_GL },
            { "150", OPTION_VAL_150_GL },
            { "200", OPTION_VAL_200_GL },
            { "300", OPTION_VAL_300_GL },
            { "400", OPTION_VAL_400_GL },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_GL },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_GL,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_GL,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_GL,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_GL,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_GL },
            { "right", OPTION_VAL_RIGHT_GL },
            { "top", OPTION_VAL_TOP_GL },
            { "bottom", OPTION_VAL_BOTTOM_GL },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_GL,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_GL,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_GL },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_GL },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_GL },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_GL },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_GL },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_GL },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_GL },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_GL },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_GL },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_GL },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_GL,
        NULL,
        SHADPS4_LOG_INFO_0_GL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_GL,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_GL,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_GL,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_GL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_gl = {
   option_cats_gl,
   option_defs_gl
};

/* RETRO_LANGUAGE_HE */

#define CATEGORY_SYSTEM_LABEL_HE NULL
#define CATEGORY_SYSTEM_INFO_0_HE NULL
#define CATEGORY_VIDEO_LABEL_HE NULL
#define CATEGORY_VIDEO_INFO_0_HE NULL
#define CATEGORY_AUDIO_LABEL_HE NULL
#define CATEGORY_AUDIO_INFO_0_HE NULL
#define CATEGORY_TROPHIES_LABEL_HE NULL
#define CATEGORY_TROPHIES_INFO_0_HE NULL
#define CATEGORY_LOGGING_LABEL_HE NULL
#define CATEGORY_LOGGING_INFO_0_HE NULL
#define SHADPS4_NEO_MODE_LABEL_HE NULL
#define SHADPS4_NEO_MODE_INFO_0_HE NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_HE NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_HE NULL
#define OPTION_VAL_1_HE NULL
#define OPTION_VAL_18_HE NULL
#define OPTION_VAL_21_HE NULL
#define OPTION_VAL_23_HE NULL
#define OPTION_VAL_14_HE NULL
#define OPTION_VAL_6_HE NULL
#define OPTION_VAL_12_HE NULL
#define OPTION_VAL_22_HE NULL
#define OPTION_VAL_2_HE NULL
#define OPTION_VAL_4_HE NULL
#define OPTION_VAL_25_HE NULL
#define OPTION_VAL_24_HE NULL
#define OPTION_VAL_29_HE NULL
#define OPTION_VAL_5_HE NULL
#define OPTION_VAL_0_HE NULL
#define OPTION_VAL_9_HE NULL
#define OPTION_VAL_15_HE NULL
#define OPTION_VAL_16_HE NULL
#define OPTION_VAL_17_HE NULL
#define OPTION_VAL_7_HE NULL
#define OPTION_VAL_26_HE NULL
#define OPTION_VAL_8_HE NULL
#define OPTION_VAL_11_HE NULL
#define OPTION_VAL_20_HE NULL
#define OPTION_VAL_3_HE NULL
#define OPTION_VAL_13_HE NULL
#define OPTION_VAL_27_HE NULL
#define OPTION_VAL_10_HE NULL
#define OPTION_VAL_19_HE NULL
#define OPTION_VAL_30_HE NULL
#define OPTION_VAL_28_HE NULL
#define SHADPS4_DEV_KIT_LABEL_HE NULL
#define SHADPS4_DEV_KIT_INFO_0_HE NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_HE NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_HE NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_HE NULL
#define OPTION_VAL_512_HE NULL
#define OPTION_VAL_1024_HE NULL
#define OPTION_VAL_2048_HE NULL
#define OPTION_VAL_3072_HE NULL
#define OPTION_VAL_4096_HE NULL
#define OPTION_VAL_6144_HE NULL
#define OPTION_VAL_8192_HE NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_HE NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_HE NULL
#define OPTION_VAL_CROSS_HE NULL
#define OPTION_VAL_CIRCLE_HE NULL
#define SHADPS4_NETWORK_LABEL_HE NULL
#define SHADPS4_NETWORK_INFO_0_HE NULL
#define SHADPS4_CONTROLLERS_LABEL_HE NULL
#define SHADPS4_CONTROLLERS_INFO_0_HE NULL
#define SHADPS4_SHOW_SPLASH_LABEL_HE NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_HE NULL
#define SHADPS4_RESOLUTION_LABEL_HE NULL
#define SHADPS4_RESOLUTION_INFO_0_HE NULL
#define OPTION_VAL_1280X720_HE NULL
#define OPTION_VAL_1920X1080_HE NULL
#define OPTION_VAL_2560X1440_HE NULL
#define OPTION_VAL_3840X2160_HE NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_HE NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_HE NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_HE NULL
#define OPTION_VAL_60_HE NULL
#define OPTION_VAL_90_HE NULL
#define OPTION_VAL_120_HE NULL
#define OPTION_VAL_144_HE NULL
#define OPTION_VAL_240_HE NULL
#define SHADPS4_READBACKS_LABEL_HE NULL
#define SHADPS4_READBACKS_INFO_0_HE NULL
#define OPTION_VAL_RELAXED_HE NULL
#define OPTION_VAL_PRECISE_HE NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_HE NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_HE NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_HE NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_HE NULL
#define SHADPS4_SHADER_CACHE_LABEL_HE NULL
#define SHADPS4_SHADER_CACHE_INFO_0_HE NULL
#define SHADPS4_SHOW_FPS_LABEL_HE NULL
#define SHADPS4_SHOW_FPS_INFO_0_HE NULL
#define SHADPS4_VOLUME_LABEL_HE NULL
#define SHADPS4_VOLUME_INFO_0_HE NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_HE NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_HE NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_HE NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_HE NULL
#define OPTION_VAL_40_HE NULL
#define OPTION_VAL_50_HE NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_HE NULL
#define OPTION_VAL_70_HE NULL
#define OPTION_VAL_80_HE NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_HE NULL
#define OPTION_VAL_100_HE NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_HE NULL
#define OPTION_VAL_150_HE NULL
#define OPTION_VAL_200_HE NULL
#define OPTION_VAL_300_HE NULL
#define OPTION_VAL_400_HE NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_HE NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_HE NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_HE NULL
#define SHADPS4_TROPHY_POSITION_LABEL_HE NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_HE NULL
#define OPTION_VAL_LEFT_HE NULL
#define OPTION_VAL_RIGHT_HE NULL
#define OPTION_VAL_TOP_HE NULL
#define OPTION_VAL_BOTTOM_HE NULL
#define SHADPS4_TROPHY_DURATION_LABEL_HE NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_HE NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_HE NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_HE NULL
#define SHADPS4_LOG_INFO_0_HE NULL
#define SHADPS4_LOG_SEPARATE_LABEL_HE NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_HE NULL
#define SHADPS4_LOG_SYNC_LABEL_HE NULL
#define SHADPS4_LOG_SYNC_INFO_0_HE NULL

struct retro_core_option_v2_category option_cats_he[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_HE,
        CATEGORY_SYSTEM_INFO_0_HE
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_HE,
        CATEGORY_VIDEO_INFO_0_HE
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_HE,
        CATEGORY_AUDIO_INFO_0_HE
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_HE,
        CATEGORY_TROPHIES_INFO_0_HE
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_HE,
        CATEGORY_LOGGING_INFO_0_HE
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_he[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_HE,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_HE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_HE,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_HE,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_HE },
            { "18", OPTION_VAL_18_HE },
            { "21", OPTION_VAL_21_HE },
            { "23", OPTION_VAL_23_HE },
            { "14", OPTION_VAL_14_HE },
            { "6", OPTION_VAL_6_HE },
            { "12", OPTION_VAL_12_HE },
            { "22", OPTION_VAL_22_HE },
            { "2", OPTION_VAL_2_HE },
            { "4", OPTION_VAL_4_HE },
            { "25", OPTION_VAL_25_HE },
            { "24", OPTION_VAL_24_HE },
            { "29", OPTION_VAL_29_HE },
            { "5", OPTION_VAL_5_HE },
            { "0", OPTION_VAL_0_HE },
            { "9", OPTION_VAL_9_HE },
            { "15", OPTION_VAL_15_HE },
            { "16", OPTION_VAL_16_HE },
            { "17", OPTION_VAL_17_HE },
            { "7", OPTION_VAL_7_HE },
            { "26", OPTION_VAL_26_HE },
            { "8", OPTION_VAL_8_HE },
            { "11", OPTION_VAL_11_HE },
            { "20", OPTION_VAL_20_HE },
            { "3", OPTION_VAL_3_HE },
            { "13", OPTION_VAL_13_HE },
            { "27", OPTION_VAL_27_HE },
            { "10", OPTION_VAL_10_HE },
            { "19", OPTION_VAL_19_HE },
            { "30", OPTION_VAL_30_HE },
            { "28", OPTION_VAL_28_HE },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_HE,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_HE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_HE,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_HE,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_HE },
            { "512", OPTION_VAL_512_HE },
            { "1024", OPTION_VAL_1024_HE },
            { "2048", OPTION_VAL_2048_HE },
            { "3072", OPTION_VAL_3072_HE },
            { "4096", OPTION_VAL_4096_HE },
            { "6144", OPTION_VAL_6144_HE },
            { "8192", OPTION_VAL_8192_HE },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_HE,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_HE,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_HE },
            { "circle", OPTION_VAL_CIRCLE_HE },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_HE,
        NULL,
        SHADPS4_NETWORK_INFO_0_HE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_HE,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_HE,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_HE,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_HE,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_HE,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_HE,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_HE },
            { "1920x1080", OPTION_VAL_1920X1080_HE },
            { "2560x1440", OPTION_VAL_2560X1440_HE },
            { "3840x2160", OPTION_VAL_3840X2160_HE },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_HE,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_HE,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_HE },
            { "60",  OPTION_VAL_60_HE },
            { "90",  OPTION_VAL_90_HE },
            { "120", OPTION_VAL_120_HE },
            { "144", OPTION_VAL_144_HE },
            { "240", OPTION_VAL_240_HE },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_HE,
        NULL,
        SHADPS4_READBACKS_INFO_0_HE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_HE },
            { "precise",  OPTION_VAL_PRECISE_HE },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_HE,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_HE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_HE,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_HE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_HE,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_HE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_HE,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_HE,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_HE,
        NULL,
        SHADPS4_VOLUME_INFO_0_HE,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_HE },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_HE },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_HE },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_HE },
            { "40",  OPTION_VAL_40_HE },
            { "50",  OPTION_VAL_50_HE },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_HE },
            { "70",  OPTION_VAL_70_HE },
            { "80",  OPTION_VAL_80_HE },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_HE },
            { "100", OPTION_VAL_100_HE },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_HE },
            { "150", OPTION_VAL_150_HE },
            { "200", OPTION_VAL_200_HE },
            { "300", OPTION_VAL_300_HE },
            { "400", OPTION_VAL_400_HE },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_HE },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_HE,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_HE,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_HE,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_HE,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_HE },
            { "right", OPTION_VAL_RIGHT_HE },
            { "top", OPTION_VAL_TOP_HE },
            { "bottom", OPTION_VAL_BOTTOM_HE },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_HE,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_HE,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_HE },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_HE },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_HE },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_HE },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_HE },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_HE },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_HE },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_HE },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_HE },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_HE },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_HE,
        NULL,
        SHADPS4_LOG_INFO_0_HE,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_HE,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_HE,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_HE,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_HE,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_he = {
   option_cats_he,
   option_defs_he
};

/* RETRO_LANGUAGE_HU */

#define CATEGORY_SYSTEM_LABEL_HU NULL
#define CATEGORY_SYSTEM_INFO_0_HU NULL
#define CATEGORY_VIDEO_LABEL_HU NULL
#define CATEGORY_VIDEO_INFO_0_HU NULL
#define CATEGORY_AUDIO_LABEL_HU NULL
#define CATEGORY_AUDIO_INFO_0_HU NULL
#define CATEGORY_TROPHIES_LABEL_HU NULL
#define CATEGORY_TROPHIES_INFO_0_HU NULL
#define CATEGORY_LOGGING_LABEL_HU NULL
#define CATEGORY_LOGGING_INFO_0_HU NULL
#define SHADPS4_NEO_MODE_LABEL_HU NULL
#define SHADPS4_NEO_MODE_INFO_0_HU NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_HU NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_HU NULL
#define OPTION_VAL_1_HU NULL
#define OPTION_VAL_18_HU NULL
#define OPTION_VAL_21_HU NULL
#define OPTION_VAL_23_HU NULL
#define OPTION_VAL_14_HU NULL
#define OPTION_VAL_6_HU NULL
#define OPTION_VAL_12_HU NULL
#define OPTION_VAL_22_HU NULL
#define OPTION_VAL_2_HU NULL
#define OPTION_VAL_4_HU NULL
#define OPTION_VAL_25_HU NULL
#define OPTION_VAL_24_HU NULL
#define OPTION_VAL_29_HU NULL
#define OPTION_VAL_5_HU NULL
#define OPTION_VAL_0_HU NULL
#define OPTION_VAL_9_HU NULL
#define OPTION_VAL_15_HU NULL
#define OPTION_VAL_16_HU NULL
#define OPTION_VAL_17_HU NULL
#define OPTION_VAL_7_HU NULL
#define OPTION_VAL_26_HU NULL
#define OPTION_VAL_8_HU NULL
#define OPTION_VAL_11_HU NULL
#define OPTION_VAL_20_HU NULL
#define OPTION_VAL_3_HU NULL
#define OPTION_VAL_13_HU NULL
#define OPTION_VAL_27_HU NULL
#define OPTION_VAL_10_HU NULL
#define OPTION_VAL_19_HU NULL
#define OPTION_VAL_30_HU NULL
#define OPTION_VAL_28_HU NULL
#define SHADPS4_DEV_KIT_LABEL_HU NULL
#define SHADPS4_DEV_KIT_INFO_0_HU NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_HU NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_HU NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_HU NULL
#define OPTION_VAL_512_HU NULL
#define OPTION_VAL_1024_HU NULL
#define OPTION_VAL_2048_HU NULL
#define OPTION_VAL_3072_HU NULL
#define OPTION_VAL_4096_HU NULL
#define OPTION_VAL_6144_HU NULL
#define OPTION_VAL_8192_HU NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_HU NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_HU NULL
#define OPTION_VAL_CROSS_HU NULL
#define OPTION_VAL_CIRCLE_HU NULL
#define SHADPS4_NETWORK_LABEL_HU NULL
#define SHADPS4_NETWORK_INFO_0_HU NULL
#define SHADPS4_CONTROLLERS_LABEL_HU NULL
#define SHADPS4_CONTROLLERS_INFO_0_HU NULL
#define SHADPS4_SHOW_SPLASH_LABEL_HU NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_HU NULL
#define SHADPS4_RESOLUTION_LABEL_HU NULL
#define SHADPS4_RESOLUTION_INFO_0_HU NULL
#define OPTION_VAL_1280X720_HU NULL
#define OPTION_VAL_1920X1080_HU NULL
#define OPTION_VAL_2560X1440_HU NULL
#define OPTION_VAL_3840X2160_HU NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_HU NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_HU NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_HU NULL
#define OPTION_VAL_60_HU NULL
#define OPTION_VAL_90_HU NULL
#define OPTION_VAL_120_HU NULL
#define OPTION_VAL_144_HU NULL
#define OPTION_VAL_240_HU NULL
#define SHADPS4_READBACKS_LABEL_HU NULL
#define SHADPS4_READBACKS_INFO_0_HU NULL
#define OPTION_VAL_RELAXED_HU NULL
#define OPTION_VAL_PRECISE_HU NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_HU NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_HU NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_HU NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_HU NULL
#define SHADPS4_SHADER_CACHE_LABEL_HU NULL
#define SHADPS4_SHADER_CACHE_INFO_0_HU NULL
#define SHADPS4_SHOW_FPS_LABEL_HU NULL
#define SHADPS4_SHOW_FPS_INFO_0_HU NULL
#define SHADPS4_VOLUME_LABEL_HU NULL
#define SHADPS4_VOLUME_INFO_0_HU NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_HU NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_HU NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_HU NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_HU NULL
#define OPTION_VAL_40_HU NULL
#define OPTION_VAL_50_HU NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_HU NULL
#define OPTION_VAL_70_HU NULL
#define OPTION_VAL_80_HU NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_HU NULL
#define OPTION_VAL_100_HU NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_HU NULL
#define OPTION_VAL_150_HU NULL
#define OPTION_VAL_200_HU NULL
#define OPTION_VAL_300_HU NULL
#define OPTION_VAL_400_HU NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_HU NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_HU NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_HU NULL
#define SHADPS4_TROPHY_POSITION_LABEL_HU NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_HU NULL
#define OPTION_VAL_LEFT_HU NULL
#define OPTION_VAL_RIGHT_HU NULL
#define OPTION_VAL_TOP_HU NULL
#define OPTION_VAL_BOTTOM_HU NULL
#define SHADPS4_TROPHY_DURATION_LABEL_HU NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_HU NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_HU NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_HU NULL
#define SHADPS4_LOG_INFO_0_HU NULL
#define SHADPS4_LOG_SEPARATE_LABEL_HU NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_HU NULL
#define SHADPS4_LOG_SYNC_LABEL_HU NULL
#define SHADPS4_LOG_SYNC_INFO_0_HU NULL

struct retro_core_option_v2_category option_cats_hu[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_HU,
        CATEGORY_SYSTEM_INFO_0_HU
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_HU,
        CATEGORY_VIDEO_INFO_0_HU
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_HU,
        CATEGORY_AUDIO_INFO_0_HU
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_HU,
        CATEGORY_TROPHIES_INFO_0_HU
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_HU,
        CATEGORY_LOGGING_INFO_0_HU
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_hu[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_HU,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_HU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_HU,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_HU,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_HU },
            { "18", OPTION_VAL_18_HU },
            { "21", OPTION_VAL_21_HU },
            { "23", OPTION_VAL_23_HU },
            { "14", OPTION_VAL_14_HU },
            { "6", OPTION_VAL_6_HU },
            { "12", OPTION_VAL_12_HU },
            { "22", OPTION_VAL_22_HU },
            { "2", OPTION_VAL_2_HU },
            { "4", OPTION_VAL_4_HU },
            { "25", OPTION_VAL_25_HU },
            { "24", OPTION_VAL_24_HU },
            { "29", OPTION_VAL_29_HU },
            { "5", OPTION_VAL_5_HU },
            { "0", OPTION_VAL_0_HU },
            { "9", OPTION_VAL_9_HU },
            { "15", OPTION_VAL_15_HU },
            { "16", OPTION_VAL_16_HU },
            { "17", OPTION_VAL_17_HU },
            { "7", OPTION_VAL_7_HU },
            { "26", OPTION_VAL_26_HU },
            { "8", OPTION_VAL_8_HU },
            { "11", OPTION_VAL_11_HU },
            { "20", OPTION_VAL_20_HU },
            { "3", OPTION_VAL_3_HU },
            { "13", OPTION_VAL_13_HU },
            { "27", OPTION_VAL_27_HU },
            { "10", OPTION_VAL_10_HU },
            { "19", OPTION_VAL_19_HU },
            { "30", OPTION_VAL_30_HU },
            { "28", OPTION_VAL_28_HU },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_HU,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_HU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_HU,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_HU,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_HU },
            { "512", OPTION_VAL_512_HU },
            { "1024", OPTION_VAL_1024_HU },
            { "2048", OPTION_VAL_2048_HU },
            { "3072", OPTION_VAL_3072_HU },
            { "4096", OPTION_VAL_4096_HU },
            { "6144", OPTION_VAL_6144_HU },
            { "8192", OPTION_VAL_8192_HU },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_HU,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_HU,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_HU },
            { "circle", OPTION_VAL_CIRCLE_HU },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_HU,
        NULL,
        SHADPS4_NETWORK_INFO_0_HU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_HU,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_HU,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_HU,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_HU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_HU,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_HU,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_HU },
            { "1920x1080", OPTION_VAL_1920X1080_HU },
            { "2560x1440", OPTION_VAL_2560X1440_HU },
            { "3840x2160", OPTION_VAL_3840X2160_HU },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_HU,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_HU,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_HU },
            { "60",  OPTION_VAL_60_HU },
            { "90",  OPTION_VAL_90_HU },
            { "120", OPTION_VAL_120_HU },
            { "144", OPTION_VAL_144_HU },
            { "240", OPTION_VAL_240_HU },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_HU,
        NULL,
        SHADPS4_READBACKS_INFO_0_HU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_HU },
            { "precise",  OPTION_VAL_PRECISE_HU },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_HU,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_HU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_HU,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_HU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_HU,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_HU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_HU,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_HU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_HU,
        NULL,
        SHADPS4_VOLUME_INFO_0_HU,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_HU },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_HU },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_HU },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_HU },
            { "40",  OPTION_VAL_40_HU },
            { "50",  OPTION_VAL_50_HU },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_HU },
            { "70",  OPTION_VAL_70_HU },
            { "80",  OPTION_VAL_80_HU },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_HU },
            { "100", OPTION_VAL_100_HU },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_HU },
            { "150", OPTION_VAL_150_HU },
            { "200", OPTION_VAL_200_HU },
            { "300", OPTION_VAL_300_HU },
            { "400", OPTION_VAL_400_HU },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_HU },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_HU,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_HU,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_HU,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_HU,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_HU },
            { "right", OPTION_VAL_RIGHT_HU },
            { "top", OPTION_VAL_TOP_HU },
            { "bottom", OPTION_VAL_BOTTOM_HU },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_HU,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_HU,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_HU },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_HU },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_HU },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_HU },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_HU },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_HU },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_HU },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_HU },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_HU },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_HU },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_HU,
        NULL,
        SHADPS4_LOG_INFO_0_HU,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_HU,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_HU,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_HU,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_HU,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_hu = {
   option_cats_hu,
   option_defs_hu
};

/* RETRO_LANGUAGE_ID */

#define CATEGORY_SYSTEM_LABEL_ID NULL
#define CATEGORY_SYSTEM_INFO_0_ID NULL
#define CATEGORY_VIDEO_LABEL_ID NULL
#define CATEGORY_VIDEO_INFO_0_ID NULL
#define CATEGORY_AUDIO_LABEL_ID NULL
#define CATEGORY_AUDIO_INFO_0_ID NULL
#define CATEGORY_TROPHIES_LABEL_ID NULL
#define CATEGORY_TROPHIES_INFO_0_ID NULL
#define CATEGORY_LOGGING_LABEL_ID NULL
#define CATEGORY_LOGGING_INFO_0_ID NULL
#define SHADPS4_NEO_MODE_LABEL_ID NULL
#define SHADPS4_NEO_MODE_INFO_0_ID NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_ID NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_ID NULL
#define OPTION_VAL_1_ID NULL
#define OPTION_VAL_18_ID NULL
#define OPTION_VAL_21_ID NULL
#define OPTION_VAL_23_ID NULL
#define OPTION_VAL_14_ID NULL
#define OPTION_VAL_6_ID NULL
#define OPTION_VAL_12_ID NULL
#define OPTION_VAL_22_ID NULL
#define OPTION_VAL_2_ID NULL
#define OPTION_VAL_4_ID NULL
#define OPTION_VAL_25_ID NULL
#define OPTION_VAL_24_ID NULL
#define OPTION_VAL_29_ID NULL
#define OPTION_VAL_5_ID NULL
#define OPTION_VAL_0_ID NULL
#define OPTION_VAL_9_ID NULL
#define OPTION_VAL_15_ID NULL
#define OPTION_VAL_16_ID NULL
#define OPTION_VAL_17_ID NULL
#define OPTION_VAL_7_ID NULL
#define OPTION_VAL_26_ID NULL
#define OPTION_VAL_8_ID NULL
#define OPTION_VAL_11_ID NULL
#define OPTION_VAL_20_ID NULL
#define OPTION_VAL_3_ID NULL
#define OPTION_VAL_13_ID NULL
#define OPTION_VAL_27_ID NULL
#define OPTION_VAL_10_ID NULL
#define OPTION_VAL_19_ID NULL
#define OPTION_VAL_30_ID NULL
#define OPTION_VAL_28_ID NULL
#define SHADPS4_DEV_KIT_LABEL_ID NULL
#define SHADPS4_DEV_KIT_INFO_0_ID NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_ID NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_ID NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_ID NULL
#define OPTION_VAL_512_ID NULL
#define OPTION_VAL_1024_ID NULL
#define OPTION_VAL_2048_ID NULL
#define OPTION_VAL_3072_ID NULL
#define OPTION_VAL_4096_ID NULL
#define OPTION_VAL_6144_ID NULL
#define OPTION_VAL_8192_ID NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_ID NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_ID NULL
#define OPTION_VAL_CROSS_ID NULL
#define OPTION_VAL_CIRCLE_ID NULL
#define SHADPS4_NETWORK_LABEL_ID NULL
#define SHADPS4_NETWORK_INFO_0_ID NULL
#define SHADPS4_CONTROLLERS_LABEL_ID NULL
#define SHADPS4_CONTROLLERS_INFO_0_ID NULL
#define SHADPS4_SHOW_SPLASH_LABEL_ID NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_ID NULL
#define SHADPS4_RESOLUTION_LABEL_ID NULL
#define SHADPS4_RESOLUTION_INFO_0_ID NULL
#define OPTION_VAL_1280X720_ID NULL
#define OPTION_VAL_1920X1080_ID NULL
#define OPTION_VAL_2560X1440_ID NULL
#define OPTION_VAL_3840X2160_ID NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_ID NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_ID NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_ID NULL
#define OPTION_VAL_60_ID NULL
#define OPTION_VAL_90_ID NULL
#define OPTION_VAL_120_ID NULL
#define OPTION_VAL_144_ID NULL
#define OPTION_VAL_240_ID NULL
#define SHADPS4_READBACKS_LABEL_ID NULL
#define SHADPS4_READBACKS_INFO_0_ID NULL
#define OPTION_VAL_RELAXED_ID NULL
#define OPTION_VAL_PRECISE_ID NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_ID NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_ID NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_ID NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_ID NULL
#define SHADPS4_SHADER_CACHE_LABEL_ID NULL
#define SHADPS4_SHADER_CACHE_INFO_0_ID NULL
#define SHADPS4_SHOW_FPS_LABEL_ID NULL
#define SHADPS4_SHOW_FPS_INFO_0_ID NULL
#define SHADPS4_VOLUME_LABEL_ID NULL
#define SHADPS4_VOLUME_INFO_0_ID NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_ID NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_ID NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_ID NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_ID NULL
#define OPTION_VAL_40_ID NULL
#define OPTION_VAL_50_ID NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_ID NULL
#define OPTION_VAL_70_ID NULL
#define OPTION_VAL_80_ID NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_ID NULL
#define OPTION_VAL_100_ID NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_ID NULL
#define OPTION_VAL_150_ID NULL
#define OPTION_VAL_200_ID NULL
#define OPTION_VAL_300_ID NULL
#define OPTION_VAL_400_ID NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_ID NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_ID NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_ID NULL
#define SHADPS4_TROPHY_POSITION_LABEL_ID NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_ID NULL
#define OPTION_VAL_LEFT_ID NULL
#define OPTION_VAL_RIGHT_ID NULL
#define OPTION_VAL_TOP_ID NULL
#define OPTION_VAL_BOTTOM_ID NULL
#define SHADPS4_TROPHY_DURATION_LABEL_ID NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_ID NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_ID NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_ID NULL
#define SHADPS4_LOG_INFO_0_ID NULL
#define SHADPS4_LOG_SEPARATE_LABEL_ID NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_ID NULL
#define SHADPS4_LOG_SYNC_LABEL_ID NULL
#define SHADPS4_LOG_SYNC_INFO_0_ID NULL

struct retro_core_option_v2_category option_cats_id[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_ID,
        CATEGORY_SYSTEM_INFO_0_ID
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_ID,
        CATEGORY_VIDEO_INFO_0_ID
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_ID,
        CATEGORY_AUDIO_INFO_0_ID
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_ID,
        CATEGORY_TROPHIES_INFO_0_ID
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_ID,
        CATEGORY_LOGGING_INFO_0_ID
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_id[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_ID,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_ID,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_ID,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_ID,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_ID },
            { "18", OPTION_VAL_18_ID },
            { "21", OPTION_VAL_21_ID },
            { "23", OPTION_VAL_23_ID },
            { "14", OPTION_VAL_14_ID },
            { "6", OPTION_VAL_6_ID },
            { "12", OPTION_VAL_12_ID },
            { "22", OPTION_VAL_22_ID },
            { "2", OPTION_VAL_2_ID },
            { "4", OPTION_VAL_4_ID },
            { "25", OPTION_VAL_25_ID },
            { "24", OPTION_VAL_24_ID },
            { "29", OPTION_VAL_29_ID },
            { "5", OPTION_VAL_5_ID },
            { "0", OPTION_VAL_0_ID },
            { "9", OPTION_VAL_9_ID },
            { "15", OPTION_VAL_15_ID },
            { "16", OPTION_VAL_16_ID },
            { "17", OPTION_VAL_17_ID },
            { "7", OPTION_VAL_7_ID },
            { "26", OPTION_VAL_26_ID },
            { "8", OPTION_VAL_8_ID },
            { "11", OPTION_VAL_11_ID },
            { "20", OPTION_VAL_20_ID },
            { "3", OPTION_VAL_3_ID },
            { "13", OPTION_VAL_13_ID },
            { "27", OPTION_VAL_27_ID },
            { "10", OPTION_VAL_10_ID },
            { "19", OPTION_VAL_19_ID },
            { "30", OPTION_VAL_30_ID },
            { "28", OPTION_VAL_28_ID },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_ID,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_ID,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_ID,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_ID,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_ID },
            { "512", OPTION_VAL_512_ID },
            { "1024", OPTION_VAL_1024_ID },
            { "2048", OPTION_VAL_2048_ID },
            { "3072", OPTION_VAL_3072_ID },
            { "4096", OPTION_VAL_4096_ID },
            { "6144", OPTION_VAL_6144_ID },
            { "8192", OPTION_VAL_8192_ID },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_ID,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_ID,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_ID },
            { "circle", OPTION_VAL_CIRCLE_ID },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_ID,
        NULL,
        SHADPS4_NETWORK_INFO_0_ID,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_ID,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_ID,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_ID,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_ID,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_ID,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_ID,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_ID },
            { "1920x1080", OPTION_VAL_1920X1080_ID },
            { "2560x1440", OPTION_VAL_2560X1440_ID },
            { "3840x2160", OPTION_VAL_3840X2160_ID },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_ID,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_ID,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_ID },
            { "60",  OPTION_VAL_60_ID },
            { "90",  OPTION_VAL_90_ID },
            { "120", OPTION_VAL_120_ID },
            { "144", OPTION_VAL_144_ID },
            { "240", OPTION_VAL_240_ID },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_ID,
        NULL,
        SHADPS4_READBACKS_INFO_0_ID,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_ID },
            { "precise",  OPTION_VAL_PRECISE_ID },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_ID,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_ID,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_ID,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_ID,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_ID,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_ID,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_ID,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_ID,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_ID,
        NULL,
        SHADPS4_VOLUME_INFO_0_ID,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_ID },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_ID },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_ID },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_ID },
            { "40",  OPTION_VAL_40_ID },
            { "50",  OPTION_VAL_50_ID },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_ID },
            { "70",  OPTION_VAL_70_ID },
            { "80",  OPTION_VAL_80_ID },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_ID },
            { "100", OPTION_VAL_100_ID },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_ID },
            { "150", OPTION_VAL_150_ID },
            { "200", OPTION_VAL_200_ID },
            { "300", OPTION_VAL_300_ID },
            { "400", OPTION_VAL_400_ID },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_ID },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_ID,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_ID,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_ID,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_ID,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_ID },
            { "right", OPTION_VAL_RIGHT_ID },
            { "top", OPTION_VAL_TOP_ID },
            { "bottom", OPTION_VAL_BOTTOM_ID },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_ID,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_ID,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_ID },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_ID },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_ID },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_ID },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_ID },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_ID },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_ID },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_ID },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_ID },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_ID },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_ID,
        NULL,
        SHADPS4_LOG_INFO_0_ID,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_ID,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_ID,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_ID,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_ID,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_id = {
   option_cats_id,
   option_defs_id
};

/* RETRO_LANGUAGE_IT */

#define CATEGORY_SYSTEM_LABEL_IT NULL
#define CATEGORY_SYSTEM_INFO_0_IT NULL
#define CATEGORY_VIDEO_LABEL_IT NULL
#define CATEGORY_VIDEO_INFO_0_IT NULL
#define CATEGORY_AUDIO_LABEL_IT NULL
#define CATEGORY_AUDIO_INFO_0_IT NULL
#define CATEGORY_TROPHIES_LABEL_IT NULL
#define CATEGORY_TROPHIES_INFO_0_IT NULL
#define CATEGORY_LOGGING_LABEL_IT NULL
#define CATEGORY_LOGGING_INFO_0_IT NULL
#define SHADPS4_NEO_MODE_LABEL_IT NULL
#define SHADPS4_NEO_MODE_INFO_0_IT NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_IT NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_IT NULL
#define OPTION_VAL_1_IT NULL
#define OPTION_VAL_18_IT NULL
#define OPTION_VAL_21_IT NULL
#define OPTION_VAL_23_IT NULL
#define OPTION_VAL_14_IT NULL
#define OPTION_VAL_6_IT NULL
#define OPTION_VAL_12_IT NULL
#define OPTION_VAL_22_IT NULL
#define OPTION_VAL_2_IT NULL
#define OPTION_VAL_4_IT NULL
#define OPTION_VAL_25_IT NULL
#define OPTION_VAL_24_IT NULL
#define OPTION_VAL_29_IT NULL
#define OPTION_VAL_5_IT NULL
#define OPTION_VAL_0_IT NULL
#define OPTION_VAL_9_IT NULL
#define OPTION_VAL_15_IT NULL
#define OPTION_VAL_16_IT NULL
#define OPTION_VAL_17_IT NULL
#define OPTION_VAL_7_IT NULL
#define OPTION_VAL_26_IT NULL
#define OPTION_VAL_8_IT NULL
#define OPTION_VAL_11_IT NULL
#define OPTION_VAL_20_IT NULL
#define OPTION_VAL_3_IT NULL
#define OPTION_VAL_13_IT NULL
#define OPTION_VAL_27_IT NULL
#define OPTION_VAL_10_IT NULL
#define OPTION_VAL_19_IT NULL
#define OPTION_VAL_30_IT NULL
#define OPTION_VAL_28_IT NULL
#define SHADPS4_DEV_KIT_LABEL_IT NULL
#define SHADPS4_DEV_KIT_INFO_0_IT NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_IT NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_IT NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_IT NULL
#define OPTION_VAL_512_IT NULL
#define OPTION_VAL_1024_IT NULL
#define OPTION_VAL_2048_IT NULL
#define OPTION_VAL_3072_IT NULL
#define OPTION_VAL_4096_IT NULL
#define OPTION_VAL_6144_IT NULL
#define OPTION_VAL_8192_IT NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_IT NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_IT NULL
#define OPTION_VAL_CROSS_IT NULL
#define OPTION_VAL_CIRCLE_IT NULL
#define SHADPS4_NETWORK_LABEL_IT NULL
#define SHADPS4_NETWORK_INFO_0_IT NULL
#define SHADPS4_CONTROLLERS_LABEL_IT NULL
#define SHADPS4_CONTROLLERS_INFO_0_IT NULL
#define SHADPS4_SHOW_SPLASH_LABEL_IT NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_IT NULL
#define SHADPS4_RESOLUTION_LABEL_IT NULL
#define SHADPS4_RESOLUTION_INFO_0_IT NULL
#define OPTION_VAL_1280X720_IT NULL
#define OPTION_VAL_1920X1080_IT NULL
#define OPTION_VAL_2560X1440_IT NULL
#define OPTION_VAL_3840X2160_IT NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_IT NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_IT NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_IT NULL
#define OPTION_VAL_60_IT NULL
#define OPTION_VAL_90_IT NULL
#define OPTION_VAL_120_IT NULL
#define OPTION_VAL_144_IT NULL
#define OPTION_VAL_240_IT NULL
#define SHADPS4_READBACKS_LABEL_IT NULL
#define SHADPS4_READBACKS_INFO_0_IT NULL
#define OPTION_VAL_RELAXED_IT NULL
#define OPTION_VAL_PRECISE_IT NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_IT NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_IT NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_IT NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_IT NULL
#define SHADPS4_SHADER_CACHE_LABEL_IT NULL
#define SHADPS4_SHADER_CACHE_INFO_0_IT NULL
#define SHADPS4_SHOW_FPS_LABEL_IT NULL
#define SHADPS4_SHOW_FPS_INFO_0_IT NULL
#define SHADPS4_VOLUME_LABEL_IT NULL
#define SHADPS4_VOLUME_INFO_0_IT NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_IT NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_IT NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_IT NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_IT NULL
#define OPTION_VAL_40_IT NULL
#define OPTION_VAL_50_IT NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_IT NULL
#define OPTION_VAL_70_IT NULL
#define OPTION_VAL_80_IT NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_IT NULL
#define OPTION_VAL_100_IT NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_IT NULL
#define OPTION_VAL_150_IT NULL
#define OPTION_VAL_200_IT NULL
#define OPTION_VAL_300_IT NULL
#define OPTION_VAL_400_IT NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_IT NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_IT NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_IT NULL
#define SHADPS4_TROPHY_POSITION_LABEL_IT NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_IT NULL
#define OPTION_VAL_LEFT_IT NULL
#define OPTION_VAL_RIGHT_IT NULL
#define OPTION_VAL_TOP_IT NULL
#define OPTION_VAL_BOTTOM_IT NULL
#define SHADPS4_TROPHY_DURATION_LABEL_IT NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_IT NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_IT NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_IT NULL
#define SHADPS4_LOG_INFO_0_IT NULL
#define SHADPS4_LOG_SEPARATE_LABEL_IT NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_IT NULL
#define SHADPS4_LOG_SYNC_LABEL_IT NULL
#define SHADPS4_LOG_SYNC_INFO_0_IT NULL

struct retro_core_option_v2_category option_cats_it[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_IT,
        CATEGORY_SYSTEM_INFO_0_IT
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_IT,
        CATEGORY_VIDEO_INFO_0_IT
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_IT,
        CATEGORY_AUDIO_INFO_0_IT
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_IT,
        CATEGORY_TROPHIES_INFO_0_IT
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_IT,
        CATEGORY_LOGGING_INFO_0_IT
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_it[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_IT,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_IT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_IT,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_IT,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_IT },
            { "18", OPTION_VAL_18_IT },
            { "21", OPTION_VAL_21_IT },
            { "23", OPTION_VAL_23_IT },
            { "14", OPTION_VAL_14_IT },
            { "6", OPTION_VAL_6_IT },
            { "12", OPTION_VAL_12_IT },
            { "22", OPTION_VAL_22_IT },
            { "2", OPTION_VAL_2_IT },
            { "4", OPTION_VAL_4_IT },
            { "25", OPTION_VAL_25_IT },
            { "24", OPTION_VAL_24_IT },
            { "29", OPTION_VAL_29_IT },
            { "5", OPTION_VAL_5_IT },
            { "0", OPTION_VAL_0_IT },
            { "9", OPTION_VAL_9_IT },
            { "15", OPTION_VAL_15_IT },
            { "16", OPTION_VAL_16_IT },
            { "17", OPTION_VAL_17_IT },
            { "7", OPTION_VAL_7_IT },
            { "26", OPTION_VAL_26_IT },
            { "8", OPTION_VAL_8_IT },
            { "11", OPTION_VAL_11_IT },
            { "20", OPTION_VAL_20_IT },
            { "3", OPTION_VAL_3_IT },
            { "13", OPTION_VAL_13_IT },
            { "27", OPTION_VAL_27_IT },
            { "10", OPTION_VAL_10_IT },
            { "19", OPTION_VAL_19_IT },
            { "30", OPTION_VAL_30_IT },
            { "28", OPTION_VAL_28_IT },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_IT,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_IT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_IT,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_IT,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_IT },
            { "512", OPTION_VAL_512_IT },
            { "1024", OPTION_VAL_1024_IT },
            { "2048", OPTION_VAL_2048_IT },
            { "3072", OPTION_VAL_3072_IT },
            { "4096", OPTION_VAL_4096_IT },
            { "6144", OPTION_VAL_6144_IT },
            { "8192", OPTION_VAL_8192_IT },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_IT,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_IT,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_IT },
            { "circle", OPTION_VAL_CIRCLE_IT },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_IT,
        NULL,
        SHADPS4_NETWORK_INFO_0_IT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_IT,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_IT,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_IT,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_IT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_IT,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_IT,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_IT },
            { "1920x1080", OPTION_VAL_1920X1080_IT },
            { "2560x1440", OPTION_VAL_2560X1440_IT },
            { "3840x2160", OPTION_VAL_3840X2160_IT },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_IT,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_IT,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_IT },
            { "60",  OPTION_VAL_60_IT },
            { "90",  OPTION_VAL_90_IT },
            { "120", OPTION_VAL_120_IT },
            { "144", OPTION_VAL_144_IT },
            { "240", OPTION_VAL_240_IT },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_IT,
        NULL,
        SHADPS4_READBACKS_INFO_0_IT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_IT },
            { "precise",  OPTION_VAL_PRECISE_IT },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_IT,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_IT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_IT,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_IT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_IT,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_IT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_IT,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_IT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_IT,
        NULL,
        SHADPS4_VOLUME_INFO_0_IT,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_IT },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_IT },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_IT },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_IT },
            { "40",  OPTION_VAL_40_IT },
            { "50",  OPTION_VAL_50_IT },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_IT },
            { "70",  OPTION_VAL_70_IT },
            { "80",  OPTION_VAL_80_IT },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_IT },
            { "100", OPTION_VAL_100_IT },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_IT },
            { "150", OPTION_VAL_150_IT },
            { "200", OPTION_VAL_200_IT },
            { "300", OPTION_VAL_300_IT },
            { "400", OPTION_VAL_400_IT },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_IT },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_IT,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_IT,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_IT,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_IT,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_IT },
            { "right", OPTION_VAL_RIGHT_IT },
            { "top", OPTION_VAL_TOP_IT },
            { "bottom", OPTION_VAL_BOTTOM_IT },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_IT,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_IT,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_IT },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_IT },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_IT },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_IT },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_IT },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_IT },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_IT },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_IT },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_IT },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_IT },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_IT,
        NULL,
        SHADPS4_LOG_INFO_0_IT,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_IT,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_IT,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_IT,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_IT,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_it = {
   option_cats_it,
   option_defs_it
};

/* RETRO_LANGUAGE_JA */

#define CATEGORY_SYSTEM_LABEL_JA NULL
#define CATEGORY_SYSTEM_INFO_0_JA NULL
#define CATEGORY_VIDEO_LABEL_JA NULL
#define CATEGORY_VIDEO_INFO_0_JA NULL
#define CATEGORY_AUDIO_LABEL_JA NULL
#define CATEGORY_AUDIO_INFO_0_JA NULL
#define CATEGORY_TROPHIES_LABEL_JA NULL
#define CATEGORY_TROPHIES_INFO_0_JA NULL
#define CATEGORY_LOGGING_LABEL_JA NULL
#define CATEGORY_LOGGING_INFO_0_JA NULL
#define SHADPS4_NEO_MODE_LABEL_JA NULL
#define SHADPS4_NEO_MODE_INFO_0_JA NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_JA NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_JA NULL
#define OPTION_VAL_1_JA NULL
#define OPTION_VAL_18_JA NULL
#define OPTION_VAL_21_JA NULL
#define OPTION_VAL_23_JA NULL
#define OPTION_VAL_14_JA NULL
#define OPTION_VAL_6_JA NULL
#define OPTION_VAL_12_JA NULL
#define OPTION_VAL_22_JA NULL
#define OPTION_VAL_2_JA NULL
#define OPTION_VAL_4_JA NULL
#define OPTION_VAL_25_JA NULL
#define OPTION_VAL_24_JA NULL
#define OPTION_VAL_29_JA NULL
#define OPTION_VAL_5_JA NULL
#define OPTION_VAL_0_JA NULL
#define OPTION_VAL_9_JA NULL
#define OPTION_VAL_15_JA NULL
#define OPTION_VAL_16_JA NULL
#define OPTION_VAL_17_JA NULL
#define OPTION_VAL_7_JA NULL
#define OPTION_VAL_26_JA NULL
#define OPTION_VAL_8_JA NULL
#define OPTION_VAL_11_JA NULL
#define OPTION_VAL_20_JA NULL
#define OPTION_VAL_3_JA NULL
#define OPTION_VAL_13_JA NULL
#define OPTION_VAL_27_JA NULL
#define OPTION_VAL_10_JA NULL
#define OPTION_VAL_19_JA NULL
#define OPTION_VAL_30_JA NULL
#define OPTION_VAL_28_JA NULL
#define SHADPS4_DEV_KIT_LABEL_JA NULL
#define SHADPS4_DEV_KIT_INFO_0_JA NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_JA NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_JA NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_JA NULL
#define OPTION_VAL_512_JA NULL
#define OPTION_VAL_1024_JA NULL
#define OPTION_VAL_2048_JA NULL
#define OPTION_VAL_3072_JA NULL
#define OPTION_VAL_4096_JA NULL
#define OPTION_VAL_6144_JA NULL
#define OPTION_VAL_8192_JA NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_JA NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_JA NULL
#define OPTION_VAL_CROSS_JA NULL
#define OPTION_VAL_CIRCLE_JA NULL
#define SHADPS4_NETWORK_LABEL_JA NULL
#define SHADPS4_NETWORK_INFO_0_JA NULL
#define SHADPS4_CONTROLLERS_LABEL_JA NULL
#define SHADPS4_CONTROLLERS_INFO_0_JA NULL
#define SHADPS4_SHOW_SPLASH_LABEL_JA NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_JA NULL
#define SHADPS4_RESOLUTION_LABEL_JA NULL
#define SHADPS4_RESOLUTION_INFO_0_JA NULL
#define OPTION_VAL_1280X720_JA NULL
#define OPTION_VAL_1920X1080_JA NULL
#define OPTION_VAL_2560X1440_JA NULL
#define OPTION_VAL_3840X2160_JA NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_JA NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_JA NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_JA NULL
#define OPTION_VAL_60_JA NULL
#define OPTION_VAL_90_JA NULL
#define OPTION_VAL_120_JA NULL
#define OPTION_VAL_144_JA NULL
#define OPTION_VAL_240_JA NULL
#define SHADPS4_READBACKS_LABEL_JA NULL
#define SHADPS4_READBACKS_INFO_0_JA NULL
#define OPTION_VAL_RELAXED_JA NULL
#define OPTION_VAL_PRECISE_JA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_JA NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_JA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_JA NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_JA NULL
#define SHADPS4_SHADER_CACHE_LABEL_JA NULL
#define SHADPS4_SHADER_CACHE_INFO_0_JA NULL
#define SHADPS4_SHOW_FPS_LABEL_JA NULL
#define SHADPS4_SHOW_FPS_INFO_0_JA NULL
#define SHADPS4_VOLUME_LABEL_JA NULL
#define SHADPS4_VOLUME_INFO_0_JA NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_JA NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_JA NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_JA NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_JA NULL
#define OPTION_VAL_40_JA NULL
#define OPTION_VAL_50_JA NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_JA NULL
#define OPTION_VAL_70_JA NULL
#define OPTION_VAL_80_JA NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_JA NULL
#define OPTION_VAL_100_JA NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_JA NULL
#define OPTION_VAL_150_JA NULL
#define OPTION_VAL_200_JA NULL
#define OPTION_VAL_300_JA NULL
#define OPTION_VAL_400_JA NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_JA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_JA NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_JA NULL
#define SHADPS4_TROPHY_POSITION_LABEL_JA NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_JA NULL
#define OPTION_VAL_LEFT_JA NULL
#define OPTION_VAL_RIGHT_JA NULL
#define OPTION_VAL_TOP_JA NULL
#define OPTION_VAL_BOTTOM_JA NULL
#define SHADPS4_TROPHY_DURATION_LABEL_JA NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_JA NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_JA NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_JA NULL
#define SHADPS4_LOG_INFO_0_JA NULL
#define SHADPS4_LOG_SEPARATE_LABEL_JA NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_JA NULL
#define SHADPS4_LOG_SYNC_LABEL_JA NULL
#define SHADPS4_LOG_SYNC_INFO_0_JA NULL

struct retro_core_option_v2_category option_cats_ja[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_JA,
        CATEGORY_SYSTEM_INFO_0_JA
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_JA,
        CATEGORY_VIDEO_INFO_0_JA
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_JA,
        CATEGORY_AUDIO_INFO_0_JA
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_JA,
        CATEGORY_TROPHIES_INFO_0_JA
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_JA,
        CATEGORY_LOGGING_INFO_0_JA
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ja[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_JA,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_JA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_JA,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_JA,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_JA },
            { "18", OPTION_VAL_18_JA },
            { "21", OPTION_VAL_21_JA },
            { "23", OPTION_VAL_23_JA },
            { "14", OPTION_VAL_14_JA },
            { "6", OPTION_VAL_6_JA },
            { "12", OPTION_VAL_12_JA },
            { "22", OPTION_VAL_22_JA },
            { "2", OPTION_VAL_2_JA },
            { "4", OPTION_VAL_4_JA },
            { "25", OPTION_VAL_25_JA },
            { "24", OPTION_VAL_24_JA },
            { "29", OPTION_VAL_29_JA },
            { "5", OPTION_VAL_5_JA },
            { "0", OPTION_VAL_0_JA },
            { "9", OPTION_VAL_9_JA },
            { "15", OPTION_VAL_15_JA },
            { "16", OPTION_VAL_16_JA },
            { "17", OPTION_VAL_17_JA },
            { "7", OPTION_VAL_7_JA },
            { "26", OPTION_VAL_26_JA },
            { "8", OPTION_VAL_8_JA },
            { "11", OPTION_VAL_11_JA },
            { "20", OPTION_VAL_20_JA },
            { "3", OPTION_VAL_3_JA },
            { "13", OPTION_VAL_13_JA },
            { "27", OPTION_VAL_27_JA },
            { "10", OPTION_VAL_10_JA },
            { "19", OPTION_VAL_19_JA },
            { "30", OPTION_VAL_30_JA },
            { "28", OPTION_VAL_28_JA },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_JA,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_JA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_JA,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_JA,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_JA },
            { "512", OPTION_VAL_512_JA },
            { "1024", OPTION_VAL_1024_JA },
            { "2048", OPTION_VAL_2048_JA },
            { "3072", OPTION_VAL_3072_JA },
            { "4096", OPTION_VAL_4096_JA },
            { "6144", OPTION_VAL_6144_JA },
            { "8192", OPTION_VAL_8192_JA },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_JA,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_JA,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_JA },
            { "circle", OPTION_VAL_CIRCLE_JA },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_JA,
        NULL,
        SHADPS4_NETWORK_INFO_0_JA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_JA,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_JA,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_JA,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_JA,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_JA,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_JA,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_JA },
            { "1920x1080", OPTION_VAL_1920X1080_JA },
            { "2560x1440", OPTION_VAL_2560X1440_JA },
            { "3840x2160", OPTION_VAL_3840X2160_JA },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_JA,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_JA,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_JA },
            { "60",  OPTION_VAL_60_JA },
            { "90",  OPTION_VAL_90_JA },
            { "120", OPTION_VAL_120_JA },
            { "144", OPTION_VAL_144_JA },
            { "240", OPTION_VAL_240_JA },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_JA,
        NULL,
        SHADPS4_READBACKS_INFO_0_JA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_JA },
            { "precise",  OPTION_VAL_PRECISE_JA },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_JA,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_JA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_JA,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_JA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_JA,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_JA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_JA,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_JA,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_JA,
        NULL,
        SHADPS4_VOLUME_INFO_0_JA,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_JA },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_JA },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_JA },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_JA },
            { "40",  OPTION_VAL_40_JA },
            { "50",  OPTION_VAL_50_JA },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_JA },
            { "70",  OPTION_VAL_70_JA },
            { "80",  OPTION_VAL_80_JA },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_JA },
            { "100", OPTION_VAL_100_JA },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_JA },
            { "150", OPTION_VAL_150_JA },
            { "200", OPTION_VAL_200_JA },
            { "300", OPTION_VAL_300_JA },
            { "400", OPTION_VAL_400_JA },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_JA },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_JA,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_JA,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_JA,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_JA,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_JA },
            { "right", OPTION_VAL_RIGHT_JA },
            { "top", OPTION_VAL_TOP_JA },
            { "bottom", OPTION_VAL_BOTTOM_JA },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_JA,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_JA,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_JA },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_JA },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_JA },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_JA },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_JA },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_JA },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_JA },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_JA },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_JA },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_JA },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_JA,
        NULL,
        SHADPS4_LOG_INFO_0_JA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_JA,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_JA,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_JA,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_JA,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_ja = {
   option_cats_ja,
   option_defs_ja
};

/* RETRO_LANGUAGE_KO */

#define CATEGORY_SYSTEM_LABEL_KO NULL
#define CATEGORY_SYSTEM_INFO_0_KO NULL
#define CATEGORY_VIDEO_LABEL_KO NULL
#define CATEGORY_VIDEO_INFO_0_KO NULL
#define CATEGORY_AUDIO_LABEL_KO NULL
#define CATEGORY_AUDIO_INFO_0_KO NULL
#define CATEGORY_TROPHIES_LABEL_KO NULL
#define CATEGORY_TROPHIES_INFO_0_KO NULL
#define CATEGORY_LOGGING_LABEL_KO NULL
#define CATEGORY_LOGGING_INFO_0_KO NULL
#define SHADPS4_NEO_MODE_LABEL_KO NULL
#define SHADPS4_NEO_MODE_INFO_0_KO NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_KO NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_KO NULL
#define OPTION_VAL_1_KO NULL
#define OPTION_VAL_18_KO NULL
#define OPTION_VAL_21_KO NULL
#define OPTION_VAL_23_KO NULL
#define OPTION_VAL_14_KO NULL
#define OPTION_VAL_6_KO NULL
#define OPTION_VAL_12_KO NULL
#define OPTION_VAL_22_KO NULL
#define OPTION_VAL_2_KO NULL
#define OPTION_VAL_4_KO NULL
#define OPTION_VAL_25_KO NULL
#define OPTION_VAL_24_KO NULL
#define OPTION_VAL_29_KO NULL
#define OPTION_VAL_5_KO NULL
#define OPTION_VAL_0_KO NULL
#define OPTION_VAL_9_KO NULL
#define OPTION_VAL_15_KO NULL
#define OPTION_VAL_16_KO NULL
#define OPTION_VAL_17_KO NULL
#define OPTION_VAL_7_KO NULL
#define OPTION_VAL_26_KO NULL
#define OPTION_VAL_8_KO NULL
#define OPTION_VAL_11_KO NULL
#define OPTION_VAL_20_KO NULL
#define OPTION_VAL_3_KO NULL
#define OPTION_VAL_13_KO NULL
#define OPTION_VAL_27_KO NULL
#define OPTION_VAL_10_KO NULL
#define OPTION_VAL_19_KO NULL
#define OPTION_VAL_30_KO NULL
#define OPTION_VAL_28_KO NULL
#define SHADPS4_DEV_KIT_LABEL_KO NULL
#define SHADPS4_DEV_KIT_INFO_0_KO NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_KO NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_KO NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_KO NULL
#define OPTION_VAL_512_KO NULL
#define OPTION_VAL_1024_KO NULL
#define OPTION_VAL_2048_KO NULL
#define OPTION_VAL_3072_KO NULL
#define OPTION_VAL_4096_KO NULL
#define OPTION_VAL_6144_KO NULL
#define OPTION_VAL_8192_KO NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_KO NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_KO NULL
#define OPTION_VAL_CROSS_KO NULL
#define OPTION_VAL_CIRCLE_KO NULL
#define SHADPS4_NETWORK_LABEL_KO NULL
#define SHADPS4_NETWORK_INFO_0_KO NULL
#define SHADPS4_CONTROLLERS_LABEL_KO NULL
#define SHADPS4_CONTROLLERS_INFO_0_KO NULL
#define SHADPS4_SHOW_SPLASH_LABEL_KO NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_KO NULL
#define SHADPS4_RESOLUTION_LABEL_KO NULL
#define SHADPS4_RESOLUTION_INFO_0_KO NULL
#define OPTION_VAL_1280X720_KO NULL
#define OPTION_VAL_1920X1080_KO NULL
#define OPTION_VAL_2560X1440_KO NULL
#define OPTION_VAL_3840X2160_KO NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_KO NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_KO NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_KO NULL
#define OPTION_VAL_60_KO NULL
#define OPTION_VAL_90_KO NULL
#define OPTION_VAL_120_KO NULL
#define OPTION_VAL_144_KO NULL
#define OPTION_VAL_240_KO NULL
#define SHADPS4_READBACKS_LABEL_KO NULL
#define SHADPS4_READBACKS_INFO_0_KO NULL
#define OPTION_VAL_RELAXED_KO NULL
#define OPTION_VAL_PRECISE_KO NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_KO NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_KO NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_KO NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_KO NULL
#define SHADPS4_SHADER_CACHE_LABEL_KO NULL
#define SHADPS4_SHADER_CACHE_INFO_0_KO NULL
#define SHADPS4_SHOW_FPS_LABEL_KO NULL
#define SHADPS4_SHOW_FPS_INFO_0_KO NULL
#define SHADPS4_VOLUME_LABEL_KO NULL
#define SHADPS4_VOLUME_INFO_0_KO NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_KO NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_KO NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_KO NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_KO NULL
#define OPTION_VAL_40_KO NULL
#define OPTION_VAL_50_KO NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_KO NULL
#define OPTION_VAL_70_KO NULL
#define OPTION_VAL_80_KO NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_KO NULL
#define OPTION_VAL_100_KO NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_KO NULL
#define OPTION_VAL_150_KO NULL
#define OPTION_VAL_200_KO NULL
#define OPTION_VAL_300_KO NULL
#define OPTION_VAL_400_KO NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_KO NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_KO NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_KO NULL
#define SHADPS4_TROPHY_POSITION_LABEL_KO NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_KO NULL
#define OPTION_VAL_LEFT_KO NULL
#define OPTION_VAL_RIGHT_KO NULL
#define OPTION_VAL_TOP_KO NULL
#define OPTION_VAL_BOTTOM_KO NULL
#define SHADPS4_TROPHY_DURATION_LABEL_KO NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_KO NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_KO NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_KO NULL
#define SHADPS4_LOG_INFO_0_KO NULL
#define SHADPS4_LOG_SEPARATE_LABEL_KO NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_KO NULL
#define SHADPS4_LOG_SYNC_LABEL_KO NULL
#define SHADPS4_LOG_SYNC_INFO_0_KO NULL

struct retro_core_option_v2_category option_cats_ko[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_KO,
        CATEGORY_SYSTEM_INFO_0_KO
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_KO,
        CATEGORY_VIDEO_INFO_0_KO
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_KO,
        CATEGORY_AUDIO_INFO_0_KO
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_KO,
        CATEGORY_TROPHIES_INFO_0_KO
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_KO,
        CATEGORY_LOGGING_INFO_0_KO
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ko[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_KO,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_KO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_KO,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_KO,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_KO },
            { "18", OPTION_VAL_18_KO },
            { "21", OPTION_VAL_21_KO },
            { "23", OPTION_VAL_23_KO },
            { "14", OPTION_VAL_14_KO },
            { "6", OPTION_VAL_6_KO },
            { "12", OPTION_VAL_12_KO },
            { "22", OPTION_VAL_22_KO },
            { "2", OPTION_VAL_2_KO },
            { "4", OPTION_VAL_4_KO },
            { "25", OPTION_VAL_25_KO },
            { "24", OPTION_VAL_24_KO },
            { "29", OPTION_VAL_29_KO },
            { "5", OPTION_VAL_5_KO },
            { "0", OPTION_VAL_0_KO },
            { "9", OPTION_VAL_9_KO },
            { "15", OPTION_VAL_15_KO },
            { "16", OPTION_VAL_16_KO },
            { "17", OPTION_VAL_17_KO },
            { "7", OPTION_VAL_7_KO },
            { "26", OPTION_VAL_26_KO },
            { "8", OPTION_VAL_8_KO },
            { "11", OPTION_VAL_11_KO },
            { "20", OPTION_VAL_20_KO },
            { "3", OPTION_VAL_3_KO },
            { "13", OPTION_VAL_13_KO },
            { "27", OPTION_VAL_27_KO },
            { "10", OPTION_VAL_10_KO },
            { "19", OPTION_VAL_19_KO },
            { "30", OPTION_VAL_30_KO },
            { "28", OPTION_VAL_28_KO },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_KO,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_KO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_KO,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_KO,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_KO },
            { "512", OPTION_VAL_512_KO },
            { "1024", OPTION_VAL_1024_KO },
            { "2048", OPTION_VAL_2048_KO },
            { "3072", OPTION_VAL_3072_KO },
            { "4096", OPTION_VAL_4096_KO },
            { "6144", OPTION_VAL_6144_KO },
            { "8192", OPTION_VAL_8192_KO },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_KO,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_KO,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_KO },
            { "circle", OPTION_VAL_CIRCLE_KO },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_KO,
        NULL,
        SHADPS4_NETWORK_INFO_0_KO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_KO,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_KO,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_KO,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_KO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_KO,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_KO,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_KO },
            { "1920x1080", OPTION_VAL_1920X1080_KO },
            { "2560x1440", OPTION_VAL_2560X1440_KO },
            { "3840x2160", OPTION_VAL_3840X2160_KO },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_KO,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_KO,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_KO },
            { "60",  OPTION_VAL_60_KO },
            { "90",  OPTION_VAL_90_KO },
            { "120", OPTION_VAL_120_KO },
            { "144", OPTION_VAL_144_KO },
            { "240", OPTION_VAL_240_KO },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_KO,
        NULL,
        SHADPS4_READBACKS_INFO_0_KO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_KO },
            { "precise",  OPTION_VAL_PRECISE_KO },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_KO,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_KO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_KO,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_KO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_KO,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_KO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_KO,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_KO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_KO,
        NULL,
        SHADPS4_VOLUME_INFO_0_KO,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_KO },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_KO },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_KO },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_KO },
            { "40",  OPTION_VAL_40_KO },
            { "50",  OPTION_VAL_50_KO },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_KO },
            { "70",  OPTION_VAL_70_KO },
            { "80",  OPTION_VAL_80_KO },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_KO },
            { "100", OPTION_VAL_100_KO },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_KO },
            { "150", OPTION_VAL_150_KO },
            { "200", OPTION_VAL_200_KO },
            { "300", OPTION_VAL_300_KO },
            { "400", OPTION_VAL_400_KO },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_KO },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_KO,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_KO,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_KO,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_KO,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_KO },
            { "right", OPTION_VAL_RIGHT_KO },
            { "top", OPTION_VAL_TOP_KO },
            { "bottom", OPTION_VAL_BOTTOM_KO },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_KO,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_KO,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_KO },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_KO },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_KO },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_KO },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_KO },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_KO },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_KO },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_KO },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_KO },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_KO },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_KO,
        NULL,
        SHADPS4_LOG_INFO_0_KO,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_KO,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_KO,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_KO,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_KO,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_ko = {
   option_cats_ko,
   option_defs_ko
};

/* RETRO_LANGUAGE_NL */

#define CATEGORY_SYSTEM_LABEL_NL NULL
#define CATEGORY_SYSTEM_INFO_0_NL NULL
#define CATEGORY_VIDEO_LABEL_NL NULL
#define CATEGORY_VIDEO_INFO_0_NL NULL
#define CATEGORY_AUDIO_LABEL_NL NULL
#define CATEGORY_AUDIO_INFO_0_NL NULL
#define CATEGORY_TROPHIES_LABEL_NL NULL
#define CATEGORY_TROPHIES_INFO_0_NL NULL
#define CATEGORY_LOGGING_LABEL_NL NULL
#define CATEGORY_LOGGING_INFO_0_NL NULL
#define SHADPS4_NEO_MODE_LABEL_NL NULL
#define SHADPS4_NEO_MODE_INFO_0_NL NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_NL NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_NL NULL
#define OPTION_VAL_1_NL NULL
#define OPTION_VAL_18_NL NULL
#define OPTION_VAL_21_NL NULL
#define OPTION_VAL_23_NL NULL
#define OPTION_VAL_14_NL NULL
#define OPTION_VAL_6_NL NULL
#define OPTION_VAL_12_NL NULL
#define OPTION_VAL_22_NL NULL
#define OPTION_VAL_2_NL NULL
#define OPTION_VAL_4_NL NULL
#define OPTION_VAL_25_NL NULL
#define OPTION_VAL_24_NL NULL
#define OPTION_VAL_29_NL NULL
#define OPTION_VAL_5_NL NULL
#define OPTION_VAL_0_NL NULL
#define OPTION_VAL_9_NL NULL
#define OPTION_VAL_15_NL NULL
#define OPTION_VAL_16_NL NULL
#define OPTION_VAL_17_NL NULL
#define OPTION_VAL_7_NL NULL
#define OPTION_VAL_26_NL NULL
#define OPTION_VAL_8_NL NULL
#define OPTION_VAL_11_NL NULL
#define OPTION_VAL_20_NL NULL
#define OPTION_VAL_3_NL NULL
#define OPTION_VAL_13_NL NULL
#define OPTION_VAL_27_NL NULL
#define OPTION_VAL_10_NL NULL
#define OPTION_VAL_19_NL NULL
#define OPTION_VAL_30_NL NULL
#define OPTION_VAL_28_NL NULL
#define SHADPS4_DEV_KIT_LABEL_NL NULL
#define SHADPS4_DEV_KIT_INFO_0_NL NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_NL NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_NL NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_NL NULL
#define OPTION_VAL_512_NL NULL
#define OPTION_VAL_1024_NL NULL
#define OPTION_VAL_2048_NL NULL
#define OPTION_VAL_3072_NL NULL
#define OPTION_VAL_4096_NL NULL
#define OPTION_VAL_6144_NL NULL
#define OPTION_VAL_8192_NL NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_NL NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_NL NULL
#define OPTION_VAL_CROSS_NL NULL
#define OPTION_VAL_CIRCLE_NL NULL
#define SHADPS4_NETWORK_LABEL_NL NULL
#define SHADPS4_NETWORK_INFO_0_NL NULL
#define SHADPS4_CONTROLLERS_LABEL_NL NULL
#define SHADPS4_CONTROLLERS_INFO_0_NL NULL
#define SHADPS4_SHOW_SPLASH_LABEL_NL NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_NL NULL
#define SHADPS4_RESOLUTION_LABEL_NL NULL
#define SHADPS4_RESOLUTION_INFO_0_NL NULL
#define OPTION_VAL_1280X720_NL NULL
#define OPTION_VAL_1920X1080_NL NULL
#define OPTION_VAL_2560X1440_NL NULL
#define OPTION_VAL_3840X2160_NL NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_NL NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_NL NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_NL NULL
#define OPTION_VAL_60_NL NULL
#define OPTION_VAL_90_NL NULL
#define OPTION_VAL_120_NL NULL
#define OPTION_VAL_144_NL NULL
#define OPTION_VAL_240_NL NULL
#define SHADPS4_READBACKS_LABEL_NL NULL
#define SHADPS4_READBACKS_INFO_0_NL NULL
#define OPTION_VAL_RELAXED_NL NULL
#define OPTION_VAL_PRECISE_NL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_NL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_NL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_NL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_NL NULL
#define SHADPS4_SHADER_CACHE_LABEL_NL NULL
#define SHADPS4_SHADER_CACHE_INFO_0_NL NULL
#define SHADPS4_SHOW_FPS_LABEL_NL NULL
#define SHADPS4_SHOW_FPS_INFO_0_NL NULL
#define SHADPS4_VOLUME_LABEL_NL NULL
#define SHADPS4_VOLUME_INFO_0_NL NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_NL NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_NL NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_NL NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_NL NULL
#define OPTION_VAL_40_NL NULL
#define OPTION_VAL_50_NL NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_NL NULL
#define OPTION_VAL_70_NL NULL
#define OPTION_VAL_80_NL NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_NL NULL
#define OPTION_VAL_100_NL NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_NL NULL
#define OPTION_VAL_150_NL NULL
#define OPTION_VAL_200_NL NULL
#define OPTION_VAL_300_NL NULL
#define OPTION_VAL_400_NL NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_NL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_NL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_NL NULL
#define SHADPS4_TROPHY_POSITION_LABEL_NL NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_NL NULL
#define OPTION_VAL_LEFT_NL NULL
#define OPTION_VAL_RIGHT_NL NULL
#define OPTION_VAL_TOP_NL NULL
#define OPTION_VAL_BOTTOM_NL NULL
#define SHADPS4_TROPHY_DURATION_LABEL_NL NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_NL NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_NL NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_NL NULL
#define SHADPS4_LOG_INFO_0_NL NULL
#define SHADPS4_LOG_SEPARATE_LABEL_NL NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_NL NULL
#define SHADPS4_LOG_SYNC_LABEL_NL NULL
#define SHADPS4_LOG_SYNC_INFO_0_NL NULL

struct retro_core_option_v2_category option_cats_nl[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_NL,
        CATEGORY_SYSTEM_INFO_0_NL
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_NL,
        CATEGORY_VIDEO_INFO_0_NL
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_NL,
        CATEGORY_AUDIO_INFO_0_NL
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_NL,
        CATEGORY_TROPHIES_INFO_0_NL
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_NL,
        CATEGORY_LOGGING_INFO_0_NL
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_nl[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_NL,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_NL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_NL,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_NL,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_NL },
            { "18", OPTION_VAL_18_NL },
            { "21", OPTION_VAL_21_NL },
            { "23", OPTION_VAL_23_NL },
            { "14", OPTION_VAL_14_NL },
            { "6", OPTION_VAL_6_NL },
            { "12", OPTION_VAL_12_NL },
            { "22", OPTION_VAL_22_NL },
            { "2", OPTION_VAL_2_NL },
            { "4", OPTION_VAL_4_NL },
            { "25", OPTION_VAL_25_NL },
            { "24", OPTION_VAL_24_NL },
            { "29", OPTION_VAL_29_NL },
            { "5", OPTION_VAL_5_NL },
            { "0", OPTION_VAL_0_NL },
            { "9", OPTION_VAL_9_NL },
            { "15", OPTION_VAL_15_NL },
            { "16", OPTION_VAL_16_NL },
            { "17", OPTION_VAL_17_NL },
            { "7", OPTION_VAL_7_NL },
            { "26", OPTION_VAL_26_NL },
            { "8", OPTION_VAL_8_NL },
            { "11", OPTION_VAL_11_NL },
            { "20", OPTION_VAL_20_NL },
            { "3", OPTION_VAL_3_NL },
            { "13", OPTION_VAL_13_NL },
            { "27", OPTION_VAL_27_NL },
            { "10", OPTION_VAL_10_NL },
            { "19", OPTION_VAL_19_NL },
            { "30", OPTION_VAL_30_NL },
            { "28", OPTION_VAL_28_NL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_NL,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_NL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_NL,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_NL,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_NL },
            { "512", OPTION_VAL_512_NL },
            { "1024", OPTION_VAL_1024_NL },
            { "2048", OPTION_VAL_2048_NL },
            { "3072", OPTION_VAL_3072_NL },
            { "4096", OPTION_VAL_4096_NL },
            { "6144", OPTION_VAL_6144_NL },
            { "8192", OPTION_VAL_8192_NL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_NL,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_NL,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_NL },
            { "circle", OPTION_VAL_CIRCLE_NL },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_NL,
        NULL,
        SHADPS4_NETWORK_INFO_0_NL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_NL,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_NL,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_NL,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_NL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_NL,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_NL,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_NL },
            { "1920x1080", OPTION_VAL_1920X1080_NL },
            { "2560x1440", OPTION_VAL_2560X1440_NL },
            { "3840x2160", OPTION_VAL_3840X2160_NL },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_NL,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_NL,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_NL },
            { "60",  OPTION_VAL_60_NL },
            { "90",  OPTION_VAL_90_NL },
            { "120", OPTION_VAL_120_NL },
            { "144", OPTION_VAL_144_NL },
            { "240", OPTION_VAL_240_NL },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_NL,
        NULL,
        SHADPS4_READBACKS_INFO_0_NL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_NL },
            { "precise",  OPTION_VAL_PRECISE_NL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_NL,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_NL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_NL,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_NL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_NL,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_NL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_NL,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_NL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_NL,
        NULL,
        SHADPS4_VOLUME_INFO_0_NL,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_NL },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_NL },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_NL },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_NL },
            { "40",  OPTION_VAL_40_NL },
            { "50",  OPTION_VAL_50_NL },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_NL },
            { "70",  OPTION_VAL_70_NL },
            { "80",  OPTION_VAL_80_NL },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_NL },
            { "100", OPTION_VAL_100_NL },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_NL },
            { "150", OPTION_VAL_150_NL },
            { "200", OPTION_VAL_200_NL },
            { "300", OPTION_VAL_300_NL },
            { "400", OPTION_VAL_400_NL },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_NL },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_NL,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_NL,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_NL,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_NL,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_NL },
            { "right", OPTION_VAL_RIGHT_NL },
            { "top", OPTION_VAL_TOP_NL },
            { "bottom", OPTION_VAL_BOTTOM_NL },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_NL,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_NL,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_NL },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_NL },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_NL },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_NL },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_NL },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_NL },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_NL },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_NL },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_NL },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_NL },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_NL,
        NULL,
        SHADPS4_LOG_INFO_0_NL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_NL,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_NL,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_NL,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_NL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_nl = {
   option_cats_nl,
   option_defs_nl
};

/* RETRO_LANGUAGE_NO */

#define CATEGORY_SYSTEM_LABEL_NO NULL
#define CATEGORY_SYSTEM_INFO_0_NO NULL
#define CATEGORY_VIDEO_LABEL_NO NULL
#define CATEGORY_VIDEO_INFO_0_NO NULL
#define CATEGORY_AUDIO_LABEL_NO NULL
#define CATEGORY_AUDIO_INFO_0_NO NULL
#define CATEGORY_TROPHIES_LABEL_NO NULL
#define CATEGORY_TROPHIES_INFO_0_NO NULL
#define CATEGORY_LOGGING_LABEL_NO NULL
#define CATEGORY_LOGGING_INFO_0_NO NULL
#define SHADPS4_NEO_MODE_LABEL_NO NULL
#define SHADPS4_NEO_MODE_INFO_0_NO NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_NO NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_NO NULL
#define OPTION_VAL_1_NO NULL
#define OPTION_VAL_18_NO NULL
#define OPTION_VAL_21_NO NULL
#define OPTION_VAL_23_NO NULL
#define OPTION_VAL_14_NO NULL
#define OPTION_VAL_6_NO NULL
#define OPTION_VAL_12_NO NULL
#define OPTION_VAL_22_NO NULL
#define OPTION_VAL_2_NO NULL
#define OPTION_VAL_4_NO NULL
#define OPTION_VAL_25_NO NULL
#define OPTION_VAL_24_NO NULL
#define OPTION_VAL_29_NO NULL
#define OPTION_VAL_5_NO NULL
#define OPTION_VAL_0_NO NULL
#define OPTION_VAL_9_NO NULL
#define OPTION_VAL_15_NO NULL
#define OPTION_VAL_16_NO NULL
#define OPTION_VAL_17_NO NULL
#define OPTION_VAL_7_NO NULL
#define OPTION_VAL_26_NO NULL
#define OPTION_VAL_8_NO NULL
#define OPTION_VAL_11_NO NULL
#define OPTION_VAL_20_NO NULL
#define OPTION_VAL_3_NO NULL
#define OPTION_VAL_13_NO NULL
#define OPTION_VAL_27_NO NULL
#define OPTION_VAL_10_NO NULL
#define OPTION_VAL_19_NO NULL
#define OPTION_VAL_30_NO NULL
#define OPTION_VAL_28_NO NULL
#define SHADPS4_DEV_KIT_LABEL_NO NULL
#define SHADPS4_DEV_KIT_INFO_0_NO NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_NO NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_NO NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_NO NULL
#define OPTION_VAL_512_NO NULL
#define OPTION_VAL_1024_NO NULL
#define OPTION_VAL_2048_NO NULL
#define OPTION_VAL_3072_NO NULL
#define OPTION_VAL_4096_NO NULL
#define OPTION_VAL_6144_NO NULL
#define OPTION_VAL_8192_NO NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_NO NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_NO NULL
#define OPTION_VAL_CROSS_NO NULL
#define OPTION_VAL_CIRCLE_NO NULL
#define SHADPS4_NETWORK_LABEL_NO NULL
#define SHADPS4_NETWORK_INFO_0_NO NULL
#define SHADPS4_CONTROLLERS_LABEL_NO NULL
#define SHADPS4_CONTROLLERS_INFO_0_NO NULL
#define SHADPS4_SHOW_SPLASH_LABEL_NO NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_NO NULL
#define SHADPS4_RESOLUTION_LABEL_NO NULL
#define SHADPS4_RESOLUTION_INFO_0_NO NULL
#define OPTION_VAL_1280X720_NO NULL
#define OPTION_VAL_1920X1080_NO NULL
#define OPTION_VAL_2560X1440_NO NULL
#define OPTION_VAL_3840X2160_NO NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_NO NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_NO NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_NO NULL
#define OPTION_VAL_60_NO NULL
#define OPTION_VAL_90_NO NULL
#define OPTION_VAL_120_NO NULL
#define OPTION_VAL_144_NO NULL
#define OPTION_VAL_240_NO NULL
#define SHADPS4_READBACKS_LABEL_NO NULL
#define SHADPS4_READBACKS_INFO_0_NO NULL
#define OPTION_VAL_RELAXED_NO NULL
#define OPTION_VAL_PRECISE_NO NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_NO NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_NO NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_NO NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_NO NULL
#define SHADPS4_SHADER_CACHE_LABEL_NO NULL
#define SHADPS4_SHADER_CACHE_INFO_0_NO NULL
#define SHADPS4_SHOW_FPS_LABEL_NO NULL
#define SHADPS4_SHOW_FPS_INFO_0_NO NULL
#define SHADPS4_VOLUME_LABEL_NO NULL
#define SHADPS4_VOLUME_INFO_0_NO NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_NO NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_NO NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_NO NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_NO NULL
#define OPTION_VAL_40_NO NULL
#define OPTION_VAL_50_NO NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_NO NULL
#define OPTION_VAL_70_NO NULL
#define OPTION_VAL_80_NO NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_NO NULL
#define OPTION_VAL_100_NO NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_NO NULL
#define OPTION_VAL_150_NO NULL
#define OPTION_VAL_200_NO NULL
#define OPTION_VAL_300_NO NULL
#define OPTION_VAL_400_NO NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_NO NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_NO NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_NO NULL
#define SHADPS4_TROPHY_POSITION_LABEL_NO NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_NO NULL
#define OPTION_VAL_LEFT_NO NULL
#define OPTION_VAL_RIGHT_NO NULL
#define OPTION_VAL_TOP_NO NULL
#define OPTION_VAL_BOTTOM_NO NULL
#define SHADPS4_TROPHY_DURATION_LABEL_NO NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_NO NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_NO NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_NO NULL
#define SHADPS4_LOG_INFO_0_NO NULL
#define SHADPS4_LOG_SEPARATE_LABEL_NO NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_NO NULL
#define SHADPS4_LOG_SYNC_LABEL_NO NULL
#define SHADPS4_LOG_SYNC_INFO_0_NO NULL

struct retro_core_option_v2_category option_cats_no[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_NO,
        CATEGORY_SYSTEM_INFO_0_NO
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_NO,
        CATEGORY_VIDEO_INFO_0_NO
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_NO,
        CATEGORY_AUDIO_INFO_0_NO
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_NO,
        CATEGORY_TROPHIES_INFO_0_NO
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_NO,
        CATEGORY_LOGGING_INFO_0_NO
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_no[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_NO,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_NO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_NO,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_NO,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_NO },
            { "18", OPTION_VAL_18_NO },
            { "21", OPTION_VAL_21_NO },
            { "23", OPTION_VAL_23_NO },
            { "14", OPTION_VAL_14_NO },
            { "6", OPTION_VAL_6_NO },
            { "12", OPTION_VAL_12_NO },
            { "22", OPTION_VAL_22_NO },
            { "2", OPTION_VAL_2_NO },
            { "4", OPTION_VAL_4_NO },
            { "25", OPTION_VAL_25_NO },
            { "24", OPTION_VAL_24_NO },
            { "29", OPTION_VAL_29_NO },
            { "5", OPTION_VAL_5_NO },
            { "0", OPTION_VAL_0_NO },
            { "9", OPTION_VAL_9_NO },
            { "15", OPTION_VAL_15_NO },
            { "16", OPTION_VAL_16_NO },
            { "17", OPTION_VAL_17_NO },
            { "7", OPTION_VAL_7_NO },
            { "26", OPTION_VAL_26_NO },
            { "8", OPTION_VAL_8_NO },
            { "11", OPTION_VAL_11_NO },
            { "20", OPTION_VAL_20_NO },
            { "3", OPTION_VAL_3_NO },
            { "13", OPTION_VAL_13_NO },
            { "27", OPTION_VAL_27_NO },
            { "10", OPTION_VAL_10_NO },
            { "19", OPTION_VAL_19_NO },
            { "30", OPTION_VAL_30_NO },
            { "28", OPTION_VAL_28_NO },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_NO,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_NO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_NO,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_NO,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_NO },
            { "512", OPTION_VAL_512_NO },
            { "1024", OPTION_VAL_1024_NO },
            { "2048", OPTION_VAL_2048_NO },
            { "3072", OPTION_VAL_3072_NO },
            { "4096", OPTION_VAL_4096_NO },
            { "6144", OPTION_VAL_6144_NO },
            { "8192", OPTION_VAL_8192_NO },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_NO,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_NO,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_NO },
            { "circle", OPTION_VAL_CIRCLE_NO },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_NO,
        NULL,
        SHADPS4_NETWORK_INFO_0_NO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_NO,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_NO,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_NO,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_NO,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_NO,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_NO,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_NO },
            { "1920x1080", OPTION_VAL_1920X1080_NO },
            { "2560x1440", OPTION_VAL_2560X1440_NO },
            { "3840x2160", OPTION_VAL_3840X2160_NO },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_NO,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_NO,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_NO },
            { "60",  OPTION_VAL_60_NO },
            { "90",  OPTION_VAL_90_NO },
            { "120", OPTION_VAL_120_NO },
            { "144", OPTION_VAL_144_NO },
            { "240", OPTION_VAL_240_NO },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_NO,
        NULL,
        SHADPS4_READBACKS_INFO_0_NO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_NO },
            { "precise",  OPTION_VAL_PRECISE_NO },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_NO,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_NO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_NO,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_NO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_NO,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_NO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_NO,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_NO,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_NO,
        NULL,
        SHADPS4_VOLUME_INFO_0_NO,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_NO },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_NO },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_NO },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_NO },
            { "40",  OPTION_VAL_40_NO },
            { "50",  OPTION_VAL_50_NO },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_NO },
            { "70",  OPTION_VAL_70_NO },
            { "80",  OPTION_VAL_80_NO },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_NO },
            { "100", OPTION_VAL_100_NO },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_NO },
            { "150", OPTION_VAL_150_NO },
            { "200", OPTION_VAL_200_NO },
            { "300", OPTION_VAL_300_NO },
            { "400", OPTION_VAL_400_NO },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_NO },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_NO,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_NO,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_NO,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_NO,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_NO },
            { "right", OPTION_VAL_RIGHT_NO },
            { "top", OPTION_VAL_TOP_NO },
            { "bottom", OPTION_VAL_BOTTOM_NO },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_NO,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_NO,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_NO },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_NO },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_NO },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_NO },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_NO },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_NO },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_NO },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_NO },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_NO },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_NO },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_NO,
        NULL,
        SHADPS4_LOG_INFO_0_NO,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_NO,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_NO,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_NO,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_NO,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_no = {
   option_cats_no,
   option_defs_no
};

/* RETRO_LANGUAGE_PL */

#define CATEGORY_SYSTEM_LABEL_PL NULL
#define CATEGORY_SYSTEM_INFO_0_PL NULL
#define CATEGORY_VIDEO_LABEL_PL NULL
#define CATEGORY_VIDEO_INFO_0_PL NULL
#define CATEGORY_AUDIO_LABEL_PL NULL
#define CATEGORY_AUDIO_INFO_0_PL NULL
#define CATEGORY_TROPHIES_LABEL_PL NULL
#define CATEGORY_TROPHIES_INFO_0_PL NULL
#define CATEGORY_LOGGING_LABEL_PL NULL
#define CATEGORY_LOGGING_INFO_0_PL NULL
#define SHADPS4_NEO_MODE_LABEL_PL NULL
#define SHADPS4_NEO_MODE_INFO_0_PL NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_PL NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_PL NULL
#define OPTION_VAL_1_PL NULL
#define OPTION_VAL_18_PL NULL
#define OPTION_VAL_21_PL NULL
#define OPTION_VAL_23_PL NULL
#define OPTION_VAL_14_PL NULL
#define OPTION_VAL_6_PL NULL
#define OPTION_VAL_12_PL NULL
#define OPTION_VAL_22_PL NULL
#define OPTION_VAL_2_PL NULL
#define OPTION_VAL_4_PL NULL
#define OPTION_VAL_25_PL NULL
#define OPTION_VAL_24_PL NULL
#define OPTION_VAL_29_PL NULL
#define OPTION_VAL_5_PL NULL
#define OPTION_VAL_0_PL NULL
#define OPTION_VAL_9_PL NULL
#define OPTION_VAL_15_PL NULL
#define OPTION_VAL_16_PL NULL
#define OPTION_VAL_17_PL NULL
#define OPTION_VAL_7_PL NULL
#define OPTION_VAL_26_PL NULL
#define OPTION_VAL_8_PL NULL
#define OPTION_VAL_11_PL NULL
#define OPTION_VAL_20_PL NULL
#define OPTION_VAL_3_PL NULL
#define OPTION_VAL_13_PL NULL
#define OPTION_VAL_27_PL NULL
#define OPTION_VAL_10_PL NULL
#define OPTION_VAL_19_PL NULL
#define OPTION_VAL_30_PL NULL
#define OPTION_VAL_28_PL NULL
#define SHADPS4_DEV_KIT_LABEL_PL NULL
#define SHADPS4_DEV_KIT_INFO_0_PL NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_PL NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_PL NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_PL NULL
#define OPTION_VAL_512_PL NULL
#define OPTION_VAL_1024_PL NULL
#define OPTION_VAL_2048_PL NULL
#define OPTION_VAL_3072_PL NULL
#define OPTION_VAL_4096_PL NULL
#define OPTION_VAL_6144_PL NULL
#define OPTION_VAL_8192_PL NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_PL NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_PL NULL
#define OPTION_VAL_CROSS_PL NULL
#define OPTION_VAL_CIRCLE_PL NULL
#define SHADPS4_NETWORK_LABEL_PL NULL
#define SHADPS4_NETWORK_INFO_0_PL NULL
#define SHADPS4_CONTROLLERS_LABEL_PL NULL
#define SHADPS4_CONTROLLERS_INFO_0_PL NULL
#define SHADPS4_SHOW_SPLASH_LABEL_PL NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_PL NULL
#define SHADPS4_RESOLUTION_LABEL_PL NULL
#define SHADPS4_RESOLUTION_INFO_0_PL NULL
#define OPTION_VAL_1280X720_PL NULL
#define OPTION_VAL_1920X1080_PL NULL
#define OPTION_VAL_2560X1440_PL NULL
#define OPTION_VAL_3840X2160_PL NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_PL NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_PL NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_PL NULL
#define OPTION_VAL_60_PL NULL
#define OPTION_VAL_90_PL NULL
#define OPTION_VAL_120_PL NULL
#define OPTION_VAL_144_PL NULL
#define OPTION_VAL_240_PL NULL
#define SHADPS4_READBACKS_LABEL_PL NULL
#define SHADPS4_READBACKS_INFO_0_PL NULL
#define OPTION_VAL_RELAXED_PL NULL
#define OPTION_VAL_PRECISE_PL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_PL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_PL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_PL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_PL NULL
#define SHADPS4_SHADER_CACHE_LABEL_PL NULL
#define SHADPS4_SHADER_CACHE_INFO_0_PL NULL
#define SHADPS4_SHOW_FPS_LABEL_PL NULL
#define SHADPS4_SHOW_FPS_INFO_0_PL NULL
#define SHADPS4_VOLUME_LABEL_PL NULL
#define SHADPS4_VOLUME_INFO_0_PL NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_PL NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_PL NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_PL NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_PL NULL
#define OPTION_VAL_40_PL NULL
#define OPTION_VAL_50_PL NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_PL NULL
#define OPTION_VAL_70_PL NULL
#define OPTION_VAL_80_PL NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_PL NULL
#define OPTION_VAL_100_PL NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_PL NULL
#define OPTION_VAL_150_PL NULL
#define OPTION_VAL_200_PL NULL
#define OPTION_VAL_300_PL NULL
#define OPTION_VAL_400_PL NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_PL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_PL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_PL NULL
#define SHADPS4_TROPHY_POSITION_LABEL_PL NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_PL NULL
#define OPTION_VAL_LEFT_PL NULL
#define OPTION_VAL_RIGHT_PL NULL
#define OPTION_VAL_TOP_PL NULL
#define OPTION_VAL_BOTTOM_PL NULL
#define SHADPS4_TROPHY_DURATION_LABEL_PL NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_PL NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_PL NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_PL NULL
#define SHADPS4_LOG_INFO_0_PL NULL
#define SHADPS4_LOG_SEPARATE_LABEL_PL NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_PL NULL
#define SHADPS4_LOG_SYNC_LABEL_PL NULL
#define SHADPS4_LOG_SYNC_INFO_0_PL NULL

struct retro_core_option_v2_category option_cats_pl[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_PL,
        CATEGORY_SYSTEM_INFO_0_PL
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_PL,
        CATEGORY_VIDEO_INFO_0_PL
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_PL,
        CATEGORY_AUDIO_INFO_0_PL
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_PL,
        CATEGORY_TROPHIES_INFO_0_PL
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_PL,
        CATEGORY_LOGGING_INFO_0_PL
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pl[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_PL,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_PL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_PL,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_PL,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_PL },
            { "18", OPTION_VAL_18_PL },
            { "21", OPTION_VAL_21_PL },
            { "23", OPTION_VAL_23_PL },
            { "14", OPTION_VAL_14_PL },
            { "6", OPTION_VAL_6_PL },
            { "12", OPTION_VAL_12_PL },
            { "22", OPTION_VAL_22_PL },
            { "2", OPTION_VAL_2_PL },
            { "4", OPTION_VAL_4_PL },
            { "25", OPTION_VAL_25_PL },
            { "24", OPTION_VAL_24_PL },
            { "29", OPTION_VAL_29_PL },
            { "5", OPTION_VAL_5_PL },
            { "0", OPTION_VAL_0_PL },
            { "9", OPTION_VAL_9_PL },
            { "15", OPTION_VAL_15_PL },
            { "16", OPTION_VAL_16_PL },
            { "17", OPTION_VAL_17_PL },
            { "7", OPTION_VAL_7_PL },
            { "26", OPTION_VAL_26_PL },
            { "8", OPTION_VAL_8_PL },
            { "11", OPTION_VAL_11_PL },
            { "20", OPTION_VAL_20_PL },
            { "3", OPTION_VAL_3_PL },
            { "13", OPTION_VAL_13_PL },
            { "27", OPTION_VAL_27_PL },
            { "10", OPTION_VAL_10_PL },
            { "19", OPTION_VAL_19_PL },
            { "30", OPTION_VAL_30_PL },
            { "28", OPTION_VAL_28_PL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_PL,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_PL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_PL,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_PL,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_PL },
            { "512", OPTION_VAL_512_PL },
            { "1024", OPTION_VAL_1024_PL },
            { "2048", OPTION_VAL_2048_PL },
            { "3072", OPTION_VAL_3072_PL },
            { "4096", OPTION_VAL_4096_PL },
            { "6144", OPTION_VAL_6144_PL },
            { "8192", OPTION_VAL_8192_PL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_PL,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_PL,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_PL },
            { "circle", OPTION_VAL_CIRCLE_PL },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_PL,
        NULL,
        SHADPS4_NETWORK_INFO_0_PL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_PL,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_PL,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_PL,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_PL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_PL,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_PL,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_PL },
            { "1920x1080", OPTION_VAL_1920X1080_PL },
            { "2560x1440", OPTION_VAL_2560X1440_PL },
            { "3840x2160", OPTION_VAL_3840X2160_PL },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_PL,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_PL,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_PL },
            { "60",  OPTION_VAL_60_PL },
            { "90",  OPTION_VAL_90_PL },
            { "120", OPTION_VAL_120_PL },
            { "144", OPTION_VAL_144_PL },
            { "240", OPTION_VAL_240_PL },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_PL,
        NULL,
        SHADPS4_READBACKS_INFO_0_PL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_PL },
            { "precise",  OPTION_VAL_PRECISE_PL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_PL,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_PL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_PL,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_PL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_PL,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_PL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_PL,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_PL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_PL,
        NULL,
        SHADPS4_VOLUME_INFO_0_PL,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_PL },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_PL },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_PL },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_PL },
            { "40",  OPTION_VAL_40_PL },
            { "50",  OPTION_VAL_50_PL },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_PL },
            { "70",  OPTION_VAL_70_PL },
            { "80",  OPTION_VAL_80_PL },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_PL },
            { "100", OPTION_VAL_100_PL },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_PL },
            { "150", OPTION_VAL_150_PL },
            { "200", OPTION_VAL_200_PL },
            { "300", OPTION_VAL_300_PL },
            { "400", OPTION_VAL_400_PL },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_PL },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_PL,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_PL,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_PL,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_PL,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_PL },
            { "right", OPTION_VAL_RIGHT_PL },
            { "top", OPTION_VAL_TOP_PL },
            { "bottom", OPTION_VAL_BOTTOM_PL },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_PL,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_PL,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_PL },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_PL },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_PL },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_PL },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_PL },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_PL },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_PL },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_PL },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_PL },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_PL },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_PL,
        NULL,
        SHADPS4_LOG_INFO_0_PL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_PL,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_PL,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_PL,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_PL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_pl = {
   option_cats_pl,
   option_defs_pl
};

/* RETRO_LANGUAGE_PT_BR */

#define CATEGORY_SYSTEM_LABEL_PT_BR NULL
#define CATEGORY_SYSTEM_INFO_0_PT_BR NULL
#define CATEGORY_VIDEO_LABEL_PT_BR NULL
#define CATEGORY_VIDEO_INFO_0_PT_BR NULL
#define CATEGORY_AUDIO_LABEL_PT_BR NULL
#define CATEGORY_AUDIO_INFO_0_PT_BR NULL
#define CATEGORY_TROPHIES_LABEL_PT_BR NULL
#define CATEGORY_TROPHIES_INFO_0_PT_BR NULL
#define CATEGORY_LOGGING_LABEL_PT_BR NULL
#define CATEGORY_LOGGING_INFO_0_PT_BR NULL
#define SHADPS4_NEO_MODE_LABEL_PT_BR NULL
#define SHADPS4_NEO_MODE_INFO_0_PT_BR NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_PT_BR NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_PT_BR NULL
#define OPTION_VAL_1_PT_BR NULL
#define OPTION_VAL_18_PT_BR NULL
#define OPTION_VAL_21_PT_BR NULL
#define OPTION_VAL_23_PT_BR NULL
#define OPTION_VAL_14_PT_BR NULL
#define OPTION_VAL_6_PT_BR NULL
#define OPTION_VAL_12_PT_BR NULL
#define OPTION_VAL_22_PT_BR NULL
#define OPTION_VAL_2_PT_BR NULL
#define OPTION_VAL_4_PT_BR NULL
#define OPTION_VAL_25_PT_BR NULL
#define OPTION_VAL_24_PT_BR NULL
#define OPTION_VAL_29_PT_BR NULL
#define OPTION_VAL_5_PT_BR NULL
#define OPTION_VAL_0_PT_BR NULL
#define OPTION_VAL_9_PT_BR NULL
#define OPTION_VAL_15_PT_BR NULL
#define OPTION_VAL_16_PT_BR NULL
#define OPTION_VAL_17_PT_BR NULL
#define OPTION_VAL_7_PT_BR NULL
#define OPTION_VAL_26_PT_BR NULL
#define OPTION_VAL_8_PT_BR NULL
#define OPTION_VAL_11_PT_BR NULL
#define OPTION_VAL_20_PT_BR NULL
#define OPTION_VAL_3_PT_BR NULL
#define OPTION_VAL_13_PT_BR NULL
#define OPTION_VAL_27_PT_BR NULL
#define OPTION_VAL_10_PT_BR NULL
#define OPTION_VAL_19_PT_BR NULL
#define OPTION_VAL_30_PT_BR NULL
#define OPTION_VAL_28_PT_BR NULL
#define SHADPS4_DEV_KIT_LABEL_PT_BR NULL
#define SHADPS4_DEV_KIT_INFO_0_PT_BR NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_PT_BR NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_PT_BR NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_PT_BR NULL
#define OPTION_VAL_512_PT_BR NULL
#define OPTION_VAL_1024_PT_BR NULL
#define OPTION_VAL_2048_PT_BR NULL
#define OPTION_VAL_3072_PT_BR NULL
#define OPTION_VAL_4096_PT_BR NULL
#define OPTION_VAL_6144_PT_BR NULL
#define OPTION_VAL_8192_PT_BR NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_PT_BR NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_PT_BR NULL
#define OPTION_VAL_CROSS_PT_BR NULL
#define OPTION_VAL_CIRCLE_PT_BR NULL
#define SHADPS4_NETWORK_LABEL_PT_BR NULL
#define SHADPS4_NETWORK_INFO_0_PT_BR NULL
#define SHADPS4_CONTROLLERS_LABEL_PT_BR NULL
#define SHADPS4_CONTROLLERS_INFO_0_PT_BR NULL
#define SHADPS4_SHOW_SPLASH_LABEL_PT_BR NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_PT_BR NULL
#define SHADPS4_RESOLUTION_LABEL_PT_BR NULL
#define SHADPS4_RESOLUTION_INFO_0_PT_BR NULL
#define OPTION_VAL_1280X720_PT_BR NULL
#define OPTION_VAL_1920X1080_PT_BR NULL
#define OPTION_VAL_2560X1440_PT_BR NULL
#define OPTION_VAL_3840X2160_PT_BR NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_PT_BR NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_PT_BR NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_PT_BR NULL
#define OPTION_VAL_60_PT_BR NULL
#define OPTION_VAL_90_PT_BR NULL
#define OPTION_VAL_120_PT_BR NULL
#define OPTION_VAL_144_PT_BR NULL
#define OPTION_VAL_240_PT_BR NULL
#define SHADPS4_READBACKS_LABEL_PT_BR NULL
#define SHADPS4_READBACKS_INFO_0_PT_BR NULL
#define OPTION_VAL_RELAXED_PT_BR NULL
#define OPTION_VAL_PRECISE_PT_BR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_PT_BR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_PT_BR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_PT_BR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_PT_BR NULL
#define SHADPS4_SHADER_CACHE_LABEL_PT_BR NULL
#define SHADPS4_SHADER_CACHE_INFO_0_PT_BR NULL
#define SHADPS4_SHOW_FPS_LABEL_PT_BR NULL
#define SHADPS4_SHOW_FPS_INFO_0_PT_BR NULL
#define SHADPS4_VOLUME_LABEL_PT_BR NULL
#define SHADPS4_VOLUME_INFO_0_PT_BR NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_PT_BR NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_PT_BR NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_PT_BR NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_PT_BR NULL
#define OPTION_VAL_40_PT_BR NULL
#define OPTION_VAL_50_PT_BR NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_PT_BR NULL
#define OPTION_VAL_70_PT_BR NULL
#define OPTION_VAL_80_PT_BR NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_PT_BR NULL
#define OPTION_VAL_100_PT_BR NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_PT_BR NULL
#define OPTION_VAL_150_PT_BR NULL
#define OPTION_VAL_200_PT_BR NULL
#define OPTION_VAL_300_PT_BR NULL
#define OPTION_VAL_400_PT_BR NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_PT_BR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_PT_BR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_PT_BR NULL
#define SHADPS4_TROPHY_POSITION_LABEL_PT_BR NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_PT_BR NULL
#define OPTION_VAL_LEFT_PT_BR NULL
#define OPTION_VAL_RIGHT_PT_BR NULL
#define OPTION_VAL_TOP_PT_BR NULL
#define OPTION_VAL_BOTTOM_PT_BR NULL
#define SHADPS4_TROPHY_DURATION_LABEL_PT_BR NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_PT_BR NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_PT_BR NULL
#define SHADPS4_LOG_INFO_0_PT_BR NULL
#define SHADPS4_LOG_SEPARATE_LABEL_PT_BR NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_PT_BR NULL
#define SHADPS4_LOG_SYNC_LABEL_PT_BR NULL
#define SHADPS4_LOG_SYNC_INFO_0_PT_BR NULL

struct retro_core_option_v2_category option_cats_pt_br[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_PT_BR,
        CATEGORY_SYSTEM_INFO_0_PT_BR
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_PT_BR,
        CATEGORY_VIDEO_INFO_0_PT_BR
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_PT_BR,
        CATEGORY_AUDIO_INFO_0_PT_BR
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_PT_BR,
        CATEGORY_TROPHIES_INFO_0_PT_BR
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_PT_BR,
        CATEGORY_LOGGING_INFO_0_PT_BR
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pt_br[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_PT_BR,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_PT_BR,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_PT_BR },
            { "18", OPTION_VAL_18_PT_BR },
            { "21", OPTION_VAL_21_PT_BR },
            { "23", OPTION_VAL_23_PT_BR },
            { "14", OPTION_VAL_14_PT_BR },
            { "6", OPTION_VAL_6_PT_BR },
            { "12", OPTION_VAL_12_PT_BR },
            { "22", OPTION_VAL_22_PT_BR },
            { "2", OPTION_VAL_2_PT_BR },
            { "4", OPTION_VAL_4_PT_BR },
            { "25", OPTION_VAL_25_PT_BR },
            { "24", OPTION_VAL_24_PT_BR },
            { "29", OPTION_VAL_29_PT_BR },
            { "5", OPTION_VAL_5_PT_BR },
            { "0", OPTION_VAL_0_PT_BR },
            { "9", OPTION_VAL_9_PT_BR },
            { "15", OPTION_VAL_15_PT_BR },
            { "16", OPTION_VAL_16_PT_BR },
            { "17", OPTION_VAL_17_PT_BR },
            { "7", OPTION_VAL_7_PT_BR },
            { "26", OPTION_VAL_26_PT_BR },
            { "8", OPTION_VAL_8_PT_BR },
            { "11", OPTION_VAL_11_PT_BR },
            { "20", OPTION_VAL_20_PT_BR },
            { "3", OPTION_VAL_3_PT_BR },
            { "13", OPTION_VAL_13_PT_BR },
            { "27", OPTION_VAL_27_PT_BR },
            { "10", OPTION_VAL_10_PT_BR },
            { "19", OPTION_VAL_19_PT_BR },
            { "30", OPTION_VAL_30_PT_BR },
            { "28", OPTION_VAL_28_PT_BR },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_PT_BR,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_PT_BR,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_PT_BR },
            { "512", OPTION_VAL_512_PT_BR },
            { "1024", OPTION_VAL_1024_PT_BR },
            { "2048", OPTION_VAL_2048_PT_BR },
            { "3072", OPTION_VAL_3072_PT_BR },
            { "4096", OPTION_VAL_4096_PT_BR },
            { "6144", OPTION_VAL_6144_PT_BR },
            { "8192", OPTION_VAL_8192_PT_BR },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_PT_BR,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_PT_BR },
            { "circle", OPTION_VAL_CIRCLE_PT_BR },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_PT_BR,
        NULL,
        SHADPS4_NETWORK_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_PT_BR,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_PT_BR,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_PT_BR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_PT_BR,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_PT_BR,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_PT_BR },
            { "1920x1080", OPTION_VAL_1920X1080_PT_BR },
            { "2560x1440", OPTION_VAL_2560X1440_PT_BR },
            { "3840x2160", OPTION_VAL_3840X2160_PT_BR },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_PT_BR,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_PT_BR,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_PT_BR },
            { "60",  OPTION_VAL_60_PT_BR },
            { "90",  OPTION_VAL_90_PT_BR },
            { "120", OPTION_VAL_120_PT_BR },
            { "144", OPTION_VAL_144_PT_BR },
            { "240", OPTION_VAL_240_PT_BR },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_PT_BR,
        NULL,
        SHADPS4_READBACKS_INFO_0_PT_BR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_PT_BR },
            { "precise",  OPTION_VAL_PRECISE_PT_BR },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_PT_BR,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_PT_BR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_PT_BR,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_PT_BR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_PT_BR,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_PT_BR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_PT_BR,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_PT_BR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_PT_BR,
        NULL,
        SHADPS4_VOLUME_INFO_0_PT_BR,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_PT_BR },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_PT_BR },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_PT_BR },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_PT_BR },
            { "40",  OPTION_VAL_40_PT_BR },
            { "50",  OPTION_VAL_50_PT_BR },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_PT_BR },
            { "70",  OPTION_VAL_70_PT_BR },
            { "80",  OPTION_VAL_80_PT_BR },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_PT_BR },
            { "100", OPTION_VAL_100_PT_BR },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_PT_BR },
            { "150", OPTION_VAL_150_PT_BR },
            { "200", OPTION_VAL_200_PT_BR },
            { "300", OPTION_VAL_300_PT_BR },
            { "400", OPTION_VAL_400_PT_BR },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_PT_BR },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_PT_BR,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_PT_BR,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_PT_BR,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_PT_BR,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_PT_BR },
            { "right", OPTION_VAL_RIGHT_PT_BR },
            { "top", OPTION_VAL_TOP_PT_BR },
            { "bottom", OPTION_VAL_BOTTOM_PT_BR },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_PT_BR,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_PT_BR,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_PT_BR },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_PT_BR },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_PT_BR },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_PT_BR },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_PT_BR },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_PT_BR },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_PT_BR },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_PT_BR },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_PT_BR },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_PT_BR },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_PT_BR,
        NULL,
        SHADPS4_LOG_INFO_0_PT_BR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_PT_BR,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_PT_BR,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_PT_BR,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_PT_BR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_pt_br = {
   option_cats_pt_br,
   option_defs_pt_br
};

/* RETRO_LANGUAGE_PT_PT */

#define CATEGORY_SYSTEM_LABEL_PT_PT NULL
#define CATEGORY_SYSTEM_INFO_0_PT_PT NULL
#define CATEGORY_VIDEO_LABEL_PT_PT NULL
#define CATEGORY_VIDEO_INFO_0_PT_PT NULL
#define CATEGORY_AUDIO_LABEL_PT_PT NULL
#define CATEGORY_AUDIO_INFO_0_PT_PT NULL
#define CATEGORY_TROPHIES_LABEL_PT_PT NULL
#define CATEGORY_TROPHIES_INFO_0_PT_PT NULL
#define CATEGORY_LOGGING_LABEL_PT_PT NULL
#define CATEGORY_LOGGING_INFO_0_PT_PT NULL
#define SHADPS4_NEO_MODE_LABEL_PT_PT NULL
#define SHADPS4_NEO_MODE_INFO_0_PT_PT NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_PT_PT NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_PT_PT NULL
#define OPTION_VAL_1_PT_PT NULL
#define OPTION_VAL_18_PT_PT NULL
#define OPTION_VAL_21_PT_PT NULL
#define OPTION_VAL_23_PT_PT NULL
#define OPTION_VAL_14_PT_PT NULL
#define OPTION_VAL_6_PT_PT NULL
#define OPTION_VAL_12_PT_PT NULL
#define OPTION_VAL_22_PT_PT NULL
#define OPTION_VAL_2_PT_PT NULL
#define OPTION_VAL_4_PT_PT NULL
#define OPTION_VAL_25_PT_PT NULL
#define OPTION_VAL_24_PT_PT NULL
#define OPTION_VAL_29_PT_PT NULL
#define OPTION_VAL_5_PT_PT NULL
#define OPTION_VAL_0_PT_PT NULL
#define OPTION_VAL_9_PT_PT NULL
#define OPTION_VAL_15_PT_PT NULL
#define OPTION_VAL_16_PT_PT NULL
#define OPTION_VAL_17_PT_PT NULL
#define OPTION_VAL_7_PT_PT NULL
#define OPTION_VAL_26_PT_PT NULL
#define OPTION_VAL_8_PT_PT NULL
#define OPTION_VAL_11_PT_PT NULL
#define OPTION_VAL_20_PT_PT NULL
#define OPTION_VAL_3_PT_PT NULL
#define OPTION_VAL_13_PT_PT NULL
#define OPTION_VAL_27_PT_PT NULL
#define OPTION_VAL_10_PT_PT NULL
#define OPTION_VAL_19_PT_PT NULL
#define OPTION_VAL_30_PT_PT NULL
#define OPTION_VAL_28_PT_PT NULL
#define SHADPS4_DEV_KIT_LABEL_PT_PT NULL
#define SHADPS4_DEV_KIT_INFO_0_PT_PT NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_PT_PT NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_PT_PT NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_PT_PT NULL
#define OPTION_VAL_512_PT_PT NULL
#define OPTION_VAL_1024_PT_PT NULL
#define OPTION_VAL_2048_PT_PT NULL
#define OPTION_VAL_3072_PT_PT NULL
#define OPTION_VAL_4096_PT_PT NULL
#define OPTION_VAL_6144_PT_PT NULL
#define OPTION_VAL_8192_PT_PT NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_PT_PT NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_PT_PT NULL
#define OPTION_VAL_CROSS_PT_PT NULL
#define OPTION_VAL_CIRCLE_PT_PT NULL
#define SHADPS4_NETWORK_LABEL_PT_PT NULL
#define SHADPS4_NETWORK_INFO_0_PT_PT NULL
#define SHADPS4_CONTROLLERS_LABEL_PT_PT NULL
#define SHADPS4_CONTROLLERS_INFO_0_PT_PT NULL
#define SHADPS4_SHOW_SPLASH_LABEL_PT_PT NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_PT_PT NULL
#define SHADPS4_RESOLUTION_LABEL_PT_PT NULL
#define SHADPS4_RESOLUTION_INFO_0_PT_PT NULL
#define OPTION_VAL_1280X720_PT_PT NULL
#define OPTION_VAL_1920X1080_PT_PT NULL
#define OPTION_VAL_2560X1440_PT_PT NULL
#define OPTION_VAL_3840X2160_PT_PT NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_PT_PT NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_PT_PT NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_PT_PT NULL
#define OPTION_VAL_60_PT_PT NULL
#define OPTION_VAL_90_PT_PT NULL
#define OPTION_VAL_120_PT_PT NULL
#define OPTION_VAL_144_PT_PT NULL
#define OPTION_VAL_240_PT_PT NULL
#define SHADPS4_READBACKS_LABEL_PT_PT NULL
#define SHADPS4_READBACKS_INFO_0_PT_PT NULL
#define OPTION_VAL_RELAXED_PT_PT NULL
#define OPTION_VAL_PRECISE_PT_PT NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_PT_PT NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_PT_PT NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_PT_PT NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_PT_PT NULL
#define SHADPS4_SHADER_CACHE_LABEL_PT_PT NULL
#define SHADPS4_SHADER_CACHE_INFO_0_PT_PT NULL
#define SHADPS4_SHOW_FPS_LABEL_PT_PT NULL
#define SHADPS4_SHOW_FPS_INFO_0_PT_PT NULL
#define SHADPS4_VOLUME_LABEL_PT_PT NULL
#define SHADPS4_VOLUME_INFO_0_PT_PT NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_PT_PT NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_PT_PT NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_PT_PT NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_PT_PT NULL
#define OPTION_VAL_40_PT_PT NULL
#define OPTION_VAL_50_PT_PT NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_PT_PT NULL
#define OPTION_VAL_70_PT_PT NULL
#define OPTION_VAL_80_PT_PT NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_PT_PT NULL
#define OPTION_VAL_100_PT_PT NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_PT_PT NULL
#define OPTION_VAL_150_PT_PT NULL
#define OPTION_VAL_200_PT_PT NULL
#define OPTION_VAL_300_PT_PT NULL
#define OPTION_VAL_400_PT_PT NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_PT_PT NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_PT_PT NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_PT_PT NULL
#define SHADPS4_TROPHY_POSITION_LABEL_PT_PT NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_PT_PT NULL
#define OPTION_VAL_LEFT_PT_PT NULL
#define OPTION_VAL_RIGHT_PT_PT NULL
#define OPTION_VAL_TOP_PT_PT NULL
#define OPTION_VAL_BOTTOM_PT_PT NULL
#define SHADPS4_TROPHY_DURATION_LABEL_PT_PT NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_PT_PT NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_PT_PT NULL
#define SHADPS4_LOG_INFO_0_PT_PT NULL
#define SHADPS4_LOG_SEPARATE_LABEL_PT_PT NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_PT_PT NULL
#define SHADPS4_LOG_SYNC_LABEL_PT_PT NULL
#define SHADPS4_LOG_SYNC_INFO_0_PT_PT NULL

struct retro_core_option_v2_category option_cats_pt_pt[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_PT_PT,
        CATEGORY_SYSTEM_INFO_0_PT_PT
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_PT_PT,
        CATEGORY_VIDEO_INFO_0_PT_PT
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_PT_PT,
        CATEGORY_AUDIO_INFO_0_PT_PT
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_PT_PT,
        CATEGORY_TROPHIES_INFO_0_PT_PT
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_PT_PT,
        CATEGORY_LOGGING_INFO_0_PT_PT
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_pt_pt[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_PT_PT,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_PT_PT,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_PT_PT },
            { "18", OPTION_VAL_18_PT_PT },
            { "21", OPTION_VAL_21_PT_PT },
            { "23", OPTION_VAL_23_PT_PT },
            { "14", OPTION_VAL_14_PT_PT },
            { "6", OPTION_VAL_6_PT_PT },
            { "12", OPTION_VAL_12_PT_PT },
            { "22", OPTION_VAL_22_PT_PT },
            { "2", OPTION_VAL_2_PT_PT },
            { "4", OPTION_VAL_4_PT_PT },
            { "25", OPTION_VAL_25_PT_PT },
            { "24", OPTION_VAL_24_PT_PT },
            { "29", OPTION_VAL_29_PT_PT },
            { "5", OPTION_VAL_5_PT_PT },
            { "0", OPTION_VAL_0_PT_PT },
            { "9", OPTION_VAL_9_PT_PT },
            { "15", OPTION_VAL_15_PT_PT },
            { "16", OPTION_VAL_16_PT_PT },
            { "17", OPTION_VAL_17_PT_PT },
            { "7", OPTION_VAL_7_PT_PT },
            { "26", OPTION_VAL_26_PT_PT },
            { "8", OPTION_VAL_8_PT_PT },
            { "11", OPTION_VAL_11_PT_PT },
            { "20", OPTION_VAL_20_PT_PT },
            { "3", OPTION_VAL_3_PT_PT },
            { "13", OPTION_VAL_13_PT_PT },
            { "27", OPTION_VAL_27_PT_PT },
            { "10", OPTION_VAL_10_PT_PT },
            { "19", OPTION_VAL_19_PT_PT },
            { "30", OPTION_VAL_30_PT_PT },
            { "28", OPTION_VAL_28_PT_PT },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_PT_PT,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_PT_PT,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_PT_PT },
            { "512", OPTION_VAL_512_PT_PT },
            { "1024", OPTION_VAL_1024_PT_PT },
            { "2048", OPTION_VAL_2048_PT_PT },
            { "3072", OPTION_VAL_3072_PT_PT },
            { "4096", OPTION_VAL_4096_PT_PT },
            { "6144", OPTION_VAL_6144_PT_PT },
            { "8192", OPTION_VAL_8192_PT_PT },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_PT_PT,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_PT_PT },
            { "circle", OPTION_VAL_CIRCLE_PT_PT },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_PT_PT,
        NULL,
        SHADPS4_NETWORK_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_PT_PT,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_PT_PT,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_PT_PT,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_PT_PT,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_PT_PT,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_PT_PT },
            { "1920x1080", OPTION_VAL_1920X1080_PT_PT },
            { "2560x1440", OPTION_VAL_2560X1440_PT_PT },
            { "3840x2160", OPTION_VAL_3840X2160_PT_PT },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_PT_PT,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_PT_PT,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_PT_PT },
            { "60",  OPTION_VAL_60_PT_PT },
            { "90",  OPTION_VAL_90_PT_PT },
            { "120", OPTION_VAL_120_PT_PT },
            { "144", OPTION_VAL_144_PT_PT },
            { "240", OPTION_VAL_240_PT_PT },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_PT_PT,
        NULL,
        SHADPS4_READBACKS_INFO_0_PT_PT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_PT_PT },
            { "precise",  OPTION_VAL_PRECISE_PT_PT },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_PT_PT,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_PT_PT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_PT_PT,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_PT_PT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_PT_PT,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_PT_PT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_PT_PT,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_PT_PT,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_PT_PT,
        NULL,
        SHADPS4_VOLUME_INFO_0_PT_PT,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_PT_PT },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_PT_PT },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_PT_PT },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_PT_PT },
            { "40",  OPTION_VAL_40_PT_PT },
            { "50",  OPTION_VAL_50_PT_PT },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_PT_PT },
            { "70",  OPTION_VAL_70_PT_PT },
            { "80",  OPTION_VAL_80_PT_PT },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_PT_PT },
            { "100", OPTION_VAL_100_PT_PT },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_PT_PT },
            { "150", OPTION_VAL_150_PT_PT },
            { "200", OPTION_VAL_200_PT_PT },
            { "300", OPTION_VAL_300_PT_PT },
            { "400", OPTION_VAL_400_PT_PT },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_PT_PT },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_PT_PT,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_PT_PT,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_PT_PT,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_PT_PT,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_PT_PT },
            { "right", OPTION_VAL_RIGHT_PT_PT },
            { "top", OPTION_VAL_TOP_PT_PT },
            { "bottom", OPTION_VAL_BOTTOM_PT_PT },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_PT_PT,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_PT_PT,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_PT_PT },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_PT_PT },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_PT_PT },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_PT_PT },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_PT_PT },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_PT_PT },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_PT_PT },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_PT_PT },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_PT_PT },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_PT_PT },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_PT_PT,
        NULL,
        SHADPS4_LOG_INFO_0_PT_PT,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_PT_PT,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_PT_PT,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_PT_PT,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_PT_PT,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_pt_pt = {
   option_cats_pt_pt,
   option_defs_pt_pt
};

/* RETRO_LANGUAGE_RU */

#define CATEGORY_SYSTEM_LABEL_RU NULL
#define CATEGORY_SYSTEM_INFO_0_RU NULL
#define CATEGORY_VIDEO_LABEL_RU NULL
#define CATEGORY_VIDEO_INFO_0_RU NULL
#define CATEGORY_AUDIO_LABEL_RU NULL
#define CATEGORY_AUDIO_INFO_0_RU NULL
#define CATEGORY_TROPHIES_LABEL_RU NULL
#define CATEGORY_TROPHIES_INFO_0_RU NULL
#define CATEGORY_LOGGING_LABEL_RU NULL
#define CATEGORY_LOGGING_INFO_0_RU NULL
#define SHADPS4_NEO_MODE_LABEL_RU NULL
#define SHADPS4_NEO_MODE_INFO_0_RU NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_RU NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_RU NULL
#define OPTION_VAL_1_RU NULL
#define OPTION_VAL_18_RU NULL
#define OPTION_VAL_21_RU NULL
#define OPTION_VAL_23_RU NULL
#define OPTION_VAL_14_RU NULL
#define OPTION_VAL_6_RU NULL
#define OPTION_VAL_12_RU NULL
#define OPTION_VAL_22_RU NULL
#define OPTION_VAL_2_RU NULL
#define OPTION_VAL_4_RU NULL
#define OPTION_VAL_25_RU NULL
#define OPTION_VAL_24_RU NULL
#define OPTION_VAL_29_RU NULL
#define OPTION_VAL_5_RU NULL
#define OPTION_VAL_0_RU NULL
#define OPTION_VAL_9_RU NULL
#define OPTION_VAL_15_RU NULL
#define OPTION_VAL_16_RU NULL
#define OPTION_VAL_17_RU NULL
#define OPTION_VAL_7_RU NULL
#define OPTION_VAL_26_RU NULL
#define OPTION_VAL_8_RU NULL
#define OPTION_VAL_11_RU NULL
#define OPTION_VAL_20_RU NULL
#define OPTION_VAL_3_RU NULL
#define OPTION_VAL_13_RU NULL
#define OPTION_VAL_27_RU NULL
#define OPTION_VAL_10_RU NULL
#define OPTION_VAL_19_RU NULL
#define OPTION_VAL_30_RU NULL
#define OPTION_VAL_28_RU NULL
#define SHADPS4_DEV_KIT_LABEL_RU NULL
#define SHADPS4_DEV_KIT_INFO_0_RU NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_RU NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_RU NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_RU NULL
#define OPTION_VAL_512_RU NULL
#define OPTION_VAL_1024_RU NULL
#define OPTION_VAL_2048_RU NULL
#define OPTION_VAL_3072_RU NULL
#define OPTION_VAL_4096_RU NULL
#define OPTION_VAL_6144_RU NULL
#define OPTION_VAL_8192_RU NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_RU NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_RU NULL
#define OPTION_VAL_CROSS_RU NULL
#define OPTION_VAL_CIRCLE_RU NULL
#define SHADPS4_NETWORK_LABEL_RU NULL
#define SHADPS4_NETWORK_INFO_0_RU NULL
#define SHADPS4_CONTROLLERS_LABEL_RU NULL
#define SHADPS4_CONTROLLERS_INFO_0_RU NULL
#define SHADPS4_SHOW_SPLASH_LABEL_RU NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_RU NULL
#define SHADPS4_RESOLUTION_LABEL_RU NULL
#define SHADPS4_RESOLUTION_INFO_0_RU NULL
#define OPTION_VAL_1280X720_RU NULL
#define OPTION_VAL_1920X1080_RU NULL
#define OPTION_VAL_2560X1440_RU NULL
#define OPTION_VAL_3840X2160_RU NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_RU NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_RU NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_RU NULL
#define OPTION_VAL_60_RU NULL
#define OPTION_VAL_90_RU NULL
#define OPTION_VAL_120_RU NULL
#define OPTION_VAL_144_RU NULL
#define OPTION_VAL_240_RU NULL
#define SHADPS4_READBACKS_LABEL_RU NULL
#define SHADPS4_READBACKS_INFO_0_RU NULL
#define OPTION_VAL_RELAXED_RU NULL
#define OPTION_VAL_PRECISE_RU NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_RU NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_RU NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_RU NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_RU NULL
#define SHADPS4_SHADER_CACHE_LABEL_RU NULL
#define SHADPS4_SHADER_CACHE_INFO_0_RU NULL
#define SHADPS4_SHOW_FPS_LABEL_RU NULL
#define SHADPS4_SHOW_FPS_INFO_0_RU NULL
#define SHADPS4_VOLUME_LABEL_RU NULL
#define SHADPS4_VOLUME_INFO_0_RU NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_RU NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_RU NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_RU NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_RU NULL
#define OPTION_VAL_40_RU NULL
#define OPTION_VAL_50_RU NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_RU NULL
#define OPTION_VAL_70_RU NULL
#define OPTION_VAL_80_RU NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_RU NULL
#define OPTION_VAL_100_RU NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_RU NULL
#define OPTION_VAL_150_RU NULL
#define OPTION_VAL_200_RU NULL
#define OPTION_VAL_300_RU NULL
#define OPTION_VAL_400_RU NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_RU NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_RU NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_RU NULL
#define SHADPS4_TROPHY_POSITION_LABEL_RU NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_RU NULL
#define OPTION_VAL_LEFT_RU NULL
#define OPTION_VAL_RIGHT_RU NULL
#define OPTION_VAL_TOP_RU NULL
#define OPTION_VAL_BOTTOM_RU NULL
#define SHADPS4_TROPHY_DURATION_LABEL_RU NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_RU NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_RU NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_RU NULL
#define SHADPS4_LOG_INFO_0_RU NULL
#define SHADPS4_LOG_SEPARATE_LABEL_RU NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_RU NULL
#define SHADPS4_LOG_SYNC_LABEL_RU NULL
#define SHADPS4_LOG_SYNC_INFO_0_RU NULL

struct retro_core_option_v2_category option_cats_ru[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_RU,
        CATEGORY_SYSTEM_INFO_0_RU
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_RU,
        CATEGORY_VIDEO_INFO_0_RU
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_RU,
        CATEGORY_AUDIO_INFO_0_RU
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_RU,
        CATEGORY_TROPHIES_INFO_0_RU
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_RU,
        CATEGORY_LOGGING_INFO_0_RU
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_ru[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_RU,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_RU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_RU,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_RU,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_RU },
            { "18", OPTION_VAL_18_RU },
            { "21", OPTION_VAL_21_RU },
            { "23", OPTION_VAL_23_RU },
            { "14", OPTION_VAL_14_RU },
            { "6", OPTION_VAL_6_RU },
            { "12", OPTION_VAL_12_RU },
            { "22", OPTION_VAL_22_RU },
            { "2", OPTION_VAL_2_RU },
            { "4", OPTION_VAL_4_RU },
            { "25", OPTION_VAL_25_RU },
            { "24", OPTION_VAL_24_RU },
            { "29", OPTION_VAL_29_RU },
            { "5", OPTION_VAL_5_RU },
            { "0", OPTION_VAL_0_RU },
            { "9", OPTION_VAL_9_RU },
            { "15", OPTION_VAL_15_RU },
            { "16", OPTION_VAL_16_RU },
            { "17", OPTION_VAL_17_RU },
            { "7", OPTION_VAL_7_RU },
            { "26", OPTION_VAL_26_RU },
            { "8", OPTION_VAL_8_RU },
            { "11", OPTION_VAL_11_RU },
            { "20", OPTION_VAL_20_RU },
            { "3", OPTION_VAL_3_RU },
            { "13", OPTION_VAL_13_RU },
            { "27", OPTION_VAL_27_RU },
            { "10", OPTION_VAL_10_RU },
            { "19", OPTION_VAL_19_RU },
            { "30", OPTION_VAL_30_RU },
            { "28", OPTION_VAL_28_RU },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_RU,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_RU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_RU,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_RU,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_RU },
            { "512", OPTION_VAL_512_RU },
            { "1024", OPTION_VAL_1024_RU },
            { "2048", OPTION_VAL_2048_RU },
            { "3072", OPTION_VAL_3072_RU },
            { "4096", OPTION_VAL_4096_RU },
            { "6144", OPTION_VAL_6144_RU },
            { "8192", OPTION_VAL_8192_RU },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_RU,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_RU,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_RU },
            { "circle", OPTION_VAL_CIRCLE_RU },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_RU,
        NULL,
        SHADPS4_NETWORK_INFO_0_RU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_RU,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_RU,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_RU,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_RU,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_RU,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_RU,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_RU },
            { "1920x1080", OPTION_VAL_1920X1080_RU },
            { "2560x1440", OPTION_VAL_2560X1440_RU },
            { "3840x2160", OPTION_VAL_3840X2160_RU },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_RU,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_RU,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_RU },
            { "60",  OPTION_VAL_60_RU },
            { "90",  OPTION_VAL_90_RU },
            { "120", OPTION_VAL_120_RU },
            { "144", OPTION_VAL_144_RU },
            { "240", OPTION_VAL_240_RU },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_RU,
        NULL,
        SHADPS4_READBACKS_INFO_0_RU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_RU },
            { "precise",  OPTION_VAL_PRECISE_RU },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_RU,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_RU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_RU,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_RU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_RU,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_RU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_RU,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_RU,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_RU,
        NULL,
        SHADPS4_VOLUME_INFO_0_RU,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_RU },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_RU },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_RU },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_RU },
            { "40",  OPTION_VAL_40_RU },
            { "50",  OPTION_VAL_50_RU },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_RU },
            { "70",  OPTION_VAL_70_RU },
            { "80",  OPTION_VAL_80_RU },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_RU },
            { "100", OPTION_VAL_100_RU },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_RU },
            { "150", OPTION_VAL_150_RU },
            { "200", OPTION_VAL_200_RU },
            { "300", OPTION_VAL_300_RU },
            { "400", OPTION_VAL_400_RU },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_RU },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_RU,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_RU,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_RU,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_RU,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_RU },
            { "right", OPTION_VAL_RIGHT_RU },
            { "top", OPTION_VAL_TOP_RU },
            { "bottom", OPTION_VAL_BOTTOM_RU },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_RU,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_RU,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_RU },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_RU },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_RU },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_RU },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_RU },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_RU },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_RU },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_RU },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_RU },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_RU },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_RU,
        NULL,
        SHADPS4_LOG_INFO_0_RU,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_RU,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_RU,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_RU,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_RU,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_ru = {
   option_cats_ru,
   option_defs_ru
};

/* RETRO_LANGUAGE_SK */

#define CATEGORY_SYSTEM_LABEL_SK NULL
#define CATEGORY_SYSTEM_INFO_0_SK NULL
#define CATEGORY_VIDEO_LABEL_SK NULL
#define CATEGORY_VIDEO_INFO_0_SK NULL
#define CATEGORY_AUDIO_LABEL_SK NULL
#define CATEGORY_AUDIO_INFO_0_SK NULL
#define CATEGORY_TROPHIES_LABEL_SK NULL
#define CATEGORY_TROPHIES_INFO_0_SK NULL
#define CATEGORY_LOGGING_LABEL_SK NULL
#define CATEGORY_LOGGING_INFO_0_SK NULL
#define SHADPS4_NEO_MODE_LABEL_SK NULL
#define SHADPS4_NEO_MODE_INFO_0_SK NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_SK NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_SK NULL
#define OPTION_VAL_1_SK NULL
#define OPTION_VAL_18_SK NULL
#define OPTION_VAL_21_SK NULL
#define OPTION_VAL_23_SK NULL
#define OPTION_VAL_14_SK NULL
#define OPTION_VAL_6_SK NULL
#define OPTION_VAL_12_SK NULL
#define OPTION_VAL_22_SK NULL
#define OPTION_VAL_2_SK NULL
#define OPTION_VAL_4_SK NULL
#define OPTION_VAL_25_SK NULL
#define OPTION_VAL_24_SK NULL
#define OPTION_VAL_29_SK NULL
#define OPTION_VAL_5_SK NULL
#define OPTION_VAL_0_SK NULL
#define OPTION_VAL_9_SK NULL
#define OPTION_VAL_15_SK NULL
#define OPTION_VAL_16_SK NULL
#define OPTION_VAL_17_SK NULL
#define OPTION_VAL_7_SK NULL
#define OPTION_VAL_26_SK NULL
#define OPTION_VAL_8_SK NULL
#define OPTION_VAL_11_SK NULL
#define OPTION_VAL_20_SK NULL
#define OPTION_VAL_3_SK NULL
#define OPTION_VAL_13_SK NULL
#define OPTION_VAL_27_SK NULL
#define OPTION_VAL_10_SK NULL
#define OPTION_VAL_19_SK NULL
#define OPTION_VAL_30_SK NULL
#define OPTION_VAL_28_SK NULL
#define SHADPS4_DEV_KIT_LABEL_SK NULL
#define SHADPS4_DEV_KIT_INFO_0_SK NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_SK NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_SK NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_SK NULL
#define OPTION_VAL_512_SK NULL
#define OPTION_VAL_1024_SK NULL
#define OPTION_VAL_2048_SK NULL
#define OPTION_VAL_3072_SK NULL
#define OPTION_VAL_4096_SK NULL
#define OPTION_VAL_6144_SK NULL
#define OPTION_VAL_8192_SK NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_SK NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_SK NULL
#define OPTION_VAL_CROSS_SK NULL
#define OPTION_VAL_CIRCLE_SK NULL
#define SHADPS4_NETWORK_LABEL_SK NULL
#define SHADPS4_NETWORK_INFO_0_SK NULL
#define SHADPS4_CONTROLLERS_LABEL_SK NULL
#define SHADPS4_CONTROLLERS_INFO_0_SK NULL
#define SHADPS4_SHOW_SPLASH_LABEL_SK NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_SK NULL
#define SHADPS4_RESOLUTION_LABEL_SK NULL
#define SHADPS4_RESOLUTION_INFO_0_SK NULL
#define OPTION_VAL_1280X720_SK NULL
#define OPTION_VAL_1920X1080_SK NULL
#define OPTION_VAL_2560X1440_SK NULL
#define OPTION_VAL_3840X2160_SK NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_SK NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_SK NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_SK NULL
#define OPTION_VAL_60_SK NULL
#define OPTION_VAL_90_SK NULL
#define OPTION_VAL_120_SK NULL
#define OPTION_VAL_144_SK NULL
#define OPTION_VAL_240_SK NULL
#define SHADPS4_READBACKS_LABEL_SK NULL
#define SHADPS4_READBACKS_INFO_0_SK NULL
#define OPTION_VAL_RELAXED_SK NULL
#define OPTION_VAL_PRECISE_SK NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_SK NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_SK NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_SK NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_SK NULL
#define SHADPS4_SHADER_CACHE_LABEL_SK NULL
#define SHADPS4_SHADER_CACHE_INFO_0_SK NULL
#define SHADPS4_SHOW_FPS_LABEL_SK NULL
#define SHADPS4_SHOW_FPS_INFO_0_SK NULL
#define SHADPS4_VOLUME_LABEL_SK NULL
#define SHADPS4_VOLUME_INFO_0_SK NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_SK NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_SK NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_SK NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_SK NULL
#define OPTION_VAL_40_SK NULL
#define OPTION_VAL_50_SK NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_SK NULL
#define OPTION_VAL_70_SK NULL
#define OPTION_VAL_80_SK NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_SK NULL
#define OPTION_VAL_100_SK NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_SK NULL
#define OPTION_VAL_150_SK NULL
#define OPTION_VAL_200_SK NULL
#define OPTION_VAL_300_SK NULL
#define OPTION_VAL_400_SK NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_SK NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_SK NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_SK NULL
#define SHADPS4_TROPHY_POSITION_LABEL_SK NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_SK NULL
#define OPTION_VAL_LEFT_SK NULL
#define OPTION_VAL_RIGHT_SK NULL
#define OPTION_VAL_TOP_SK NULL
#define OPTION_VAL_BOTTOM_SK NULL
#define SHADPS4_TROPHY_DURATION_LABEL_SK NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_SK NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_SK NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_SK NULL
#define SHADPS4_LOG_INFO_0_SK NULL
#define SHADPS4_LOG_SEPARATE_LABEL_SK NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_SK NULL
#define SHADPS4_LOG_SYNC_LABEL_SK NULL
#define SHADPS4_LOG_SYNC_INFO_0_SK NULL

struct retro_core_option_v2_category option_cats_sk[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_SK,
        CATEGORY_SYSTEM_INFO_0_SK
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_SK,
        CATEGORY_VIDEO_INFO_0_SK
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_SK,
        CATEGORY_AUDIO_INFO_0_SK
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_SK,
        CATEGORY_TROPHIES_INFO_0_SK
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_SK,
        CATEGORY_LOGGING_INFO_0_SK
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sk[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_SK,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_SK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_SK,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_SK,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_SK },
            { "18", OPTION_VAL_18_SK },
            { "21", OPTION_VAL_21_SK },
            { "23", OPTION_VAL_23_SK },
            { "14", OPTION_VAL_14_SK },
            { "6", OPTION_VAL_6_SK },
            { "12", OPTION_VAL_12_SK },
            { "22", OPTION_VAL_22_SK },
            { "2", OPTION_VAL_2_SK },
            { "4", OPTION_VAL_4_SK },
            { "25", OPTION_VAL_25_SK },
            { "24", OPTION_VAL_24_SK },
            { "29", OPTION_VAL_29_SK },
            { "5", OPTION_VAL_5_SK },
            { "0", OPTION_VAL_0_SK },
            { "9", OPTION_VAL_9_SK },
            { "15", OPTION_VAL_15_SK },
            { "16", OPTION_VAL_16_SK },
            { "17", OPTION_VAL_17_SK },
            { "7", OPTION_VAL_7_SK },
            { "26", OPTION_VAL_26_SK },
            { "8", OPTION_VAL_8_SK },
            { "11", OPTION_VAL_11_SK },
            { "20", OPTION_VAL_20_SK },
            { "3", OPTION_VAL_3_SK },
            { "13", OPTION_VAL_13_SK },
            { "27", OPTION_VAL_27_SK },
            { "10", OPTION_VAL_10_SK },
            { "19", OPTION_VAL_19_SK },
            { "30", OPTION_VAL_30_SK },
            { "28", OPTION_VAL_28_SK },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_SK,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_SK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_SK,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_SK,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_SK },
            { "512", OPTION_VAL_512_SK },
            { "1024", OPTION_VAL_1024_SK },
            { "2048", OPTION_VAL_2048_SK },
            { "3072", OPTION_VAL_3072_SK },
            { "4096", OPTION_VAL_4096_SK },
            { "6144", OPTION_VAL_6144_SK },
            { "8192", OPTION_VAL_8192_SK },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_SK,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_SK,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_SK },
            { "circle", OPTION_VAL_CIRCLE_SK },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_SK,
        NULL,
        SHADPS4_NETWORK_INFO_0_SK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_SK,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_SK,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_SK,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_SK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_SK,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_SK,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_SK },
            { "1920x1080", OPTION_VAL_1920X1080_SK },
            { "2560x1440", OPTION_VAL_2560X1440_SK },
            { "3840x2160", OPTION_VAL_3840X2160_SK },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_SK,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_SK,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_SK },
            { "60",  OPTION_VAL_60_SK },
            { "90",  OPTION_VAL_90_SK },
            { "120", OPTION_VAL_120_SK },
            { "144", OPTION_VAL_144_SK },
            { "240", OPTION_VAL_240_SK },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_SK,
        NULL,
        SHADPS4_READBACKS_INFO_0_SK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_SK },
            { "precise",  OPTION_VAL_PRECISE_SK },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_SK,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_SK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_SK,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_SK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_SK,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_SK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_SK,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_SK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_SK,
        NULL,
        SHADPS4_VOLUME_INFO_0_SK,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_SK },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_SK },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_SK },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_SK },
            { "40",  OPTION_VAL_40_SK },
            { "50",  OPTION_VAL_50_SK },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_SK },
            { "70",  OPTION_VAL_70_SK },
            { "80",  OPTION_VAL_80_SK },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_SK },
            { "100", OPTION_VAL_100_SK },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_SK },
            { "150", OPTION_VAL_150_SK },
            { "200", OPTION_VAL_200_SK },
            { "300", OPTION_VAL_300_SK },
            { "400", OPTION_VAL_400_SK },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_SK },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_SK,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_SK,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_SK,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_SK,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_SK },
            { "right", OPTION_VAL_RIGHT_SK },
            { "top", OPTION_VAL_TOP_SK },
            { "bottom", OPTION_VAL_BOTTOM_SK },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_SK,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_SK,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_SK },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_SK },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_SK },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_SK },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_SK },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_SK },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_SK },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_SK },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_SK },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_SK },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_SK,
        NULL,
        SHADPS4_LOG_INFO_0_SK,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_SK,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_SK,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_SK,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_SK,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_sk = {
   option_cats_sk,
   option_defs_sk
};

/* RETRO_LANGUAGE_SV */

#define CATEGORY_SYSTEM_LABEL_SV NULL
#define CATEGORY_SYSTEM_INFO_0_SV NULL
#define CATEGORY_VIDEO_LABEL_SV NULL
#define CATEGORY_VIDEO_INFO_0_SV NULL
#define CATEGORY_AUDIO_LABEL_SV NULL
#define CATEGORY_AUDIO_INFO_0_SV NULL
#define CATEGORY_TROPHIES_LABEL_SV NULL
#define CATEGORY_TROPHIES_INFO_0_SV NULL
#define CATEGORY_LOGGING_LABEL_SV NULL
#define CATEGORY_LOGGING_INFO_0_SV NULL
#define SHADPS4_NEO_MODE_LABEL_SV NULL
#define SHADPS4_NEO_MODE_INFO_0_SV NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_SV NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_SV NULL
#define OPTION_VAL_1_SV NULL
#define OPTION_VAL_18_SV NULL
#define OPTION_VAL_21_SV NULL
#define OPTION_VAL_23_SV NULL
#define OPTION_VAL_14_SV NULL
#define OPTION_VAL_6_SV NULL
#define OPTION_VAL_12_SV NULL
#define OPTION_VAL_22_SV NULL
#define OPTION_VAL_2_SV NULL
#define OPTION_VAL_4_SV NULL
#define OPTION_VAL_25_SV NULL
#define OPTION_VAL_24_SV NULL
#define OPTION_VAL_29_SV NULL
#define OPTION_VAL_5_SV NULL
#define OPTION_VAL_0_SV NULL
#define OPTION_VAL_9_SV NULL
#define OPTION_VAL_15_SV NULL
#define OPTION_VAL_16_SV NULL
#define OPTION_VAL_17_SV NULL
#define OPTION_VAL_7_SV NULL
#define OPTION_VAL_26_SV NULL
#define OPTION_VAL_8_SV NULL
#define OPTION_VAL_11_SV NULL
#define OPTION_VAL_20_SV NULL
#define OPTION_VAL_3_SV NULL
#define OPTION_VAL_13_SV NULL
#define OPTION_VAL_27_SV NULL
#define OPTION_VAL_10_SV NULL
#define OPTION_VAL_19_SV NULL
#define OPTION_VAL_30_SV NULL
#define OPTION_VAL_28_SV NULL
#define SHADPS4_DEV_KIT_LABEL_SV NULL
#define SHADPS4_DEV_KIT_INFO_0_SV NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_SV NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_SV NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_SV NULL
#define OPTION_VAL_512_SV NULL
#define OPTION_VAL_1024_SV NULL
#define OPTION_VAL_2048_SV NULL
#define OPTION_VAL_3072_SV NULL
#define OPTION_VAL_4096_SV NULL
#define OPTION_VAL_6144_SV NULL
#define OPTION_VAL_8192_SV NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_SV NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_SV NULL
#define OPTION_VAL_CROSS_SV NULL
#define OPTION_VAL_CIRCLE_SV NULL
#define SHADPS4_NETWORK_LABEL_SV NULL
#define SHADPS4_NETWORK_INFO_0_SV NULL
#define SHADPS4_CONTROLLERS_LABEL_SV NULL
#define SHADPS4_CONTROLLERS_INFO_0_SV NULL
#define SHADPS4_SHOW_SPLASH_LABEL_SV NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_SV NULL
#define SHADPS4_RESOLUTION_LABEL_SV NULL
#define SHADPS4_RESOLUTION_INFO_0_SV NULL
#define OPTION_VAL_1280X720_SV NULL
#define OPTION_VAL_1920X1080_SV NULL
#define OPTION_VAL_2560X1440_SV NULL
#define OPTION_VAL_3840X2160_SV NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_SV NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_SV NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_SV NULL
#define OPTION_VAL_60_SV NULL
#define OPTION_VAL_90_SV NULL
#define OPTION_VAL_120_SV NULL
#define OPTION_VAL_144_SV NULL
#define OPTION_VAL_240_SV NULL
#define SHADPS4_READBACKS_LABEL_SV NULL
#define SHADPS4_READBACKS_INFO_0_SV NULL
#define OPTION_VAL_RELAXED_SV NULL
#define OPTION_VAL_PRECISE_SV NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_SV NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_SV NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_SV NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_SV NULL
#define SHADPS4_SHADER_CACHE_LABEL_SV NULL
#define SHADPS4_SHADER_CACHE_INFO_0_SV NULL
#define SHADPS4_SHOW_FPS_LABEL_SV NULL
#define SHADPS4_SHOW_FPS_INFO_0_SV NULL
#define SHADPS4_VOLUME_LABEL_SV NULL
#define SHADPS4_VOLUME_INFO_0_SV NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_SV NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_SV NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_SV NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_SV NULL
#define OPTION_VAL_40_SV NULL
#define OPTION_VAL_50_SV NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_SV NULL
#define OPTION_VAL_70_SV NULL
#define OPTION_VAL_80_SV NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_SV NULL
#define OPTION_VAL_100_SV NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_SV NULL
#define OPTION_VAL_150_SV NULL
#define OPTION_VAL_200_SV NULL
#define OPTION_VAL_300_SV NULL
#define OPTION_VAL_400_SV NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_SV NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_SV NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_SV NULL
#define SHADPS4_TROPHY_POSITION_LABEL_SV NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_SV NULL
#define OPTION_VAL_LEFT_SV NULL
#define OPTION_VAL_RIGHT_SV NULL
#define OPTION_VAL_TOP_SV NULL
#define OPTION_VAL_BOTTOM_SV NULL
#define SHADPS4_TROPHY_DURATION_LABEL_SV NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_SV NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_SV NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_SV NULL
#define SHADPS4_LOG_INFO_0_SV NULL
#define SHADPS4_LOG_SEPARATE_LABEL_SV NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_SV NULL
#define SHADPS4_LOG_SYNC_LABEL_SV NULL
#define SHADPS4_LOG_SYNC_INFO_0_SV NULL

struct retro_core_option_v2_category option_cats_sv[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_SV,
        CATEGORY_SYSTEM_INFO_0_SV
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_SV,
        CATEGORY_VIDEO_INFO_0_SV
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_SV,
        CATEGORY_AUDIO_INFO_0_SV
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_SV,
        CATEGORY_TROPHIES_INFO_0_SV
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_SV,
        CATEGORY_LOGGING_INFO_0_SV
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_sv[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_SV,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_SV,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_SV,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_SV,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_SV },
            { "18", OPTION_VAL_18_SV },
            { "21", OPTION_VAL_21_SV },
            { "23", OPTION_VAL_23_SV },
            { "14", OPTION_VAL_14_SV },
            { "6", OPTION_VAL_6_SV },
            { "12", OPTION_VAL_12_SV },
            { "22", OPTION_VAL_22_SV },
            { "2", OPTION_VAL_2_SV },
            { "4", OPTION_VAL_4_SV },
            { "25", OPTION_VAL_25_SV },
            { "24", OPTION_VAL_24_SV },
            { "29", OPTION_VAL_29_SV },
            { "5", OPTION_VAL_5_SV },
            { "0", OPTION_VAL_0_SV },
            { "9", OPTION_VAL_9_SV },
            { "15", OPTION_VAL_15_SV },
            { "16", OPTION_VAL_16_SV },
            { "17", OPTION_VAL_17_SV },
            { "7", OPTION_VAL_7_SV },
            { "26", OPTION_VAL_26_SV },
            { "8", OPTION_VAL_8_SV },
            { "11", OPTION_VAL_11_SV },
            { "20", OPTION_VAL_20_SV },
            { "3", OPTION_VAL_3_SV },
            { "13", OPTION_VAL_13_SV },
            { "27", OPTION_VAL_27_SV },
            { "10", OPTION_VAL_10_SV },
            { "19", OPTION_VAL_19_SV },
            { "30", OPTION_VAL_30_SV },
            { "28", OPTION_VAL_28_SV },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_SV,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_SV,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_SV,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_SV,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_SV },
            { "512", OPTION_VAL_512_SV },
            { "1024", OPTION_VAL_1024_SV },
            { "2048", OPTION_VAL_2048_SV },
            { "3072", OPTION_VAL_3072_SV },
            { "4096", OPTION_VAL_4096_SV },
            { "6144", OPTION_VAL_6144_SV },
            { "8192", OPTION_VAL_8192_SV },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_SV,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_SV,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_SV },
            { "circle", OPTION_VAL_CIRCLE_SV },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_SV,
        NULL,
        SHADPS4_NETWORK_INFO_0_SV,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_SV,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_SV,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_SV,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_SV,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_SV,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_SV,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_SV },
            { "1920x1080", OPTION_VAL_1920X1080_SV },
            { "2560x1440", OPTION_VAL_2560X1440_SV },
            { "3840x2160", OPTION_VAL_3840X2160_SV },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_SV,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_SV,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_SV },
            { "60",  OPTION_VAL_60_SV },
            { "90",  OPTION_VAL_90_SV },
            { "120", OPTION_VAL_120_SV },
            { "144", OPTION_VAL_144_SV },
            { "240", OPTION_VAL_240_SV },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_SV,
        NULL,
        SHADPS4_READBACKS_INFO_0_SV,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_SV },
            { "precise",  OPTION_VAL_PRECISE_SV },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_SV,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_SV,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_SV,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_SV,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_SV,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_SV,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_SV,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_SV,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_SV,
        NULL,
        SHADPS4_VOLUME_INFO_0_SV,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_SV },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_SV },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_SV },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_SV },
            { "40",  OPTION_VAL_40_SV },
            { "50",  OPTION_VAL_50_SV },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_SV },
            { "70",  OPTION_VAL_70_SV },
            { "80",  OPTION_VAL_80_SV },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_SV },
            { "100", OPTION_VAL_100_SV },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_SV },
            { "150", OPTION_VAL_150_SV },
            { "200", OPTION_VAL_200_SV },
            { "300", OPTION_VAL_300_SV },
            { "400", OPTION_VAL_400_SV },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_SV },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_SV,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_SV,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_SV,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_SV,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_SV },
            { "right", OPTION_VAL_RIGHT_SV },
            { "top", OPTION_VAL_TOP_SV },
            { "bottom", OPTION_VAL_BOTTOM_SV },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_SV,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_SV,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_SV },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_SV },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_SV },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_SV },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_SV },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_SV },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_SV },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_SV },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_SV },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_SV },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_SV,
        NULL,
        SHADPS4_LOG_INFO_0_SV,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_SV,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_SV,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_SV,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_SV,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_sv = {
   option_cats_sv,
   option_defs_sv
};

/* RETRO_LANGUAGE_TR */

#define CATEGORY_SYSTEM_LABEL_TR NULL
#define CATEGORY_SYSTEM_INFO_0_TR NULL
#define CATEGORY_VIDEO_LABEL_TR NULL
#define CATEGORY_VIDEO_INFO_0_TR NULL
#define CATEGORY_AUDIO_LABEL_TR NULL
#define CATEGORY_AUDIO_INFO_0_TR NULL
#define CATEGORY_TROPHIES_LABEL_TR NULL
#define CATEGORY_TROPHIES_INFO_0_TR NULL
#define CATEGORY_LOGGING_LABEL_TR NULL
#define CATEGORY_LOGGING_INFO_0_TR NULL
#define SHADPS4_NEO_MODE_LABEL_TR NULL
#define SHADPS4_NEO_MODE_INFO_0_TR NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_TR NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_TR NULL
#define OPTION_VAL_1_TR NULL
#define OPTION_VAL_18_TR NULL
#define OPTION_VAL_21_TR NULL
#define OPTION_VAL_23_TR NULL
#define OPTION_VAL_14_TR NULL
#define OPTION_VAL_6_TR NULL
#define OPTION_VAL_12_TR NULL
#define OPTION_VAL_22_TR NULL
#define OPTION_VAL_2_TR NULL
#define OPTION_VAL_4_TR NULL
#define OPTION_VAL_25_TR NULL
#define OPTION_VAL_24_TR NULL
#define OPTION_VAL_29_TR NULL
#define OPTION_VAL_5_TR NULL
#define OPTION_VAL_0_TR NULL
#define OPTION_VAL_9_TR NULL
#define OPTION_VAL_15_TR NULL
#define OPTION_VAL_16_TR NULL
#define OPTION_VAL_17_TR NULL
#define OPTION_VAL_7_TR NULL
#define OPTION_VAL_26_TR NULL
#define OPTION_VAL_8_TR NULL
#define OPTION_VAL_11_TR NULL
#define OPTION_VAL_20_TR NULL
#define OPTION_VAL_3_TR NULL
#define OPTION_VAL_13_TR NULL
#define OPTION_VAL_27_TR NULL
#define OPTION_VAL_10_TR NULL
#define OPTION_VAL_19_TR NULL
#define OPTION_VAL_30_TR NULL
#define OPTION_VAL_28_TR NULL
#define SHADPS4_DEV_KIT_LABEL_TR NULL
#define SHADPS4_DEV_KIT_INFO_0_TR NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_TR NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_TR NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_TR NULL
#define OPTION_VAL_512_TR NULL
#define OPTION_VAL_1024_TR NULL
#define OPTION_VAL_2048_TR NULL
#define OPTION_VAL_3072_TR NULL
#define OPTION_VAL_4096_TR NULL
#define OPTION_VAL_6144_TR NULL
#define OPTION_VAL_8192_TR NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_TR NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_TR NULL
#define OPTION_VAL_CROSS_TR NULL
#define OPTION_VAL_CIRCLE_TR NULL
#define SHADPS4_NETWORK_LABEL_TR NULL
#define SHADPS4_NETWORK_INFO_0_TR NULL
#define SHADPS4_CONTROLLERS_LABEL_TR NULL
#define SHADPS4_CONTROLLERS_INFO_0_TR NULL
#define SHADPS4_SHOW_SPLASH_LABEL_TR NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_TR NULL
#define SHADPS4_RESOLUTION_LABEL_TR NULL
#define SHADPS4_RESOLUTION_INFO_0_TR NULL
#define OPTION_VAL_1280X720_TR NULL
#define OPTION_VAL_1920X1080_TR NULL
#define OPTION_VAL_2560X1440_TR NULL
#define OPTION_VAL_3840X2160_TR NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_TR NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_TR NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_TR NULL
#define OPTION_VAL_60_TR NULL
#define OPTION_VAL_90_TR NULL
#define OPTION_VAL_120_TR NULL
#define OPTION_VAL_144_TR NULL
#define OPTION_VAL_240_TR NULL
#define SHADPS4_READBACKS_LABEL_TR NULL
#define SHADPS4_READBACKS_INFO_0_TR NULL
#define OPTION_VAL_RELAXED_TR NULL
#define OPTION_VAL_PRECISE_TR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_TR NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_TR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_TR NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_TR NULL
#define SHADPS4_SHADER_CACHE_LABEL_TR NULL
#define SHADPS4_SHADER_CACHE_INFO_0_TR NULL
#define SHADPS4_SHOW_FPS_LABEL_TR NULL
#define SHADPS4_SHOW_FPS_INFO_0_TR NULL
#define SHADPS4_VOLUME_LABEL_TR NULL
#define SHADPS4_VOLUME_INFO_0_TR NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_TR NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_TR NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_TR NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_TR NULL
#define OPTION_VAL_40_TR NULL
#define OPTION_VAL_50_TR NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_TR NULL
#define OPTION_VAL_70_TR NULL
#define OPTION_VAL_80_TR NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_TR NULL
#define OPTION_VAL_100_TR NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_TR NULL
#define OPTION_VAL_150_TR NULL
#define OPTION_VAL_200_TR NULL
#define OPTION_VAL_300_TR NULL
#define OPTION_VAL_400_TR NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_TR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_TR NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_TR NULL
#define SHADPS4_TROPHY_POSITION_LABEL_TR NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_TR NULL
#define OPTION_VAL_LEFT_TR NULL
#define OPTION_VAL_RIGHT_TR NULL
#define OPTION_VAL_TOP_TR NULL
#define OPTION_VAL_BOTTOM_TR NULL
#define SHADPS4_TROPHY_DURATION_LABEL_TR NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_TR NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_TR NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_TR NULL
#define SHADPS4_LOG_INFO_0_TR NULL
#define SHADPS4_LOG_SEPARATE_LABEL_TR NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_TR NULL
#define SHADPS4_LOG_SYNC_LABEL_TR NULL
#define SHADPS4_LOG_SYNC_INFO_0_TR NULL

struct retro_core_option_v2_category option_cats_tr[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_TR,
        CATEGORY_SYSTEM_INFO_0_TR
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_TR,
        CATEGORY_VIDEO_INFO_0_TR
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_TR,
        CATEGORY_AUDIO_INFO_0_TR
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_TR,
        CATEGORY_TROPHIES_INFO_0_TR
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_TR,
        CATEGORY_LOGGING_INFO_0_TR
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_tr[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_TR,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_TR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_TR,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_TR,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_TR },
            { "18", OPTION_VAL_18_TR },
            { "21", OPTION_VAL_21_TR },
            { "23", OPTION_VAL_23_TR },
            { "14", OPTION_VAL_14_TR },
            { "6", OPTION_VAL_6_TR },
            { "12", OPTION_VAL_12_TR },
            { "22", OPTION_VAL_22_TR },
            { "2", OPTION_VAL_2_TR },
            { "4", OPTION_VAL_4_TR },
            { "25", OPTION_VAL_25_TR },
            { "24", OPTION_VAL_24_TR },
            { "29", OPTION_VAL_29_TR },
            { "5", OPTION_VAL_5_TR },
            { "0", OPTION_VAL_0_TR },
            { "9", OPTION_VAL_9_TR },
            { "15", OPTION_VAL_15_TR },
            { "16", OPTION_VAL_16_TR },
            { "17", OPTION_VAL_17_TR },
            { "7", OPTION_VAL_7_TR },
            { "26", OPTION_VAL_26_TR },
            { "8", OPTION_VAL_8_TR },
            { "11", OPTION_VAL_11_TR },
            { "20", OPTION_VAL_20_TR },
            { "3", OPTION_VAL_3_TR },
            { "13", OPTION_VAL_13_TR },
            { "27", OPTION_VAL_27_TR },
            { "10", OPTION_VAL_10_TR },
            { "19", OPTION_VAL_19_TR },
            { "30", OPTION_VAL_30_TR },
            { "28", OPTION_VAL_28_TR },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_TR,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_TR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_TR,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_TR,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_TR },
            { "512", OPTION_VAL_512_TR },
            { "1024", OPTION_VAL_1024_TR },
            { "2048", OPTION_VAL_2048_TR },
            { "3072", OPTION_VAL_3072_TR },
            { "4096", OPTION_VAL_4096_TR },
            { "6144", OPTION_VAL_6144_TR },
            { "8192", OPTION_VAL_8192_TR },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_TR,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_TR,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_TR },
            { "circle", OPTION_VAL_CIRCLE_TR },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_TR,
        NULL,
        SHADPS4_NETWORK_INFO_0_TR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_TR,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_TR,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_TR,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_TR,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_TR,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_TR,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_TR },
            { "1920x1080", OPTION_VAL_1920X1080_TR },
            { "2560x1440", OPTION_VAL_2560X1440_TR },
            { "3840x2160", OPTION_VAL_3840X2160_TR },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_TR,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_TR,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_TR },
            { "60",  OPTION_VAL_60_TR },
            { "90",  OPTION_VAL_90_TR },
            { "120", OPTION_VAL_120_TR },
            { "144", OPTION_VAL_144_TR },
            { "240", OPTION_VAL_240_TR },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_TR,
        NULL,
        SHADPS4_READBACKS_INFO_0_TR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_TR },
            { "precise",  OPTION_VAL_PRECISE_TR },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_TR,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_TR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_TR,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_TR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_TR,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_TR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_TR,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_TR,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_TR,
        NULL,
        SHADPS4_VOLUME_INFO_0_TR,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_TR },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_TR },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_TR },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_TR },
            { "40",  OPTION_VAL_40_TR },
            { "50",  OPTION_VAL_50_TR },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_TR },
            { "70",  OPTION_VAL_70_TR },
            { "80",  OPTION_VAL_80_TR },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_TR },
            { "100", OPTION_VAL_100_TR },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_TR },
            { "150", OPTION_VAL_150_TR },
            { "200", OPTION_VAL_200_TR },
            { "300", OPTION_VAL_300_TR },
            { "400", OPTION_VAL_400_TR },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_TR },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_TR,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_TR,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_TR,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_TR,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_TR },
            { "right", OPTION_VAL_RIGHT_TR },
            { "top", OPTION_VAL_TOP_TR },
            { "bottom", OPTION_VAL_BOTTOM_TR },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_TR,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_TR,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_TR },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_TR },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_TR },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_TR },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_TR },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_TR },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_TR },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_TR },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_TR },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_TR },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_TR,
        NULL,
        SHADPS4_LOG_INFO_0_TR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_TR,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_TR,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_TR,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_TR,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_tr = {
   option_cats_tr,
   option_defs_tr
};

/* RETRO_LANGUAGE_UK */

#define CATEGORY_SYSTEM_LABEL_UK NULL
#define CATEGORY_SYSTEM_INFO_0_UK NULL
#define CATEGORY_VIDEO_LABEL_UK NULL
#define CATEGORY_VIDEO_INFO_0_UK NULL
#define CATEGORY_AUDIO_LABEL_UK NULL
#define CATEGORY_AUDIO_INFO_0_UK NULL
#define CATEGORY_TROPHIES_LABEL_UK NULL
#define CATEGORY_TROPHIES_INFO_0_UK NULL
#define CATEGORY_LOGGING_LABEL_UK NULL
#define CATEGORY_LOGGING_INFO_0_UK NULL
#define SHADPS4_NEO_MODE_LABEL_UK NULL
#define SHADPS4_NEO_MODE_INFO_0_UK NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_UK NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_UK NULL
#define OPTION_VAL_1_UK NULL
#define OPTION_VAL_18_UK NULL
#define OPTION_VAL_21_UK NULL
#define OPTION_VAL_23_UK NULL
#define OPTION_VAL_14_UK NULL
#define OPTION_VAL_6_UK NULL
#define OPTION_VAL_12_UK NULL
#define OPTION_VAL_22_UK NULL
#define OPTION_VAL_2_UK NULL
#define OPTION_VAL_4_UK NULL
#define OPTION_VAL_25_UK NULL
#define OPTION_VAL_24_UK NULL
#define OPTION_VAL_29_UK NULL
#define OPTION_VAL_5_UK NULL
#define OPTION_VAL_0_UK NULL
#define OPTION_VAL_9_UK NULL
#define OPTION_VAL_15_UK NULL
#define OPTION_VAL_16_UK NULL
#define OPTION_VAL_17_UK NULL
#define OPTION_VAL_7_UK NULL
#define OPTION_VAL_26_UK NULL
#define OPTION_VAL_8_UK NULL
#define OPTION_VAL_11_UK NULL
#define OPTION_VAL_20_UK NULL
#define OPTION_VAL_3_UK NULL
#define OPTION_VAL_13_UK NULL
#define OPTION_VAL_27_UK NULL
#define OPTION_VAL_10_UK NULL
#define OPTION_VAL_19_UK NULL
#define OPTION_VAL_30_UK NULL
#define OPTION_VAL_28_UK NULL
#define SHADPS4_DEV_KIT_LABEL_UK NULL
#define SHADPS4_DEV_KIT_INFO_0_UK NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_UK NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_UK NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_UK NULL
#define OPTION_VAL_512_UK NULL
#define OPTION_VAL_1024_UK NULL
#define OPTION_VAL_2048_UK NULL
#define OPTION_VAL_3072_UK NULL
#define OPTION_VAL_4096_UK NULL
#define OPTION_VAL_6144_UK NULL
#define OPTION_VAL_8192_UK NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_UK NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_UK NULL
#define OPTION_VAL_CROSS_UK NULL
#define OPTION_VAL_CIRCLE_UK NULL
#define SHADPS4_NETWORK_LABEL_UK NULL
#define SHADPS4_NETWORK_INFO_0_UK NULL
#define SHADPS4_CONTROLLERS_LABEL_UK NULL
#define SHADPS4_CONTROLLERS_INFO_0_UK NULL
#define SHADPS4_SHOW_SPLASH_LABEL_UK NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_UK NULL
#define SHADPS4_RESOLUTION_LABEL_UK NULL
#define SHADPS4_RESOLUTION_INFO_0_UK NULL
#define OPTION_VAL_1280X720_UK NULL
#define OPTION_VAL_1920X1080_UK NULL
#define OPTION_VAL_2560X1440_UK NULL
#define OPTION_VAL_3840X2160_UK NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_UK NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_UK NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_UK NULL
#define OPTION_VAL_60_UK NULL
#define OPTION_VAL_90_UK NULL
#define OPTION_VAL_120_UK NULL
#define OPTION_VAL_144_UK NULL
#define OPTION_VAL_240_UK NULL
#define SHADPS4_READBACKS_LABEL_UK NULL
#define SHADPS4_READBACKS_INFO_0_UK NULL
#define OPTION_VAL_RELAXED_UK NULL
#define OPTION_VAL_PRECISE_UK NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_UK NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_UK NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_UK NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_UK NULL
#define SHADPS4_SHADER_CACHE_LABEL_UK NULL
#define SHADPS4_SHADER_CACHE_INFO_0_UK NULL
#define SHADPS4_SHOW_FPS_LABEL_UK NULL
#define SHADPS4_SHOW_FPS_INFO_0_UK NULL
#define SHADPS4_VOLUME_LABEL_UK NULL
#define SHADPS4_VOLUME_INFO_0_UK NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_UK NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_UK NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_UK NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_UK NULL
#define OPTION_VAL_40_UK NULL
#define OPTION_VAL_50_UK NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_UK NULL
#define OPTION_VAL_70_UK NULL
#define OPTION_VAL_80_UK NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_UK NULL
#define OPTION_VAL_100_UK NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_UK NULL
#define OPTION_VAL_150_UK NULL
#define OPTION_VAL_200_UK NULL
#define OPTION_VAL_300_UK NULL
#define OPTION_VAL_400_UK NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_UK NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_UK NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_UK NULL
#define SHADPS4_TROPHY_POSITION_LABEL_UK NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_UK NULL
#define OPTION_VAL_LEFT_UK NULL
#define OPTION_VAL_RIGHT_UK NULL
#define OPTION_VAL_TOP_UK NULL
#define OPTION_VAL_BOTTOM_UK NULL
#define SHADPS4_TROPHY_DURATION_LABEL_UK NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_UK NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_UK NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_UK NULL
#define SHADPS4_LOG_INFO_0_UK NULL
#define SHADPS4_LOG_SEPARATE_LABEL_UK NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_UK NULL
#define SHADPS4_LOG_SYNC_LABEL_UK NULL
#define SHADPS4_LOG_SYNC_INFO_0_UK NULL

struct retro_core_option_v2_category option_cats_uk[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_UK,
        CATEGORY_SYSTEM_INFO_0_UK
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_UK,
        CATEGORY_VIDEO_INFO_0_UK
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_UK,
        CATEGORY_AUDIO_INFO_0_UK
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_UK,
        CATEGORY_TROPHIES_INFO_0_UK
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_UK,
        CATEGORY_LOGGING_INFO_0_UK
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_uk[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_UK,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_UK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_UK,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_UK,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_UK },
            { "18", OPTION_VAL_18_UK },
            { "21", OPTION_VAL_21_UK },
            { "23", OPTION_VAL_23_UK },
            { "14", OPTION_VAL_14_UK },
            { "6", OPTION_VAL_6_UK },
            { "12", OPTION_VAL_12_UK },
            { "22", OPTION_VAL_22_UK },
            { "2", OPTION_VAL_2_UK },
            { "4", OPTION_VAL_4_UK },
            { "25", OPTION_VAL_25_UK },
            { "24", OPTION_VAL_24_UK },
            { "29", OPTION_VAL_29_UK },
            { "5", OPTION_VAL_5_UK },
            { "0", OPTION_VAL_0_UK },
            { "9", OPTION_VAL_9_UK },
            { "15", OPTION_VAL_15_UK },
            { "16", OPTION_VAL_16_UK },
            { "17", OPTION_VAL_17_UK },
            { "7", OPTION_VAL_7_UK },
            { "26", OPTION_VAL_26_UK },
            { "8", OPTION_VAL_8_UK },
            { "11", OPTION_VAL_11_UK },
            { "20", OPTION_VAL_20_UK },
            { "3", OPTION_VAL_3_UK },
            { "13", OPTION_VAL_13_UK },
            { "27", OPTION_VAL_27_UK },
            { "10", OPTION_VAL_10_UK },
            { "19", OPTION_VAL_19_UK },
            { "30", OPTION_VAL_30_UK },
            { "28", OPTION_VAL_28_UK },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_UK,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_UK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_UK,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_UK,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_UK },
            { "512", OPTION_VAL_512_UK },
            { "1024", OPTION_VAL_1024_UK },
            { "2048", OPTION_VAL_2048_UK },
            { "3072", OPTION_VAL_3072_UK },
            { "4096", OPTION_VAL_4096_UK },
            { "6144", OPTION_VAL_6144_UK },
            { "8192", OPTION_VAL_8192_UK },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_UK,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_UK,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_UK },
            { "circle", OPTION_VAL_CIRCLE_UK },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_UK,
        NULL,
        SHADPS4_NETWORK_INFO_0_UK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_UK,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_UK,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_UK,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_UK,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_UK,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_UK,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_UK },
            { "1920x1080", OPTION_VAL_1920X1080_UK },
            { "2560x1440", OPTION_VAL_2560X1440_UK },
            { "3840x2160", OPTION_VAL_3840X2160_UK },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_UK,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_UK,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_UK },
            { "60",  OPTION_VAL_60_UK },
            { "90",  OPTION_VAL_90_UK },
            { "120", OPTION_VAL_120_UK },
            { "144", OPTION_VAL_144_UK },
            { "240", OPTION_VAL_240_UK },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_UK,
        NULL,
        SHADPS4_READBACKS_INFO_0_UK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_UK },
            { "precise",  OPTION_VAL_PRECISE_UK },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_UK,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_UK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_UK,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_UK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_UK,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_UK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_UK,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_UK,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_UK,
        NULL,
        SHADPS4_VOLUME_INFO_0_UK,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_UK },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_UK },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_UK },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_UK },
            { "40",  OPTION_VAL_40_UK },
            { "50",  OPTION_VAL_50_UK },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_UK },
            { "70",  OPTION_VAL_70_UK },
            { "80",  OPTION_VAL_80_UK },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_UK },
            { "100", OPTION_VAL_100_UK },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_UK },
            { "150", OPTION_VAL_150_UK },
            { "200", OPTION_VAL_200_UK },
            { "300", OPTION_VAL_300_UK },
            { "400", OPTION_VAL_400_UK },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_UK },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_UK,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_UK,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_UK,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_UK,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_UK },
            { "right", OPTION_VAL_RIGHT_UK },
            { "top", OPTION_VAL_TOP_UK },
            { "bottom", OPTION_VAL_BOTTOM_UK },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_UK,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_UK,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_UK },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_UK },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_UK },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_UK },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_UK },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_UK },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_UK },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_UK },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_UK },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_UK },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_UK,
        NULL,
        SHADPS4_LOG_INFO_0_UK,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_UK,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_UK,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_UK,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_UK,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_uk = {
   option_cats_uk,
   option_defs_uk
};

/* RETRO_LANGUAGE_VAL */

#define CATEGORY_SYSTEM_LABEL_VAL NULL
#define CATEGORY_SYSTEM_INFO_0_VAL NULL
#define CATEGORY_VIDEO_LABEL_VAL NULL
#define CATEGORY_VIDEO_INFO_0_VAL NULL
#define CATEGORY_AUDIO_LABEL_VAL NULL
#define CATEGORY_AUDIO_INFO_0_VAL NULL
#define CATEGORY_TROPHIES_LABEL_VAL NULL
#define CATEGORY_TROPHIES_INFO_0_VAL NULL
#define CATEGORY_LOGGING_LABEL_VAL NULL
#define CATEGORY_LOGGING_INFO_0_VAL NULL
#define SHADPS4_NEO_MODE_LABEL_VAL NULL
#define SHADPS4_NEO_MODE_INFO_0_VAL NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_VAL NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_VAL NULL
#define OPTION_VAL_1_VAL NULL
#define OPTION_VAL_18_VAL NULL
#define OPTION_VAL_21_VAL NULL
#define OPTION_VAL_23_VAL NULL
#define OPTION_VAL_14_VAL NULL
#define OPTION_VAL_6_VAL NULL
#define OPTION_VAL_12_VAL NULL
#define OPTION_VAL_22_VAL NULL
#define OPTION_VAL_2_VAL NULL
#define OPTION_VAL_4_VAL NULL
#define OPTION_VAL_25_VAL NULL
#define OPTION_VAL_24_VAL NULL
#define OPTION_VAL_29_VAL NULL
#define OPTION_VAL_5_VAL NULL
#define OPTION_VAL_0_VAL NULL
#define OPTION_VAL_9_VAL NULL
#define OPTION_VAL_15_VAL NULL
#define OPTION_VAL_16_VAL NULL
#define OPTION_VAL_17_VAL NULL
#define OPTION_VAL_7_VAL NULL
#define OPTION_VAL_26_VAL NULL
#define OPTION_VAL_8_VAL NULL
#define OPTION_VAL_11_VAL NULL
#define OPTION_VAL_20_VAL NULL
#define OPTION_VAL_3_VAL NULL
#define OPTION_VAL_13_VAL NULL
#define OPTION_VAL_27_VAL NULL
#define OPTION_VAL_10_VAL NULL
#define OPTION_VAL_19_VAL NULL
#define OPTION_VAL_30_VAL NULL
#define OPTION_VAL_28_VAL NULL
#define SHADPS4_DEV_KIT_LABEL_VAL NULL
#define SHADPS4_DEV_KIT_INFO_0_VAL NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_VAL NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_VAL NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_VAL NULL
#define OPTION_VAL_512_VAL NULL
#define OPTION_VAL_1024_VAL NULL
#define OPTION_VAL_2048_VAL NULL
#define OPTION_VAL_3072_VAL NULL
#define OPTION_VAL_4096_VAL NULL
#define OPTION_VAL_6144_VAL NULL
#define OPTION_VAL_8192_VAL NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_VAL NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_VAL NULL
#define OPTION_VAL_CROSS_VAL NULL
#define OPTION_VAL_CIRCLE_VAL NULL
#define SHADPS4_NETWORK_LABEL_VAL NULL
#define SHADPS4_NETWORK_INFO_0_VAL NULL
#define SHADPS4_CONTROLLERS_LABEL_VAL NULL
#define SHADPS4_CONTROLLERS_INFO_0_VAL NULL
#define SHADPS4_SHOW_SPLASH_LABEL_VAL NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_VAL NULL
#define SHADPS4_RESOLUTION_LABEL_VAL NULL
#define SHADPS4_RESOLUTION_INFO_0_VAL NULL
#define OPTION_VAL_1280X720_VAL NULL
#define OPTION_VAL_1920X1080_VAL NULL
#define OPTION_VAL_2560X1440_VAL NULL
#define OPTION_VAL_3840X2160_VAL NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_VAL NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_VAL NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_VAL NULL
#define OPTION_VAL_60_VAL NULL
#define OPTION_VAL_90_VAL NULL
#define OPTION_VAL_120_VAL NULL
#define OPTION_VAL_144_VAL NULL
#define OPTION_VAL_240_VAL NULL
#define SHADPS4_READBACKS_LABEL_VAL NULL
#define SHADPS4_READBACKS_INFO_0_VAL NULL
#define OPTION_VAL_RELAXED_VAL NULL
#define OPTION_VAL_PRECISE_VAL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_VAL NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_VAL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_VAL NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_VAL NULL
#define SHADPS4_SHADER_CACHE_LABEL_VAL NULL
#define SHADPS4_SHADER_CACHE_INFO_0_VAL NULL
#define SHADPS4_SHOW_FPS_LABEL_VAL NULL
#define SHADPS4_SHOW_FPS_INFO_0_VAL NULL
#define SHADPS4_VOLUME_LABEL_VAL NULL
#define SHADPS4_VOLUME_INFO_0_VAL NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_VAL NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_VAL NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_VAL NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_VAL NULL
#define OPTION_VAL_40_VAL NULL
#define OPTION_VAL_50_VAL NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_VAL NULL
#define OPTION_VAL_70_VAL NULL
#define OPTION_VAL_80_VAL NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_VAL NULL
#define OPTION_VAL_100_VAL NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_VAL NULL
#define OPTION_VAL_150_VAL NULL
#define OPTION_VAL_200_VAL NULL
#define OPTION_VAL_300_VAL NULL
#define OPTION_VAL_400_VAL NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_VAL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_VAL NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_VAL NULL
#define SHADPS4_TROPHY_POSITION_LABEL_VAL NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_VAL NULL
#define OPTION_VAL_LEFT_VAL NULL
#define OPTION_VAL_RIGHT_VAL NULL
#define OPTION_VAL_TOP_VAL NULL
#define OPTION_VAL_BOTTOM_VAL NULL
#define SHADPS4_TROPHY_DURATION_LABEL_VAL NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_VAL NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_VAL NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_VAL NULL
#define SHADPS4_LOG_INFO_0_VAL NULL
#define SHADPS4_LOG_SEPARATE_LABEL_VAL NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_VAL NULL
#define SHADPS4_LOG_SYNC_LABEL_VAL NULL
#define SHADPS4_LOG_SYNC_INFO_0_VAL NULL

struct retro_core_option_v2_category option_cats_val[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_VAL,
        CATEGORY_SYSTEM_INFO_0_VAL
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_VAL,
        CATEGORY_VIDEO_INFO_0_VAL
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_VAL,
        CATEGORY_AUDIO_INFO_0_VAL
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_VAL,
        CATEGORY_TROPHIES_INFO_0_VAL
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_VAL,
        CATEGORY_LOGGING_INFO_0_VAL
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_val[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_VAL,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_VAL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_VAL,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_VAL,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_VAL },
            { "18", OPTION_VAL_18_VAL },
            { "21", OPTION_VAL_21_VAL },
            { "23", OPTION_VAL_23_VAL },
            { "14", OPTION_VAL_14_VAL },
            { "6", OPTION_VAL_6_VAL },
            { "12", OPTION_VAL_12_VAL },
            { "22", OPTION_VAL_22_VAL },
            { "2", OPTION_VAL_2_VAL },
            { "4", OPTION_VAL_4_VAL },
            { "25", OPTION_VAL_25_VAL },
            { "24", OPTION_VAL_24_VAL },
            { "29", OPTION_VAL_29_VAL },
            { "5", OPTION_VAL_5_VAL },
            { "0", OPTION_VAL_0_VAL },
            { "9", OPTION_VAL_9_VAL },
            { "15", OPTION_VAL_15_VAL },
            { "16", OPTION_VAL_16_VAL },
            { "17", OPTION_VAL_17_VAL },
            { "7", OPTION_VAL_7_VAL },
            { "26", OPTION_VAL_26_VAL },
            { "8", OPTION_VAL_8_VAL },
            { "11", OPTION_VAL_11_VAL },
            { "20", OPTION_VAL_20_VAL },
            { "3", OPTION_VAL_3_VAL },
            { "13", OPTION_VAL_13_VAL },
            { "27", OPTION_VAL_27_VAL },
            { "10", OPTION_VAL_10_VAL },
            { "19", OPTION_VAL_19_VAL },
            { "30", OPTION_VAL_30_VAL },
            { "28", OPTION_VAL_28_VAL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_VAL,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_VAL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_VAL,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_VAL,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_VAL },
            { "512", OPTION_VAL_512_VAL },
            { "1024", OPTION_VAL_1024_VAL },
            { "2048", OPTION_VAL_2048_VAL },
            { "3072", OPTION_VAL_3072_VAL },
            { "4096", OPTION_VAL_4096_VAL },
            { "6144", OPTION_VAL_6144_VAL },
            { "8192", OPTION_VAL_8192_VAL },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_VAL,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_VAL,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_VAL },
            { "circle", OPTION_VAL_CIRCLE_VAL },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_VAL,
        NULL,
        SHADPS4_NETWORK_INFO_0_VAL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_VAL,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_VAL,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_VAL,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_VAL,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_VAL,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_VAL,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_VAL },
            { "1920x1080", OPTION_VAL_1920X1080_VAL },
            { "2560x1440", OPTION_VAL_2560X1440_VAL },
            { "3840x2160", OPTION_VAL_3840X2160_VAL },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_VAL,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_VAL,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_VAL },
            { "60",  OPTION_VAL_60_VAL },
            { "90",  OPTION_VAL_90_VAL },
            { "120", OPTION_VAL_120_VAL },
            { "144", OPTION_VAL_144_VAL },
            { "240", OPTION_VAL_240_VAL },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_VAL,
        NULL,
        SHADPS4_READBACKS_INFO_0_VAL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_VAL },
            { "precise",  OPTION_VAL_PRECISE_VAL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_VAL,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_VAL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_VAL,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_VAL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_VAL,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_VAL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_VAL,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_VAL,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_VAL,
        NULL,
        SHADPS4_VOLUME_INFO_0_VAL,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_VAL },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_VAL },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_VAL },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_VAL },
            { "40",  OPTION_VAL_40_VAL },
            { "50",  OPTION_VAL_50_VAL },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_VAL },
            { "70",  OPTION_VAL_70_VAL },
            { "80",  OPTION_VAL_80_VAL },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_VAL },
            { "100", OPTION_VAL_100_VAL },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_VAL },
            { "150", OPTION_VAL_150_VAL },
            { "200", OPTION_VAL_200_VAL },
            { "300", OPTION_VAL_300_VAL },
            { "400", OPTION_VAL_400_VAL },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_VAL },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_VAL,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_VAL,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_VAL,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_VAL,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_VAL },
            { "right", OPTION_VAL_RIGHT_VAL },
            { "top", OPTION_VAL_TOP_VAL },
            { "bottom", OPTION_VAL_BOTTOM_VAL },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_VAL,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_VAL,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_VAL },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_VAL },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_VAL },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_VAL },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_VAL },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_VAL },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_VAL },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_VAL },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_VAL },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_VAL },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_VAL,
        NULL,
        SHADPS4_LOG_INFO_0_VAL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_VAL,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_VAL,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_VAL,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_VAL,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_val = {
   option_cats_val,
   option_defs_val
};

/* RETRO_LANGUAGE_VN */

#define CATEGORY_SYSTEM_LABEL_VN NULL
#define CATEGORY_SYSTEM_INFO_0_VN NULL
#define CATEGORY_VIDEO_LABEL_VN NULL
#define CATEGORY_VIDEO_INFO_0_VN NULL
#define CATEGORY_AUDIO_LABEL_VN NULL
#define CATEGORY_AUDIO_INFO_0_VN NULL
#define CATEGORY_TROPHIES_LABEL_VN NULL
#define CATEGORY_TROPHIES_INFO_0_VN NULL
#define CATEGORY_LOGGING_LABEL_VN NULL
#define CATEGORY_LOGGING_INFO_0_VN NULL
#define SHADPS4_NEO_MODE_LABEL_VN NULL
#define SHADPS4_NEO_MODE_INFO_0_VN NULL
#define SHADPS4_CONSOLE_LANGUAGE_LABEL_VN NULL
#define SHADPS4_CONSOLE_LANGUAGE_INFO_0_VN NULL
#define OPTION_VAL_1_VN NULL
#define OPTION_VAL_18_VN NULL
#define OPTION_VAL_21_VN NULL
#define OPTION_VAL_23_VN NULL
#define OPTION_VAL_14_VN NULL
#define OPTION_VAL_6_VN NULL
#define OPTION_VAL_12_VN NULL
#define OPTION_VAL_22_VN NULL
#define OPTION_VAL_2_VN NULL
#define OPTION_VAL_4_VN NULL
#define OPTION_VAL_25_VN NULL
#define OPTION_VAL_24_VN NULL
#define OPTION_VAL_29_VN NULL
#define OPTION_VAL_5_VN NULL
#define OPTION_VAL_0_VN NULL
#define OPTION_VAL_9_VN NULL
#define OPTION_VAL_15_VN NULL
#define OPTION_VAL_16_VN NULL
#define OPTION_VAL_17_VN NULL
#define OPTION_VAL_7_VN NULL
#define OPTION_VAL_26_VN NULL
#define OPTION_VAL_8_VN NULL
#define OPTION_VAL_11_VN NULL
#define OPTION_VAL_20_VN NULL
#define OPTION_VAL_3_VN NULL
#define OPTION_VAL_13_VN NULL
#define OPTION_VAL_27_VN NULL
#define OPTION_VAL_10_VN NULL
#define OPTION_VAL_19_VN NULL
#define OPTION_VAL_30_VN NULL
#define OPTION_VAL_28_VN NULL
#define SHADPS4_DEV_KIT_LABEL_VN NULL
#define SHADPS4_DEV_KIT_INFO_0_VN NULL
#define SHADPS4_EXTRA_MEMORY_LABEL_VN NULL
#define SHADPS4_EXTRA_MEMORY_INFO_0_VN NULL
#define OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_VN NULL
#define OPTION_VAL_512_VN NULL
#define OPTION_VAL_1024_VN NULL
#define OPTION_VAL_2048_VN NULL
#define OPTION_VAL_3072_VN NULL
#define OPTION_VAL_4096_VN NULL
#define OPTION_VAL_6144_VN NULL
#define OPTION_VAL_8192_VN NULL
#define SHADPS4_CONFIRM_BUTTON_LABEL_VN NULL
#define SHADPS4_CONFIRM_BUTTON_INFO_0_VN NULL
#define OPTION_VAL_CROSS_VN NULL
#define OPTION_VAL_CIRCLE_VN NULL
#define SHADPS4_NETWORK_LABEL_VN NULL
#define SHADPS4_NETWORK_INFO_0_VN NULL
#define SHADPS4_CONTROLLERS_LABEL_VN NULL
#define SHADPS4_CONTROLLERS_INFO_0_VN NULL
#define SHADPS4_SHOW_SPLASH_LABEL_VN NULL
#define SHADPS4_SHOW_SPLASH_INFO_0_VN NULL
#define SHADPS4_RESOLUTION_LABEL_VN NULL
#define SHADPS4_RESOLUTION_INFO_0_VN NULL
#define OPTION_VAL_1280X720_VN NULL
#define OPTION_VAL_1920X1080_VN NULL
#define OPTION_VAL_2560X1440_VN NULL
#define OPTION_VAL_3840X2160_VN NULL
#define SHADPS4_VBLANK_FREQUENCY_LABEL_VN NULL
#define SHADPS4_VBLANK_FREQUENCY_INFO_0_VN NULL
#define OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_VN NULL
#define OPTION_VAL_60_VN NULL
#define OPTION_VAL_90_VN NULL
#define OPTION_VAL_120_VN NULL
#define OPTION_VAL_144_VN NULL
#define OPTION_VAL_240_VN NULL
#define SHADPS4_READBACKS_LABEL_VN NULL
#define SHADPS4_READBACKS_INFO_0_VN NULL
#define OPTION_VAL_RELAXED_VN NULL
#define OPTION_VAL_PRECISE_VN NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_VN NULL
#define SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_VN NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_LABEL_VN NULL
#define SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_VN NULL
#define SHADPS4_SHADER_CACHE_LABEL_VN NULL
#define SHADPS4_SHADER_CACHE_INFO_0_VN NULL
#define SHADPS4_SHOW_FPS_LABEL_VN NULL
#define SHADPS4_SHOW_FPS_INFO_0_VN NULL
#define SHADPS4_VOLUME_LABEL_VN NULL
#define SHADPS4_VOLUME_INFO_0_VN NULL
#define OPTION_VAL_0_SHADPS4_VOLUME_VN NULL
#define OPTION_VAL_10_SHADPS4_VOLUME_VN NULL
#define OPTION_VAL_20_SHADPS4_VOLUME_VN NULL
#define OPTION_VAL_30_SHADPS4_VOLUME_VN NULL
#define OPTION_VAL_40_VN NULL
#define OPTION_VAL_50_VN NULL
#define OPTION_VAL_60_SHADPS4_VOLUME_VN NULL
#define OPTION_VAL_70_VN NULL
#define OPTION_VAL_80_VN NULL
#define OPTION_VAL_90_SHADPS4_VOLUME_VN NULL
#define OPTION_VAL_100_VN NULL
#define OPTION_VAL_125_SHADPS4_VOLUME_VN NULL
#define OPTION_VAL_150_VN NULL
#define OPTION_VAL_200_VN NULL
#define OPTION_VAL_300_VN NULL
#define OPTION_VAL_400_VN NULL
#define OPTION_VAL_500_SHADPS4_VOLUME_VN NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_LABEL_VN NULL
#define SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_VN NULL
#define SHADPS4_TROPHY_POSITION_LABEL_VN NULL
#define SHADPS4_TROPHY_POSITION_INFO_0_VN NULL
#define OPTION_VAL_LEFT_VN NULL
#define OPTION_VAL_RIGHT_VN NULL
#define OPTION_VAL_TOP_VN NULL
#define OPTION_VAL_BOTTOM_VN NULL
#define SHADPS4_TROPHY_DURATION_LABEL_VN NULL
#define SHADPS4_TROPHY_DURATION_INFO_0_VN NULL
#define OPTION_VAL_1_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_2_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_3_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_4_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_5_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_6_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_7_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_8_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_9_SHADPS4_TROPHY_DURATION_VN NULL
#define OPTION_VAL_10_SHADPS4_TROPHY_DURATION_VN NULL
#define SHADPS4_LOG_INFO_0_VN NULL
#define SHADPS4_LOG_SEPARATE_LABEL_VN NULL
#define SHADPS4_LOG_SEPARATE_INFO_0_VN NULL
#define SHADPS4_LOG_SYNC_LABEL_VN NULL
#define SHADPS4_LOG_SYNC_INFO_0_VN NULL

struct retro_core_option_v2_category option_cats_vn[] = {
    {
        "system",
        CATEGORY_SYSTEM_LABEL_VN,
        CATEGORY_SYSTEM_INFO_0_VN
    },
    {
        "video",
        CATEGORY_VIDEO_LABEL_VN,
        CATEGORY_VIDEO_INFO_0_VN
    },
    {
        "audio",
        CATEGORY_AUDIO_LABEL_VN,
        CATEGORY_AUDIO_INFO_0_VN
    },
    {
        "trophies",
        CATEGORY_TROPHIES_LABEL_VN,
        CATEGORY_TROPHIES_INFO_0_VN
    },
    {
        "logging",
        CATEGORY_LOGGING_LABEL_VN,
        CATEGORY_LOGGING_INFO_0_VN
    },
    { NULL, NULL, NULL },
};
struct retro_core_option_v2_definition option_defs_vn[] = {
    {
        "shadps4_neo_mode",
        SHADPS4_NEO_MODE_LABEL_VN,
        NULL,
        SHADPS4_NEO_MODE_INFO_0_VN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        SHADPS4_CONSOLE_LANGUAGE_LABEL_VN,
        NULL,
        SHADPS4_CONSOLE_LANGUAGE_INFO_0_VN,
        NULL,
        "system",
        {
            { "1", OPTION_VAL_1_VN },
            { "18", OPTION_VAL_18_VN },
            { "21", OPTION_VAL_21_VN },
            { "23", OPTION_VAL_23_VN },
            { "14", OPTION_VAL_14_VN },
            { "6", OPTION_VAL_6_VN },
            { "12", OPTION_VAL_12_VN },
            { "22", OPTION_VAL_22_VN },
            { "2", OPTION_VAL_2_VN },
            { "4", OPTION_VAL_4_VN },
            { "25", OPTION_VAL_25_VN },
            { "24", OPTION_VAL_24_VN },
            { "29", OPTION_VAL_29_VN },
            { "5", OPTION_VAL_5_VN },
            { "0", OPTION_VAL_0_VN },
            { "9", OPTION_VAL_9_VN },
            { "15", OPTION_VAL_15_VN },
            { "16", OPTION_VAL_16_VN },
            { "17", OPTION_VAL_17_VN },
            { "7", OPTION_VAL_7_VN },
            { "26", OPTION_VAL_26_VN },
            { "8", OPTION_VAL_8_VN },
            { "11", OPTION_VAL_11_VN },
            { "20", OPTION_VAL_20_VN },
            { "3", OPTION_VAL_3_VN },
            { "13", OPTION_VAL_13_VN },
            { "27", OPTION_VAL_27_VN },
            { "10", OPTION_VAL_10_VN },
            { "19", OPTION_VAL_19_VN },
            { "30", OPTION_VAL_30_VN },
            { "28", OPTION_VAL_28_VN },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        SHADPS4_DEV_KIT_LABEL_VN,
        NULL,
        SHADPS4_DEV_KIT_INFO_0_VN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        SHADPS4_EXTRA_MEMORY_LABEL_VN,
        NULL,
        SHADPS4_EXTRA_MEMORY_INFO_0_VN,
        NULL,
        "system",
        {
            { "0", OPTION_VAL_0_SHADPS4_EXTRA_MEMORY_VN },
            { "512", OPTION_VAL_512_VN },
            { "1024", OPTION_VAL_1024_VN },
            { "2048", OPTION_VAL_2048_VN },
            { "3072", OPTION_VAL_3072_VN },
            { "4096", OPTION_VAL_4096_VN },
            { "6144", OPTION_VAL_6144_VN },
            { "8192", OPTION_VAL_8192_VN },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        SHADPS4_CONFIRM_BUTTON_LABEL_VN,
        NULL,
        SHADPS4_CONFIRM_BUTTON_INFO_0_VN,
        NULL,
        "system",
        {
            { "cross",  OPTION_VAL_CROSS_VN },
            { "circle", OPTION_VAL_CIRCLE_VN },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        SHADPS4_NETWORK_LABEL_VN,
        NULL,
        SHADPS4_NETWORK_INFO_0_VN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_controllers",
        SHADPS4_CONTROLLERS_LABEL_VN,
        NULL,
        SHADPS4_CONTROLLERS_INFO_0_VN,
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        SHADPS4_SHOW_SPLASH_LABEL_VN,
        NULL,
        SHADPS4_SHOW_SPLASH_INFO_0_VN,
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_resolution",
        SHADPS4_RESOLUTION_LABEL_VN,
        NULL,
        SHADPS4_RESOLUTION_INFO_0_VN,
        NULL,
        "video",
        {
            { "1280x720",  OPTION_VAL_1280X720_VN },
            { "1920x1080", OPTION_VAL_1920X1080_VN },
            { "2560x1440", OPTION_VAL_2560X1440_VN },
            { "3840x2160", OPTION_VAL_3840X2160_VN },
            { NULL, NULL },
        },
        "1280x720"
    },
    {
        "shadps4_vblank_frequency",
        SHADPS4_VBLANK_FREQUENCY_LABEL_VN,
        NULL,
        SHADPS4_VBLANK_FREQUENCY_INFO_0_VN,
        NULL,
        "video",
        {
            { "30",  OPTION_VAL_30_SHADPS4_VBLANK_FREQUENCY_VN },
            { "60",  OPTION_VAL_60_VN },
            { "90",  OPTION_VAL_90_VN },
            { "120", OPTION_VAL_120_VN },
            { "144", OPTION_VAL_144_VN },
            { "240", OPTION_VAL_240_VN },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        SHADPS4_READBACKS_LABEL_VN,
        NULL,
        SHADPS4_READBACKS_INFO_0_VN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  OPTION_VAL_RELAXED_VN },
            { "precise",  OPTION_VAL_PRECISE_VN },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        SHADPS4_DIRECT_MEMORY_ACCESS_LABEL_VN,
        NULL,
        SHADPS4_DIRECT_MEMORY_ACCESS_INFO_0_VN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        SHADPS4_READBACK_LINEAR_IMAGES_LABEL_VN,
        NULL,
        SHADPS4_READBACK_LINEAR_IMAGES_INFO_0_VN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_shader_cache",
        SHADPS4_SHADER_CACHE_LABEL_VN,
        NULL,
        SHADPS4_SHADER_CACHE_INFO_0_VN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        SHADPS4_SHOW_FPS_LABEL_VN,
        NULL,
        SHADPS4_SHOW_FPS_INFO_0_VN,
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        SHADPS4_VOLUME_LABEL_VN,
        NULL,
        SHADPS4_VOLUME_INFO_0_VN,
        NULL,
        "audio",
        {
            { "0",   OPTION_VAL_0_SHADPS4_VOLUME_VN },
            { "10",  OPTION_VAL_10_SHADPS4_VOLUME_VN },
            { "20",  OPTION_VAL_20_SHADPS4_VOLUME_VN },
            { "30",  OPTION_VAL_30_SHADPS4_VOLUME_VN },
            { "40",  OPTION_VAL_40_VN },
            { "50",  OPTION_VAL_50_VN },
            { "60",  OPTION_VAL_60_SHADPS4_VOLUME_VN },
            { "70",  OPTION_VAL_70_VN },
            { "80",  OPTION_VAL_80_VN },
            { "90",  OPTION_VAL_90_SHADPS4_VOLUME_VN },
            { "100", OPTION_VAL_100_VN },
            { "125", OPTION_VAL_125_SHADPS4_VOLUME_VN },
            { "150", OPTION_VAL_150_VN },
            { "200", OPTION_VAL_200_VN },
            { "300", OPTION_VAL_300_VN },
            { "400", OPTION_VAL_400_VN },
            { "500", OPTION_VAL_500_SHADPS4_VOLUME_VN },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        SHADPS4_TROPHY_NOTIFICATIONS_LABEL_VN,
        NULL,
        SHADPS4_TROPHY_NOTIFICATIONS_INFO_0_VN,
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        SHADPS4_TROPHY_POSITION_LABEL_VN,
        NULL,
        SHADPS4_TROPHY_POSITION_INFO_0_VN,
        NULL,
        "trophies",
        {
            { "left", OPTION_VAL_LEFT_VN },
            { "right", OPTION_VAL_RIGHT_VN },
            { "top", OPTION_VAL_TOP_VN },
            { "bottom", OPTION_VAL_BOTTOM_VN },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        SHADPS4_TROPHY_DURATION_LABEL_VN,
        NULL,
        SHADPS4_TROPHY_DURATION_INFO_0_VN,
        NULL,
        "trophies",
        {
            { "1", OPTION_VAL_1_SHADPS4_TROPHY_DURATION_VN },
            { "2", OPTION_VAL_2_SHADPS4_TROPHY_DURATION_VN },
            { "3", OPTION_VAL_3_SHADPS4_TROPHY_DURATION_VN },
            { "4", OPTION_VAL_4_SHADPS4_TROPHY_DURATION_VN },
            { "5", OPTION_VAL_5_SHADPS4_TROPHY_DURATION_VN },
            { "6", OPTION_VAL_6_SHADPS4_TROPHY_DURATION_VN },
            { "7", OPTION_VAL_7_SHADPS4_TROPHY_DURATION_VN },
            { "8", OPTION_VAL_8_SHADPS4_TROPHY_DURATION_VN },
            { "9", OPTION_VAL_9_SHADPS4_TROPHY_DURATION_VN },
            { "10", OPTION_VAL_10_SHADPS4_TROPHY_DURATION_VN },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        CATEGORY_LOGGING_LABEL_VN,
        NULL,
        SHADPS4_LOG_INFO_0_VN,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        SHADPS4_LOG_SEPARATE_LABEL_VN,
        NULL,
        SHADPS4_LOG_SEPARATE_INFO_0_VN,
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        SHADPS4_LOG_SYNC_LABEL_VN,
        NULL,
        SHADPS4_LOG_SYNC_INFO_0_VN,
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};
struct retro_core_options_v2 options_vn = {
   option_cats_vn,
   option_defs_vn
};


#ifdef __cplusplus
}
#endif

#endif