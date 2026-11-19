// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <retro_atomic.h>
#include "common/obj_table.h"

#include "common/logging/log.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/np/np_types.h"
#include "core/libraries/np/np_web_api2/np_web_api2.h"

namespace Libraries::Np::NpWebApi2 {

class UserContext;

/// The game's time in microseconds, as sceKernelGetProcessTime counted
inline u64 NowUs() {
    return Libraries::Kernel::GameNow() / 1000;
}

/// Held through its table (obj_table.c): found, held and given back without a lock. Its flags
/// and timer are atomic words, set and read by any thread.
class PushEventHandle {
public:
    obj_head_t head{}; // first: its id, its slot and how it is freed
    obj_table_t* home{};

    PushEventHandle() = default;

    s32 GetId() {
        return static_cast<s32>(head.id);
    }

    void SetTimeout(u32 new_timeout) {
        retro_atomic_store_release_int(&timeout, static_cast<int>(new_timeout));
    }

    void AddUser() {
        (void)obj_table_hold(home, head.index);
    }

    void RemoveUser() {
        obj_table_put_back(home, &head);
    }

    void Abort() {
        retro_atomic_store_release_int(&aborted, 1);
    }

    bool IsAborted() {
        return retro_atomic_load_acquire_int(&aborted) != 0;
    }

    /// Marked once: false if it already was
    bool MarkForDeletion() {
        return retro_atomic_exchange_int(&deleting, 1) == 0;
    }

    bool IsDeleted() {
        return retro_atomic_load_acquire_int(&deleting) != 0;
    }

    void SetEndTime() {
        const u32 t = static_cast<u32>(retro_atomic_load_acquire_int(&timeout));
        if (t != 0) {
            retro_atomic_store_release_64(&end_time, static_cast<s64>(NowUs() + t));
        }
    }

    void ClearEndTime() {
        retro_atomic_store_release_64(&end_time, 0);
    }

    u64 GetEndTime() {
        return static_cast<u64>(retro_atomic_load_acquire_64(&end_time));
    }

    bool IsTimedout() {
        return retro_atomic_load_acquire_int(&timed_out) != 0;
    }

    void CheckTimeout(u64 time) {
        const u64 end = GetEndTime();
        if (end != 0 && end < time && retro_atomic_exchange_int(&timed_out, 1) == 0) {
            Abort();
        }
    }

    /// Held by others than the caller
    bool IsBusy() {
        return obj_table_users(home, head.index) > 1;
    }

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<PushEventHandle*>(object);
    }

private:
    retro_atomic_int_t timeout{0};
    retro_atomic_int_t aborted{0};
    retro_atomic_int_t deleting{0};
    retro_atomic_int_t timed_out{0};
    retro_atomic_64_t end_time{0};
};

class PushEventFilter {
public:
    obj_head_t head{}; // first: its id, its slot and how it is freed
    obj_table_t* home{};

    PushEventFilter(const char* service_name, OrbisNpServiceLabel service_label, bool is_internal)
        : np_service_label(service_label), internal(is_internal),
          np_service_name(service_name ? service_name : "") {}

    void RemoveUser() {
        obj_table_put_back(home, &head);
    }

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<PushEventFilter*>(object);
    }

    s32 Initialize(PushEventHandle* handle,
                   const OrbisNpWebApi2PushEventFilterParameter* filter_param,
                   u64 filter_param_num);

    s32 GetId() {
        return static_cast<s32>(head.id);
    }

private:
    OrbisNpServiceLabel np_service_label{};
    bool internal{};
    std::vector<OrbisNpWebApi2PushEventFilterParameter> filter_params{};
    std::vector<std::vector<OrbisNpWebApi2PushEventExtdDataKey>> data_key_copy_storage{};
    std::string np_service_name{};
};

struct PushEventCallback {
    obj_head_t head; // first: its id, its slot and how it is freed
    s32 filter_id;
    OrbisNpWebApi2PushEventCallback cb_func;
    void* user_arg;

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<PushEventCallback*>(object);
    }
};

class PushEventPushContext {
public:
    obj_head_t head{}; // first: its slot and how it is freed; found by its own id
    obj_table_t* home{};

    PushEventPushContext(u32 new_timeout, UserContext* user_ctx)
        : timeout(new_timeout), parent_user_ctx(user_ctx) {}

    void RemoveUser() {
        obj_table_put_back(home, &head);
    }

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<PushEventPushContext*>(object);
    }

    void Initialize();

    OrbisNpWebApi2PushEventPushContextId* GetId() {
        return &id;
    }

    // Returns the actual numerical id currently stored in id.
    s64 GetFakeId() {
        s64 fake_id{};
        std::memcpy(&fake_id, &id, sizeof(s64));
        return fake_id;
    }

    void Start() {
        started = true;
    }

    /// A deleter waiting for a running callback is woken as the state changes
    void SetState(s32 new_state) {
        retro_atomic_store_release_int(&state, new_state);
        retro_eventcount_notify(obj_table_released());
    }

    bool CallbackRunning() {
        return retro_atomic_load_acquire_int(&state) == 2;
    }

private:
    retro_atomic_int_t state{0};
    u32 timeout{};
    UserContext* parent_user_ctx{};
    u64 end_time{};
    u64 start_time{};
    OrbisNpWebApi2PushEventPushContextId id{};
    bool started{};
};

struct PushEventPushContextCallback {
    obj_head_t head; // first: its id, its slot and how it is freed
    s32 filter_id;
    OrbisNpWebApi2PushEventPushContextCallback cb_func;
    void* user_arg;

    static void Destroy(obj_head_t* object) {
        delete reinterpret_cast<PushEventPushContextCallback*>(object);
    }
};

}; // namespace Libraries::Np::NpWebApi2