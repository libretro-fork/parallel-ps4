// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <retro_atomic.h>
#include "common/obj_table.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/np/np_web_api2/np_web_api2.h"
#include "core/libraries/np/np_web_api2/np_web_api2_internal.h"
#include "core/libraries/np/np_web_api2/np_web_api2_push_event.h"
#include "core/libraries/system/userservice.h"

#include <string>
#include <vector>

namespace Libraries::Np::NpWebApi2 {

class UserContext;
class Request;

// How many of each a table holds (obj_table.c): found, held and given back without a lock, an
// id naming its slot and how often the table's slots were used
constexpr int MaxLibraryContexts = 32;
constexpr int MaxUserContexts = 16;
constexpr int MaxRequests = 128;
constexpr int MaxHandles = 64;
constexpr int MaxFilters = 64;
constexpr int MaxCallbacks = 64;
constexpr int MaxPushContexts = 32;

/// The library's contexts; a wait for their holders to let go is woken by
/// obj_table_released(), on the game's time
obj_table_t* LibraryContexts();

class LibraryContext {
public:
    obj_head_t head{}; // first: its id, its slot and how it is freed

    LibraryContext(s32 type, s32 http_ctx_id, u64 pool_size, const char* name)
        : http_ctx_id(http_ctx_id), type(static_cast<LibraryContextType>(type)),
          pool_size(pool_size), name(name ? name : "") {
        head.destroy = Destroy;
        obj_table_init(&user_contexts, MaxUserContexts);
        obj_table_init(&push_event_handles, MaxHandles);
        obj_table_init(&push_event_filters, MaxFilters);
    }
    ~LibraryContext();

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<LibraryContext*>(object);
    }

    void AddUser() {
        (void)obj_table_hold(LibraryContexts(), head.index);
    }

    void RemoveUser() {
        obj_table_put_back(LibraryContexts(), &head);
    }

    s32 GetId() {
        return static_cast<s32>(head.id);
    }

    s32 GetHttpCtxId() {
        return http_ctx_id;
    }

    u64 GetPoolSize() {
        return pool_size;
    }

    const char* GetName() {
        return name.empty() ? nullptr : name.data();
    }

    bool IsDebug() {
        return type == LibraryContextType::Debug;
    }

    bool IsNormal() {
        return type == LibraryContextType::Normal;
    }

    bool IsInternal() {
        return type == LibraryContextType::Internal;
    }

    bool IsPresence() {
        return type == LibraryContextType::Presence;
    }

    bool IsDeleted() {
        return retro_atomic_load_acquire_int(&deleting) != 0;
    }

    /// Marked once: false if it already was
    bool MarkForDeletion() {
        return retro_atomic_exchange_int(&deleting, 1) == 0;
    }

    /// Held by others than the caller
    bool IsBusy() {
        return obj_table_users(LibraryContexts(), head.index) > 1;
    }

    void CheckTimeout();

    s32 CreatePushEventHandle();
    PushEventHandle* GetPushEventHandle(s32 handle_id);

    void SetHandleEndTime(s32 handle_id) {
        if (PushEventHandle* handle = GetPushEventHandle(handle_id)) {
            handle->SetEndTime();
            handle->RemoveUser();
        }
    }

    void AbortAllPushEventHandles() {
        for (int i = 0; i < push_event_handles.count; i++) {
            auto* handle = static_cast<PushEventHandle*>(obj_table_hold(&push_event_handles, i));
            if (handle != nullptr) {
                handle->Abort();
                handle->RemoveUser();
            }
        }
    }

    bool HasBusyPushEventHandles() {
        for (int i = 0; i < push_event_handles.count; i++) {
            if (obj_table_live(&push_event_handles, i) &&
                obj_table_users(&push_event_handles, i) > 0) {
                return true;
            }
        }
        return false;
    }

    s32 DeletePushEventHandle(PushEventHandle* handle);

    s32 CreatePushEventFilter(s32 handle_id, const char* np_service_name,
                              OrbisNpServiceLabel np_service_label,
                              const OrbisNpWebApi2PushEventFilterParameter* filter_param,
                              u64 filter_param_num, bool internal);

    /// Held: given back with RemoveUser
    PushEventFilter* GetPushEventFilter(s32 filter_id) {
        return reinterpret_cast<PushEventFilter*>(
            obj_table_find(&push_event_filters, filter_id, 8));
    }

