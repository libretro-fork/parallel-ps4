/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>

#include "common/c_bridge.h"
#include "core/emulator_settings.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/sysmodule/sysmodule_error.h"
#include "core/libraries/sysmodule/sysmodule_internal.h"
#include "core/libraries/sysmodule/sysmodule_table.h"
#include "core/linker_c.h"

bool sysmodule_need_scelibc = false, sysmodule_need_fios2 = false;

/* A module's name and what is added to it, as long as the C++ std::string could be */
#define SYSMODULE_NAME_SIZE 128

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

s32 sysmodule_get_module_handle(s32 id, s32* handle) {
    u64 i;
    if (id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }
    for (i = 0; i < SYSMODULE_NUM_MODULES; i++) {
        const OrbisSysmoduleModuleInternal* mod = &g_modules_array[i];
        if (mod->id != (u32)id) {
            continue;
        }
        if (mod->is_loaded < 1) {
            return ORBIS_SYSMODULE_NOT_LOADED;
        }
        if (handle != NULL) {
            *handle = mod->handle;
        }
        return ORBIS_OK;
    }
    return ORBIS_SYSMODULE_INVALID_ID;
}

bool sysmodule_should_hide_name(const char* module_name) {
    u64 i;
    for (i = 0; i < SYSMODULE_NUM_MODULES; i++) {
        const OrbisSysmoduleModuleInternal* mod = &g_modules_array[i];
        u64 name_length;
        char name_copy[0x100];
        s32 result;
        if ((mod->flags & SYSMODULE_IS_GAME) == 0) {
            continue;
        }
        name_length = strlen(mod->name);
        strncpy(name_copy, mod->name, sizeof(name_copy));
        /* Module table stores names without extensions, so check with .prx appended to the
         * name. */
        strncpy(&name_copy[name_length], ".prx", 4);
        result = strncmp(module_name, name_copy, sizeof(name_copy));
        if (result == 0) {
            return true;
        }

        /* libSceFios2 and libc are checked as both sprx or prx modules. */
        if (i == 3) {
            result = strncmp(module_name, "libSceFios2.sprx", sizeof(name_copy));
        } else if (i == 4) {
            result = strncmp(module_name, "libc.sprx", sizeof(name_copy));
        }

        if (result == 0) {
            return true;
        }
    }
    return false;
}

bool sysmodule_is_debug_module(s32 id) {
    u64 i;
    for (i = 0; i < SYSMODULE_NUM_MODULES; i++) {
        const OrbisSysmoduleModuleInternal* mod = &g_modules_array[i];
        if (mod->id == (u32)id && (mod->flags & SYSMODULE_IS_DEBUG) != 0) {
            return true;
        }
    }
    return false;
}

u32 sysmodule_validate_module_id(s32 id) {
    s32 sdk_ver = 0;
    if ((id & 0x7fffffff) == 0) {
        return (u32)ORBIS_SYSMODULE_INVALID_ID;
    }

    C_ASSERT_MSG(!kernel_get_compiled_sdk_version(&sdk_ver), "validateModuleId",
                 "Failed to retrieve compiled SDK version");

    /* libSceGameCustomDialog isn't loadable on SDK >= 7.50 */
    if (id == 0xb8 && sdk_ver >= KERNEL_FW_750) {
        return (u32)ORBIS_SYSMODULE_INVALID_ID;
    }

    /* libSceNpSnsFacebookDialog isn't loadable on SDK >= 7.00 */
    if (id == 0xb0 && sdk_ver >= KERNEL_FW_700) {
        return (u32)ORBIS_SYSMODULE_INVALID_ID;
    }

    /* libSceJson isn't loadable on SDK >= 3.00 */
    if (id == 0x80 && sdk_ver >= KERNEL_FW_300) {
        return (u32)ORBIS_SYSMODULE_INVALID_ID;
    }

    /* Cannot load debug modules on retail hardware. */
    if (sysmodule_is_debug_module(id) && !emu_settings_get_dev_kit_mode()) {
        return (u32)ORBIS_SYSMODULE_INVALID_ID;
    }

    return ORBIS_OK;
}

/* @path, a module @mod_name loaded from: its handle, or the program stops */
static s32 LoadSysModule(const char* path, const char* mod_name, s32 argc, const void* argv,
                         s32* start_result) {
    const s32 handle = linker_load_and_start_module(path, (u64)argc, argv, start_result);
    if (handle < 0) {
        char msg[SYSMODULE_NAME_SIZE + 32];
        strlcpy(msg, "Failed to load module ", sizeof(msg));
        strlcat(msg, mod_name, sizeof(msg));
        c_assert_fail(__FILE__, __LINE__, "loadModuleInternal", msg);
    }
    return handle;
}

