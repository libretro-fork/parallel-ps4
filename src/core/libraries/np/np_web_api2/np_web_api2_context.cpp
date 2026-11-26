// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/libraries/network/http.h"
#include "core/libraries/network/http2.h"
#include "core/libraries/np/np_common.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/np_handler/np_handler.h"
#include "core/libraries/np/np_web_api2/np_web_api2_context.h"

namespace Libraries::Np::NpWebApi2 {

// Each object of @table held in turn, for @visit, then given back
static void ForEach(obj_table_t* table, void (*visit)(void* object, void* arg), void* arg) {
    for (int i = 0; i < table->count; i++) {
        void* object = obj_table_hold(table, i);
        if (object != nullptr) {
            visit(object, arg);
            obj_table_put_back(table, static_cast<obj_head_t*>(object));
        }
    }
}

LibraryContext::~LibraryContext() {
    obj_table_destroy(&user_contexts);
    obj_table_destroy(&push_event_handles);
    obj_table_destroy(&push_event_filters);
}

void LibraryContext::CheckTimeout() {
    u64 time = NowUs();
    ForEach(
        &user_contexts,
        [](void* object, void*) { static_cast<UserContext*>(object)->CheckTimeout(); }, nullptr);
    ForEach(
        &push_event_handles,
        [](void* object, void* arg) {
            static_cast<PushEventHandle*>(object)->CheckTimeout(*static_cast<u64*>(arg));
        },
        &time);
}

s32 LibraryContext::CreateUserContext(OrbisUserServiceUserId user_id) {
    UserContext* user_ctx = new UserContext(this, user_id);
    // Its id: this context's above, its slot and the table's count below
    if (!obj_table_put(&user_contexts, &user_ctx->head, 6, 16, static_cast<s64>(GetId()) << 0x10)) {
        delete user_ctx;
        LOG_ERROR(Lib_NpWebApi2, "Too many user contexts");
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_MAX;
    }
    return user_ctx->GetId();
}

void LibraryContext::DeleteAllUserContexts() {
    for (int i = 0; i < user_contexts.count; i++) {
        auto* user_ctx = static_cast<UserContext*>(obj_table_hold(&user_contexts, i));
        if (user_ctx != nullptr) {
            const s32 user_ctx_id = user_ctx->GetId();
            user_ctx->RemoveUser();
            deleteUserContext(user_ctx_id);
        }
    }
}

s32 LibraryContext::CreatePushEventHandle() {
    PushEventHandle* handle = new PushEventHandle();
    handle->head.destroy = PushEventHandle::Destroy;
    handle->home = &push_event_handles;
    if (!obj_table_put(&push_event_handles, &handle->head, 8, 31, 0)) {
        delete handle;
        LOG_ERROR(Lib_NpWebApi2, "Too many push event handles");
        return ORBIS_NP_WEBAPI2_ERROR_OUT_OF_MEMORY;
    }
    return handle->GetId();
}

PushEventHandle* LibraryContext::GetPushEventHandle(s32 handle_id) {
    return reinterpret_cast<PushEventHandle*>(obj_table_find(&push_event_handles, handle_id, 8));
}

s32 LibraryContext::DeletePushEventHandle(PushEventHandle* handle) {
    s32 handle_id = handle->GetId();
    if (!handle->MarkForDeletion()) {
        handle->RemoveUser();
        LOG_ERROR(Lib_NpWebApi2, "handle with id 0x%x is already deleted", handle_id);
        return ORBIS_NP_WEBAPI2_ERROR_HANDLE_NOT_FOUND;
    }

    handle->Abort();
    obj_table_remove(&push_event_handles, &handle->head);
    // Its other holders let go, on the game's time; the last one frees it
    Libraries::Kernel::GameWait(obj_table_released(), 0, [handle] { return !handle->IsBusy(); });
    handle->RemoveUser();
    return ORBIS_OK;
}

s32 LibraryContext::CreatePushEventFilter(
    s32 handle_id, const char* np_service_name, OrbisNpServiceLabel np_service_label,
    const OrbisNpWebApi2PushEventFilterParameter* filter_param, u64 filter_param_num,
    bool internal) {
    PushEventHandle* handle = GetPushEventHandle(handle_id);
    if (handle == nullptr) {
        LOG_ERROR(Lib_NpWebApi2, "No push event handle with id 0x%x", handle_id);
        return ORBIS_NP_WEBAPI2_ERROR_HANDLE_NOT_FOUND;
    }

    PushEventFilter* filter = new PushEventFilter(np_service_name, np_service_label, internal);
    filter->head.destroy = PushEventFilter::Destroy;
    filter->home = &push_event_filters;
    s32 result = filter->Initialize(handle, filter_param, filter_param_num);
    handle->RemoveUser();
    if (result < 0) {
        // Failed to initialize filter.
        delete filter;
        return result;
    }
    if (!obj_table_put(&push_event_filters, &filter->head, 8, 31, 0)) {
        delete filter;
        LOG_ERROR(Lib_NpWebApi2, "Too many push event filters");
        return ORBIS_NP_WEBAPI2_ERROR_OUT_OF_MEMORY;
    }
    return filter->GetId();
}

UserContext* LibraryContext::GetUserContext(s32 user_ctx_id) {
    UserContext* user_ctx =
        reinterpret_cast<UserContext*>(obj_table_find(&user_contexts, user_ctx_id, 6));
    if (user_ctx != nullptr && user_ctx->IsDeleted()) {
        user_ctx->RemoveUser();
        return nullptr;
    }
    return user_ctx;
}

UserContext* LibraryContext::GetUserContextByUserId(OrbisUserServiceUserId user_id) {
    if (user_id == ORBIS_USER_SERVICE_USER_ID_INVALID) {
        return nullptr;
    }

    for (int i = 0; i < user_contexts.count; i++) {
        auto* user_ctx = static_cast<UserContext*>(obj_table_hold(&user_contexts, i));
        if (user_ctx == nullptr) {
            continue;
        }
        if (user_ctx->GetUserId() == user_id) {
            return user_ctx;
        }
        user_ctx->RemoveUser();
    }
    return nullptr;
}

s32 PS4_SYSV_ABI internalPreSendCallback(s32 http_request_id, s32 ssl_id,
                                         Libraries::Http2::OrbisHttp2PreSendCallbackData* data,
                                         void* user_arg) {
    s64 user_ctx_id = reinterpret_cast<s64>(user_arg);
    LOG_ERROR(Lib_NpWebApi2,
              "Unimplemented, http_request_id = 0x%x, ssl_id = 0x%x, user_ctx_id = 0x%" PRIx64,
              http_request_id, ssl_id, user_ctx_id);
    return ORBIS_OK;
}

s32 UserContext::Initialize() {
    const char* name = this->parent_ctx->GetName();

    char sw_version[8];
    std::memset(sw_version, 0, sizeof(sw_version));
    Libraries::Np::NpCommon::sceNpGetSdkVersion(sw_version);

    char user_agent_buf[0x40];
    std::memset(user_agent_buf, 0, sizeof(user_agent_buf));
    if (name) {
        std::snprintf(user_agent_buf, sizeof(user_agent_buf), "NpWebApi2/%s (%s)", sw_version,
                      name);
    } else {
        std::snprintf(user_agent_buf, sizeof(user_agent_buf), "NpWebApi2/%s", sw_version);
    }
    this->user_agent = std::string{user_agent_buf};

    s32 http_ctx_id = this->parent_ctx->GetHttpCtxId();
    http_template_id = Libraries::Http2::sceHttp2CreateTemplate(
        http_ctx_id, user_agent_buf,
        Libraries::Http2::OrbisHttp2HttpVersion::ORBIS_HTTP2_VERSION_2_0, 0);
    if (http_template_id < 0) {
        LOG_ERROR(Lib_NpWebApi2, "Failed to create HTTP template, error = 0x%x", http_template_id);
        return http_template_id;
    }

    s32 result = Libraries::Http2::sceHttp2SetPreSendCallback(
        http_template_id, internalPreSendCallback, reinterpret_cast<void*>(head.id));
    if (result < 0) {
        LOG_ERROR(Lib_NpWebApi2, "Failed to set pre-send callback, error = 0x%x", result);
        return result;
    }

    return result;
}

UserContext::~UserContext() {
    obj_table_destroy(&requests);
    obj_table_destroy(&push_event_callbacks);
    obj_table_destroy(&push_contexts);
    obj_table_destroy(&push_context_callbacks);
}

void UserContext::CheckTimeout() {
    ForEach(
        &requests, [](void* object, void*) { static_cast<Request*>(object)->CheckTimeout(); },
        nullptr);
}

s32 UserContext::CreatePushEventCallback(s32 filter_id, OrbisNpWebApi2PushEventCallback cb_func,
                                         void* user_arg) {
    PushEventCallback* callback = new PushEventCallback{{}, filter_id, cb_func, user_arg};
    callback->head.destroy = PushEventCallback::Destroy;
    if (!obj_table_put(&push_event_callbacks, &callback->head, 8, 31, 0)) {
        delete callback;
        LOG_ERROR(Lib_NpWebApi2, "Too many push event callbacks");
        return ORBIS_NP_WEBAPI2_ERROR_OUT_OF_MEMORY;
    }
    return static_cast<s32>(callback->head.id);
}

// The object of @id off @table, once no one else holds it, freed (obj_table.c)
static bool DeleteById(obj_table_t* table, s64 id) {
    obj_head_t* object = obj_table_find(table, id, 8);
    if (object == nullptr) {
        return false;
    }
    if (obj_table_retire(table, object)) {
        const int index = object->index;
        Libraries::Kernel::GameWait(obj_table_released(), 0,
                                    [table, index] { return obj_table_users(table, index) <= 1; });
    }
    obj_table_put_back(table, object);
    return true;
}

s32 UserContext::DeletePushEventCallback(s32 callback_id) {
    if (!DeleteById(&push_event_callbacks, callback_id)) {
        LOG_ERROR(Lib_NpWebApi2, "No push event callback with id 0x%x", callback_id);
        return ORBIS_NP_WEBAPI2_ERROR_PUSH_EVENT_CALLBACK_NOT_FOUND;
    }
    return ORBIS_OK;
}

s32 UserContext::CreatePushContext(OrbisNpWebApi2PushEventPushContextId* push_ctx_id) {
    PushEventPushContext* push_ctx = new PushEventPushContext(5000, this);
    push_ctx->head.destroy = PushEventPushContext::Destroy;
    push_ctx->home = &push_contexts;
    push_ctx->Initialize();
    if (!obj_table_put(&push_contexts, &push_ctx->head, 6, 31, 0)) {
        delete push_ctx;
        LOG_ERROR(Lib_NpWebApi2, "Too many push contexts");
        return ORBIS_NP_WEBAPI2_ERROR_OUT_OF_MEMORY;
    }
    std::memcpy(push_ctx_id, push_ctx->GetId(), sizeof(*push_ctx_id));
    return ORBIS_OK;
}

PushEventPushContext* UserContext::GetPushContext(
    const OrbisNpWebApi2PushEventPushContextId* push_ctx_id) {
    s64 raw_id{};
    std::memcpy(&raw_id, push_ctx_id, sizeof(s64));
    for (int i = 0; i < push_contexts.count; i++) {
        auto* push_ctx = static_cast<PushEventPushContext*>(obj_table_hold(&push_contexts, i));
        if (push_ctx == nullptr) {
            continue;
        }
        if (push_ctx->GetFakeId() == raw_id) {
            return push_ctx;
        }
        push_ctx->RemoveUser();
    }
    return nullptr;
}

void UserContext::DeletePushContext(PushEventPushContext* push_ctx) {
    if (obj_table_retire(&push_contexts, &push_ctx->head)) {
        // Its callback done and its other holders gone, on the game's time
        const int index = push_ctx->head.index;
        obj_table_t* table = &push_contexts;
        Libraries::Kernel::GameWait(obj_table_released(), 0, [push_ctx, table, index] {
            return !push_ctx->CallbackRunning() && obj_table_users(table, index) <= 1;
        });
    }
    push_ctx->RemoveUser();
}

s32 UserContext::CreatePushContextCallback(s32 filter_id,
                                           OrbisNpWebApi2PushEventPushContextCallback cb_func,
                                           void* user_arg) {
    PushEventPushContextCallback* callback =
        new PushEventPushContextCallback{{}, filter_id, cb_func, user_arg};
    callback->head.destroy = PushEventPushContextCallback::Destroy;
    if (!obj_table_put(&push_context_callbacks, &callback->head, 8, 31, 0)) {
        delete callback;
        LOG_ERROR(Lib_NpWebApi2, "Too many push context callbacks");
        return ORBIS_NP_WEBAPI2_ERROR_OUT_OF_MEMORY;
    }
    return static_cast<s32>(callback->head.id);
}

s32 UserContext::DeletePushContextCallback(s32 callback_id) {
    if (!DeleteById(&push_context_callbacks, callback_id)) {
        LOG_ERROR(Lib_NpWebApi2, "No push event push context callback with id 0x%x", callback_id);
        return ORBIS_NP_WEBAPI2_ERROR_PUSH_EVENT_CALLBACK_NOT_FOUND;
    }
    return ORBIS_OK;
}

s32 UserContext::CreateRequest(const char* api_group, const char* path, const char* method,
                               const OrbisNpWebApi2ContentParameter* content_parameter,
                               bool multipart, Request** request) {
    Request* new_request =
        new Request(this->parent_ctx, api_group, path, method, multipart, content_parameter);
    new_request->home = &requests;
    // Its id: this user context's above, its slot and the table's count below
    if (!obj_table_put(&requests, &new_request->head, 8, 31, static_cast<s64>(GetId()) << 0x20)) {
        delete new_request;
        *request = nullptr;
        LOG_ERROR(Lib_NpWebApi2, "Too many requests");
        return ORBIS_NP_WEBAPI2_ERROR_OUT_OF_MEMORY;
    }
    *request = new_request;
    return ORBIS_OK;
}

Request* UserContext::GetRequest(s64 request_id) {
    return reinterpret_cast<Request*>(obj_table_find(&requests, request_id, 8));
}

bool UserContext::HasBusyRequests() {
    for (int i = 0; i < requests.count; i++) {
        if (obj_table_users(&requests, i) > 0) {
            return true;
        }
    }
    return false;
}

void UserContext::AbortAllRequests() {
    ForEach(
        &requests, [](void* object, void*) { static_cast<Request*>(object)->Abort(); }, nullptr);
}

void UserContext::Delete() {
    if (this->http_template_id != 0) {
        Libraries::Http2::sceHttp2DeleteTemplate(this->http_template_id);
        this->http_template_id = 0;
    }
    // Off its context; the caller's hold given back frees it, with its requests and callbacks
    obj_table_remove(parent_ctx->UserContexts(), &head);
    RemoveUser();
}

static bool IsInternalHeader(const char* header_name) {
    std::string lower_name{header_name};
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](char c) { return std::tolower(c); });
    // These headers are all skipped if the request's library context
    // was not created through the IntInitialize functions.
    return lower_name.compare("content-type") == 0 || lower_name.compare("user-agent") == 0 ||
           lower_name.compare("authorization") == 0 ||
           lower_name.compare("x-psn-np-title-id") == 0 ||
           lower_name.compare("x-psn-np-title-token") == 0 ||
           lower_name.compare("x-psn-request-id") == 0 ||
           lower_name.compare("x-psn-sdk-ver") == 0 ||
           lower_name.compare("x-psn-debug-settings") == 0 || lower_name.compare("npdebug") == 0 ||
           lower_name.compare("x-psn-trc-check-notification-enabled") == 0 ||
           lower_name.compare("x-psn-webtrace-enabled") == 0 ||
           lower_name.compare("x-psn-npwebapi2-context") == 0 ||
           lower_name.compare("x-psn-fake-rate-limit-enabled") == 0 ||
           lower_name.compare("x-psn-fake-rate-limit-targets") == 0;
}

