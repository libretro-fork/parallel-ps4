/* SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_CORE_EMULATOR_SETTINGS_H
#define SHADPS4_CORE_EMULATOR_SETTINGS_H

/* The emulator's settings, kept in config.json in the user directory, with
 * per-game overrides in custom_configs/<serial>.json.
 *
 * Every setting has a factory default, a base value (config.json) and,
 * when it can be overridden per game, an optional game-specific value.
 * Each one listed in EMU_SETTINGS_LIST gets
 *
 *   <type> emu_settings_get_<name>(void);
 *   void   emu_settings_set_<name>(<type> value, bool game_specific);
 *
 * unless it is marked CUSTOM, in which case its accessors are declared
 * below by hand. Strings are returned as pointers into the settings; they
 * stay valid until that setting is changed. */

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define EMU_SETTINGS_STRING_MAX 256
#define EMU_SETTINGS_PATH_MAX 4096
#define EMU_SETTINGS_INSTALL_DIRS_MAX 16

enum emu_usb_backend {
    EMU_USB_BACKEND_REAL = 0,
    EMU_USB_BACKEND_SKYLANDERS_PORTAL,
    EMU_USB_BACKEND_INFINITY_BASE,
    EMU_USB_BACKEND_DIMENSIONS_TOYPAD
};

enum emu_readbacks_mode {
    EMU_READBACKS_DISABLED = 0,
    EMU_READBACKS_RELAXED,
    EMU_READBACKS_PRECISE
};

/* Which value a getter returns. */
enum emu_config_mode {
    /* the game-specific value if there is one, else the base value */
    EMU_CONFIG_MODE_DEFAULT = 0,
    /* the base value */
    EMU_CONFIG_MODE_GLOBAL,
    /* the factory default */
    EMU_CONFIG_MODE_CLEAN
};

enum emu_settings_section {
    EMU_SECTION_GENERAL = 0,
    EMU_SECTION_NETWORK,
    EMU_SECTION_LOG,
    EMU_SECTION_DEBUG,
    EMU_SECTION_INPUT,
    EMU_SECTION_GPU,
    EMU_SECTION_VULKAN,
    EMU_SECTION_COUNT
};

/* Flags: OVR can be overridden per game; 0 is global only. */
#define EMU_SETTINGS_FLAG_0 0
#define EMU_SETTINGS_FLAG_OVR 1

/* C types of the setting types. */
#define EMU_SETTINGS_CTYPE_BOOL bool
#define EMU_SETTINGS_CTYPE_INT int
#define EMU_SETTINGS_CTYPE_UINT unsigned
#define EMU_SETTINGS_CTYPE_U64 uint64_t
#define EMU_SETTINGS_CTYPE_DOUBLE double
#define EMU_SETTINGS_CTYPE_STRING const char*
#define EMU_SETTINGS_CTYPE_PATH const char*

#ifdef _WIN32
#define EMU_SETTINGS_LIST_WIN32_LOG EMU_SETTING(log_type, "type", LOG, STRING, "wincolor", OVR, GEN)
#else
#define EMU_SETTINGS_LIST_WIN32_LOG
#endif

