// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <queues/mpsc_stack.h>
#include "common/elf_info.h"
#include "common/enum_names.h"
#include "common/run_once.h"
#include "core/emulator_settings.h"
#include "core/libraries/kernel/process.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/network/http.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/np_handler/np_handler.h"
#include "np_web_api_internal.h"

namespace Libraries::Np::NpWebApi {

// The library's objects, in tables without a lock (obj_table.c). An id names its slot in its
// low bits and how often the slot was used above them, so a stale id finds nothing.
static constexpr int MaxContexts = 32;
static constexpr int MaxUserContexts = 16;
static constexpr int MaxRequests = 128;
static constexpr int MaxHandles = 64;
static constexpr int MaxFilters = 64;
static constexpr int MaxCallbacks = 64;

static obj_table_t g_contexts;
static run_once_t g_tables_once = RUN_ONCE_INIT;

static void TablesInit() {
    run_once(&g_tables_once, [](void*) { obj_table_init(&g_contexts, MaxContexts); }, nullptr);
}

static thread_local s32 g_last_webapi_error = ORBIS_OK;

static s32 captureWebApiError(const char* body, u64 len);
static retro_atomic_64_t g_last_timeout_check{0};
static s32 g_sdk_ver = 0;

// Until @done(), on the game's time, woken as objects are given back (obj_table.c)
template <typename Done>
static void WaitUntil(Done&& done) {
    Kernel::GameWait(obj_table_released(), 0, done);
}

// Whether @id names an object of @table now
static bool HasObject(obj_table_t* table, s64 id) {
    obj_head_t* object = obj_table_find(table, id, 8);
    if (object == nullptr) {
        return false;
    }
    obj_table_put_back(table, object);
    return true;
}

template <typename T>
static void DestroyAs(obj_head_t* object) {
    delete reinterpret_cast<T*>(object);
}

static void DestroyUserContext(obj_head_t* object) {
    auto* user_context = reinterpret_cast<OrbisNpWebApiUserContext*>(object);
    obj_table_destroy(&user_context->requests);
    obj_table_destroy(&user_context->pushEventCallbacks);
    obj_table_destroy(&user_context->servicePushEventCallbacks);
    obj_table_destroy(&user_context->extendedPushEventCallbacks);
    delete user_context;
}

static void DestroyContext(obj_head_t* object) {
    auto* context = reinterpret_cast<OrbisNpWebApiContext*>(object);
    obj_table_destroy(&context->userContexts);
    obj_table_destroy(&context->handles);
    obj_table_destroy(&context->pushEventFilters);
    obj_table_destroy(&context->servicePushEventFilters);
    obj_table_destroy(&context->extendedPushEventFilters);
    delete context;
}

// The game's time in microseconds, as sceKernelGetProcessTime counted
static u64 NowUs() {
    return Kernel::GameNow() / 1000;
}

s32 initializeLibrary() {
    TablesInit();
    return Kernel::sceKernelGetCompiledSdkVersion(&g_sdk_ver);
}

s32 getCompiledSdkVersion() {
    return g_sdk_ver;
}

s32 createLibraryContext(s32 libHttpCtxId, u64 poolSize, const char* name, s32 type) {
    TablesInit();
    auto* new_context = new OrbisNpWebApiContext{};
    new_context->base.destroy = DestroyContext;
    new_context->libHttpCtxId = libHttpCtxId;
    new_context->type = type;
    retro_atomic_int_init(&new_context->terminated, 0);
    if (name != nullptr) {
        new_context->name = std::string(name);
    }
    if (!obj_table_init(&new_context->userContexts, MaxUserContexts) ||
        !obj_table_init(&new_context->handles, MaxHandles) ||
        !obj_table_init(&new_context->pushEventFilters, MaxFilters) ||
        !obj_table_init(&new_context->servicePushEventFilters, MaxFilters) ||
        !obj_table_init(&new_context->extendedPushEventFilters, MaxFilters)) {
        DestroyContext(&new_context->base);
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_MAX;
    }

    // Its id below 0x8000: its slot in six bits, the generation above
    if (!obj_table_put(&g_contexts, &new_context->base, 6, 15, 0)) {
        DestroyContext(&new_context->base);
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_MAX;
    }
    new_context->libCtxId = static_cast<s32>(new_context->base.id);
    return new_context->libCtxId;
}

OrbisNpWebApiContext* findAndValidateContext(s32 libCtxId, s32 flag) {
    TablesInit();
    if (libCtxId < 1 || libCtxId >= 0x8000) {
        return nullptr;
    }
    auto* context =
        reinterpret_cast<OrbisNpWebApiContext*>(obj_table_find(&g_contexts, libCtxId, 6));
    if (context == nullptr) {
        return nullptr;
    }
    if (flag == 0 && retro_atomic_load_acquire_int(&context->terminated) != 0) {
        releaseContext(context);
        return nullptr;
    }
    return context;
}

void releaseContext(OrbisNpWebApiContext* context) {
    obj_table_put_back(&g_contexts, &context->base);
}

bool isContextTerminated(OrbisNpWebApiContext* context) {
    return retro_atomic_load_acquire_int(&context->terminated) != 0;
}

bool isContextBusy(OrbisNpWebApiContext* context) {
    return obj_table_users(&g_contexts, context->base.index) > 1;
}

bool areContextHandlesBusy(OrbisNpWebApiContext* context) {
    for (int i = 0; i < context->handles.count; i++) {
        if (obj_table_live(&context->handles, i) && obj_table_users(&context->handles, i) > 0) {
            return true;
        }
    }
    return false;
}

void markContextAsTerminated(OrbisNpWebApiContext* context) {
    retro_atomic_store_release_int(&context->terminated, 1);
}

void checkContextTimeout(OrbisNpWebApiContext* context) {
    const u64 time = NowUs();

    for (int i = 0; i < context->userContexts.count; i++) {
        auto* user_context =
            static_cast<OrbisNpWebApiUserContext*>(obj_table_hold(&context->userContexts, i));
        if (user_context == nullptr) {
            continue;
        }
        checkUserContextTimeout(user_context);
        releaseUserContext(user_context);
    }

    for (int i = 0; i < context->handles.count; i++) {
        auto* handle = static_cast<OrbisNpWebApiHandle*>(obj_table_hold(&context->handles, i));
        if (handle == nullptr) {
            continue;
        }
        const u64 end = static_cast<u64>(retro_atomic_load_acquire_64(&handle->handleEndTime));
        if (retro_atomic_load_acquire_int(&handle->handleTimeout) != 0 && end != 0 && end < time &&
            retro_atomic_exchange_int(&handle->timedOut, 1) == 0) {
            const s32 handle_id = handle->handleId;
            releaseHandle(context, handle);
            abortHandle(context->libCtxId, handle_id);
            continue;
        }
        releaseHandle(context, handle);
    }
}

void checkTimeout() {
    TablesInit();
    const u64 time = NowUs();
    const s64 last = retro_atomic_load_acquire_64(&g_last_timeout_check);
    if (time < static_cast<u64>(last) + 1000 ||
        !retro_atomic_cas_64(&g_last_timeout_check, last, static_cast<s64>(time))) {
        return;
    }

    for (int i = 0; i < g_contexts.count; i++) {
        auto* context = static_cast<OrbisNpWebApiContext*>(obj_table_hold(&g_contexts, i));
        if (context == nullptr) {
            continue;
        }
        checkContextTimeout(context);
        releaseContext(context);
    }
}

s32 deleteContext(s32 libCtxId) {
    auto* context = findAndValidateContext(libCtxId, 1);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }
    // Freed with all it holds once the last holder lets go
    obj_table_remove(&g_contexts, &context->base);
    releaseContext(context);
    return ORBIS_OK;
}