s32 sysmodule_load_module_internal(s32 index, s32 argc, const void* argv, s32* res_out) {
    static s32 stub_handle = 100;
    /* If the module is already loaded, increment is_loaded and return ORBIS_OK. */
    OrbisSysmoduleModuleInternal* mod = &g_modules_array[index];
    s32 start_result = 0;
    if (mod->is_loaded > 0) {
        mod->is_loaded++;
        return ORBIS_OK;
    }

    /* Most of the logic the actual module has here is to get the correct location of this
     * module. Since we only care about a small subset of LLEs, we can simplify this logic. */
    if ((mod->flags & SYSMODULE_IS_GAME) != 0) {
        char guest_path[SYSMODULE_NAME_SIZE + 32];
        s32 result;
        strlcpy(guest_path, "/app0/sce_module/", sizeof(guest_path));
        strlcat(guest_path, mod->name, sizeof(guest_path));
        strlcat(guest_path, ".prx", sizeof(guest_path));
        result = linker_load_and_start_module(guest_path, (u64)argc, argv, &start_result);
        /* If the module is missing, the library prints a very helpful message for developers.
         * We'll just log an error. */
        if (result < 0) {
            c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "loadModuleInternal",
                  "Failed to load game library %s", guest_path);
            return result;
        }
        /* On success, the library validates module params and the module SDK version.
         * We don't store the information this uses, so skip the proper checks. */
        mod->handle = result;
        mod->is_loaded++;
    } else {
        /* This is not a game library. We'll need to perform some checks,
         * but we don't need to perform the path resolution logic the actual library has. */
        char mod_name[SYSMODULE_NAME_SIZE];
        char sys_module_path[PATH_MAX_LENGTH];
        char module_path[PATH_MAX_LENGTH];
        const char* serial;
        s32 hle;
        bool has_hle = false;
        strlcpy(mod_name, mod->name, sizeof(mod_name));

        /* libSceGnmDriver case */
        if (index == 0xd && emu_settings_get_dev_kit_mode()) {
            /* There are some other checks involved here that I am not familiar with. Since
             * we're not exactly running libSceGnmDriver LLE, this shouldn't matter too much. */
            strlcat(mod_name, "_padebug", sizeof(mod_name));
        }

        /* libSceSsl2 case: with legacy SSL, it would be libSceSsl (index 0x15) instead; never,
         * for now */

        /* libSceVrTracker case */
        if (index == 0xb3 && emu_settings_get_dev_kit_mode()) {
            strlcat(mod_name, "_debug", sizeof(mod_name));
        }

        if ((mod->flags & SYSMODULE_IS_NEO) == 0 && (mod->flags & SYSMODULE_IS_NEO_MODE) != 0 &&
            kernel_is_neo_mode() == 1) {
            /* PS4 Pro running in enhanced mode */
            strlcat(mod_name, "ForNeoMode", sizeof(mod_name));
        } else if ((mod->flags & SYSMODULE_IS_NEO) != 0 && emu_settings_get_neo_mode()) {
            /* PS4 Pro running in base mode */
            strlcat(mod_name, "ForNeo", sizeof(mod_name));
        }

        /* Append .sprx extension. */
        strlcat(mod_name, ".sprx", sizeof(mod_name));

        /* Now we need to check if the requested library is allowed to LLE.
         * First, we allow all modules from game-specific sys_modules */
        emu_settings_get_sys_modules_dir(sys_module_path, sizeof(sys_module_path));
        serial = kernel_game_serial();
        if (serial[0]) {
            char serial_dir[PATH_MAX_LENGTH];
            char game_specific_module_path[PATH_MAX_LENGTH];
            fill_pathname_join_special(serial_dir, sys_module_path, serial, sizeof(serial_dir));
            fill_pathname_join_special(game_specific_module_path, serial_dir, mod_name,
                                       sizeof(game_specific_module_path));
            if (path_is_valid(game_specific_module_path)) {
                /* The requested module is present in the game-specific sys_modules, load it. */
                c_log(C_LOG_LOADER, C_LOG_LEVEL_INFO, "loadModuleInternal",
                      "Loading %s from game serial file %s", mod_name, serial);
                mod->handle =
                    LoadSysModule(game_specific_module_path, mod_name, argc, argv, &start_result);
                mod->is_loaded++;
                if (res_out != NULL) {
                    *res_out = start_result;
                }
                return ORBIS_OK;
            }
        }

        /* We need to check a few things here.
         * First, check if this is a module we allow LLE for. */
        hle = sysmodule_hle_find(mod_name, &has_hle);
        if (hle < 0) {
            /* Not an allowed LLE, stub success without warning. */
            mod->is_loaded++;
            /* Some internal checks rely on a handle, stub a valid one. */
            mod->handle = stub_handle++;
            if (res_out != NULL) {
                *res_out = ORBIS_OK;
            }
            return ORBIS_OK;
        }

        /* Allowed module, check if it exists */
        fill_pathname_join_special(module_path, sys_module_path, mod_name, sizeof(module_path));
        if (path_is_valid(module_path)) {
            c_log(C_LOG_LOADER, C_LOG_LEVEL_INFO, "loadModuleInternal", "Loading %s", mod_name);
            mod->handle = LoadSysModule(module_path, mod_name, argc, argv, &start_result);
        } else {
            /* Allowed LLE that isn't present: its HLE, when there is one. When loading HLEs, we
             * need to relocate imports. This ensures later module loads can see our HLE
             * functions. */
            if (has_hle) {
                c_log(C_LOG_LOADER, C_LOG_LEVEL_INFO, "loadModuleInternal",
                      "Can't Load %s switching to HLE", mod_name);
                sysmodule_hle_register(hle);
            } else {
                c_log(C_LOG_LOADER, C_LOG_LEVEL_INFO, "loadModuleInternal",
                      "No HLE available for %s module", mod_name);
            }
            mod->handle = stub_handle++;
        }

        /* Mark module as loaded. */
        mod->is_loaded++;
    }

    /* Only successful loads will reach here */
    if (res_out != NULL) {
        *res_out = start_result;
    }

    return ORBIS_OK;
}

