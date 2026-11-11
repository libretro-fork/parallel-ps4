// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <retro_atomic.h>
#include "common/combiner.h"
#include "core/libraries/kernel/kernel.h"
#include "core/libraries/kernel/posix_error.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/libs.h"
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
#include "core/guest_cpu/guest_callback.h"
#endif

namespace Libraries::Kernel {

static constexpr u32 PthreadKeysMax = 256;
static constexpr u32 PthreadDestructorIterations = 4;

static std::array<PthreadKey, PthreadKeysMax> ThreadKeytable{};
static Common::Combiner KeytableCombiner;

int PS4_SYSV_ABI posix_pthread_key_create(PthreadKeyT* key, PthreadKeyDestructor destructor) {
    return KeytableCombiner.Run([&]() -> int {
        const auto it = std::ranges::find_if(ThreadKeytable, [](const PthreadKey& k) {
            return retro_atomic_load_seq_cst_int(&k.allocated) == 0;
        });
        if (it != ThreadKeytable.end()) {
            (void)retro_atomic_exchange_int(&it->allocated, 1);
            it->destructor = destructor;
            retro_atomic_fetch_add_int(&it->seqno, 1);
            *key = static_cast<PthreadKeyT>(std::distance(ThreadKeytable.begin(), it));
            return 0;
        }
        return POSIX_EAGAIN;
    });
}

int PS4_SYSV_ABI posix_pthread_key_delete(PthreadKeyT key) {
    if (key >= PthreadKeysMax) {
        return POSIX_EINVAL;
    }

    return KeytableCombiner.Run([&]() -> int {
        if (!retro_atomic_load_seq_cst_int(&ThreadKeytable[key].allocated)) {
            return POSIX_EINVAL;
        }

        (void)retro_atomic_exchange_int(&ThreadKeytable[key].allocated, 0);
        return 0;
    });
}

void _thread_cleanupspecific() {
    Pthread* curthread = g_curthread;
    PthreadKeyDestructor destructor;
    const void* data = nullptr;

    if (curthread->specific == nullptr) {
        return;
    }

    for (int i = 0; (i < PthreadDestructorIterations) && (curthread->specific_data_count > 0);
         i++) {
        for (int key = 0; (key < PthreadKeysMax) && (curthread->specific_data_count > 0); key++) {
            destructor = nullptr;

            // The table is read in its combiner; the destructor runs outside it
            KeytableCombiner.Run([&] {
                if (retro_atomic_load_seq_cst_int(&ThreadKeytable[key].allocated) != 0 &&
                    (curthread->specific[key].data != nullptr)) {
                    if (curthread->specific[key].seqno ==
                        retro_atomic_load_seq_cst_int(&ThreadKeytable[key].seqno)) {
                        data = curthread->specific[key].data;
                        destructor = ThreadKeytable[key].destructor;
                    }
                    curthread->specific[key].data = nullptr;
                    curthread->specific_data_count--;
                } else if (curthread->specific[key].data != nullptr) {
                    /*
                     * This can happen if the key is deleted via
                     * pthread_key_delete without first setting the value
                     * to NULL in all threads.  POSIX says that the
                     * destructor is not invoked in this case.
                     */
                    curthread->specific[key].data = nullptr;
                    curthread->specific_data_count--;
                }
            });

            /*
             * If there is a destructor, call it
             * outside the key table's combiner:
             */
            if (destructor != nullptr) {
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
                Core::GuestCpu::RunGuestFunctionOrAbort(
                    reinterpret_cast<void*>(destructor), "pthread key destructor", data);
#else
                destructor(data);
#endif
            }
        }
    }
    delete[] curthread->specific;
    curthread->specific = nullptr;
    if (curthread->specific_data_count > 0) {
        LOG_WARNING(Lib_Kernel, "Thread has exited with leftover thread-specific data");
    }
}

int PS4_SYSV_ABI posix_pthread_setspecific(PthreadKeyT key, const void* value) {
    Pthread* pthread = g_curthread;

    if (!pthread->specific) {
        pthread->specific = new (std::nothrow) PthreadSpecificElem[PthreadKeysMax]{};
        if (pthread->specific == nullptr) {
            return POSIX_ENOMEM;
        }
    }
    if (key >= PthreadKeysMax) {
        return POSIX_EINVAL;
    }
    if (!retro_atomic_load_seq_cst_int(&ThreadKeytable[key].allocated)) {
        return POSIX_EINVAL;
    }

    if (pthread->specific[key].data == nullptr) {
        if (value != nullptr) {
            pthread->specific_data_count++;
        }
    } else if (value == nullptr) {
        pthread->specific_data_count--;
    }
    pthread->specific[key].data = value;
    pthread->specific[key].seqno = retro_atomic_load_seq_cst_int(&ThreadKeytable[key].seqno);
    return 0;
}

const void* PS4_SYSV_ABI posix_pthread_getspecific(PthreadKeyT key) {
    Pthread* pthread = g_curthread;

    if (!pthread->specific || key >= PthreadKeysMax) {
        return nullptr;
    }

    if (retro_atomic_load_seq_cst_int(&ThreadKeytable[key].allocated) != 0 &&
        (pthread->specific[key].seqno ==
         retro_atomic_load_seq_cst_int(&ThreadKeytable[key].seqno))) {
        return pthread->specific[key].data;
    }

    return nullptr;
}

void RegisterSpec(Core::Loader::SymbolsResolver* sym) {
    // Posix
    LIB_FUNCTION("mqULNdimTn0", "libScePosix", 1, "libkernel", posix_pthread_key_create);
    LIB_FUNCTION("6BpEZuDT7YI", "libScePosix", 1, "libkernel", posix_pthread_key_delete);
    LIB_FUNCTION("0-KXaS70xy4", "libScePosix", 1, "libkernel", posix_pthread_getspecific);
    LIB_FUNCTION("WrOLvHU0yQM", "libScePosix", 1, "libkernel", posix_pthread_setspecific);

    // Posix-Kernel
    LIB_FUNCTION("mqULNdimTn0", "libkernel", 1, "libkernel", posix_pthread_key_create);
    LIB_FUNCTION("0-KXaS70xy4", "libkernel", 1, "libkernel", posix_pthread_getspecific);
    LIB_FUNCTION("WrOLvHU0yQM", "libkernel", 1, "libkernel", posix_pthread_setspecific);

    // Orbis
    LIB_FUNCTION("geDaqgH9lTg", "libkernel", 1, "libkernel", ORBIS(posix_pthread_key_create));
    LIB_FUNCTION("PrdHuuDekhY", "libkernel", 1, "libkernel", ORBIS(posix_pthread_key_delete));
    LIB_FUNCTION("eoht7mQOCmo", "libkernel", 1, "libkernel", posix_pthread_getspecific);
    LIB_FUNCTION("+BzXYkqYeLE", "libkernel", 1, "libkernel", ORBIS(posix_pthread_setspecific));
}

} // namespace Libraries::Kernel