s32 terminateContext(s32 libCtxId) {
    OrbisNpWebApiContext* ctx = findAndValidateContext(libCtxId);
    if (ctx == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    if (g_sdk_ver < Common::ElfInfo::FW_400 && isContextBusy(ctx)) {
        releaseContext(ctx);
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_BUSY;
    }

    for (int i = 0; i < ctx->userContexts.count; i++) {
        auto* user_context =
            static_cast<OrbisNpWebApiUserContext*>(obj_table_peek(&ctx->userContexts, i));
        if (user_context == nullptr || !obj_table_live(&ctx->userContexts, i)) {
            continue;
        }
        s32 result = deleteUserContext(user_context->userCtxId);
        if (result != ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND ||
            g_sdk_ver < Common::ElfInfo::FW_400) {
            releaseContext(ctx);
            return result;
        }
    }

    if (g_sdk_ver >= Common::ElfInfo::FW_400) {
        for (int i = 0; i < ctx->handles.count; i++) {
            auto* handle = static_cast<OrbisNpWebApiHandle*>(obj_table_hold(&ctx->handles, i));
            if (handle == nullptr) {
                continue;
            }
            const s32 handle_id = handle->handleId;
            releaseHandle(ctx, handle);
            abortHandle(libCtxId, handle_id);
        }
        if (retro_atomic_exchange_int(&ctx->terminated, 1) != 0) {
            releaseContext(ctx);
            return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
        }
        // Until no other call is in it and no handle is held
        WaitUntil([ctx] { return !isContextBusy(ctx) && !areContextHandlesBusy(ctx); });
    }

    releaseContext(ctx);
    return deleteContext(libCtxId);
}

OrbisNpWebApiUserContext* findUserContextByUserId(OrbisNpWebApiContext* context,
                                                  OrbisUserServiceUserId userId) {
    if (userId == ORBIS_USER_SERVICE_USER_ID_INVALID) {
        return nullptr;
    }

    for (int i = 0; i < context->userContexts.count; i++) {
        auto* user_context =
            static_cast<OrbisNpWebApiUserContext*>(obj_table_hold(&context->userContexts, i));
        if (user_context == nullptr) {
            continue;
        }
        if (user_context->userId == userId) {
            return user_context;
        }
        releaseUserContext(user_context);
    }
    return nullptr;
}

OrbisNpWebApiUserContext* findUserContext(OrbisNpWebApiContext* context, s32 titleUserCtxId) {
    if ((titleUserCtxId >> 0x10) != context->libCtxId) {
        return nullptr;
    }
    auto* user_context = reinterpret_cast<OrbisNpWebApiUserContext*>(
        obj_table_find(&context->userContexts, titleUserCtxId, 6));
    if (user_context == nullptr) {
        return nullptr;
    }
    if (retro_atomic_load_acquire_int(&user_context->deleted) != 0) {
        releaseUserContext(user_context);
        return nullptr;
    }
    return user_context;
}

s32 createUserContextWithOnlineId(s32 libCtxId, OrbisNpOnlineId* onlineId) {
    LOG_WARNING(Lib_NpWebApi, "called libCtxId = %d", libCtxId);

    OrbisUserServiceUserId user_id = 0;
    sceUserServiceGetInitialUser(&user_id);
    return createUserContext(libCtxId, user_id);
}

s32 createUserContext(s32 libCtxId, OrbisUserServiceUserId userId) {
    LOG_INFO(Lib_NpWebApi, "libCtxId = %d, userId = %d", libCtxId, userId);
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContextByUserId(context, userId);
    if (user_context != nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_ALREADY_EXIST;
    }

    user_context = new OrbisNpWebApiUserContext{};
    user_context->base.destroy = DestroyUserContext;
    user_context->parentContext = context;
    user_context->userId = userId;
    retro_atomic_int_init(&user_context->deleted, 0);
    if (!obj_table_init(&user_context->requests, MaxRequests) ||
        !obj_table_init(&user_context->pushEventCallbacks, MaxCallbacks) ||
        !obj_table_init(&user_context->servicePushEventCallbacks, MaxCallbacks) ||
        !obj_table_init(&user_context->extendedPushEventCallbacks, MaxCallbacks) ||
        !obj_table_put(&context->userContexts, &user_context->base, 6, 16,
                       static_cast<s64>(libCtxId) << 0x10)) {
        DestroyUserContext(&user_context->base);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_MAX;
    }
    user_context->userCtxId = static_cast<s32>(user_context->base.id);

    // TODO: Internal structs related to libSceHttp use are initialized here.
    releaseContext(context);
    return user_context->userCtxId;
}

s32 registerNotificationCallback(s32 titleUserCtxId, OrbisNpWebApiNotificationCallback cbFunc,
                                 void* pUserArg) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    retro_atomic_store_release_ptr(&user_context->pNotificationCallbackUserArgs, pUserArg);
    retro_atomic_store_release_ptr(&user_context->notificationCallbackFunction,
                                   reinterpret_cast<void*>(cbFunc));
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 unregisterNotificationCallback(s32 titleUserCtxId) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    retro_atomic_store_release_ptr(&user_context->notificationCallbackFunction, nullptr);
    retro_atomic_store_release_ptr(&user_context->pNotificationCallbackUserArgs, nullptr);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

bool isUserContextBusy(OrbisNpWebApiUserContext* userContext) {
    return obj_table_users(&userContext->parentContext->userContexts, userContext->base.index) > 1;
}

bool areUserContextRequestsBusy(OrbisNpWebApiUserContext* userContext) {
    for (int i = 0; i < userContext->requests.count; i++) {
        if (obj_table_live(&userContext->requests, i) &&
            obj_table_users(&userContext->requests, i) > 0) {
            return true;
        }
    }
    return false;
}

void releaseUserContext(OrbisNpWebApiUserContext* userContext) {
    obj_table_put_back(&userContext->parentContext->userContexts, &userContext->base);
}

void checkUserContextTimeout(OrbisNpWebApiUserContext* userContext) {
    for (int i = 0; i < userContext->requests.count; i++) {
        auto* request =
            static_cast<OrbisNpWebApiRequest*>(obj_table_hold(&userContext->requests, i));
        if (request == nullptr) {
            continue;
        }
        const s64 request_id = request->requestId;
        const u64 end = static_cast<u64>(retro_atomic_load_acquire_64(&request->requestEndTime));
        const bool expired =
            end != 0 && end < NowUs() && retro_atomic_exchange_int(&request->timedOut, 1) == 0;
        releaseRequest(request);
        if (expired) {
            abortRequest(request_id);
        }
    }
}

s32 deleteUserContext(s32 titleUserCtxId) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (g_sdk_ver < Common::ElfInfo::FW_400) {
        if (isUserContextBusy(user_context) || areUserContextRequestsBusy(user_context)) {
            releaseUserContext(user_context);
            releaseContext(context);
            return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_BUSY;
        }
    } else {
        for (int i = 0; i < user_context->requests.count; i++) {
            auto* request =
                static_cast<OrbisNpWebApiRequest*>(obj_table_hold(&user_context->requests, i));
            if (request == nullptr) {
                continue;
            }
            abortRequestInternal(context, user_context, request);
            releaseRequest(request);
        }

        if (retro_atomic_exchange_int(&user_context->deleted, 1) != 0) {
            releaseUserContext(user_context);
            releaseContext(context);
            return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
        }

        // Until no other call is in it and none of its requests is held
        WaitUntil([user_context] {
            return !isUserContextBusy(user_context) && !areUserContextRequestsBusy(user_context);
        });
    }

    // Freed with its requests and callbacks once the last holder lets go
    obj_table_remove(&context->userContexts, &user_context->base);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

static void DestroyRequest(obj_head_t* object) {
    delete reinterpret_cast<OrbisNpWebApiRequest*>(object);
}

s32 createRequest(s32 titleUserCtxId, const char* pApiGroup, const char* pPath,
                  OrbisNpWebApiHttpMethod method,
                  const OrbisNpWebApiContentParameter* pContentParameter,
                  const OrbisNpWebApiIntCreateRequestExtraArgs* pInternalArgs, s64* pRequestId,
                  bool isMultipart) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (g_sdk_ver >= Common::ElfInfo::FW_400 &&
        retro_atomic_load_acquire_int(&user_context->deleted) != 0) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (pInternalArgs != nullptr) {
        ASSERT_MSG(pInternalArgs->unk_0 == nullptr && pInternalArgs->unk_1 == nullptr &&
                       pInternalArgs->unk_2 == nullptr,
                   "Internal arguments for requests not supported");
    }

    auto* request = new OrbisNpWebApiRequest{};
    request->base.destroy = DestroyRequest;
    request->parentContext = context;
    request->parentUserContext = user_context;
    request->userMethod = method;
    request->multipart = isMultipart;
    retro_atomic_int_init(&request->aborted, 0);
    retro_atomic_int_init(&request->requestTimeout, 0);
    retro_atomic_64_init(&request->requestEndTime, 0);
    retro_atomic_int_init(&request->timedOut, 0);

    if (pApiGroup != nullptr) {
        request->userApiGroup = std::string(pApiGroup);
    }

    if (pPath != nullptr) {
        request->userPath = std::string(pPath);
    }

    if (pContentParameter != nullptr) {
        request->userContentLength = pContentParameter->contentLength;
        if (pContentParameter->pContentType != nullptr) {
            request->userContentType = std::string(pContentParameter->pContentType);
        }
    }

    // Its id: the user context's above 32 bits, its slot and generation below
    if (!obj_table_put(&user_context->requests, &request->base, 8, 31,
                       static_cast<s64>(titleUserCtxId) << 0x20)) {
        delete request;
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    request->requestId = request->base.id;

    if (pRequestId != nullptr) {
        *pRequestId = request->requestId;
    }

    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

OrbisNpWebApiRequest* findRequest(OrbisNpWebApiUserContext* userContext, s64 requestId) {
    if (static_cast<s32>(requestId >> 0x20) != userContext->userCtxId) {
        return nullptr;
    }
    return reinterpret_cast<OrbisNpWebApiRequest*>(
        obj_table_find(&userContext->requests, requestId, 8));
}

OrbisNpWebApiRequest* findRequestAndMarkBusy(OrbisNpWebApiUserContext* userContext, s64 requestId) {
    return findRequest(userContext, requestId);
}

bool isRequestBusy(OrbisNpWebApiRequest* request) {
    return obj_table_users(&request->parentUserContext->requests, request->base.index) > 1;
}

s32 setRequestTimeout(s64 requestId, u32 timeout) {
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiRequest* request = findRequestAndMarkBusy(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    retro_atomic_store_release_int(&request->requestTimeout, static_cast<int>(timeout));

    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

void startRequestTimer(OrbisNpWebApiRequest* request) {
    const u32 timeout = static_cast<u32>(retro_atomic_load_acquire_int(&request->requestTimeout));
    if (timeout != 0) {
        (void)retro_atomic_cas_64(&request->requestEndTime, 0, static_cast<s64>(NowUs() + timeout));
    }
}

void checkRequestTimeout(OrbisNpWebApiRequest* request) {
    const u64 end = static_cast<u64>(retro_atomic_load_acquire_64(&request->requestEndTime));
    if (end != 0 && end < NowUs() && retro_atomic_exchange_int(&request->timedOut, 1) == 0) {
        abortRequest(request->requestId);
    }
}

s32 setMultipartContentType(s64 requestId, const char* pTypeName, const char* pBoundary) {
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiRequest* request = findRequest(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }
    request->multipartContentType = (pTypeName != nullptr) ? pTypeName : "";
    request->multipartBoundary = (pBoundary != nullptr) ? pBoundary : "";
    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 addMultipartPart(s64 requestId, const OrbisNpWebApiMultipartPartParameter* pParam,
                     s32* pIndex) {
    if (pParam == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    }
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiRequest* request = findRequest(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    // The library supplies Content-Length itself (from contentLength), so skip an app-provided one.
    const auto isContentLength = [](const char* name) {
        const char* t = "content-length";
        for (; *name != '\0' && *t != '\0'; ++name, ++t) {
            char c = *name;
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
            if (c != *t) {
                return false;
            }
        }
        return *name == '\0' && *t == '\0';
    };

    OrbisNpWebApiRequest::MultipartPart part;
    part.contentLength = pParam->contentLength;
    std::string headers;
    for (u64 i = 0; i < pParam->headerNum; ++i) {
        const OrbisNpWebApiHttpHeader& h = pParam->pHeaders[i];
        if (h.pName == nullptr || h.pValue == nullptr || isContentLength(h.pName)) {
            continue;
        }
        headers += h.pName;
        headers += ": ";
        headers += h.pValue;
        headers += "\r\n";
    }
    headers += "Content-Length: ";
    headers += std::to_string(pParam->contentLength);
    headers += "\r\n\r\n";
    part.rawHeaders = std::move(headers);

    request->multipartParts.push_back(std::move(part));
    if (pIndex != nullptr) {
        *pIndex = static_cast<s32>(request->multipartParts.size()); // 1-based part index
    }
    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 sendRequest(s64 requestId, s32 partIndex, const void* pData, u64 dataSize, s8 flag,
                OrbisNpWebApiResponseInformationOption* pRespInfoOption) {
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiRequest* request = findRequestAndMarkBusy(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    startRequestTimer(request);

    // Multipart requests accumulate each part payload across sendMultipartRequest() calls,the
    // full multipart/mixed body is framed and transmitted once every declared part is complete.
    std::string multipartBody;
    const void* sendData = pData;
    u64 sendSize = dataSize;
    if (request->multipart) {
        if (partIndex < 1 || partIndex > static_cast<s32>(request->multipartParts.size())) {
            releaseRequest(request);
            releaseUserContext(user_context);
            releaseContext(context);
            return ORBIS_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
        }
        auto& part = request->multipartParts[partIndex - 1];
        if (pData != nullptr && dataSize > 0) {
            part.data.append(static_cast<const char*>(pData), dataSize);
        }
        part.sentSize += dataSize;

        bool allComplete = true;
        for (const auto& p : request->multipartParts) {
            if (p.sentSize < p.contentLength) {
                allComplete = false;
                break;
            }
        }
        if (!allComplete) {
            // Further part payloads still expected; nothing to transmit yet.
            releaseRequest(request);
            releaseUserContext(user_context);
            releaseContext(context);
            return ORBIS_OK;
        }

        std::string boundary = request->multipartBoundary;
        if (boundary.empty()) {
            // Fallback boundary derived from the unique request id.
            static const char kHex[] = "0123456789abcdef";
            boundary = "----shadPS4Boundary";
            const u64 v = static_cast<u64>(requestId);
            for (int shift = 60; shift >= 0; shift -= 4) {
                boundary += kHex[(v >> shift) & 0xf];
            }
        }
        const std::string typeName = request->multipartContentType.empty()
                                         ? std::string("multipart/mixed")
                                         : request->multipartContentType;
        for (const auto& p : request->multipartParts) {
            multipartBody += "--";
            multipartBody += boundary;
            multipartBody += "\r\n";
            multipartBody += p.rawHeaders; // header lines + blank line separating headers/body
            multipartBody += p.data;
            multipartBody += "\r\n";
        }
        multipartBody += "--";
        multipartBody += boundary;
        multipartBody += "--\r\n";

        request->userContentType = typeName + "; boundary=" + boundary;
        request->userContentLength = multipartBody.size();
        sendData = multipartBody.data();
        sendSize = multipartBody.size();
    }

    if (g_sdk_ver >= Common::ElfInfo::FW_250 && !request->sent) {
        request->sent = true;
    }

    if (retro_atomic_load_acquire_int(&request->timedOut) == 0 &&
        retro_atomic_load_acquire_int(&request->aborted) != 0) {
        releaseRequest(request);
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_ABORTED;
    }

    // Stubbing sceNpManagerIntGetSigninState call with a config check.
    if (!emu_settings_is_shadnet_enabled()) {
        releaseRequest(request);
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
    }

    if (request->http_request_id == 0) {
        std::string base_url = emu_settings_get_shadnet_webapi_server();
        // sceHttpCreateConnectionWithURL expects a template id, not the raw libhttp
        // context id that NpWebApi was initialized with. Create a template from the
        // context first, then open the connection against it.
        const s32 tmpl_id = Libraries::Http::sceHttpCreateTemplate(
            context->libHttpCtxId, "libhttp", /*httpVer=*/2, /*isAutoProxyConf=*/0);
        if (tmpl_id < 0) {
            LOG_ERROR(Lib_NpWebApi, "sceHttpCreateTemplate failed: 0x%x", tmpl_id);
            releaseRequest(request);
            releaseUserContext(user_context);
            releaseContext(context);
            return tmpl_id;
        }
        request->http_template_id = tmpl_id;
        const int conn_id = Libraries::Http::sceHttpCreateConnectionWithURL(
            tmpl_id, base_url.c_str(), /*enableKeepalive=*/true);
        if (conn_id < 0) {
            LOG_ERROR(Lib_NpWebApi, "sceHttpCreateConnectionWithURL failed: 0x%x", conn_id);
            Libraries::Http::sceHttpDeleteTemplate(tmpl_id);
            request->http_template_id = 0;
            releaseRequest(request);
            releaseUserContext(user_context);
            releaseContext(context);
            return conn_id;
        }
        request->http_connection_id = conn_id;

        // Convert the WebAPI method enum to libhttp's SceHttpMethods
        // enum. The two enums have different orderings:
        //
        //   OrbisNpWebApiHttpMethod: GET=0, POST=1, PUT=2, DELETE=3, PATCH=4
        //   SceHttpMethods:          GET=0, POST=1, HEAD=2, OPTIONS=3,
        //                            PUT=4, DELETE=5, TRACE=6, CONNECT=7
        s32 sceMethod;
        switch (request->userMethod) {
        case ORBIS_NP_WEBAPI_HTTP_METHOD_GET:
            sceMethod = 0;
            break;
        case ORBIS_NP_WEBAPI_HTTP_METHOD_POST:
            sceMethod = 1;
            break;
        case ORBIS_NP_WEBAPI_HTTP_METHOD_PUT:
            sceMethod = 4;
            break;
        case ORBIS_NP_WEBAPI_HTTP_METHOD_DELETE:
            sceMethod = 5;
            break;
        case ORBIS_NP_WEBAPI_HTTP_METHOD_PATCH:
            sceMethod = 8; // out-of-band PATCH marker recognised by libhttp
            break;
        default:
            LOG_ERROR(Lib_NpWebApi, "unknown method enum value %d",
                      static_cast<int>(request->userMethod));
            releaseRequest(request);
            releaseUserContext(user_context);
            releaseContext(context);
            return ORBIS_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
        }
        const std::string full_url = base_url + request->userPath;
        const int req_id = Libraries::Http::sceHttpCreateRequestWithURL(
            conn_id, sceMethod, full_url.c_str(), request->userContentLength);
        if (req_id < 0) {
            LOG_ERROR(Lib_NpWebApi, "sceHttpCreateRequestWithURL failed: 0x%x", req_id);
            Libraries::Http::sceHttpDeleteConnection(conn_id);
            request->http_connection_id = 0;
            Libraries::Http::sceHttpDeleteTemplate(tmpl_id);
            request->http_template_id = 0;
            releaseRequest(request);
            releaseUserContext(user_context);
            releaseContext(context);
            return req_id;
        }
        request->http_request_id = req_id;

        // Add Content-Type if the caller specified one.
        // adds Accept-Encoding, User-Agent, OAuth Authorization , etc ....
        // TODO check if we need to add them
        if (!request->userContentType.empty()) {
            Libraries::Http::sceHttpAddRequestHeader(req_id, "Content-Type",
                                                     request->userContentType.c_str(), /*mode=*/0);
        }

        const std::string bearer = NpHandler::GetInstance().GetBearerToken(user_context->userId);
        if (!bearer.empty()) {
            const std::string auth_value = "Bearer " + bearer;
            Libraries::Http::sceHttpAddRequestHeader(req_id, "Authorization", auth_value.c_str(),
                                                     /*mode=*/0);
        } else {
            LOG_WARNING(Lib_NpWebApi,
                        "no bearer token for user_id=%d; request to '%s' will "
                        "be unauthenticated (expect 401 from server)",
                        user_context->userId, (request->userPath).c_str());
        }

        // Replay app-supplied headers. Content-Type is already emitted above from the
        // ContentParameter (userContentType),skip a duplicate app-supplied Content-Type so the
        // request never carries two
        const auto isContentType = [](const std::string& name) {
            constexpr std::string_view target = "content-type";
            if (name.size() != target.size()) {
                return false;
            }
            for (size_t i = 0; i < name.size(); ++i) {
                char c = name[i];
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
                if (c != target[i]) {
                    return false;
                }
            }
            return true;
        };
        const bool haveContentType = !request->userContentType.empty();
        for (const auto& [hname, hvalue] : request->userHeaders) {
            if (haveContentType && isContentType(hname)) {
                continue;
            }
            Libraries::Http::sceHttpAddRequestHeader(req_id, hname.c_str(), hvalue.c_str(),
                                                     /*mode=*/0);
        }
    }

    setRequestState(request, 4);

    const s32 send_err =
        Libraries::Http::sceHttpSendRequest(request->http_request_id, sendData, sendSize);
    if (send_err < 0) {
        LOG_ERROR(Lib_NpWebApi, "sceHttpSendRequest failed: 0x%x", send_err);
        releaseRequest(request);
        releaseUserContext(user_context);
        releaseContext(context);
        return send_err;
    }

    LOG_INFO(Lib_NpWebApi, "requestId=0x%" PRIx64 " apiGroup='%s' path='%s' method=%s httpReqId=%d",
             requestId, (request->userApiGroup).c_str(), (request->userPath).c_str(),
             enum_name(&enum_names_Libraries_Np_NpWebApi_OrbisNpWebApiHttpMethod,
                       static_cast<s64>(request->userMethod)),
             request->http_request_id);

    s32 sendResult = ORBIS_OK;
    if (flag != 0) {
        s32 status = 0;
        if (Libraries::Http::sceHttpGetStatusCode(request->http_request_id, &status) >= 0) {
            if (pRespInfoOption != nullptr) {
                pRespInfoOption->httpStatus = status;
            }
            if (status >= 400) {
                std::string errBody;
                char buf[256];
                for (;;) {
                    const s32 n = Libraries::Http::sceHttpReadData(request->http_request_id, buf,
                                                                   sizeof(buf));
                    if (n <= 0)
                        break;
                    errBody.append(buf, static_cast<size_t>(n));
                    if (errBody.size() > 64u * 1024u)
                        break; // sanity cap on a runaway error body
                }
                if (pRespInfoOption != nullptr) {
                    pRespInfoOption->responseDataSize = errBody.size();
                    char* const dst = pRespInfoOption->pErrorObject;
                    const u64 cap = pRespInfoOption->errorObjectSize;
                    if (dst != nullptr && cap > 0) {
                        const u64 n = std::min<u64>(errBody.size(), cap - 1);
                        if (n > 0)
                            std::memcpy(dst, errBody.data(), n);
                        dst[n] = '\0';
                    }
                }
                const s32 npErr = captureWebApiError(errBody.data(), errBody.size());
                if (npErr > 0 && npErr < 0xc00000) {
                    sendResult = static_cast<s32>(0x82000000u | static_cast<u32>(npErr));
                } else if (status >= 100 && status < 600) {
                    sendResult = static_cast<s32>(0x82f00000u | static_cast<u32>(status));
                } else {
                    sendResult = static_cast<s32>(0x82ffffffu);
                }
            }
        }
    }

    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);
    return sendResult;
}

s32 abortRequestInternal(OrbisNpWebApiContext* context, OrbisNpWebApiUserContext* userContext,
                         OrbisNpWebApiRequest* request) {
    if (context == nullptr || userContext == nullptr || request == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    }

    (void)retro_atomic_exchange_int(&request->aborted, 1);

    // TODO: Should also abort any Np requests and Http requests tied to this request.

    return ORBIS_OK;
}

s32 abortRequest(s64 requestId) {
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiRequest* request = findRequest(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    s32 result = abortRequestInternal(context, user_context, request);

    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);
    return result;
}

void releaseRequest(OrbisNpWebApiRequest* request) {
    obj_table_put_back(&request->parentUserContext->requests, &request->base);
}

s32 deleteRequest(s64 requestId) {
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiRequest* request = findRequestAndMarkBusy(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    if (g_sdk_ver < Common::ElfInfo::FW_400 && isRequestBusy(request)) {
        releaseRequest(request);
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_BUSY;
    }

    abortRequestInternal(context, user_context, request);
    // Until no other call holds it, on the game's time
    WaitUntil([request] { return !isRequestBusy(request); });

    if (request->http_request_id != 0) {
        Libraries::Http::sceHttpDeleteRequest(request->http_request_id);
        request->http_request_id = 0;
    }
    if (request->http_connection_id != 0) {
        Libraries::Http::sceHttpDeleteConnection(request->http_connection_id);
        request->http_connection_id = 0;
    }
    if (request->http_template_id != 0) {
        Libraries::Http::sceHttpDeleteTemplate(request->http_template_id);
        request->http_template_id = 0;
    }
    obj_table_remove(&user_context->requests, &request->base);
    releaseRequest(request);

    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 createHandleInternal(OrbisNpWebApiContext* context) {
    auto* handle = new OrbisNpWebApiHandle{};
    handle->base.destroy = DestroyAs<OrbisNpWebApiHandle>;
    retro_atomic_int_init(&handle->aborted, 0);
    retro_atomic_int_init(&handle->deleted, 0);
    retro_atomic_int_init(&handle->handleTimeout, 0);
    retro_atomic_64_init(&handle->handleEndTime, 0);
    retro_atomic_int_init(&handle->timedOut, 0);
    if (!obj_table_put(&context->handles, &handle->base, 8, 31, 0)) {
        delete handle;
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    handle->handleId = static_cast<s32>(handle->base.id);
    return handle->handleId;
}

s32 createHandle(s32 libCtxId) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = createHandleInternal(context);
    releaseContext(context);
    return result;
}

s32 setHandleTimeoutInternal(OrbisNpWebApiContext* context, s32 handleId, u32 timeout) {
    // A handle's timer is there from SDK 3.00
    OrbisNpWebApiHandle* handle = nullptr;
    if (g_sdk_ver < Common::ElfInfo::FW_300 || getHandle(context, handleId, &handle) != ORBIS_OK) {
        return ORBIS_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
    }
    retro_atomic_store_release_int(&handle->handleTimeout, static_cast<int>(timeout));
    releaseHandle(context, handle);
    return ORBIS_OK;
}

s32 setHandleTimeout(s32 libCtxId, s32 handleId, u32 timeout) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = setHandleTimeoutInternal(context, handleId, timeout);
    releaseContext(context);
    return result;
}

void startHandleTimer(OrbisNpWebApiContext* context, s32 handleId) {
    OrbisNpWebApiHandle* handle = nullptr;
    if (g_sdk_ver < Common::ElfInfo::FW_300 || getHandle(context, handleId, &handle) != ORBIS_OK) {
        return;
    }
    const u32 timeout = static_cast<u32>(retro_atomic_load_acquire_int(&handle->handleTimeout));
    if (timeout != 0) {
        retro_atomic_store_release_64(&handle->handleEndTime, static_cast<s64>(NowUs() + timeout));
    }
    releaseHandle(context, handle);
}

void releaseHandle(OrbisNpWebApiContext* context, OrbisNpWebApiHandle* handle) {
    if (handle != nullptr) {
        obj_table_put_back(&context->handles, &handle->base);
    }
}

s32 getHandle(OrbisNpWebApiContext* context, s32 handleId, OrbisNpWebApiHandle** handleOut) {
    auto* handle =
        reinterpret_cast<OrbisNpWebApiHandle*>(obj_table_find(&context->handles, handleId, 8));
    if (handle == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
    }
    if (handleOut != nullptr) {
        *handleOut = handle;
    } else {
        releaseHandle(context, handle);
    }
    return ORBIS_OK;
}

s32 abortHandle(s32 libCtxId, s32 handleId) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiHandle* handle;
    s32 result = getHandle(context, handleId, &handle);
    if (result == ORBIS_OK) {
        (void)retro_atomic_exchange_int(&handle->aborted, 1);
        // TODO: sceNpAsmClientAbortRequest call
        releaseHandle(context, handle);
    }

    releaseContext(context);
    return result;
}

s32 deleteHandleInternal(OrbisNpWebApiContext* context, s32 handleId) {
    OrbisNpWebApiHandle* handle = nullptr;
    if (getHandle(context, handleId, &handle) != ORBIS_OK) {
        return ORBIS_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
    }

    if (g_sdk_ver >= Common::ElfInfo::FW_400) {
        if (retro_atomic_exchange_int(&handle->deleted, 1) != 0) {
            releaseHandle(context, handle);
            return ORBIS_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
        }
        abortHandle(context->libCtxId, handleId);
        // Until this call is the only one holding it, on the game's time
        WaitUntil([context, handle] {
            return obj_table_users(&context->handles, handle->base.index) <= 1;
        });
    } else if (obj_table_users(&context->handles, handle->base.index) > 1) {
        releaseHandle(context, handle);
        return ORBIS_NP_WEBAPI_ERROR_HANDLE_BUSY;
    }

    obj_table_remove(&context->handles, &handle->base);
    releaseHandle(context, handle);
    return ORBIS_OK;
}

s32 deleteHandle(s32 libCtxId, s32 handleId) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = deleteHandleInternal(context, handleId);
    releaseContext(context);
    return result;
}

s32 createPushEventFilterInternal(OrbisNpWebApiContext* context,
                                  const OrbisNpWebApiPushEventFilterParameter* pFilterParam,
                                  u64 filterParamNum) {
    // Made whole, then put in its table, where others may find it
    auto* filter = new OrbisNpWebApiPushEventFilter{};
    filter->base.destroy = DestroyAs<OrbisNpWebApiPushEventFilter>;
    filter->parentContext = context;

    if (pFilterParam != nullptr && filterParamNum != 0) {
        for (u64 param_idx = 0; param_idx < filterParamNum; param_idx++) {
            OrbisNpWebApiPushEventFilterParameter copy = OrbisNpWebApiPushEventFilterParameter{};
            memcpy(&copy, &pFilterParam[param_idx], sizeof(OrbisNpWebApiPushEventFilterParameter));
            LOG_INFO(Lib_NpWebApi, "  filterParam[%" PRIu64 "] dataType='%s'", param_idx,
                     copy.dataType.val);
            filter->filterParams.emplace_back(copy);
        }
    }
    if (!obj_table_put(&context->pushEventFilters, &filter->base, 8, 31, 0)) {
        delete filter;
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    const s32 filterId = static_cast<s32>(filter->base.id);
    filter->filterId = filterId;
    LOG_INFO(Lib_NpWebApi, "filterId=%d dataTypeParams=%" PRIu64, filterId, filterParamNum);
    return filterId;
}

s32 createPushEventFilter(s32 libCtxId, const OrbisNpWebApiPushEventFilterParameter* pFilterParam,
                          u64 filterParamNum) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = createPushEventFilterInternal(context, pFilterParam, filterParamNum);
    releaseContext(context);
    return result;
}

s32 deletePushEventFilterInternal(OrbisNpWebApiContext* context, s32 filterId) {
    obj_head_t* filter = obj_table_find(&context->pushEventFilters, filterId, 8);
    if (filter == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
    }
    obj_table_remove(&context->pushEventFilters, filter);
    obj_table_put_back(&context->pushEventFilters, filter);
    return ORBIS_OK;
}

s32 deletePushEventFilter(s32 libCtxId, s32 filterId) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = deletePushEventFilterInternal(context, filterId);
    releaseContext(context);
    return result;
}

s32 registerPushEventCallbackInternal(OrbisNpWebApiUserContext* userContext, s32 filterId,
                                      OrbisNpWebApiPushEventCallback cbFunc, void* pUserArg) {
    // Made whole, then put in its table, where others may find it
    auto* cb = new OrbisNpWebApiRegisteredPushEventCallback{};
    cb->base.destroy = DestroyAs<OrbisNpWebApiRegisteredPushEventCallback>;
    cb->filterId = filterId;
    cb->cbFunc = cbFunc;
    cb->pUserArg = pUserArg;

    if (!obj_table_put(&userContext->pushEventCallbacks, &cb->base, 8, 31, 0)) {
        delete cb;
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    const s32 cbId = static_cast<s32>(cb->base.id);
    cb->callbackId = cbId;
    return cbId;
}

s32 registerPushEventCallback(s32 titleUserCtxId, s32 filterId,
                              OrbisNpWebApiPushEventCallback cbFunc, void* pUserArg) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (g_sdk_ver >= Common::ElfInfo::FW_250 && !HasObject(&context->pushEventFilters, filterId)) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
    }

    s32 result = registerPushEventCallbackInternal(user_context, filterId, cbFunc, pUserArg);
    releaseUserContext(user_context);
    releaseContext(context);
    return result;
}

s32 unregisterPushEventCallback(s32 titleUserCtxId, s32 callbackId) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    obj_head_t* callback = obj_table_find(&user_context->pushEventCallbacks, callbackId, 8);
    if (callback == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_PUSH_EVENT_CALLBACK_NOT_FOUND;
    }

    obj_table_remove(&user_context->pushEventCallbacks, callback);
    obj_table_put_back(&user_context->pushEventCallbacks, callback);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 createServicePushEventFilterInternal(
    OrbisNpWebApiContext* context, s32 handleId, const char* pNpServiceName,
    OrbisNpServiceLabel npServiceLabel,
    const OrbisNpWebApiServicePushEventFilterParameter* pFilterParam, u64 filterParamNum) {
    OrbisNpWebApiHandle* handle = nullptr;
    if (getHandle(context, handleId, &handle) != ORBIS_OK) {
        return ORBIS_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
    }

    if (pNpServiceName != nullptr && !emu_settings_is_shadnet_enabled()) {
        // Seems sceNpManagerIntGetUserList fails?
        LOG_DEBUG(Lib_NpWebApi, "Cannot create service push event while shadNet is disabled");
        releaseHandle(context, handle);
        return ORBIS_NP_WEBAPI_ERROR_SIGNED_IN_USER_NOT_FOUND;
    }

    // Made whole, then put in its table, where others may find it
    auto* filter = new OrbisNpWebApiServicePushEventFilter{};
    filter->base.destroy = DestroyAs<OrbisNpWebApiServicePushEventFilter>;
    filter->parentContext = context;

    if (pNpServiceName == nullptr) {
        filter->internal = true;
    } else {
        filter->npServiceName = std::string(pNpServiceName);
    }

    filter->npServiceLabel = npServiceLabel;
    if (pFilterParam != nullptr && filterParamNum != 0) {
        for (u64 param_idx = 0; param_idx < filterParamNum; param_idx++) {
            OrbisNpWebApiServicePushEventFilterParameter copy =
                OrbisNpWebApiServicePushEventFilterParameter{};
            memcpy(&copy, &pFilterParam[param_idx],
                   sizeof(OrbisNpWebApiServicePushEventFilterParameter));
            LOG_INFO(Lib_NpWebApi, "  filterParam[%" PRIu64 "] data_type='%s'", param_idx,
                     copy.dataType.val);
            filter->filterParams.emplace_back(copy);
        }
    }

    releaseHandle(context, handle);
    if (!obj_table_put(&context->servicePushEventFilters, &filter->base, 8, 31, 0)) {
        delete filter;
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    const s32 filterId = static_cast<s32>(filter->base.id);
    filter->filterId = filterId;
    LOG_INFO(Lib_NpWebApi, "filterId=%d dataTypeParams=%" PRIu64, filterId, filterParamNum);
    return filterId;
}

s32 createServicePushEventFilter(s32 libCtxId, s32 handleId, const char* pNpServiceName,
                                 OrbisNpServiceLabel npServiceLabel,
                                 const OrbisNpWebApiServicePushEventFilterParameter* pFilterParam,
                                 u64 filterParamNum) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    startHandleTimer(context, handleId);
    s32 result = createServicePushEventFilterInternal(context, handleId, pNpServiceName,
                                                      npServiceLabel, pFilterParam, filterParamNum);
    releaseContext(context);
    return result;
}

s32 deleteServicePushEventFilterInternal(OrbisNpWebApiContext* context, s32 filterId) {
    obj_head_t* filter = obj_table_find(&context->servicePushEventFilters, filterId, 8);
    if (filter == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_FILTER_NOT_FOUND;
    }
    obj_table_remove(&context->servicePushEventFilters, filter);
    obj_table_put_back(&context->servicePushEventFilters, filter);
    return ORBIS_OK;
}

s32 deleteServicePushEventFilter(s32 libCtxId, s32 filterId) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = deleteServicePushEventFilterInternal(context, filterId);
    releaseContext(context);
    return result;
}

s32 registerServicePushEventCallbackInternal(
    OrbisNpWebApiUserContext* userContext, s32 filterId,
    OrbisNpWebApiServicePushEventCallback cbFunc,
    OrbisNpWebApiInternalServicePushEventCallback intCbFunc,
    OrbisNpWebApiInternalServicePushEventCallbackA intCbFuncA, void* pUserArg) {
    if (cbFunc == nullptr && intCbFunc == nullptr && intCbFuncA == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    }

    // Made whole, then put in its table, where others may find it
    auto* cb = new OrbisNpWebApiRegisteredServicePushEventCallback{};
    cb->base.destroy = DestroyAs<OrbisNpWebApiRegisteredServicePushEventCallback>;
    cb->filterId = filterId;
    cb->cbFunc = cbFunc;
    cb->internalCbFunc = intCbFunc;
    cb->internalCbFuncA = intCbFuncA;
    cb->pUserArg = pUserArg;

    if (!obj_table_put(&userContext->servicePushEventCallbacks, &cb->base, 8, 31, 0)) {
        delete cb;
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    const s32 cbId = static_cast<s32>(cb->base.id);
    cb->callbackId = cbId;
    return cbId;
}

s32 registerServicePushEventCallback(s32 titleUserCtxId, s32 filterId,
                                     OrbisNpWebApiServicePushEventCallback cbFunc,
                                     OrbisNpWebApiInternalServicePushEventCallback intCbFunc,
                                     OrbisNpWebApiInternalServicePushEventCallbackA intCbFuncA,
                                     void* pUserArg) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (g_sdk_ver >= Common::ElfInfo::FW_250 &&
        !HasObject(&context->servicePushEventFilters, filterId)) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_FILTER_NOT_FOUND;
    }

    s32 result = registerServicePushEventCallbackInternal(user_context, filterId, cbFunc, intCbFunc,
                                                          intCbFuncA, pUserArg);
    releaseUserContext(user_context);
    releaseContext(context);
    return result;
}

s32 unregisterServicePushEventCallback(s32 titleUserCtxId, s32 callbackId) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    obj_head_t* callback = obj_table_find(&user_context->servicePushEventCallbacks, callbackId, 8);
    if (callback == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_CALLBACK_NOT_FOUND;
    }

    obj_table_remove(&user_context->servicePushEventCallbacks, callback);
    obj_table_put_back(&user_context->servicePushEventCallbacks, callback);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 createExtendedPushEventFilterInternal(
    OrbisNpWebApiContext* context, s32 handleId, const char* pNpServiceName,
    OrbisNpServiceLabel npServiceLabel,
    const OrbisNpWebApiExtdPushEventFilterParameter* pFilterParam, u64 filterParamNum,
    bool internal) {
    OrbisNpWebApiHandle* handle = nullptr;
    if (getHandle(context, handleId, &handle) != ORBIS_OK) {
        return ORBIS_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
    }

    if (pNpServiceName != nullptr && !emu_settings_is_shadnet_enabled()) {
        // Seems sceNpManagerIntGetUserList fails?
        LOG_DEBUG(Lib_NpWebApi, "Cannot create extended push event while shadNet is disabled");
        releaseHandle(context, handle);
        return ORBIS_NP_WEBAPI_ERROR_SIGNED_IN_USER_NOT_FOUND;
    }

    // Made whole, then put in its table, where others may find it
    auto* filter = new OrbisNpWebApiExtendedPushEventFilter{};
    filter->base.destroy = DestroyAs<OrbisNpWebApiExtendedPushEventFilter>;
    filter->internal = internal;
    filter->parentContext = context;

    if (pNpServiceName == nullptr) {
        npServiceLabel = ORBIS_NP_INVALID_SERVICE_LABEL;
    } else {
        filter->npServiceName = std::string(pNpServiceName);
    }

    filter->npServiceLabel = npServiceLabel;

    LOG_INFO(Lib_NpWebApi, "service='%s' label=0x%x dataTypeParams=%" PRIu64,
             pNpServiceName ? pNpServiceName : "null", npServiceLabel, filterParamNum);
    if (pFilterParam != nullptr && filterParamNum != 0) {
        for (u64 param_idx = 0; param_idx < filterParamNum; param_idx++) {
            OrbisNpWebApiExtdPushEventFilterParameter copy =
                OrbisNpWebApiExtdPushEventFilterParameter{};
            memcpy(&copy, &pFilterParam[param_idx],
                   sizeof(OrbisNpWebApiExtdPushEventFilterParameter));
            LOG_INFO(Lib_NpWebApi, "  filterParam[%" PRIu64 "] dataType='%s' extdKeys=%" PRIu64,
                     param_idx, copy.dataType.val, copy.extdDataKeyNum); // debug
            std::vector<std::string> keys;
            if (copy.pExtdDataKey != nullptr) {
                for (u64 k = 0; k < copy.extdDataKeyNum; k++) {
                    LOG_INFO(Lib_NpWebApi, "    extdDataKey[%" PRIu64 "]='%s'", k,
                             copy.pExtdDataKey[k].val); // debug
                    keys.emplace_back(
                        copy.pExtdDataKey[k].val,
                        strnlen(copy.pExtdDataKey[k].val, sizeof(copy.pExtdDataKey[k].val)));
                }
            }
            copy.pExtdDataKey = nullptr; // guest memory, see extdDataKeys
            filter->filterParams.emplace_back(copy);
            filter->extdDataKeys.emplace_back(std::move(keys));
            // TODO: Every parameter is registered with an extended data filter through
            // sceNpPushRegisterExtendedDataFilter
        }
    }

    releaseHandle(context, handle);
    if (!obj_table_put(&context->extendedPushEventFilters, &filter->base, 8, 31, 0)) {
        delete filter;
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    const s32 filterId = static_cast<s32>(filter->base.id);
    filter->filterId = filterId;
    LOG_INFO(Lib_NpWebApi, "filterId=%d dataTypeParams=%" PRIu64, filterId, filterParamNum);
    return filterId;
}

s32 createExtendedPushEventFilter(s32 libCtxId, s32 handleId, const char* pNpServiceName,
                                  OrbisNpServiceLabel npServiceLabel,
                                  const OrbisNpWebApiExtdPushEventFilterParameter* pFilterParam,
                                  u64 filterParamNum, bool internal) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    startHandleTimer(context, handleId);
    s32 result = createExtendedPushEventFilterInternal(
        context, handleId, pNpServiceName, npServiceLabel, pFilterParam, filterParamNum, internal);
    releaseContext(context);
    return result;
}

s32 deleteExtendedPushEventFilterInternal(OrbisNpWebApiContext* context, s32 filterId) {
    obj_head_t* filter = obj_table_find(&context->extendedPushEventFilters, filterId, 8);
    if (filter == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_EXTD_PUSH_EVENT_FILTER_NOT_FOUND;
    }
    obj_table_remove(&context->extendedPushEventFilters, filter);
    obj_table_put_back(&context->extendedPushEventFilters, filter);
    return ORBIS_OK;
}

s32 deleteExtendedPushEventFilter(s32 libCtxId, s32 filterId) {
    OrbisNpWebApiContext* context = findAndValidateContext(libCtxId);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    s32 result = deleteExtendedPushEventFilterInternal(context, filterId);
    releaseContext(context);
    return result;
}

s32 registerExtdPushEventCallbackInternal(OrbisNpWebApiUserContext* userContext, s32 filterId,
                                          OrbisNpWebApiExtdPushEventCallback cbFunc,
                                          OrbisNpWebApiExtdPushEventCallbackA cbFuncA,
                                          void* pUserArg) {

    if (cbFunc == nullptr && cbFuncA == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    }
    // Made whole, then put in its table, where others may find it
    auto* cb = new OrbisNpWebApiRegisteredExtendedPushEventCallback{};
    cb->base.destroy = DestroyAs<OrbisNpWebApiRegisteredExtendedPushEventCallback>;
    cb->filterId = filterId;
    cb->cbFunc = cbFunc;
    cb->cbFuncA = cbFuncA;
    cb->pUserArg = pUserArg;

    if (!obj_table_put(&userContext->extendedPushEventCallbacks, &cb->base, 8, 31, 0)) {
        delete cb;
        return ORBIS_NP_WEBAPI_ERROR_OUT_OF_MEMORY;
    }
    const s32 cbId = static_cast<s32>(cb->base.id);
    cb->callbackId = cbId;
    return cbId;
}

s32 registerExtdPushEventCallback(s32 titleUserCtxId, s32 filterId,
                                  OrbisNpWebApiExtdPushEventCallback cbFunc,
                                  OrbisNpWebApiExtdPushEventCallbackA cbFuncA, void* pUserArg) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    if (!HasObject(&context->extendedPushEventFilters, filterId)) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_EXTD_PUSH_EVENT_FILTER_NOT_FOUND;
    }

    s32 result =
        registerExtdPushEventCallbackInternal(user_context, filterId, cbFunc, cbFuncA, pUserArg);
    releaseUserContext(user_context);
    releaseContext(context);
    return result;
}

