/* SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceFiber. On x86-64 a switch is a host setjmp/longjmp (fiber_context.c); with FEX the
 * guest's registers of the HLE call that switches are set aside and replaced. */

#include <retro_atomic.h>
#include <stdio.h>
#include <string.h>

#include "common/c_bridge.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/fiber/fiber.h"
#include "core/libraries/fiber/fiber_error.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/tls_c.h"
#ifdef _WIN32
#include "common/combine.h"
#include "common/run_once.h"
#endif
#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
#include "core/guest_cpu/hle_frame_c.h"
#endif

#define FIBER_SIGNATURE0 0xdef1649cu
#define FIBER_SIGNATURE1 0xb37592a0u
#define FIBER_OPT_SIGNATURE 0xbb40e64du
#define FIBER_STACK_SIGNATURE 0x7149f2ca7149f2caull
#define FIBER_STACK_SIZE_CHECK 0xdeadbeefdeadbeefull

/* Common::ElfInfo::FW_350: from this SDK on, a fiber starts with the FPU registers set */
#define FIBER_FW_350 0x3500000u

static retro_atomic_int_t context_size_check = RETRO_ATOMIC_INT_INITIALIZER(0);

static OrbisFiberContext* fiber_get_context(void) {
    return *core_tcb_fiber();
}

static int fiber_magic_ok(const OrbisFiber* fiber) {
    return fiber->magic_start == FIBER_SIGNATURE0 && fiber->magic_end == FIBER_SIGNATURE1;
}

#ifndef SHADPS4_ENABLE_FEX_GUEST_CPU
/* In fiber_context.c. _sceFiberSetJmp returns 0, and 1 again when _sceFiberLongJmp is given
 * its context. */
s32 PS4_SYSV_ABI _sceFiberSetJmp(OrbisFiberContext* ctx) __asm__("_sceFiberSetJmp")
    __attribute__((returns_twice));
s32 PS4_SYSV_ABI _sceFiberLongJmp(OrbisFiberContext* ctx) __asm__("_sceFiberLongJmp");
void PS4_SYSV_ABI _sceFiberSwitchEntry(OrbisFiberData* data,
                                       u32 set_fpu) __asm__("_sceFiberSwitchEntry");
void PS4_SYSV_ABI _sceFiberForceQuit(u64 ret) __asm__("_sceFiberForceQuit");

/* Where a fiber's entry returns to (from _sceFiberSwitchEntry) */
__attribute__((used)) void PS4_SYSV_ABI _sceFiberForceQuit(u64 ret) {
    OrbisFiberContext* g_ctx = fiber_get_context();
    g_ctx->return_val = ret;
    _sceFiberLongJmp(g_ctx);
}
#endif

#ifdef _WIN32
/* Windows puts the record of a fault on the stack of the thread that faulted, so a fiber cannot
 * run on game memory the GPU caches may write-protect: a fault there finds no room for its
 * record and ends the process. A fiber runs on a stack of its own from the threads' stack
 * allocator instead, one for each context the game gives (fibers given the same context share
 * it), in a fixed table the combiner runs on one at a time. */
#define FIBER_STACKS_MAX 1024

typedef struct fiber_stack {
    uintptr_t start; /* the game's context */
    u64 size;
    void* stack;
    u32 num_fibers;
} fiber_stack_t;

static fiber_stack_t g_fiber_stacks[FIBER_STACKS_MAX];
static u32 g_fiber_stack_count;
static combiner_t g_fiber_stack_lock;
static run_once_t g_fiber_stack_once = RUN_ONCE_INIT;

typedef struct fiber_stack_call {
    void* addr_context;
    u64 size_context;
    const OrbisFiber* fiber;
    void* stack;
} fiber_stack_call_t;

static void fiber_stack_init_once(void* arg) {
    (void)arg;
    combiner_init(&g_fiber_stack_lock);
}

static void fiber_stack_remove(u32 i) {
    kernel_fiber_stack_free(g_fiber_stacks[i].stack, g_fiber_stacks[i].size);
    g_fiber_stacks[i] = g_fiber_stacks[--g_fiber_stack_count];
}