s32 sysmodule_load_module(s32 id, s32 argc, const void* argv, s32* res_out) {
    /* Retrieve the module to load from the table */
    OrbisSysmoduleModuleInternal requested_module;
    u64 m;
    s64 i;
    memset(&requested_module, 0, sizeof(requested_module));
    for (m = 0; m < SYSMODULE_NUM_MODULES; m++) {
        if (g_modules_array[m].id == (u32)id) {
            requested_module = g_modules_array[m];
            break;
        }
    }
    if (requested_module.id != (u32)id || requested_module.id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    /* Every module has a pointer to an array of indexes to modules that need loading. */
    if (requested_module.to_load == NULL) {
        /* Seems like ORBIS_SYSMODULE_LOCK_FAILED is a generic internal error. */
        return ORBIS_SYSMODULE_LOCK_FAILED;
    }

    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_INFO, "loadModule", "Loading %s",
          requested_module.name);

    /* Loop through every module that requires loading, in reverse order */
    for (i = requested_module.num_to_load - 1; i >= 0; i--) {
        /* Modules flagged as debug modules only load for devkits */
        const u32 mod_index = requested_module.to_load[i];
        s32 result;
        if (!emu_settings_get_dev_kit_mode() &&
            (g_modules_array[mod_index].flags & SYSMODULE_IS_DEBUG) != 0) {
            continue;
        }

        /* Arguments and result should only be applied to the requested module
         * Dependencies don't receive these values. */
        if (i != 0) {
            result = sysmodule_load_module_internal((s32)mod_index, 0, NULL, NULL);
        } else {
            result = sysmodule_load_module_internal((s32)mod_index, argc, argv, res_out);
        }

        /* If loading any module fails, abort there. */
        if (result != ORBIS_OK) {
            return result;
        }
    }
    return ORBIS_OK;
}