    bool HasPushEventFilter(s32 filter_id) {
        PushEventFilter* filter = GetPushEventFilter(filter_id);
        if (filter == nullptr) {
            return false;
        }
        filter->RemoveUser();
        return true;
    }

    /// Held by the caller: off the table, freed as the last holder lets go
    void DeletePushEventFilter(PushEventFilter* filter) {
        obj_table_remove(&push_event_filters, &filter->head);
        filter->RemoveUser();
    }

    s32 CreateUserContext(OrbisUserServiceUserId user_id);
    UserContext* GetUserContext(s32 user_ctx_id);
    UserContext* GetUserContextByUserId(OrbisUserServiceUserId user_id);

    obj_table_t* UserContexts() {
        return &user_contexts;
    }

    void DeleteAllUserContexts();

private:
    s32 http_ctx_id{};
    enum LibraryContextType : s32 {
        Debug = 0,
        Normal = 1,
        Internal = 2,
        Presence = 3,
    } type;
    u64 pool_size{};
    retro_atomic_int_t deleting{0};
    std::string name{};
    obj_table_t user_contexts{};
    obj_table_t push_event_handles{};
    obj_table_t push_event_filters{};
};

class UserContext {
public:
    obj_head_t head{}; // first: its id, its slot and how it is freed

    UserContext(LibraryContext* parent, OrbisUserServiceUserId user_id)
        : user_id(user_id), parent_ctx(parent) {
        head.destroy = Destroy;
        obj_table_init(&requests, MaxRequests);
        obj_table_init(&push_event_callbacks, MaxCallbacks);
        obj_table_init(&push_contexts, MaxPushContexts);
        obj_table_init(&push_context_callbacks, MaxCallbacks);
    }
    ~UserContext();

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<UserContext*>(object);
    }

    void AddUser() {
        (void)obj_table_hold(parent_ctx->UserContexts(), head.index);
    }

    void RemoveUser() {
        obj_table_put_back(parent_ctx->UserContexts(), &head);
    }

    bool IsDeleted() {
        return retro_atomic_load_acquire_int(&deleting) != 0;
    }

    /// Marked once: false if it already was
    bool MarkForDeletion() {
        return retro_atomic_exchange_int(&deleting, 1) == 0;
    }

    /// Held by others than the caller
    bool IsBusy() {
        return obj_table_users(parent_ctx->UserContexts(), head.index) > 1;
    }

    s32 GetId() {
        return static_cast<s32>(head.id);
    }

    OrbisUserServiceUserId GetUserId() {
        return user_id;
    }

    s32 GetHttpTemplateId() {
        return http_template_id;
    }

    s32 Initialize();

    void CheckTimeout();

    s32 CreatePushEventCallback(s32 filter_id, OrbisNpWebApi2PushEventCallback cb_func,
                                void* user_arg);
    s32 DeletePushEventCallback(s32 callback_id);

    s32 CreatePushContext(OrbisNpWebApi2PushEventPushContextId* push_ctx_id);
    PushEventPushContext* GetPushContext(const OrbisNpWebApi2PushEventPushContextId* push_ctx_id);
    void DeletePushContext(PushEventPushContext* push_ctx_id);

    s32 CreatePushContextCallback(s32 filter_id, OrbisNpWebApi2PushEventPushContextCallback cb_func,
                                  void* user_arg);
    s32 DeletePushContextCallback(s32 callback_id);

    s32 CreateRequest(const char* api_group, const char* path, const char* method,
                      const OrbisNpWebApi2ContentParameter* content_parameter, bool multipart,
                      Request** request);
    Request* GetRequest(s64 request_id);
    bool HasBusyRequests();
    void AbortAllRequests();

    obj_table_t* Requests() {
        return &requests;
    }

    /// Held by the caller: off its context, freed with all it holds as the last holder lets go
    void Delete();

private:
    OrbisUserServiceUserId user_id{};
    s32 http_template_id{};
    retro_atomic_int_t deleting{0};
    LibraryContext* parent_ctx{};
    std::string user_agent{};
    obj_table_t requests{};
    obj_table_t push_event_callbacks{};
    obj_table_t push_contexts{};
    obj_table_t push_context_callbacks{};
};