s32 Request::AddHttpRequestHeader(const char* field_name, const char* field_value) {
    if (!this->parent_ctx->IsInternal() && IsInternalHeader(field_name)) {
        LOG_WARNING(Lib_NpWebApi2, "This is not an internal context, skipping field name %s",
                    field_name);
        return ORBIS_OK;
    }

    // Before it is sent, by the game's one caller of it
    this->http_headers.emplace_back(new HttpRequestHeader(field_name, field_value));
    return ORBIS_OK;
}

s32 Request::CreateHttpRequest(s32 http_template_id, const char* url) {
    const char* method = this->method.empty() ? "UNKNOWN" : this->method.data();
    s32 http_request_id = Libraries::Http2::sceHttp2CreateRequestWithURL(http_template_id, method,
                                                                         url, this->content_length);
    if (http_request_id < 0) {
        LOG_ERROR(Lib_NpWebApi2, "Failed to create Http2 request, error = 0x%x", http_request_id);
        return http_request_id;
    }
    this->http_request_id = http_request_id;
    s32 result = Libraries::Http2::sceHttp2AddRequestHeader(http_request_id, "Content-Type",
                                                            this->content_type.data(), 0);
    if (result < 0) {
        LOG_ERROR(Lib_NpWebApi2, "Failed to add Content-Type request header, error = 0x%x", result);
        return result;
    }

    UserContext* user_ctx = this->parent_ctx->GetUserContext(static_cast<s32>(head.id >> 0x20));
    if (user_ctx == nullptr) {
        LOG_ERROR(Lib_NpWebApi2, "No user context for request id 0x%" PRIx64, head.id);
        return ORBIS_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    }
    const std::string bearer = NpHandler::GetInstance().GetBearerToken(user_ctx->GetUserId());
    if (!bearer.empty()) {
        const std::string auth_value = "Bearer " + bearer;
        result = Libraries::Http2::sceHttp2AddRequestHeader(http_request_id, "Authorization",
                                                            auth_value.data(), 0);
    }
    if (bearer.empty() || result < 0) {
        LOG_WARNING(Lib_NpWebApi2, "Failed to add Authorization request header");
    }

    // This would go on to add several other PSN-specific headers.
    // As shadNet does not appear to use them, the logic is skipped for now.

    for (HttpRequestHeader* header : this->http_headers) {
        s32 result = Libraries::Http2::sceHttp2AddRequestHeader(
            http_request_id, header->field_name.data(), header->field_value.data(), 0);
        if (result < 0) {
            LOG_ERROR(Lib_NpWebApi2,
                      "Failed to add request header, name = %s, value = %s, error = 0x%x",
                      header->field_name.data(), header->field_value.data(), result);
            user_ctx->RemoveUser();
            return result;
        }
    }
    user_ctx->RemoveUser();
    return result;
}