static void fiber_acquire_stack_locked(void* ctx) {
    fiber_stack_call_t* c = (fiber_stack_call_t*)ctx;
    const uintptr_t start = (uintptr_t)c->addr_context;
    u32 i = 0;
    while (i < g_fiber_stack_count) {
        fiber_stack_t* s = &g_fiber_stacks[i];
        if (s->start < start + c->size_context && start < s->start + s->size) {
            if (s->start == start && s->size == c->size_context) {
                s->num_fibers++;
                c->stack = s->stack;
                return;
            }
            /* A PS4 would overwrite the older context, so no fiber runs on it anymore; this
             * also frees the stacks of fibers never finalized */
            fiber_stack_remove(i);
            continue;
        }
        i++;
    }
    c->stack = NULL;
    if (g_fiber_stack_count == FIBER_STACKS_MAX) {
        c_log(C_LOG_LIB_FIBER, C_LOG_LEVEL_ERROR, "fiber_acquire_stack",
              "%d fiber stacks in use: this fiber runs on its context", FIBER_STACKS_MAX);
        return;
    }
    c->stack = kernel_fiber_stack_create(c->size_context);
    if (c->stack != NULL) {
        fiber_stack_t* s = &g_fiber_stacks[g_fiber_stack_count++];
        s->start = start;
        s->size = c->size_context;
        s->stack = c->stack;
        s->num_fibers = 1;
    }
}

static void fiber_release_stack_locked(void* ctx) {
    const fiber_stack_call_t* c = (const fiber_stack_call_t*)ctx;
    u32 i;
    for (i = 0; i < g_fiber_stack_count; i++) {
        fiber_stack_t* s = &g_fiber_stacks[i];
        if (s->start == (uintptr_t)c->fiber->addr_context) {
            if (s->stack == c->fiber->context_start && --s->num_fibers == 0) {
                fiber_stack_remove(i);
            }
            return;
        }
    }
}
#endif

/* Where a fiber given @addr_context runs: on Windows a stack of its own, else the context */
static void* fiber_acquire_stack(void* addr_context, u64 size_context) {
#ifdef _WIN32
    fiber_stack_call_t c;
    run_once(&g_fiber_stack_once, fiber_stack_init_once, NULL);
    c.addr_context = addr_context;
    c.size_context = size_context;
    c.fiber = NULL;
    c.stack = NULL;
    combiner_run(&g_fiber_stack_lock, fiber_acquire_stack_locked, &c);
    return c.stack != NULL ? c.stack : addr_context;
#else
    (void)size_context;
    return addr_context;
#endif
}

/* A finalized fiber's stack goes back once no fiber uses it */
static void fiber_release_stack(const OrbisFiber* fiber) {
#ifdef _WIN32
    fiber_stack_call_t c;
    if (fiber->context_start == fiber->addr_context) {
        return; /* on its context */
    }
    run_once(&g_fiber_stack_once, fiber_stack_init_once, NULL);
    c.addr_context = NULL;
    c.size_context = 0;
    c.fiber = fiber;
    c.stack = NULL;
    combiner_run(&g_fiber_stack_lock, fiber_release_stack_locked, &c);
#else
    (void)fiber;
#endif
}

/* Stops the emulator: the fiber's stack of @stack_size bytes overflowed */
static void fiber_stack_overflow(u64 stack_size) {
    char msg[80];
    if (stack_size >> 32) {
        snprintf(msg, sizeof(msg), "Stack overflow detected in fiber with size = 0x%x%08x",
                 (unsigned)(stack_size >> 32), (unsigned)(stack_size & 0xffffffffu));
    } else {
        snprintf(msg, sizeof(msg), "Stack overflow detected in fiber with size = 0x%x",
                 (unsigned)stack_size);
    }
    c_unreachable(__FILE__, __LINE__, "_sceFiberCheckStackOverflow", msg);
}

static void PS4_SYSV_ABI _sceFiberCheckStackOverflow(OrbisFiberContext* ctx) {
    const OrbisFiber* fiber = ctx->current_fiber;
    if (fiber->addr_context && *(u64*)fiber->context_start != FIBER_STACK_SIGNATURE) {
        fiber_stack_overflow(ctx->current_fiber->size_context);
    }
}

/* The stack's first word signed, and with the size check the rest filled with its pattern */
static void fiber_sign_stack(OrbisFiber* fiber, int size_check) {
    *(u64*)fiber->context_start = FIBER_STACK_SIGNATURE;
    if (size_check) {
        u64* stack_ptr = (u64*)fiber->context_start + 1;
        u64* stack_end = (u64*)fiber->context_end;
        while (stack_ptr < stack_end) {
            *stack_ptr++ = FIBER_STACK_SIZE_CHECK;
        }
    }
}

static s32 PS4_SYSV_ABI _sceFiberAttachContext(OrbisFiber* fiber, void* addr_context,
                                               u64 size_context) {
    if (size_context && size_context < ORBIS_FIBER_CONTEXT_MINIMUM_SIZE) {
        return ORBIS_FIBER_ERROR_RANGE;
    }
    if (size_context & 15) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (!addr_context || !size_context) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (fiber->addr_context) {
        return ORBIS_FIBER_ERROR_INVALID;
    }

    fiber->addr_context = addr_context;
    fiber->size_context = size_context;
    fiber->context_start = fiber_acquire_stack(addr_context, size_context);
    fiber->context_end = (u8*)fiber->context_start + size_context;
    fiber_sign_stack(fiber, (fiber->flags & ORBIS_FIBER_FLAG_CONTEXT_SIZE_CHECK) != 0);
    return ORBIS_OK;
}