s32 registerExtdPushEventCallbackA(s32 titleUserCtxId, s32 filterId,
                                   OrbisNpWebApiExtdPushEventCallbackA cbFunc, void* pUserArg) {
    return registerExtdPushEventCallback(titleUserCtxId, filterId, nullptr, cbFunc, pUserArg);
}

s32 unregisterExtdPushEventCallback(s32 titleUserCtxId, s32 callbackId) {
    OrbisNpWebApiContext* context = findAndValidateContext(titleUserCtxId >> 0x10);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, titleUserCtxId);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    obj_head_t* callback = obj_table_find(&user_context->extendedPushEventCallbacks, callbackId, 8);
    if (callback == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_EXTD_PUSH_EVENT_CALLBACK_NOT_FOUND;
    }

    obj_table_remove(&user_context->extendedPushEventCallbacks, callback);
    obj_table_put_back(&user_context->extendedPushEventCallbacks, callback);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI getHttpRequestIdFromRequest(OrbisNpWebApiRequest* request)

{
    return request->http_request_id;
}

// Request/response header support

bool iEquals(const char* a, u64 aLen, const char* b) {
    u64 i = 0;
    for (; i < aLen && b[i]; ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
            return false;
    }
    return i == aLen && b[i] == '\0';
}

// Locate `field`'s value in a raw HTTP response header block (CRLF-separated
// "Name: Value" lines, possibly led by a status line). Returns false if absent.
bool findHeaderValue(const char* block, u64 blockLen, const char* field, std::string& outValue) {
    u64 i = 0;
    while (i < blockLen) {
        u64 lineStart = i;
        while (i < blockLen && block[i] != '\n')
            ++i;
        u64 lineEnd = i; // points at '\n' or end
        if (i < blockLen)
            ++i; // step past '\n'
        // Trim a trailing '\r'.
        if (lineEnd > lineStart && block[lineEnd - 1] == '\r')
            --lineEnd;
        // Split on the first ':'.
        u64 colon = lineStart;
        while (colon < lineEnd && block[colon] != ':')
            ++colon;
        if (colon >= lineEnd)
            continue; // no colon (status line / blank)
        u64 nameLen = colon - lineStart;
        if (!iEquals(block + lineStart, nameLen, field))
            continue;
        u64 vs = colon + 1;
        while (vs < lineEnd && (block[vs] == ' ' || block[vs] == '\t'))
            ++vs;
        outValue.assign(block + vs, lineEnd - vs);
        return true;
    }
    return false;
}