s32 Request::SendHttpRequest(void* data, u64 data_size) {
    s32 result = 0;
    if (this->content_length != 0 && this->sent_data == 0) {
        // Real library seems to do some calculations, some parts including IPC calls.
        // Not entirely sure how accurate this is in practice.
        result = Libraries::Http2::sceHttp2SetRequestContentLength(this->http_request_id,
                                                                   this->content_length);
        if (result < 0) {
            LOG_ERROR(Lib_NpWebApi2, "Failed to set content length, error = 0x%x", result);
            return result;
        }
    }
    if (!data || data_size == 0 || this->content_length == 0) {
        result = Libraries::Http2::sceHttp2SendRequest(this->http_request_id, nullptr, 0);
        if (result < 0) {
            LOG_ERROR(Lib_NpWebApi2, "Failed to send request, error = 0x%x", result);
            return result;
        }
    }

    u64 actual_size = std::min<u64>(data_size, this->content_length - this->sent_data);
    if (this->content_length <= this->sent_data) {
        // Nothing left to send
        return result;
    }
    result = Libraries::Http2::sceHttp2SendRequest(this->http_request_id, data, actual_size);
    if (result < 0) {
        LOG_ERROR(Lib_NpWebApi2, "Failed to send request, error = 0x%x", result);
        return result;
    }
    this->sent_data += actual_size;
    return result;
}