/* EMU_SETTING(name, json key, section, type, default, flags, accessors) */
#define EMU_SETTINGS_LIST                                                                          \
    EMU_SETTING(addon_install_dir, "addon_install_dir", GENERAL, PATH, "", 0, CUSTOM)              \
    EMU_SETTING(home_dir, "home_dir", GENERAL, PATH, "", 0, CUSTOM)                                \
    EMU_SETTING(sys_modules_dir, "sys_modules_dir", GENERAL, PATH, "", 0, CUSTOM)                  \
    EMU_SETTING(font_dir, "font_dir", GENERAL, PATH, "", 0, CUSTOM)                                \
    EMU_SETTING(volume_slider, "volume_slider", GENERAL, INT, 100, OVR, GEN)                       \
    EMU_SETTING(neo_mode, "neo_mode", GENERAL, BOOL, false, OVR, GEN)                              \
    EMU_SETTING(dev_kit_mode, "dev_kit_mode", GENERAL, BOOL, false, OVR, GEN)                      \
    EMU_SETTING(extra_dmem_in_mbytes, "extra_dmem_in_mbytes", GENERAL, INT, 0, OVR, GEN)           \
    EMU_SETTING(extra_fmem_in_mbytes, "extra_fmem_in_mbytes", GENERAL, INT, 0, OVR, GEN)           \
    EMU_SETTING(trophy_popup_disabled, "trophy_popup_disabled", GENERAL, BOOL, false, OVR, GEN)    \
    EMU_SETTING(trophy_notification_duration, "trophy_notification_duration", GENERAL, DOUBLE,     \
                6.0, OVR, GEN)                                                                     \
    EMU_SETTING(trophy_notification_side, "trophy_notification_side", GENERAL, STRING, "right",    \
                OVR, GEN)                                                                          \
    EMU_SETTING(show_splash, "show_splash", GENERAL, BOOL, false, OVR, GEN)                        \
    EMU_SETTING(show_fps_counter, "show_fps_counter", GENERAL, BOOL, false, 0, GEN)                \
    EMU_SETTING(console_language, "console_language", GENERAL, INT, 1, OVR, GEN)                   \
    EMU_SETTING(big_picture_scale, "big_picture_scale", GENERAL, INT, 1000, 0, GEN)                \
    EMU_SETTING(big_picture_folder_depth, "big_picture_folder_depth", GENERAL, INT, 2, 0, GEN)     \
    EMU_SETTING(redzone_patches, "redzone_patches", GENERAL, BOOL, false, OVR, GEN)                \
                                                                                                   \
    EMU_SETTING(connected_to_network, "connected_to_network", NETWORK, BOOL, false, OVR, GEN)      \
    EMU_SETTING(shad_net_enabled, "shad_net_enabled", NETWORK, BOOL, false, OVR, GEN)              \
    EMU_SETTING(shadnet_server, "shadnet_server", NETWORK, STRING, "srv.shadps4.net:31313", OVR,   \
                GEN)                                                                               \
    EMU_SETTING(shadnet_webapi_server, "shadnet_webapi_server", NETWORK, STRING,                   \
                "http://srv.shadps4.net:31315", OVR, GEN)                                          \
    EMU_SETTING(enable_upnp, "enable_upnp", NETWORK, BOOL, true, OVR, GEN)                         \
    /* 0 is any free port, 3658 the PS4's; with UPnP it is forwarded on the router */              \
    EMU_SETTING(p2p_port, "p2p_port", NETWORK, INT, 3658, OVR, GEN)                                \
    EMU_SETTING(disable_https, "disable_https", NETWORK, BOOL, false, OVR, GEN)                    \
                                                                                                   \
    EMU_SETTING(log_append, "append", LOG, BOOL, false, OVR, GEN)                                  \
    EMU_SETTING(log_enable, "enable", LOG, BOOL, true, OVR, GEN)                                   \
    EMU_SETTING(log_filter, "filter", LOG, STRING, "", OVR, GEN)                                   \
    EMU_SETTING(log_flush_level, "flush_level", LOG, STRING, "", OVR, GEN)                         \
    EMU_SETTING(log_max_skip_duration, "max_skip_duration", LOG, UINT, 5000, OVR, GEN)             \
    EMU_SETTING(log_separate, "separate", LOG, BOOL, false, OVR, GEN)                              \
    EMU_SETTING(log_size_limit, "size_limit", LOG, U64, 104857600, OVR, GEN)                       \
    EMU_SETTING(log_skip_duplicate, "skip_duplicate", LOG, BOOL, true, OVR, GEN)                   \
    EMU_SETTING(log_sync, "sync", LOG, BOOL, true, OVR, GEN)                                       \
    EMU_SETTINGS_LIST_WIN32_LOG                                                                    \
                                                                                                   \
    EMU_SETTING(debug_dump, "debug_dump", DEBUG, BOOL, false, OVR, GEN)                            \
    EMU_SETTING(shader_collect, "shader_collect", DEBUG, BOOL, false, OVR, GEN)                    \
    EMU_SETTING(config_version, "config_version", DEBUG, STRING, "", 0, GEN)                       \
                                                                                                   \
    EMU_SETTING(usb_device_backend, "usb_device_backend", INPUT, INT, EMU_USB_BACKEND_REAL, OVR,   \
                GEN)                                                                               \
    EMU_SETTING(use_special_pad, "use_special_pad", INPUT, BOOL, false, 0, GEN)                    \
    EMU_SETTING(special_pad_class, "special_pad_class", INPUT, INT, 1, 0, GEN)                     \
    EMU_SETTING(ime_accessibility_enabled, "ime_accessibility_enabled", INPUT, BOOL, false, OVR,   \
                GEN)                                                                               \
    EMU_SETTING(ime_url_mail_short_panel, "ime_url_mail_short_panel", INPUT, BOOL, false, OVR,     \
                GEN)                                                                               \
    EMU_SETTING(is_circle_enter, "is_circle_enter", INPUT, BOOL, false, OVR, GEN)                  \
                                                                                                   \
    EMU_SETTING(window_width, "window_width", GPU, UINT, 1280, OVR, GEN)                           \
    EMU_SETTING(window_height, "window_height", GPU, UINT, 720, OVR, GEN)                          \
    EMU_SETTING(internal_screen_width, "internal_screen_width", GPU, UINT, 1280, 0, GEN)           \
    EMU_SETTING(internal_screen_height, "internal_screen_height", GPU, UINT, 720, 0, GEN)          \
    EMU_SETTING(null_gpu, "null_gpu", GPU, BOOL, false, OVR, GEN)                                  \
    EMU_SETTING(copy_gpu_buffers, "copy_gpu_buffers", GPU, BOOL, false, OVR, GEN)                  \
    EMU_SETTING(readbacks_mode, "readbacks_mode", GPU, UINT, EMU_READBACKS_DISABLED, OVR, GEN)     \
    EMU_SETTING(readback_linear_images_enabled, "readback_linear_images_enabled", GPU, BOOL,       \
                false, OVR, GEN)                                                                   \
    EMU_SETTING(direct_memory_access_enabled, "direct_memory_access_enabled", GPU, BOOL, false,    \
                OVR, GEN)                                                                          \
    EMU_SETTING(dump_shaders, "dump_shaders", GPU, BOOL, false, OVR, GEN)                          \
    EMU_SETTING(patch_shaders, "patch_shaders", GPU, BOOL, false, OVR, GEN)                        \
    EMU_SETTING(vblank_frequency, "vblank_frequency", GPU, UINT, 60, OVR, CUSTOM)                  \
    EMU_SETTING(full_screen, "full_screen", GPU, BOOL, false, OVR, GEN)                            \
    EMU_SETTING(full_screen_mode, "full_screen_mode", GPU, STRING, "Windowed", OVR, GEN)           \
    EMU_SETTING(present_mode, "present_mode", GPU, STRING, "Mailbox", OVR, GEN)                    \
    EMU_SETTING(hdr_allowed, "hdr_allowed", GPU, BOOL, false, OVR, GEN)                            \
    EMU_SETTING(userfaultfd, "userfaultfd", GPU, BOOL, false, OVR, GEN)                            \
    EMU_SETTING(inline_fetch_shader, "inline_fetch_shader", GPU, BOOL, false, OVR, GEN)            \
    EMU_SETTING(fast_shader_math, "fast_shader_math", GPU, BOOL, false, OVR, GEN)                  \
                                                                                                   \
    EMU_SETTING(gpu_id, "gpu_id", VULKAN, INT, -1, OVR, GEN)                                       \
    EMU_SETTING(vkvalidation_enabled, "vkvalidation_enabled", VULKAN, BOOL, false, OVR, GEN)       \
    EMU_SETTING(vkvalidation_core_enabled, "vkvalidation_core_enabled", VULKAN, BOOL, true, OVR,   \
                GEN)                                                                               \
    EMU_SETTING(vkvalidation_sync_enabled, "vkvalidation_sync_enabled", VULKAN, BOOL, false, OVR,  \
                GEN)                                                                               \
    EMU_SETTING(vkvalidation_gpu_enabled, "vkvalidation_gpu_enabled", VULKAN, BOOL, false, OVR,    \
                GEN)                                                                               \
    EMU_SETTING(vkcrash_diagnostic_enabled, "vkcrash_diagnostic_enabled", VULKAN, BOOL, false,     \
                OVR, GEN)                                                                          \
    EMU_SETTING(vkhost_markers, "vkhost_markers", VULKAN, BOOL, false, OVR, GEN)                   \
    EMU_SETTING(vkguest_markers, "vkguest_markers", VULKAN, BOOL, false, OVR, GEN)                 \
    EMU_SETTING(pipeline_cache_enabled, "pipeline_cache_enabled", VULKAN, BOOL, false, OVR, GEN)