// extraction of the numeric WebApi error code from an error JSON body
// (e.g. {"error":{"code":2240512,...}} or {"code":...}). Returns true on success.
bool parseErrorCode(const char* body, u64 len, s32& out) {
    std::string_view sv(body, len);
    size_t k = sv.find("\"code\"");
    if (k == std::string_view::npos)
        return false;
    k += 6;
    while (k < sv.size() && (sv[k] == ' ' || sv[k] == ':' || sv[k] == '"'))
        ++k;
    bool neg = false;
    if (k < sv.size() && (sv[k] == '-' || sv[k] == '+')) {
        neg = sv[k] == '-';
        ++k;
    }
    if (k >= sv.size() || !std::isdigit((unsigned char)sv[k]))
        return false;
    long long v = 0;
    while (k < sv.size() && std::isdigit((unsigned char)sv[k])) {
        v = v * 10 + (sv[k] - '0');
        ++k;
    }
    out = static_cast<s32>(neg ? -v : v);
    return true;
}

static s32 captureWebApiError(const char* body, u64 len) {
    s32 code = 0;
    if (body != nullptr && len > 0 && parseErrorCode(body, len, code)) {
        g_last_webapi_error = code;
        return code;
    }
    return 0;
}

s32 getLastWebApiError() {
    return g_last_webapi_error;
}