s32 Request::GetAllHttpResponseHeaders() {
    char* header = nullptr;
    u64 size = 0;
    s32 result =
        Libraries::Http2::sceHttp2GetAllResponseHeaders(this->http_request_id, &header, &size);
    if (result < 0) {
        LOG_ERROR(Lib_NpWebApi2, "Failed to get response headers, error = 0x%x", result);
        return result;
    }
    this->http_response_headers = header;
    this->http_response_header_size = size;
    return result;
}

s32 Request::ParseHttpResponseHeaders(const char* field_name, char* value, u64 value_size,
                                      u64* value_size_out) {
    const char* temp_val{};
    u64 temp_size{};
    s32 result = Libraries::Http::sceHttpParseResponseHeader(this->http_response_headers,
                                                             this->http_response_header_size,
                                                             field_name, &temp_val, &temp_size);
    if (result >= 0) {
        if (value) {
            std::memset(value, 0, value_size);
            u64 size = std::min<u64>(value_size, temp_size);
            std::memcpy(value, temp_val, size);
        }
        if (value_size_out) {
            *value_size_out = temp_size;
        }
        result = ORBIS_OK;
    }
    return result;
}

s32 Request::Abort() {
    s32 result = 0;
    if (retro_atomic_exchange_int(&aborted, 1) != 0) {
        // Nothing to do.
        return result;
    }

    // Real library has multiple states, we don't since our logic is simpler.
    // Its sender sets the state, then looks for an abort; this sets the abort, then looks at the
    // state: one of the two sees the other (both full barriers)
    if (retro_atomic_load_seq_cst_int(&send_state) == 3) {
        // In the middle of an Http request, that needs aborting.
        result = Libraries::Http2::sceHttp2AbortRequest(this->http_request_id);
        if (result < 0) {
            LOG_ERROR(Lib_NpWebApi2, "Failed to abort http request, error = 0x%x", result);
        }
    }
    return result;
}

Request::~Request() {
    for (HttpRequestHeader* header : this->http_headers) {
        delete header;
    }
    this->http_headers.clear();
    if (this->http_request_id != 0) {
        Libraries::Http2::sceHttp2DeleteRequest(this->http_request_id);
    }
}

}; // namespace Libraries::Np::NpWebApi2