/* The checks the run, switch and attach calls start with */
static s32 fiber_check_target(OrbisFiber* fiber, void* addr_context) {
    if (!fiber) {
        return ORBIS_FIBER_ERROR_NULL;
    }
    if ((u64)(uintptr_t)fiber & 7 || (u64)(uintptr_t)addr_context & 15) {
        return ORBIS_FIBER_ERROR_ALIGNMENT;
    }
    if (!fiber_magic_ok(fiber)) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    return ORBIS_OK;
}

#ifndef SHADPS4_ENABLE_FEX_GUEST_CPU
/* entry(arg_on_initialize, arg_on_run_to) started on @stack_addr; does not return */
static void fiber_switch_entry(OrbisFiber* fiber, u64 arg_on_run_to, void* stack_addr,
                               u32* prev_state) {
    OrbisFiberData data;
    memset(&data, 0, sizeof(data));
    data.entry = fiber->entry;
    data.arg_on_initialize = fiber->arg_on_initialize;
    data.arg_on_run_to = arg_on_run_to;
    data.stack_addr = stack_addr;
    data.state = prev_state;
    if (fiber->flags & ORBIS_FIBER_FLAG_SET_FPU_REGS) {
        data.fpucw = 0x037f;
        data.mxcsr = 0x9fc0;
        _sceFiberSwitchEntry(&data, 1);
    } else {
        _sceFiberSwitchEntry(&data, 0);
    }
    __builtin_trap();
}

static void PS4_SYSV_ABI _sceFiberSwitchToFiber(OrbisFiber* fiber, u64 arg_on_run_to,
                                                OrbisFiberContext* ctx) {
    OrbisFiberContext* fiber_ctx = fiber->context;
    u32* prev_state = NULL;
    if (fiber_ctx) {
        ctx->arg_on_run_to = arg_on_run_to;
        _sceFiberLongJmp(fiber_ctx);
        __builtin_trap();
    }

    if (ctx->prev_fiber) {
        OrbisFiber* prev_fiber = ctx->prev_fiber;
        ctx->prev_fiber = NULL;
        prev_state = (u32*)&prev_fiber->state;
    }
    fiber_switch_entry(fiber, arg_on_run_to, (u8*)fiber->context_end, prev_state);
}

static void PS4_SYSV_ABI _sceFiberSwitch(OrbisFiber* cur_fiber, OrbisFiber* fiber,
                                         u64 arg_on_run_to, OrbisFiberContext* ctx) {
    ctx->prev_fiber = cur_fiber;
    ctx->current_fiber = fiber;

    if (fiber->addr_context == NULL) {
        ctx->prev_fiber = NULL;
        fiber_switch_entry(fiber, arg_on_run_to, (void*)(uintptr_t)(ctx->rsp & ~(u64)15),
                           (u32*)&cur_fiber->state);
    }

    _sceFiberSwitchToFiber(fiber, arg_on_run_to, ctx);
    __builtin_trap();
}

static void PS4_SYSV_ABI _sceFiberTerminate(OrbisFiber* fiber, u64 arg_on_return,
                                            OrbisFiberContext* ctx) {
    (void)fiber;
    ctx->arg_on_return = arg_on_return;
    _sceFiberLongJmp(ctx);
    __builtin_trap();
}
#endif