s32 sysmodule_unload_module(s32 id, s32 argc, const void* argv, s32* res_out, bool is_internal) {
    OrbisSysmoduleModuleInternal mod;
    s32 i;
    s64 d;
    (void)argc;
    (void)argv;
    (void)is_internal;
    memset(&mod, 0, sizeof(mod));
    for (i = 0; i < SYSMODULE_NUM_MODULES; i++) {
        bool preloaded_once = false;
        size_t p;
        mod = g_modules_array[i];
        if (mod.id != (u32)id) {
            continue;
        }

        /* Skips checking libSceDiscMap */
        if (i == 0x22) {
            continue;
        }

        /* If the module is loaded once, and is part of the second preload list,
         * then return OK and do nothing. */
        for (p = 0; p < ARRAY_COUNT(g_preload_list_2); p++) {
            if ((s32)g_preload_list_2[p] == i && mod.is_loaded == 1) {
                preloaded_once = true;
            }
        }
        if (preloaded_once) {
            return ORBIS_OK;
        }

        /* Found the correct module. */
        break;
    }

    /* If we failed to locate the module, return invalid id. */
    if (mod.id != (u32)id || mod.id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    /* If the module has no dependencies, then return an internal error. */
    if (mod.num_to_load == 0 || mod.to_load == NULL) {
        return ORBIS_SYSMODULE_LOCK_FAILED;
    }

    /* Unload the module and it's dependencies */
    for (d = 0; d < mod.num_to_load; d++) {
        const OrbisSysmoduleModuleInternal* dep_mod = &g_modules_array[mod.to_load[d]];
        /* If this is a debug module and we're not emulating a devkit, skip it. */
        if ((dep_mod->flags & SYSMODULE_IS_DEBUG) != 0 && !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        /* If the module to unload is marked as unloaded, then return not loaded */
        if (dep_mod->is_loaded == 0) {
            return ORBIS_SYSMODULE_NOT_LOADED;
        }

        /* The load count stays as it is: the C++ took it down on a copy of the module's entry.
         * Normally, this is where the real library would actually unload the module, through a
         * call to sceKernelStopUnloadModule. As we don't implement module unloading, this
         * behavior is skipped. */

        /* Stub success during requested module unload. */
        if (d == 0 && res_out != NULL) {
            *res_out = ORBIS_OK;
        }
    }
    return ORBIS_OK;
}

s32 sysmodule_preload_modules_for_libkernel(void) {
    /* For now, default to loading g_preload_list_3.
     * As far as I can tell, g_preload_list_1 seems to be some sort of list with libs
     * that games don't typically use, and g_preload_list_2 is just a reorganized version of
     * 3. */
    s32 sdk_ver = 0;
    size_t p;
    C_ASSERT_MSG(kernel_get_compiled_sdk_version(&sdk_ver) == 0, "preloadModulesForLibkernel",
                 "Failed to get compiled SDK version");
    for (p = 0; p < ARRAY_COUNT(g_preload_list_3); p++) {
        const u32 module_index = g_preload_list_3[p];
        s32 result;
        /* As per usual, these are arrays of indexes for g_modules_array
         * libSceDbg, libScePerf, libSceMat, and libSceRazorCpu_debug.
         * These are skipped unless this console is a devkit. */
        if ((module_index == 0x12 || module_index == 0x1e || module_index == 0x24 ||
             module_index == 0x26) &&
            !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        /* libSceDiscMap case, skipped on newer SDK versions. */
        if (module_index == 0x22 && sdk_ver >= KERNEL_FW_200) {
            continue;
        }

        /* libSceDbgAssist is skipped on non-testkit consoles.
         * For now, stub check to non-devkit. */
        if (module_index == 0x23 && !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        /* libSceRazorCpu, skipped for old non-devkit consoles. */
        if (module_index == 0x25 && sdk_ver < KERNEL_FW_450 && !emu_settings_get_dev_kit_mode()) {
            continue;
        }

        /* libSceHttp2, skipped for SDK versions below 7.00. */
        if (module_index == 0x28 && sdk_ver < KERNEL_FW_700) {
            continue;
        }

        /* libSceNpWebApi2 and libSceNpGameIntent, skipped for SDK versions below 7.50 */
        if ((module_index == 0x29 || module_index == 0x2a) && sdk_ver < KERNEL_FW_750) {
            continue;
        }

        if (module_index == 1 || module_index == 2) {
            /* libkernel and libSceLibcInternal aren't directly loaded here.
             * All we do for those is increment is_loaded */
            g_modules_array[module_index].is_loaded++;
            continue;
        }

        /* Load the actual module */
        result = sysmodule_load_module_internal((s32)module_index, 0, NULL, NULL);
        if (result != ORBIS_OK) {
            /* On real hardware, module preloading must succeed or the game will abort.
             * To enable users to test homebrew easier, we'll log a critical error instead. */
            if (strcmp(g_modules_array[module_index].name, "libc") == 0) {
                C_ASSERT_MSG(!sysmodule_need_scelibc, "preloadModulesForLibkernel",
                             "libc.prx cannot be loaded, but the guest attempted to use it. "
                             "This usually indicates a corrupted dump.");
            } else if (strcmp(g_modules_array[module_index].name, "libSceFios2") == 0) {
                C_ASSERT_MSG(!sysmodule_need_fios2, "preloadModulesForLibkernel",
                             "libSceFios2.prx cannot be loaded, but the guest attempted to use "
                             "it. This usually indicates a corrupted dump.");
            } else {
                c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_CRITICAL, "preloadModulesForLibkernel",
                      "Failed to preload %s, expect crashes", g_modules_array[module_index].name);
            }
        }
    }
    return ORBIS_OK;
}