enum emu_setting_id {
#define EMU_SETTING(name, key, section, type, def, flags, acc) EMU_SETTING_ID_##name,
    EMU_SETTINGS_LIST
#undef EMU_SETTING
        EMU_SETTING_ID_COUNT
};

#define EMU_SETTINGS_DECL_GEN(name, type)                                                          \
    EMU_SETTINGS_CTYPE_##type emu_settings_get_##name(void);                                       \
    void emu_settings_set_##name(EMU_SETTINGS_CTYPE_##type value, bool game_specific);
#define EMU_SETTINGS_DECL_CUSTOM(name, type)
#define EMU_SETTING(name, key, section, type, def, flags, acc) EMU_SETTINGS_DECL_##acc(name, type)
EMU_SETTINGS_LIST
#undef EMU_SETTING

/* The directory settings. Each getter writes the configured directory, or
 * the user directory it defaults to when none is set, into s and returns
 * its length (len or more means it didn't fit). */
size_t emu_settings_get_addon_install_dir(char* s, size_t len);
size_t emu_settings_get_home_dir(char* s, size_t len);
size_t emu_settings_get_sys_modules_dir(char* s, size_t len);
size_t emu_settings_get_font_dir(char* s, size_t len);
void emu_settings_set_addon_install_dir(const char* dir);
void emu_settings_set_home_dir(const char* dir);
void emu_settings_set_sys_modules_dir(const char* dir);
void emu_settings_set_font_dir(const char* dir);