s32 PS4_SYSV_ABI sceFiberInitializeImpl(OrbisFiber* fiber, const char* name, OrbisFiberEntry entry,
                                        u64 arg_on_initialize, void* addr_context, u64 size_context,
                                        const OrbisFiberOptParam* opt_param, u32 flags,
                                        u32 build_ver) {
    u32 user_flags;
    if (!fiber || !name || !entry) {
        return ORBIS_FIBER_ERROR_NULL;
    }
    if ((u64)(uintptr_t)fiber & 7 || (u64)(uintptr_t)addr_context & 15) {
        return ORBIS_FIBER_ERROR_ALIGNMENT;
    }
    if (opt_param && (u64)(uintptr_t)opt_param & 7) {
        return ORBIS_FIBER_ERROR_ALIGNMENT;
    }
    if (size_context && size_context < ORBIS_FIBER_CONTEXT_MINIMUM_SIZE) {
        return ORBIS_FIBER_ERROR_RANGE;
    }
    if (size_context & 15) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (!addr_context && size_context) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (addr_context && !size_context) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (opt_param && opt_param->magic != FIBER_OPT_SIGNATURE) {
        return ORBIS_FIBER_ERROR_INVALID;
    }

    user_flags = flags;
    if (build_ver >= FIBER_FW_350) {
        user_flags |= ORBIS_FIBER_FLAG_SET_FPU_REGS;
    }
    if ((u32)retro_atomic_load_seq_cst_int(&context_size_check)) {
        user_flags |= ORBIS_FIBER_FLAG_CONTEXT_SIZE_CHECK;
    }

    strncpy(fiber->name, name, ORBIS_FIBER_MAX_NAME_LENGTH);

    fiber->entry = entry;
    fiber->arg_on_initialize = arg_on_initialize;
    fiber->addr_context = addr_context;
    fiber->size_context = size_context;
    fiber->context = NULL;
    fiber->flags = user_flags;

    /* A low stack area is problematic, as we can easily cause a stack overflow with our HLE. */
    if (size_context && size_context <= 4096) {
        c_log(C_LOG_LIB_FIBER, C_LOG_LEVEL_WARNING, "sceFiberInitializeImpl",
              "Fiber initialized with small stack area.");
    }

    fiber->magic_start = FIBER_SIGNATURE0;
    fiber->magic_end = FIBER_SIGNATURE1;

    if (addr_context != NULL) {
        fiber->context_start = fiber_acquire_stack(addr_context, size_context);
        fiber->context_end = (u8*)fiber->context_start + size_context;
        /* The caller's flags, not those the context size check added */
        fiber_sign_stack(fiber, (flags & ORBIS_FIBER_FLAG_CONTEXT_SIZE_CHECK) != 0);
    }

    (void)retro_atomic_exchange_int(&fiber->state, ORBIS_FIBER_STATE_IDLE);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberOptParamInitialize(OrbisFiberOptParam* opt_param) {
    if (!opt_param) {
        return ORBIS_FIBER_ERROR_NULL;
    }
    if ((u64)(uintptr_t)opt_param & 7) {
        return ORBIS_FIBER_ERROR_ALIGNMENT;
    }

    opt_param->magic = FIBER_OPT_SIGNATURE;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberFinalize(OrbisFiber* fiber) {
    if (!fiber) {
        return ORBIS_FIBER_ERROR_NULL;
    }
    if ((u64)(uintptr_t)fiber & 7) {
        return ORBIS_FIBER_ERROR_ALIGNMENT;
    }
    if (!fiber_magic_ok(fiber)) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (!retro_atomic_cas_int(&fiber->state, ORBIS_FIBER_STATE_IDLE,
                              ORBIS_FIBER_STATE_TERMINATED)) {
        return ORBIS_FIBER_ERROR_STATE;
    }
    if (fiber->addr_context) {
        fiber_release_stack(fiber);
    }
    return ORBIS_OK;
}

#ifndef SHADPS4_ENABLE_FEX_GUEST_CPU
s32 PS4_SYSV_ABI sceFiberRunImpl(OrbisFiber* fiber, void* addr_context, u64 size_context,
                                 u64 arg_on_run_to, u64* arg_on_return) {
    OrbisFiberContext** tcb_fiber;
    OrbisFiberContext ctx;
    OrbisFiber* cur_fiber;
    s32 res = fiber_check_target(fiber, addr_context);
    if (res != ORBIS_OK) {
        return res;
    }

    tcb_fiber = core_tcb_fiber();
    if (*tcb_fiber) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }

    /* Caller wants to attach context and run. */
    if (addr_context != NULL || size_context != 0) {
        res = _sceFiberAttachContext(fiber, addr_context, size_context);
        if (res < 0) {
            return res;
        }
    }

    if (!retro_atomic_cas_int(&fiber->state, ORBIS_FIBER_STATE_IDLE, ORBIS_FIBER_STATE_RUN)) {
        return ORBIS_FIBER_ERROR_STATE;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.current_fiber = fiber;
    ctx.prev_fiber = NULL;
    ctx.return_val = 0;

    *tcb_fiber = &ctx;

    if (!_sceFiberSetJmp(&ctx)) {
        if (fiber->addr_context) {
            _sceFiberSwitchToFiber(fiber, arg_on_run_to, &ctx);
            __builtin_trap();
        }
        fiber_switch_entry(fiber, arg_on_run_to, (void*)(uintptr_t)(ctx.rsp & ~(u64)15), NULL);
    }

    cur_fiber = ctx.current_fiber;
    ctx.current_fiber = NULL;
    (void)retro_atomic_exchange_int(&cur_fiber->state, ORBIS_FIBER_STATE_IDLE);

    if (ctx.return_val != 0) {
        /* Fiber entry returned! This should never happen. */
        c_unreachable(__FILE__, __LINE__, "sceFiberRunImpl", "Fiber entry function returned.");
    }

    if (arg_on_return) {
        *arg_on_return = ctx.arg_on_return;
    }

    *core_tcb_fiber() = NULL;
    return ORBIS_OK;
}

/* What a fiber's own switch or return-to-thread call does when it is resumed */
static void fiber_resumed(u64* arg_on_run) {
    OrbisFiberContext* g_ctx = fiber_get_context();
    if (g_ctx->prev_fiber) {
        (void)retro_atomic_exchange_int(&g_ctx->prev_fiber->state, ORBIS_FIBER_STATE_IDLE);
        g_ctx->prev_fiber = NULL;
    }
    if (arg_on_run) {
        *arg_on_run = g_ctx->arg_on_run_to;
    }
}

s32 PS4_SYSV_ABI sceFiberSwitchImpl(OrbisFiber* fiber, void* addr_context, u64 size_context,
                                    u64 arg_on_run_to, u64* arg_on_run) {
    OrbisFiberContext* g_ctx;
    OrbisFiberContext ctx;
    OrbisFiber* cur_fiber;
    s32 res = fiber_check_target(fiber, addr_context);
    if (res != ORBIS_OK) {
        return res;
    }

    g_ctx = fiber_get_context();
    if (!g_ctx) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }

    /* Caller wants to attach context and switch. */
    if (addr_context != NULL || size_context != 0) {
        res = _sceFiberAttachContext(fiber, addr_context, size_context);
        if (res < 0) {
            return res;
        }
    }

    if (!retro_atomic_cas_int(&fiber->state, ORBIS_FIBER_STATE_IDLE, ORBIS_FIBER_STATE_RUN)) {
        return ORBIS_FIBER_ERROR_STATE;
    }

    cur_fiber = g_ctx->current_fiber;
    if (cur_fiber->addr_context == NULL) {
        _sceFiberSwitch(cur_fiber, fiber, arg_on_run_to, g_ctx);
        __builtin_trap();
    }

    memset(&ctx, 0, sizeof(ctx));
    if (!_sceFiberSetJmp(&ctx)) {
        cur_fiber->context = &ctx;
        _sceFiberCheckStackOverflow(g_ctx);
        _sceFiberSwitch(cur_fiber, fiber, arg_on_run_to, g_ctx);
        __builtin_trap();
    }

    fiber_resumed(arg_on_run);
    return ORBIS_OK;
}
#endif

s32 PS4_SYSV_ABI sceFiberGetSelf(OrbisFiber** fiber) {
    OrbisFiberContext* g_ctx;
    if (!fiber) {
        return ORBIS_FIBER_ERROR_NULL;
    }

    g_ctx = fiber_get_context();
    if (!g_ctx) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }

    *fiber = g_ctx->current_fiber;
    return ORBIS_OK;
}