s32 addHttpRequestHeaderInternal(s64 requestId, const char* pFieldName, const char* pValue) {
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiRequest* request = findRequestAndMarkBusy(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }
    request->userHeaders.emplace_back(pFieldName, pValue);
    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);
    return ORBIS_OK;
}

s32 getHttpResponseHeaderValueInternal(s64 requestId, const char* pFieldName, char* pValue,
                                       u64 valueSize, u64* pValueLength) {
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }
    OrbisNpWebApiRequest* request = findRequestAndMarkBusy(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    char* block = nullptr;
    u64 blockSize = 0;
    const s32 httpReqId = getHttpRequestIdFromRequest(request);
    const s32 err = Libraries::Http::sceHttpGetAllResponseHeaders(httpReqId, &block, &blockSize);

    s32 result = ORBIS_OK;
    if (err < 0) {
        result = err;
    } else {
        std::string value;
        const bool found = block != nullptr && findHeaderValue(block, blockSize, pFieldName, value);
        if (pValueLength != nullptr)
            *pValueLength = found ? value.size() : 0;
        if (pValue != nullptr && valueSize > 0) {
            const u64 n = found ? std::min<u64>(value.size(), valueSize - 1) : 0;
            if (n > 0)
                std::memcpy(pValue, value.data(), n);
            pValue[n] = '\0';
        }
    }

    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);
    return result;
}