/* Never below 30. */
unsigned emu_settings_get_vblank_frequency(void);
void emu_settings_set_vblank_frequency(unsigned value, bool game_specific);

/* shadNet is on when its setting is, unless it was turned off for the
 * rest of this run. */
bool emu_settings_is_shadnet_enabled(void);
void emu_settings_set_shadnet_session_disabled(bool disabled);
bool emu_settings_is_shadnet_session_disabled(void);

/* Game install directories. */
struct emu_settings_install_dir {
    char path[EMU_SETTINGS_PATH_MAX];
    bool enabled;
};
size_t emu_settings_install_dir_count(void);
const struct emu_settings_install_dir* emu_settings_install_dir_at(size_t index);
bool emu_settings_add_install_dir(const char* path, bool enabled);
void emu_settings_remove_install_dir(const char* path);
void emu_settings_set_install_dir_enabled(const char* path, bool enabled);
void emu_settings_clear_install_dirs(void);

enum emu_config_mode emu_settings_get_config_mode(void);
void emu_settings_set_config_mode(enum emu_config_mode mode);

/* Resets every setting to its factory default. */
void emu_settings_set_defaults(void);
/* Drops every game-specific value, back to the base values. */
void emu_settings_clear_game_overrides(void);

/* Reads config.json; a missing one is created with the defaults, and one
 * written by another build is saved again. Drops game-specific values. */
bool emu_settings_load(void);
/* Applies custom_configs/<serial>.json on top of the base values. Returns
 * false when the game has no such file. */
bool emu_settings_load_game(const char* serial);
/* Writes config.json, keeping the members this build doesn't know. */
bool emu_settings_save(void);
/* Writes every overridable setting to custom_configs/<serial>.json. */
bool emu_settings_save_game(const char* serial);

RETRO_END_DECLS

#endif