#ifndef SHADPS4_ENABLE_FEX_GUEST_CPU
s32 PS4_SYSV_ABI sceFiberReturnToThread(u64 arg_on_return, u64* arg_on_run) {
    OrbisFiberContext* g_ctx = fiber_get_context();
    OrbisFiber* cur_fiber;
    OrbisFiberContext ctx;
    if (!g_ctx) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }

    cur_fiber = g_ctx->current_fiber;
    if (cur_fiber->addr_context) {
        memset(&ctx, 0, sizeof(ctx));
        if (_sceFiberSetJmp(&ctx)) {
            fiber_resumed(arg_on_run);
            return ORBIS_OK;
        }

        cur_fiber->context = &ctx;
        _sceFiberCheckStackOverflow(g_ctx);
    }

    _sceFiberTerminate(cur_fiber, arg_on_return, g_ctx);
    __builtin_trap();
    return ORBIS_OK;
}
#endif

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
/* The game's code runs through FEX, so a fiber switch cannot be a host setjmp/longjmp: the
 * guest's registers of the HLE call that switches (sceFiberRun, sceFiberSwitch,
 * sceFiberReturnToThread) are set aside, and the other context's put in their place; the
 * veneer's ret then continues there. What a context's own call does after its longjmp
 * returns (the previous fiber idle, arg_on_run written) is done when it is resumed.
 *
 * What is set aside is kept on the guest's own stack, just below the call's stack pointer,
 * where the console's library and the host's setjmp keep their context (the veneer, a
 * syscall and a ret, keeps nothing there): nothing is allocated. */

#define FIBER_GPR_COUNT 16
#define FIBER_REG_RSP 4
#define FIBER_REG_RBP 5
#define FIBER_REG_RSI 6
#define FIBER_REG_RDI 7

