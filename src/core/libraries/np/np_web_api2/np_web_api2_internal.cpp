// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "common/run_once.h"
#include "core/emulator_settings.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/network/http2.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/np_types.h"
#include "core/libraries/np/np_web_api2/np_web_api2_context.h"
#include "core/libraries/np/np_web_api2/np_web_api2_internal.h"
#include "core/libraries/system/userservice.h"

namespace Libraries::Np::NpWebApi2 {

// The library contexts, in a table without a lock (obj_table.c)
static obj_table_t g_lib_contexts;
static run_once_t g_lib_contexts_once = RUN_ONCE_INIT;
static retro_atomic_64_t g_last_timeout_check{0};

obj_table_t* LibraryContexts() {
    run_once(
        &g_lib_contexts_once, [](void*) { obj_table_init(&g_lib_contexts, MaxLibraryContexts); },
        nullptr);
    return &g_lib_contexts;
}

s32 createLibraryContext(s32 http_ctx_id, s32 type, u64 pool_size, const char* name) {
    LibraryContext* lib_ctx = new LibraryContext(type, http_ctx_id, pool_size, name);
    // Its id: its slot and the table's count, under 0x8000
    if (!obj_table_put(LibraryContexts(), &lib_ctx->head, 6, 15, 0)) {
        delete lib_ctx;
        LOG_ERROR(Lib_NpWebApi2, "Too many library contexts");
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_MAX;
    }
    return lib_ctx->GetId();
}

LibraryContext* getLibraryContext(s32 lib_ctx_id) {
    LibraryContext* lib_ctx =
        reinterpret_cast<LibraryContext*>(obj_table_find(LibraryContexts(), lib_ctx_id, 6));
    if (lib_ctx != nullptr && lib_ctx->IsDeleted()) {
        lib_ctx->RemoveUser();
        return nullptr;
    }
    return lib_ctx;
}

s32 getMemoryPoolStats(s32 lib_ctx_id, OrbisNpWebApi2MemoryPoolStats* stats) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->pool_size = lib_ctx->GetPoolSize();
    }
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 createPushEventHandle(s32 lib_ctx_id) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = lib_ctx->CreatePushEventHandle();
    lib_ctx->RemoveUser();
    return result;
}