class Request {
public:
    obj_head_t head{}; // first: its id, its slot and how it is freed
    obj_table_t* home{};

    Request(LibraryContext* parent, const char* api_group, const char* path, const char* method,
            bool multipart, const OrbisNpWebApi2ContentParameter* content_parameter)
        : content_length(content_parameter ? content_parameter->content_length : 0),
          multipart_supported(multipart), parent_ctx(parent), api_group(api_group), path(path),
          method(method), content_type(content_parameter ? content_parameter->content_type : "") {
        head.destroy = Destroy;
    }
    /// Its headers, and its HTTP request unless one took it (TakeHttpRequestId)
    ~Request();

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<Request*>(object);
    }

    void AddUser() {
        (void)obj_table_hold(home, head.index);
    }

    void RemoveUser() {
        obj_table_put_back(home, &head);
    }

    s64 GetId() {
        return head.id;
    }

    /// Its HTTP request, for the caller to delete: the request no longer does
    s32 TakeHttpRequestId() {
        const s32 taken = http_request_id;
        http_request_id = 0;
        return taken;
    }

    s32 GetHttpRequestId() {
        return http_request_id;
    }

    bool IsMultipart() {
        return multipart_supported;
    }

    bool HasSent() {
        return sent;
    }

    void MarkSent() {
        sent = true;
    }

    bool Aborted() {
        return retro_atomic_load_acquire_int(&aborted) != 0;
    }

    bool Expired() {
        return retro_atomic_load_acquire_int(&expired) != 0;
    }

    bool OutOfData() {
        return content_length == sent_data;
    }

    void SetEndTime() {
        const u32 t = static_cast<u32>(retro_atomic_load_acquire_int(&timeout));
        if (t != 0) {
            (void)retro_atomic_cas_64(&end_time, 0, static_cast<s64>(NowUs() + t));
        }
    }

    void ClearEndTime() {
        retro_atomic_store_release_64(&end_time, 0);
    }

    std::string& GetApiGroup() {
        return api_group;
    }

    std::string& GetPath() {
        return path;
    }

    std::string& GetMethod() {
        return method;
    }

    std::string& GetContentType() {
        return content_type;
    }

    /// A full barrier: Abort sees it, or this thread's next look sees the abort
    void SetState(s32 state) {
        (void)retro_atomic_exchange_int(&send_state, state);
    }

    bool IsDeleted() {
        return retro_atomic_load_acquire_int(&deleting) != 0;
    }

    /// Marked once: false if it already was
    bool MarkForDeletion() {
        return retro_atomic_exchange_int(&deleting, 1) == 0;
    }

    /// Held by others than the caller
    bool IsBusy() {
        return obj_table_users(home, head.index) > 1;
    }

    s32 AddHttpRequestHeader(const char* field_name, const char* field_value);

    void SetTimeout(u32 new_timeout) {
        retro_atomic_store_release_int(&timeout, static_cast<int>(new_timeout));
    }

    void CheckTimeout() {
        const u64 end = static_cast<u64>(retro_atomic_load_acquire_64(&end_time));
        if (end != 0 && end < NowUs() && retro_atomic_exchange_int(&expired, 1) == 0) {
            Abort();
        }
    }

    s32 CreateHttpRequest(s32 http_template_id, const char* url);
    s32 SendHttpRequest(void* data, u64 data_size);
    s32 GetAllHttpResponseHeaders();
    s32 ParseHttpResponseHeaders(const char* field_name, char* value, u64 value_size,
                                 u64* value_size_out);
    s32 Abort();

private:
    u64 content_length{};
    char* http_response_headers{};
    u64 http_response_header_size{};
    s32 http_request_id{};
    retro_atomic_int_t timeout{0};
    s32 sent_data{};
    retro_atomic_int_t send_state{0};
    bool multipart_supported{};
    bool sent{};
    retro_atomic_int_t aborted{0};
    retro_atomic_int_t expired{0};
    retro_atomic_int_t deleting{0};
    retro_atomic_64_t end_time{0};
    LibraryContext* parent_ctx{};
    std::string api_group{};
    std::string path{};
    std::string method{};
    std::string content_type{};

    struct HttpRequestHeader {
        std::string field_name;
        std::string field_value;
    };
    std::vector<HttpRequestHeader*> http_headers{};
};

}; // namespace Libraries::Np::NpWebApi2