/* A fiber's call set aside, to be returned from when it is switched to again */
typedef struct fiber_suspended_call {
    u64 gpr[FIBER_GPR_COUNT];
    u64* arg_on_run;
} fiber_suspended_call;

/* A thread's fibers while sceFiberRun runs them; tcb_fiber points at ctx */
typedef struct fiber_thread_fibers {
    OrbisFiberContext ctx;
    u64 run_call[FIBER_GPR_COUNT]; /* the sceFiberRun call, returned from at the end */
    u64* arg_on_return;
} fiber_thread_fibers;

/* Room for @size bytes on the guest's stack below @rsp, 16-byte aligned */
static void* fiber_stack_room(u64 rsp, u64 size) {
    return (void*)(uintptr_t)((rsp - size) & ~(u64)15);
}

/* entry(arg_on_initialize, arg_on_run_to) on a stack, as _sceFiberSwitchEntry starts it */
static void fiber_start_entry(u64* gpr, OrbisFiberEntry entry, u64 arg_on_initialize,
                              u64 arg_on_run_to, u64 stack_top, retro_atomic_int_t* prev_state) {
    u64 rsp;
    if (prev_state) {
        (void)retro_atomic_exchange_int(prev_state, ORBIS_FIBER_STATE_IDLE);
    }
    rsp = stack_top & ~(u64)15;
    rsp -= 8;
    *(u64*)(uintptr_t)rsp = core_hle_fiber_exit_address();
    rsp -= 8;
    *(u64*)(uintptr_t)rsp = (u64)(uintptr_t)entry;
    memset(gpr, 0, FIBER_GPR_COUNT * sizeof(u64));
    gpr[FIBER_REG_RSP] = rsp; /* the veneer's ret goes to entry, with the exit above */
    gpr[FIBER_REG_RDI] = arg_on_initialize;
    gpr[FIBER_REG_RSI] = arg_on_run_to;
}

static void fiber_suspend_current(u64* gpr, OrbisFiber* fiber, u64* arg_on_run) {
    fiber_suspended_call* call =
        (fiber_suspended_call*)fiber_stack_room(gpr[FIBER_REG_RSP], sizeof(fiber_suspended_call));
    /* No room above the stack's signature: what is below is not the fiber's */
    if ((u8*)call < (u8*)fiber->context_start + 8) {
        fiber_stack_overflow(fiber->size_context);
    }
    memcpy(call->gpr, gpr, sizeof(call->gpr));
    call->arg_on_run = arg_on_run;
    fiber->context = (OrbisFiberContext*)call;
}

/* Back into a fiber's set-aside call, which then returns ORBIS_OK */
static void fiber_resume(u64* gpr, OrbisFiber* fiber, OrbisFiberContext* g_ctx) {
    fiber_suspended_call* call = (fiber_suspended_call*)fiber->context;
    fiber->context = NULL;
    memcpy(gpr, call->gpr, sizeof(call->gpr));
    if (g_ctx->prev_fiber) {
        (void)retro_atomic_exchange_int(&g_ctx->prev_fiber->state, ORBIS_FIBER_STATE_IDLE);
        g_ctx->prev_fiber = NULL;
    }
    if (call->arg_on_run) {
        *call->arg_on_run = g_ctx->arg_on_run_to;
    }
}

/* Back into the thread's sceFiberRun call, which then returns ORBIS_OK */
static void fiber_resume_thread(u64* gpr, OrbisFiberContext* g_ctx) {
    fiber_thread_fibers* fibers = (fiber_thread_fibers*)g_ctx;
    OrbisFiber* cur_fiber = g_ctx->current_fiber;
    memcpy(gpr, fibers->run_call, sizeof(fibers->run_call));
    g_ctx->current_fiber = NULL;
    (void)retro_atomic_exchange_int(&cur_fiber->state, ORBIS_FIBER_STATE_IDLE);
    if (g_ctx->return_val != 0) {
        c_unreachable(__FILE__, __LINE__, "ResumeThread", "Fiber entry function returned.");
    }
    if (fibers->arg_on_return) {
        *fibers->arg_on_return = g_ctx->arg_on_return;
    }
    *core_tcb_fiber() = NULL;
}

static void fiber_switch_to(u64* gpr, OrbisFiber* fiber, u64 arg_on_run_to,
                            OrbisFiberContext* g_ctx) {
    retro_atomic_int_t* prev_state = NULL;
    if (fiber->context) {
        g_ctx->arg_on_run_to = arg_on_run_to;
        fiber_resume(gpr, fiber, g_ctx);
        return;
    }
    if (g_ctx->prev_fiber) {
        prev_state = &g_ctx->prev_fiber->state;
        g_ctx->prev_fiber = NULL;
    }
    fiber_start_entry(gpr, fiber->entry, fiber->arg_on_initialize, arg_on_run_to,
                      (u64)(uintptr_t)fiber->context_end, prev_state);
}