s32 PS4_SYSV_ABI getHttpStatusCodeInternal(s64 requestId, s32* out_status_code) {
    s32 status_code;
    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiRequest* request = findRequestAndMarkBusy(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    // Query HTTP layer
    {
        int32_t httpReqId = getHttpRequestIdFromRequest(request);
        s32 err = Libraries::Http::sceHttpGetStatusCode(httpReqId, &status_code);

        if (out_status_code != nullptr)
            *out_status_code = status_code;

        releaseRequest(request);
        releaseUserContext(user_context);
        releaseContext(context);

        return err;
    }
}

void PS4_SYSV_ABI setRequestEndTime(OrbisNpWebApiRequest* request) {
    startRequestTimer(request);
}

void PS4_SYSV_ABI clearRequestEndTime(OrbisNpWebApiRequest* req) {
    retro_atomic_store_release_64(&req->requestEndTime, 0);
}

bool PS4_SYSV_ABI hasRequestTimedOut(OrbisNpWebApiRequest* request) {
    return retro_atomic_load_acquire_int(&request->timedOut) != 0;
}

bool PS4_SYSV_ABI isRequestAborted(OrbisNpWebApiRequest* request) {
    return retro_atomic_load_acquire_int(&request->aborted) != 0;
}

void PS4_SYSV_ABI setRequestState(OrbisNpWebApiRequest* request, u8 state) {
    request->requestState = state;
}

u64 PS4_SYSV_ABI copyRequestData(OrbisNpWebApiRequest* request, void* data, u64 size) {
    u64 readSize = 0;

    if (request->remainingData != 0) {
        u64 remainingSize = request->remainingData - request->readOffset;

        if (remainingSize != 0) {
            if (remainingSize < size) {
                size = remainingSize;
            }
            memcpy(data, request->data + request->readOffset, size);
            request->readOffset += static_cast<u32>(size);
            readSize = size;
        }
    }
    return readSize;
}

s32 PS4_SYSV_ABI readDataInternal(s64 requestId, void* pData, u64 size) {
    u32 offset;
    s32 result;
    u64 remainingSize;
    u64 bytesCopied;

    OrbisNpWebApiContext* context = findAndValidateContext(requestId >> 0x30);
    if (context == nullptr) {
        return ORBIS_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiUserContext* user_context = findUserContext(context, requestId >> 0x20);
    if (user_context == nullptr) {
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
    }

    OrbisNpWebApiRequest* request = findRequestAndMarkBusy(user_context, requestId);
    if (request == nullptr) {
        releaseUserContext(user_context);
        releaseContext(context);
        return ORBIS_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
    }

    setRequestEndTime(request);

    bytesCopied = copyRequestData(request, pData, size);
    offset = (u32)bytesCopied;
    remainingSize = size - offset;

    // If caller wants more data than buffered
    if (remainingSize != 0) {
        setRequestState(request, 5); // TODO add request states?

        if (!hasRequestTimedOut(request) && isRequestAborted(request)) {
            offset = ORBIS_NP_WEBAPI_ERROR_ABORTED;
        } else {
            int32_t httpReqId = getHttpRequestIdFromRequest(request);
            int32_t httpRead =
                Libraries::Http::sceHttpReadData(httpReqId, (u8*)pData + offset, remainingSize);

            if (httpRead < 0)
                httpRead = 0;

            offset += httpRead;
        }
    }

    // Final state resolution
    setRequestState(request, 0);

    if (hasRequestTimedOut(request)) {
        result = ORBIS_NP_WEBAPI_ERROR_TIMEOUT;
    } else if (isRequestAborted(request)) {
        result = ORBIS_NP_WEBAPI_ERROR_ABORTED;
    } else {
        result = offset;
        // Surface the server error code for sceNpWebApiGetErrorCode() when the body
        // just read belongs to an error response
        if (offset > 0) {
            s32 sc = 0;
            if (Libraries::Http::sceHttpGetStatusCode(getHttpRequestIdFromRequest(request), &sc) >=
                    0 &&
                sc >= 400) {
                captureWebApiError(reinterpret_cast<const char*>(pData), offset);
            }
        }
    }

    if (result > 0) {
        LOG_INFO(Lib_NpWebApi, "reqId=0x%" PRIx64 " -> %d bytes: %.256s", requestId, result,
                 (std::string(reinterpret_cast<const char*>(pData), std::min<u64>(result, 256)))
                     .c_str()); // debug to be removed
    }

    // Cleanup
    clearRequestEndTime(request);
    releaseRequest(request);
    releaseUserContext(user_context);
    releaseContext(context);

    return result;
}

// --- Push-event delivery -----------------------------------------------------
//
// Mirrors the native lib: notificationCallbackFunc -> FUN_0100e220 (extended) and the
// service dispatch, with matching done by FUN_010049c0. Natively this is driven by the
// NP manager's push listener thread (mnp:usr:npweblis)

struct PushPeerAddress {
    OrbisNpOnlineId onlineId;
    s32 platform;
};
// Service callback = basic callback with pNpServiceName/npServiceLabel inserted after
// callbackId; it still carries pTo/pFrom before pDataType.
using ServiceCb = PS4_SYSV_ABI void (*)(s32, s32, const char*, OrbisNpServiceLabel,
                                        const PushPeerAddress*, const PushPeerAddress*,
                                        const OrbisNpWebApiPushEventDataType*, const char*, u64,
                                        void*);
using BasicCb = PS4_SYSV_ABI void (*)(s32, s32, const PushPeerAddress*, const PushPeerAddress*,
                                      const OrbisNpWebApiPushEventDataType*, const char*, u64,
                                      void*);
using ExtdCbA = PS4_SYSV_ABI void (*)(s32, s32, const char*, OrbisNpServiceLabel,
                                      const OrbisNpPeerAddressA*, const OrbisNpOnlineId*,
                                      const OrbisNpPeerAddressA*, const OrbisNpOnlineId*,
                                      const OrbisNpWebApiPushEventDataType*, const char*, u64,
                                      const OrbisNpWebApiExtdPushEventExtdData*, u64, void*);

// Push events from the network thread, on a lock-free stack until the game's thread takes
// them (libretro-common's mpsc_stack.h)
struct PushEventNode {
    mpsc_stack_node_t node; // first: the stack links them through it
    PushEventInput ev;
};
static mpsc_stack_t g_push_stack{};

template <typename Filter>
bool filterMatches(const Filter* flt, const std::string& evServiceName, bool evHasServiceName,
                   const std::string& evDataType) {
    const bool catch_all = flt->npServiceName.empty() && flt->npServiceLabel == 0xffffffffu;
    if (catch_all) {
        if (evHasServiceName) {
            return false;
        }
    } else if (!evHasServiceName || flt->npServiceName != evServiceName) {
        return false;
    }
    for (const auto& p : flt->filterParams) {
        if (evDataType == p.dataType.val) {
            return true;
        }
    }
    return false;
}

// Network-thread hand-off only,the event is dispatched later from DrainPushEvents.
void EnqueuePushEvent(const PushEventInput& ev) {
    auto* const node = new PushEventNode{{}, ev};
    mpsc_stack_push(&g_push_stack, &node->node);
}

// Runs on the game thread from sceNpCheckCallback
void DrainPushEvents() {
    // Everything pushed so far, oldest first
    mpsc_stack_node_t* next = mpsc_stack_reverse(mpsc_stack_drain(&g_push_stack));
    while (next != nullptr) {
        auto* const event_node = reinterpret_cast<PushEventNode*>(next);
        next = next->next;
        const PushEventInput& ev = event_node->ev;
        const bool ev_has_service = !ev.npServiceName.empty();
        OrbisNpWebApiPushEventDataType dt{};
        std::snprintf(dt.val, sizeof(dt.val), "%s", ev.dataType.c_str());
        const OrbisNpOnlineId* from_p = ev.hasFrom ? &ev.fromOnlineId : nullptr;
        const OrbisNpOnlineId* to_p = ev.hasTo ? &ev.toOnlineId : nullptr;
        OrbisNpPeerAddressA to_addr{};
        OrbisNpPeerAddressA from_addr{};
        to_addr.accountId = ev.toAccountId;
        to_addr.platform = OrbisNpPlatformType::PS4;
        from_addr.accountId = ev.fromAccountId;
        from_addr.platform = OrbisNpPlatformType::PS4;
        const OrbisNpPeerAddressA* to_addr_p = ev.toAccountId != 0 ? &to_addr : nullptr;
        const OrbisNpPeerAddressA* from_addr_p = ev.fromAccountId != 0 ? &from_addr : nullptr;

        // Each context and user context held while its callbacks run: none freed under them
        for (int ctx_slot = 0; ctx_slot < g_contexts.count; ctx_slot++) {
            auto* context =
                static_cast<OrbisNpWebApiContext*>(obj_table_hold(&g_contexts, ctx_slot));
            if (context == nullptr) {
                continue;
            }
            for (int uc_slot = 0; uc_slot < context->userContexts.count; uc_slot++) {
                auto* uc = static_cast<OrbisNpWebApiUserContext*>(
                    obj_table_hold(&context->userContexts, uc_slot));
                if (uc == nullptr) {
                    continue;
                }
                if (uc->userId != ev.targetUserId) {
                    releaseUserContext(uc);
                    continue;
                }
                const s32 title_user_ctx_id = uc->userCtxId;

                for (int cb_slot = 0; cb_slot < uc->extendedPushEventCallbacks.count; cb_slot++) {
                    auto* cb = static_cast<OrbisNpWebApiRegisteredExtendedPushEventCallback*>(
                        obj_table_hold(&uc->extendedPushEventCallbacks, cb_slot));
                    if (cb == nullptr) {
                        continue;
                    }
                    const s32 cbId = cb->callbackId;
                    OrbisNpWebApiExtendedPushEventFilter* flt = nullptr;
                    do {
                        if (cb == nullptr) {
                            break;
                        }
                        void (*raw)() = cb->cbFuncA  ? reinterpret_cast<void (*)()>(cb->cbFuncA)
                                        : cb->cbFunc ? reinterpret_cast<void (*)()>(cb->cbFunc)
                                                     : nullptr;
                        if (raw == nullptr) {
                            break;
                        }
                        flt = reinterpret_cast<OrbisNpWebApiExtendedPushEventFilter*>(
                            obj_table_find(&context->extendedPushEventFilters, cb->filterId, 8));
                        if (flt == nullptr) {
                            break;
                        }
                        if (!filterMatches(flt, ev.npServiceName, ev_has_service, ev.dataType)) {
                            break;
                        }
                        // Only hand the title the extended-data keys its filter asked for for this
                        // dataType (SDK behaviour). This also keeps server-internal pairs such as
                        // fromAccountId/toAccountId/fromPlatform out of pExtdData.
                        const std::vector<std::string>* wanted_keys = nullptr;
                        for (size_t i = 0; i < flt->filterParams.size(); ++i) {
                            if (ev.dataType == flt->filterParams[i].dataType.val) {
                                if (i < flt->extdDataKeys.size()) {
                                    wanted_keys = &flt->extdDataKeys[i];
                                }
                                break;
                            }
                        }
                        auto key_requested = [wanted_keys](const std::string& k) {
                            return wanted_keys != nullptr &&
                                   std::find(wanted_keys->begin(), wanted_keys->end(), k) !=
                                       wanted_keys->end();
                        };
                        std::vector<OrbisNpWebApiExtdPushEventExtdData> exarr;
                        exarr.reserve(ev.extdData.size());
                        for (auto& [k, v] : ev.extdData) {
                            if (!key_requested(k)) {
                                continue;
                            }
                            OrbisNpWebApiExtdPushEventExtdData e{};
                            std::snprintf(e.extdDataKey.val, sizeof(e.extdDataKey.val), "%s",
                                          k.c_str());
                            e.pData = const_cast<char*>(v.data());
                            e.dataLen = v.size();
                            exarr.push_back(e);
                        }
                        // Service name/label come from the FILTER
                        const char* svc =
                            flt->npServiceName.empty() ? nullptr : flt->npServiceName.c_str();
                        // pData/pExtdData are NULL (and the counts 0) when the event
                        // carries no payload / no matching extended data.
                        const char* ext_data = ev.data.empty() ? nullptr : ev.data.data();
                        const OrbisNpWebApiExtdPushEventExtdData* ext_arr =
                            exarr.empty() ? nullptr : exarr.data();

                        LOG_INFO(Lib_NpWebApi, "invoking extd cb ctx=0x%x cbId=%d dataType='%s'",
                                 title_user_ctx_id, cbId,
                                 (ev.dataType).c_str()); // debug confirm the listener callback
                                                         // fires. to be removed
                        reinterpret_cast<ExtdCbA>(raw)(
                            title_user_ctx_id, cbId, svc, flt->npServiceLabel, to_addr_p, to_p,
                            from_addr_p, from_p, &dt, ext_data, ev.data.size(), ext_arr,
                            exarr.size(), cb->pUserArg);
                    } while (false);
                    if (flt != nullptr) {
                        obj_table_put_back(&context->extendedPushEventFilters, &flt->base);
                    }
                    obj_table_put_back(&uc->extendedPushEventCallbacks, &cb->base);
                }

                // Service push
                for (int cb_slot = 0; cb_slot < uc->servicePushEventCallbacks.count; cb_slot++) {
                    auto* cb = static_cast<OrbisNpWebApiRegisteredServicePushEventCallback*>(
                        obj_table_hold(&uc->servicePushEventCallbacks, cb_slot));
                    if (cb == nullptr) {
                        continue;
                    }
                    const s32 cbId = cb->callbackId;
                    OrbisNpWebApiServicePushEventFilter* flt = nullptr;
                    do {
                        if (cb == nullptr || cb->cbFunc == nullptr) {
                            break;
                        }
                        flt = reinterpret_cast<OrbisNpWebApiServicePushEventFilter*>(
                            obj_table_find(&context->servicePushEventFilters, cb->filterId, 8));
                        if (flt == nullptr) {
                            break;
                        }
                        if (!filterMatches(flt, ev.npServiceName, ev_has_service, ev.dataType)) {
                            break;
                        }
                        const char* svc =
                            flt->npServiceName.empty() ? nullptr : flt->npServiceName.c_str();
                        const char* svc_data = ev.data.empty() ? nullptr : ev.data.data();
                        PushPeerAddress to_peer{};   // notified user (self)
                        PushPeerAddress from_peer{}; // user that caused the event
                        if (ev.hasTo) {
                            to_peer.onlineId = ev.toOnlineId;
                        }
                        if (ev.hasFrom) {
                            from_peer.onlineId = ev.fromOnlineId;
                        }
                        reinterpret_cast<ServiceCb>(reinterpret_cast<void (*)()>(cb->cbFunc))(
                            title_user_ctx_id, cbId, svc, flt->npServiceLabel, &to_peer, &from_peer,
                            &dt, svc_data, ev.data.size(), cb->pUserArg);
                    } while (false);
                    if (flt != nullptr) {
                        obj_table_put_back(&context->servicePushEventFilters, &flt->base);
                    }
                    obj_table_put_back(&uc->servicePushEventCallbacks, &cb->base);
                }

                // Basic push
                if (!ev_has_service) {
                    for (int cb_slot = 0; cb_slot < uc->pushEventCallbacks.count; cb_slot++) {
                        auto* cb = static_cast<OrbisNpWebApiRegisteredPushEventCallback*>(
                            obj_table_hold(&uc->pushEventCallbacks, cb_slot));
                        if (cb == nullptr) {
                            continue;
                        }
                        const s32 cbId = cb->callbackId;
                        OrbisNpWebApiPushEventFilter* flt = nullptr;
                        do {
                            if (cb == nullptr || cb->cbFunc == nullptr) {
                                break;
                            }
                            flt = reinterpret_cast<OrbisNpWebApiPushEventFilter*>(
                                obj_table_find(&context->pushEventFilters, cb->filterId, 8));
                            if (flt == nullptr) {
                                break;
                            }
                            bool matched = false;
                            for (const auto& p : flt->filterParams) {
                                if (ev.dataType == p.dataType.val) {
                                    matched = true;
                                    break;
                                }
                            }
                            if (!matched) {
                                break;
                            }
                            LOG_INFO(Lib_NpWebApi,
                                     "invoking basic cb ctx=0x%x cbId=%d "
                                     "dataType='%s'",
                                     title_user_ctx_id, cbId,
                                     (ev.dataType).c_str()); // debug confirm the listener callback
                                                             // fires. to be removed
                            PushPeerAddress to_peer{};   // notified user (self)
                            PushPeerAddress from_peer{}; // user that caused the event
                            if (ev.hasTo) {
                                to_peer.onlineId = ev.toOnlineId;
                            }
                            if (ev.hasFrom) {
                                from_peer.onlineId = ev.fromOnlineId;
                            }
                            const char* p_data = ev.data.empty() ? nullptr : ev.data.data();
                            reinterpret_cast<BasicCb>(reinterpret_cast<void (*)()>(cb->cbFunc))(
                                title_user_ctx_id, cbId, &to_peer, &from_peer, &dt, p_data,
                                ev.data.size(), cb->pUserArg);
                        } while (false);
                        if (flt != nullptr) {
                            obj_table_put_back(&context->pushEventFilters, &flt->base);
                        }
                        obj_table_put_back(&uc->pushEventCallbacks, &cb->base);
                    }
                }
                releaseUserContext(uc);
            }
            releaseContext(context);
        }
        delete event_node;
    }
}
}; // namespace Libraries::Np::NpWebApi
