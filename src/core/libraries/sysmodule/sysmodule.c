/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <string.h>

#include <boolean.h>

#include "common/c_bridge.h"
#include "common/combine.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/sysmodule/sysmodule.h"
#include "core/libraries/sysmodule/sysmodule_error.h"
#include "core/libraries/sysmodule/sysmodule_internal.h"

static combiner_t g_mutex;

void sysmodule_library_init(void) {
    static bool made = false;
    if (!made) {
        made = true;
        combiner_init(&g_mutex);
    }
}

/* sysmodule_get_module_handle, run under g_mutex */
typedef struct sysmodule_handle_call {
    s32 id;
    s32* handle;
    s32 result;
} sysmodule_handle_call;

static void GetModuleHandleLocked(void* ctx) {
    sysmodule_handle_call* c = (sysmodule_handle_call*)ctx;
    c->result = sysmodule_get_module_handle(c->id, c->handle);
}

static s32 GetModuleHandleUnderMutex(s32 id, s32* handle) {
    sysmodule_handle_call c;
    c.id = id;
    c.handle = handle;
    c.result = 0;
    combiner_run(&g_mutex, GetModuleHandleLocked, &c);
    return c.result;
}

/* sysmodule_load_module or sysmodule_unload_module, run under g_mutex */
typedef struct sysmodule_load_call {
    s32 id;
    s32 argc;
    const void* argv;
    s32* res_out;
    bool unload;
    s32 result;
} sysmodule_load_call;

static void LoadModuleLocked(void* ctx) {
    sysmodule_load_call* c = (sysmodule_load_call*)ctx;
    if (c->unload) {
        c->result = sysmodule_unload_module(c->id, c->argc, c->argv, c->res_out, false);
    } else {
        c->result = sysmodule_load_module(c->id, c->argc, c->argv, c->res_out);
    }
}

static s32 LoadModuleUnderMutex(s32 id, s32 argc, const void* argv, s32* res_out, bool unload) {
    sysmodule_load_call c;
    c.id = id;
    c.argc = argc;
    c.argv = argv;
    c.res_out = res_out;
    c.unload = unload;
    c.result = 0;
    combiner_run(&g_mutex, LoadModuleLocked, &c);
    return c.result;
}

s32 PS4_SYSV_ABI sceSysmoduleGetModuleHandleInternal(OrbisSysModuleInternal id, s32* handle) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_INFO, "sceSysmoduleGetModuleHandleInternal", "called");
    if ((id & 0x7fffffff) == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    return GetModuleHandleUnderMutex((s32)id, handle);
}

s32 PS4_SYSV_ABI sceSysmoduleGetModuleInfoForUnwind(u64 addr, s32 flags,
                                                    kernel_module_info_for_unwind_t* info) {
    s32 res;
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_TRACE, "sceSysmoduleGetModuleInfoForUnwind",
          "sceSysmoduleGetModuleInfoForUnwind called");
    res = kernel_get_module_info_for_unwind(addr, flags, info);
    if (res != ORBIS_OK) {
        return res;
    }

    if (sysmodule_should_hide_name(info->name)) {
        memset(info->name, 0, sizeof(info->name));
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSysmoduleIsCalledFromSysModule(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleIsCalledFromSysModule",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSysmoduleIsCameraPreloaded(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleIsCameraPreloaded",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSysmoduleIsLoaded(OrbisSysModule id) {
    if (id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    return GetModuleHandleUnderMutex((s32)id, NULL);
}

s32 PS4_SYSV_ABI sceSysmoduleIsLoadedInternal(OrbisSysModuleInternal id) {
    if ((id & 0x7fffffff) == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    return GetModuleHandleUnderMutex((s32)id, NULL);
}

s32 PS4_SYSV_ABI sceSysmoduleLoadModule(OrbisSysModule id) {
    s32 result;
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_INFO, "sceSysmoduleLoadModule", "called, id = 0x%x",
          (unsigned)id);
    result = (s32)sysmodule_validate_module_id((s32)id);
    if (result < ORBIS_OK) {
        return result;
    }

    /* Only locks for internal loadModule call. */
    result = LoadModuleUnderMutex((s32)id, 0, NULL, NULL, false);

    if (result == ORBIS_KERNEL_ERROR_ESTART) {
        s32 sdk_ver = 0;
        result = kernel_get_compiled_sdk_version(&sdk_ver);
        if (sdk_ver < KERNEL_FW_1150 || result != ORBIS_OK) {
            return ORBIS_KERNEL_ERROR_EINVAL;
        } else {
            return ORBIS_KERNEL_ERROR_ESTART;
        }
    }

    /* The real library has some weird workaround for CUSA01478 and CUSA01495 here.
     * Unless this is proven necessary, I don't plan to handle this. */
    return result;
}

s32 PS4_SYSV_ABI sceSysmoduleLoadModuleByNameInternal(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleLoadModuleByNameInternal",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal id) {
    s32 result;
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_INFO, "sceSysmoduleLoadModuleInternal",
          "called, id = 0x%x", (unsigned)id);
    result = (s32)sysmodule_validate_module_id((s32)id);
    if (result < ORBIS_OK) {
        return result;
    }

    /* This specific module ID is loaded unlocked. */
    if (id == 0x80000039) {
        return sysmodule_load_module((s32)id, 0, NULL, NULL);
    }
    return LoadModuleUnderMutex((s32)id, 0, NULL, NULL, false);
}

s32 PS4_SYSV_ABI sceSysmoduleLoadModuleInternalWithArg(OrbisSysModuleInternal id, s32 argc,
                                                       const void* argv, u64 unk, s32* res_out) {
    s32 result;
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_INFO, "sceSysmoduleLoadModuleInternalWithArg",
          "called, id = 0x%x", (unsigned)id);
    result = (s32)sysmodule_validate_module_id((s32)id);
    if (result < ORBIS_OK) {
        return result;
    }

    if (unk != 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    return LoadModuleUnderMutex((s32)id, argc, argv, res_out, false);
}

s32 PS4_SYSV_ABI sceSysmoduleMapLibcForLibkernel(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleMapLibcForLibkernel",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSysmodulePreloadModuleForLibkernel(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_DEBUG, "sceSysmodulePreloadModuleForLibkernel",
          "called");
    return sysmodule_preload_modules_for_libkernel();
}

s32 PS4_SYSV_ABI sceSysmoduleUnloadModule(OrbisSysModule id) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleUnloadModule",
          "(STUBBED) called, id = 0x%x", (unsigned)id);
    if (id == 0) {
        return ORBIS_SYSMODULE_INVALID_ID;
    }

    return LoadModuleUnderMutex((s32)id, 0, NULL, NULL, true);
}

s32 PS4_SYSV_ABI sceSysmoduleUnloadModuleByNameInternal(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleUnloadModuleByNameInternal",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSysmoduleUnloadModuleInternal(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleUnloadModuleInternal",
          "(STUBBED) called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceSysmoduleUnloadModuleInternalWithArg(void) {
    c_log(C_LOG_LIB_SYS_MODULE, C_LOG_LEVEL_ERROR, "sceSysmoduleUnloadModuleInternalWithArg",
          "(STUBBED) called");
    return ORBIS_OK;
}