static void fiber_switch(u64* gpr, OrbisFiber* cur_fiber, OrbisFiber* fiber, u64 arg_on_run_to,
                         OrbisFiberContext* g_ctx) {
    g_ctx->prev_fiber = cur_fiber;
    g_ctx->current_fiber = fiber;
    if (fiber->addr_context == NULL) {
        /* On the thread's stack, below its sceFiberRun call */
        g_ctx->prev_fiber = NULL;
        fiber_start_entry(gpr, fiber->entry, fiber->arg_on_initialize, arg_on_run_to, g_ctx->rsp,
                          &cur_fiber->state);
        return;
    }
    fiber_switch_to(gpr, fiber, arg_on_run_to, g_ctx);
}

void PS4_SYSV_ABI fiber_entry_returned(void) {
    u64* gpr = core_hle_call_frame_gpr();
    OrbisFiberContext* g_ctx = fiber_get_context();
    C_ASSERT_MSG(gpr && g_ctx, "FiberEntryReturned", "A fiber entry returned outside a fiber");
    g_ctx->return_val = 1;
    fiber_resume_thread(gpr, g_ctx);
}

s32 PS4_SYSV_ABI sceFiberRunImpl(OrbisFiber* fiber, void* addr_context, u64 size_context,
                                 u64 arg_on_run_to, u64* arg_on_return) {
    OrbisFiberContext** tcb_fiber;
    fiber_thread_fibers* fibers;
    u64* gpr;
    s32 res = fiber_check_target(fiber, addr_context);
    if (res != ORBIS_OK) {
        return res;
    }
    tcb_fiber = core_tcb_fiber();
    if (*tcb_fiber) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }
    gpr = core_hle_call_frame_gpr();
    if (!gpr) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }
    if (addr_context != NULL || size_context != 0) {
        res = _sceFiberAttachContext(fiber, addr_context, size_context);
        if (res < 0) {
            return res;
        }
    }
    if (!retro_atomic_cas_int(&fiber->state, ORBIS_FIBER_STATE_IDLE, ORBIS_FIBER_STATE_RUN)) {
        return ORBIS_FIBER_ERROR_STATE;
    }

    /* Below the call on the thread's stack; a fiber without a stack of its own runs below it */
    fibers =
        (fiber_thread_fibers*)fiber_stack_room(gpr[FIBER_REG_RSP], sizeof(fiber_thread_fibers));
    memset(fibers, 0, sizeof(*fibers));
    fibers->ctx.current_fiber = fiber;
    fibers->arg_on_return = arg_on_return;
    memcpy(fibers->run_call, gpr, sizeof(fibers->run_call));
    fibers->ctx.rsp = (u64)(uintptr_t)fibers;
    fibers->ctx.rbp = gpr[FIBER_REG_RBP];
    *tcb_fiber = &fibers->ctx;

    if (fiber->addr_context) {
        fiber_switch_to(gpr, fiber, arg_on_run_to, &fibers->ctx);
    } else {
        fiber_start_entry(gpr, fiber->entry, fiber->arg_on_initialize, arg_on_run_to,
                          fibers->ctx.rsp, NULL);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberSwitchImpl(OrbisFiber* fiber, void* addr_context, u64 size_context,
                                    u64 arg_on_run_to, u64* arg_on_run) {
    OrbisFiberContext* g_ctx;
    OrbisFiber* cur_fiber;
    u64* gpr;
    s32 res = fiber_check_target(fiber, addr_context);
    if (res != ORBIS_OK) {
        return res;
    }
    g_ctx = fiber_get_context();
    gpr = core_hle_call_frame_gpr();
    if (!g_ctx || !gpr) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }
    if (addr_context != NULL || size_context != 0) {
        res = _sceFiberAttachContext(fiber, addr_context, size_context);
        if (res < 0) {
            return res;
        }
    }
    if (!retro_atomic_cas_int(&fiber->state, ORBIS_FIBER_STATE_IDLE, ORBIS_FIBER_STATE_RUN)) {
        return ORBIS_FIBER_ERROR_STATE;
    }

    cur_fiber = g_ctx->current_fiber;
    /* One on the thread's stack cannot be come back to: its stack is reused */
    if (cur_fiber->addr_context != NULL) {
        fiber_suspend_current(gpr, cur_fiber, arg_on_run);
        _sceFiberCheckStackOverflow(g_ctx);
    }
    fiber_switch(gpr, cur_fiber, fiber, arg_on_run_to, g_ctx);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberReturnToThread(u64 arg_on_return, u64* arg_on_run) {
    OrbisFiberContext* g_ctx = fiber_get_context();
    u64* gpr = core_hle_call_frame_gpr();
    OrbisFiber* cur_fiber;
    if (!g_ctx || !gpr) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }
    cur_fiber = g_ctx->current_fiber;
    if (cur_fiber->addr_context) {
        fiber_suspend_current(gpr, cur_fiber, arg_on_run);
        _sceFiberCheckStackOverflow(g_ctx);
    }
    g_ctx->arg_on_return = arg_on_return;
    fiber_resume_thread(gpr, g_ctx);
    return ORBIS_OK;
}
#endif