s32 setHandleTimeout(s32 lib_ctx_id, s32 handle_id, u32 timeout) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    PushEventHandle* handle = lib_ctx->GetPushEventHandle(handle_id);
    if (!handle) {
        LOG_ERROR(Lib_NpWebApi2, "No handle with id {:#x}", handle_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_HANDLE_NOT_FOUND;
    }

    handle->SetTimeout(timeout);
    handle->RemoveUser();
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 abortPushEventHandle(s32 lib_ctx_id, s32 handle_id) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    PushEventHandle* handle = lib_ctx->GetPushEventHandle(handle_id);
    if (!handle) {
        LOG_ERROR(Lib_NpWebApi2, "No handle with id {:#x}", handle_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_HANDLE_NOT_FOUND;
    }

    handle->Abort();
    handle->RemoveUser();
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 deletePushEventHandle(s32 lib_ctx_id, s32 handle_id) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    PushEventHandle* handle = lib_ctx->GetPushEventHandle(handle_id);
    if (!handle) {
        LOG_ERROR(Lib_NpWebApi2, "No handle with id {:#x}", handle_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_HANDLE_NOT_FOUND;
    }

    s32 result = lib_ctx->DeletePushEventHandle(handle);
    lib_ctx->RemoveUser();
    return result;
}

s32 createPushEventFilter(s32 lib_ctx_id, s32 handle_id, const char* np_service_name,
                          OrbisNpServiceLabel np_service_label,
                          const OrbisNpWebApi2PushEventFilterParameter* filter_param,
                          u64 filter_param_num, bool internal) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    lib_ctx->SetHandleEndTime(handle_id);
    s32 result = lib_ctx->CreatePushEventFilter(handle_id, np_service_name, np_service_label,
                                                filter_param, filter_param_num, internal);
    lib_ctx->RemoveUser();
    return result;
}

s32 deletePushEventFilter(s32 lib_ctx_id, s32 filter_id) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    PushEventFilter* filter = lib_ctx->GetPushEventFilter(filter_id);
    if (!filter) {
        LOG_ERROR(Lib_NpWebApi2, "No filter with id {:#x}", filter_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
    }

    lib_ctx->DeletePushEventFilter(filter);
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 terminateLibraryContext(s32 lib_ctx_id) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    lib_ctx->DeleteAllUserContexts();
    lib_ctx->AbortAllPushEventHandles();

    if (!lib_ctx->MarkForDeletion()) {
        LOG_ERROR(Lib_NpWebApi2, "Library context with id {:#x} is already deleted", lib_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    // Found no more; its other holders and its handles' let go, on the game's time. The last
    // hold frees it, with its handles and filters.
    obj_table_remove(LibraryContexts(), &lib_ctx->head);
    Libraries::Kernel::GameWait(obj_table_released(), 0, [lib_ctx] {
        return !lib_ctx->IsBusy() && !lib_ctx->HasBusyPushEventHandles();
    });
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 createUserContext(s32 lib_ctx_id, Libraries::UserService::OrbisUserServiceUserId user_id) {
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context with id {:#x}", lib_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContextByUserId(user_id);
    if (user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "User context already exists for user id {}", user_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_ALREADY_EXIST;
    }

    s32 user_ctx_id = lib_ctx->CreateUserContext(user_id);
    if (user_ctx_id < 0) {
        // user_ctx_id contains an error code, return it.
        lib_ctx->RemoveUser();
        return user_ctx_id;
    }

    user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (user_ctx != nullptr) {
        s32 result = user_ctx->Initialize();
        if (result < 0) {
            // Failed to initialize user_ctx, destroy it.
            user_ctx->Delete();
        } else {
            user_ctx->RemoveUser();
        }
    }

    lib_ctx->RemoveUser();
    return user_ctx_id;
}

s32 deleteUserContext(s32 user_ctx_id) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    user_ctx->AbortAllRequests();
    if (!user_ctx->MarkForDeletion()) {
        LOG_ERROR(Lib_NpWebApi2, "User context with id {:#x} is already deleted", user_ctx_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    // Its other holders and its requests' let go, on the game's time
    Libraries::Kernel::GameWait(obj_table_released(), 0, [user_ctx] {
        return !user_ctx->IsBusy() && !user_ctx->HasBusyRequests();
    });

    user_ctx->Delete();
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 registerPushEventCallback(s32 user_ctx_id, s32 filter_id,
                              OrbisNpWebApi2PushEventCallback cb_func, void* user_arg) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (!lib_ctx->HasPushEventFilter(filter_id)) {
        LOG_ERROR(Lib_NpWebApi2, "No push event filter with id {:#x}", filter_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
    }

    s32 result = user_ctx->CreatePushEventCallback(filter_id, cb_func, user_arg);
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 unregisterPushEventCallback(s32 user_ctx_id, s32 callback_id) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    s32 result = user_ctx->DeletePushEventCallback(callback_id);
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 createPushContext(s32 user_ctx_id, OrbisNpWebApi2PushEventPushContextId* push_ctx_id) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    s32 result = user_ctx->CreatePushContext(push_ctx_id);
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 startPushContextCallback(s32 user_ctx_id,
                             const OrbisNpWebApi2PushEventPushContextId* push_ctx_id) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    PushEventPushContext* push_ctx = user_ctx->GetPushContext(push_ctx_id);
    if (!push_ctx) {
        s64 raw_id{};
        std::memcpy(&raw_id, push_ctx_id, sizeof(s64));
        LOG_ERROR(Lib_NpWebApi2, "No push context with id {}", raw_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_PUSH_CONTEXT_NOT_FOUND;
    }

    push_ctx->Start();
    push_ctx->RemoveUser();
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 deletePushContext(s32 user_ctx_id, const OrbisNpWebApi2PushEventPushContextId* push_ctx_id) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    PushEventPushContext* push_ctx = user_ctx->GetPushContext(push_ctx_id);
    if (!push_ctx) {
        s64 raw_id{};
        std::memcpy(&raw_id, push_ctx_id, sizeof(s64));
        LOG_ERROR(Lib_NpWebApi2, "No push context with id {}", raw_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_PUSH_CONTEXT_NOT_FOUND;
    }

    user_ctx->DeletePushContext(push_ctx);
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

s32 registerPushContextCallback(s32 user_ctx_id, s32 filter_id,
                                OrbisNpWebApi2PushEventPushContextCallback cb_func,
                                void* user_arg) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (!lib_ctx->HasPushEventFilter(filter_id)) {
        LOG_ERROR(Lib_NpWebApi2, "No push event filter with id {:#x}", filter_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
    }

    s32 result = user_ctx->CreatePushContextCallback(filter_id, cb_func, user_arg);
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 unregisterPushContextCallback(s32 user_ctx_id, s32 callback_id) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    s32 result = user_ctx->DeletePushContextCallback(callback_id);
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

void processPushEvents() {
    // LOG_ERROR(Lib_NpWebApi2, "(STUBBED)");
}

s32 createRequest(s32 user_ctx_id, const char* api_group, const char* path, const char* method,
                  const OrbisNpWebApi2ContentParameter* content_parameter, bool multipart,
                  s64* request_id) {
    LibraryContext* lib_ctx = getLibraryContext(user_ctx_id >> 0x10);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for user context id {:#x}", user_ctx_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context with id {:#x}", user_ctx_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (std::strlen(api_group) >= 63) {
        LOG_ERROR(Lib_NpWebApi2, "API group is too long");
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_PARAMETER_TOO_LONG;
    }

    Request* request{};
    s32 result =
        user_ctx->CreateRequest(api_group, path, method, content_parameter, multipart, &request);
    if (result < 0) {
        // Request creation failed
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return result;
    }

    if (request_id) {
        *request_id = request->GetId();
    }
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 addHttpRequestHeader(s64 request_id, const char* field_name, const char* field_value) {
    s32 lib_ctx_id = static_cast<s32>(request_id >> 0x30);
    s32 user_ctx_id = static_cast<s32>(request_id >> 0x20);
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for request id {:#x}", request_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id {:#x}", request_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    Request* request = user_ctx->GetRequest(request_id);
    if (!request) {
        LOG_ERROR(Lib_NpWebApi2, "No request with id {:#x}", request_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    if (request->HasSent()) {
        LOG_ERROR(Lib_NpWebApi2, "Request is already sent");
        request->RemoveUser();
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_AFTER_SEND;
    }

    s32 result = request->AddHttpRequestHeader(field_name, field_value);
    request->RemoveUser();
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 setRequestTimeout(s64 request_id, u32 timeout) {
    s32 lib_ctx_id = static_cast<s32>(request_id >> 0x30);
    s32 user_ctx_id = static_cast<s32>(request_id >> 0x20);
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for request id {:#x}", request_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id {:#x}", request_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    Request* request = user_ctx->GetRequest(request_id);
    if (!request) {
        LOG_ERROR(Lib_NpWebApi2, "No request with id {:#x}", request_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    request->SetTimeout(timeout);
    request->RemoveUser();
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return ORBIS_OK;
}

static s32 checkRequestStatus(Request* request, UserContext* user_ctx, LibraryContext* lib_ctx) {
    if (request->Expired()) {
        LOG_ERROR(Lib_NpWebApi2, "Request timed out");
        request->RemoveUser();
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_TIMEOUT;
    }

    if (request->Aborted()) {
        LOG_ERROR(Lib_NpWebApi2, "Request aborted");
        request->RemoveUser();
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_ABORTED;
    }
    return ORBIS_OK;
}

s32 sendRequest(s64 request_id, s32 part_index, void* data, u64 data_size,
                OrbisNpWebApi2ResponseInformationOption* resp_info_option) {
    s32 lib_ctx_id = static_cast<s32>(request_id >> 0x30);
    s32 user_ctx_id = static_cast<s32>(request_id >> 0x20);
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for request id {:#x}", request_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id {:#x}", request_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    Request* request = user_ctx->GetRequest(request_id);
    if (!request) {
        LOG_ERROR(Lib_NpWebApi2, "No request with id {:#x}", request_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    request->SetEndTime();
    if (request->IsMultipart()) {
        if (part_index == 0) {
            LOG_ERROR(Lib_NpWebApi2, "part_index 0 is prohibited for multipart requests");
            request->RemoveUser();
            user_ctx->RemoveUser();
            lib_ctx->RemoveUser();
            return ORBIS_NP_WEBAPI2_ERROR_PROHIBITED_FUNCTION_CALL;
        }
        // TODO: Multipart logic
    }

    if (!request->HasSent()) {
        request->MarkSent();
    }

    request->SetState(0);
    s32 result = checkRequestStatus(request, user_ctx, lib_ctx);
    if (result != 0) {
        // Aborted or timed out.
        return result;
    }

    if (!emu_settings_is_shadnet_enabled()) {
        LOG_INFO(Lib_NpWebApi2, "Cannot send request, you are not signed in to shadNet");
        request->RemoveUser();
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_NOT_SIGNED_IN;
    }

    if (request->GetHttpRequestId() == 0) {
        std::string base_url = emu_settings_get_shadnet_webapi_server();
        s32 template_id = user_ctx->GetHttpTemplateId();
        // Technically this should be base_url + api_group + path,
        // but shadNet doesn't seem to use the api_group for endpoints?
        std::string full_url = base_url + request->GetPath();
        result = request->CreateHttpRequest(template_id, full_url.data());
        if (result < 0) {
            // Underlying request creation failed, return.
            request->RemoveUser();
            user_ctx->RemoveUser();
            lib_ctx->RemoveUser();
            return result;
        }
    }

    request->SetState(3);
    result = checkRequestStatus(request, user_ctx, lib_ctx);
    if (result != 0) {
        // Aborted or timed out.
        return result;
    }

    if (request->IsMultipart()) {
        // TODO: Multipart sends
        // result = request->SendMultipartHttpRequest(data, data_size, part_index);
    } else {
        result = request->SendHttpRequest(data, data_size);
    }

    if (result >= 0 && request->OutOfData()) {
        result = request->GetAllHttpResponseHeaders();
        if (result >= 0) {
            s32 http_request_id = request->GetHttpRequestId();
            s32 http_status{};
            result = Libraries::Http2::sceHttp2GetStatusCode(http_request_id, &http_status);
            if (result >= 0) {
                if (resp_info_option) {
                    resp_info_option->http_status = http_status;
                }
                if (http_status >= 400) {
                    result = http_status | 0x82f00000;
                }
            } else {
                LOG_ERROR(Lib_NpWebApi2, "Failed to get http status code, error = {:#x}", result);
            }
        }
    }

    request->SetState(0);
    s32 result2 = checkRequestStatus(request, user_ctx, lib_ctx);
    if (result2 != 0) {
        // Aborted or timed out.
        return result2;
    }

    request->ClearEndTime();
    request->RemoveUser();
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 getHttpResponseHeaderData(s64 request_id, const char* field_name, char* value, u64 value_size,
                              u64* value_size_out) {
    s32 lib_ctx_id = static_cast<s32>(request_id >> 0x30);
    s32 user_ctx_id = static_cast<s32>(request_id >> 0x20);
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for request id {:#x}", request_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id {:#x}", request_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    Request* request = user_ctx->GetRequest(request_id);
    if (!request) {
        LOG_ERROR(Lib_NpWebApi2, "No request with id {:#x}", request_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    s32 result = request->ParseHttpResponseHeaders(field_name, value, value_size, value_size_out);
    request->RemoveUser();
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 readData(s64 request_id, void* data, u64 size) {
    s32 lib_ctx_id = static_cast<s32>(request_id >> 0x30);
    s32 user_ctx_id = static_cast<s32>(request_id >> 0x20);
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for request id {:#x}", request_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id {:#x}", request_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    Request* request = user_ctx->GetRequest(request_id);
    if (!request) {
        LOG_ERROR(Lib_NpWebApi2, "No request with id {:#x}", request_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    request->SetEndTime();

    request->SetState(3);
    s32 result = checkRequestStatus(request, user_ctx, lib_ctx);
    if (result != 0) {
        // Aborted or timed out.
        return result;
    }

    s32 http_req_id = request->GetHttpRequestId();
    s32 result2 = Libraries::Http2::sceHttp2ReadData(http_req_id, data, size);

    request->SetState(0);
    result = checkRequestStatus(request, user_ctx, lib_ctx);
    if (result != 0) {
        // Aborted or timed out.
        return result;
    }

    request->ClearEndTime();

    request->RemoveUser();
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result2;
}

s32 abortRequest(s64 request_id) {
    s32 lib_ctx_id = static_cast<s32>(request_id >> 0x30);
    s32 user_ctx_id = static_cast<s32>(request_id >> 0x20);
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for request id {:#x}", request_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id {:#x}", request_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    Request* request = user_ctx->GetRequest(request_id);
    if (!request) {
        LOG_ERROR(Lib_NpWebApi2, "No request with id {:#x}", request_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    s32 result = request->Abort();
    request->RemoveUser();
    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    return result;
}

s32 deleteRequest(s64 request_id) {
    s32 lib_ctx_id = static_cast<s32>(request_id >> 0x30);
    s32 user_ctx_id = static_cast<s32>(request_id >> 0x20);
    LibraryContext* lib_ctx = getLibraryContext(lib_ctx_id);
    if (!lib_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No library context for request id {:#x}", request_id);
        return ORBIS_NP_WEBAPI2_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    UserContext* user_ctx = lib_ctx->GetUserContext(user_ctx_id);
    if (!user_ctx) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id {:#x}", request_id);
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    Request* request = user_ctx->GetRequest(request_id);
    if (!request) {
        LOG_ERROR(Lib_NpWebApi2, "No request with id {:#x}", request_id);
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    if (!request->MarkForDeletion()) {
        // Already marked for deletion
        LOG_ERROR(Lib_NpWebApi2, "Request with id {:#x} is already being deleted", request_id);
        request->RemoveUser();
        user_ctx->RemoveUser();
        lib_ctx->RemoveUser();
        return ORBIS_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    }

    request->Abort();
    // Found no more; its other holders let go, on the game's time
    obj_table_remove(user_ctx->Requests(), &request->head);
    Libraries::Kernel::GameWait(obj_table_released(), 0, [request] { return !request->IsBusy(); });
    // Its HTTP request deleted here, after the request itself is freed
    const s32 http_request_id = request->TakeHttpRequestId();
    request->RemoveUser();

    user_ctx->RemoveUser();
    lib_ctx->RemoveUser();
    if (http_request_id != 0) {
        Libraries::Http2::sceHttp2DeleteRequest(http_request_id);
    }
    return ORBIS_OK;
}

void checkTimeout() {
    const u64 current_time = NowUs();
    const s64 last = retro_atomic_load_acquire_64(&g_last_timeout_check);
    // Too soon, or another thread checking now: skip
    if (current_time < static_cast<u64>(last) + 1000 ||
        !retro_atomic_cas_64(&g_last_timeout_check, last, static_cast<s64>(current_time))) {
        return;
    }

    obj_table_t* table = LibraryContexts();
    for (int i = 0; i < table->count; i++) {
        auto* lib_ctx = static_cast<LibraryContext*>(obj_table_hold(table, i));
        if (lib_ctx != nullptr) {
            lib_ctx->CheckTimeout();
            lib_ctx->RemoveUser();
        }
    }
}

}; // namespace Libraries::Np::NpWebApi2