s32 PS4_SYSV_ABI sceFiberGetInfo(OrbisFiber* fiber, OrbisFiberInfo* fiber_info) {
    if (!fiber || !fiber_info) {
        return ORBIS_FIBER_ERROR_NULL;
    }
    if ((u64)(uintptr_t)fiber & 7 || (u64)(uintptr_t)fiber_info & 7) {
        return ORBIS_FIBER_ERROR_ALIGNMENT;
    }
    if (fiber_info->size != sizeof(OrbisFiberInfo)) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (!fiber_magic_ok(fiber)) {
        return ORBIS_FIBER_ERROR_INVALID;
    }

    fiber_info->entry = fiber->entry;
    fiber_info->arg_on_initialize = fiber->arg_on_initialize;
    fiber_info->addr_context = fiber->addr_context;
    fiber_info->size_context = fiber->size_context;
    strncpy(fiber_info->name, fiber->name, ORBIS_FIBER_MAX_NAME_LENGTH);

    fiber_info->size_context_margin = (u64)-1;
    if (fiber->flags & ORBIS_FIBER_FLAG_CONTEXT_SIZE_CHECK && fiber->addr_context != NULL) {
        u64 stack_margin = 0;
        u64* stack_start = (u64*)fiber->context_start;
        u64* stack_end = (u64*)fiber->context_end;

        if (*stack_start == FIBER_STACK_SIGNATURE) {
            /* The words from the bottom the stack has not reached still hold the pattern */
            u64* stack_ptr = stack_start + 1;
            while (stack_ptr < stack_end && *stack_ptr == FIBER_STACK_SIZE_CHECK) {
                stack_ptr++;
            }
            stack_margin = (u64)((u8*)stack_ptr - (u8*)(stack_start + 1));
        }

        fiber_info->size_context_margin = stack_margin;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberStartContextSizeCheck(u32 flags) {
    if (flags != 0) {
        return ORBIS_FIBER_ERROR_INVALID;
    }
    if (!retro_atomic_cas_int(&context_size_check, 0, 1)) {
        return ORBIS_FIBER_ERROR_STATE;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberStopContextSizeCheck(void) {
    if (!retro_atomic_cas_int(&context_size_check, 1, 0)) {
        return ORBIS_FIBER_ERROR_STATE;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberRename(OrbisFiber* fiber, const char* name) {
    if (!fiber || !name) {
        return ORBIS_FIBER_ERROR_NULL;
    }
    if ((u64)(uintptr_t)fiber & 7) {
        return ORBIS_FIBER_ERROR_ALIGNMENT;
    }
    if (!fiber_magic_ok(fiber)) {
        return ORBIS_FIBER_ERROR_INVALID;
    }

    strncpy(fiber->name, name, ORBIS_FIBER_MAX_NAME_LENGTH);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberGetThreadFramePointerAddress(u64* addr_frame_pointer) {
    OrbisFiberContext* g_ctx;
    if (!addr_frame_pointer) {
        return ORBIS_FIBER_ERROR_NULL;
    }

    g_ctx = fiber_get_context();
    if (!g_ctx) {
        return ORBIS_FIBER_ERROR_PERMISSION;
    }

    *addr_frame_pointer = g_ctx->rbp;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceFiberInitialize(OrbisFiber* fiber, const char* name, OrbisFiberEntry entry,
                                    u64 arg_on_initialize, void* addr_context, u64 size_context,
                                    const OrbisFiberOptParam* opt_param, u32 build_ver) {
    return sceFiberInitializeImpl(fiber, name, entry, arg_on_initialize, addr_context, size_context,
                                  opt_param, 0, build_ver);
}

s32 PS4_SYSV_ABI sceFiberRun(OrbisFiber* fiber, u64 arg_on_run_to, u64* arg_on_return) {
    return sceFiberRunImpl(fiber, NULL, 0, arg_on_run_to, arg_on_return);
}

s32 PS4_SYSV_ABI sceFiberSwitch(OrbisFiber* fiber, u64 arg_on_run_to, u64* arg_on_run) {
    return sceFiberSwitchImpl(fiber, NULL, 0, arg_on_run_to, arg_on_run);
}
