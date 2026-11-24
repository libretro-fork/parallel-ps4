/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* libSceFiber's context switches on the host (x86-64 GNU assembler; the FEX build switches on
 * the guest's registers instead, in fiber.c). The offsets are OrbisFiberContext's and
 * OrbisFiberData's, in fiber.h. */

#include "common/types.h"

#if defined(__x86_64__) && !defined(SHADPS4_ENABLE_FEX_GUEST_CPU)

__asm__(".att_syntax prefix\n"
        ".global _sceFiberSetJmp\n"
        "_sceFiberSetJmp:\n"
        "    movq %rax, 0x0(%rdi)\n"

        "    movq (%rsp), %rdx\n"
        "    movq %rdx, 0x10(%rdi)\n"

        "    movq %rcx, 0x08(%rdi)\n"
        "    movq %rbx, 0x18(%rdi)\n"
        "    movq %rsp, 0x20(%rdi)\n"
        "    movq %rbp, 0x28(%rdi)\n"

        "    movq %r8,  0x30(%rdi)\n"
        "    movq %r9,  0x38(%rdi)\n"
        "    movq %r10, 0x40(%rdi)\n"
        "    movq %r11, 0x48(%rdi)\n");

__asm__("    movq %r12, 0x50(%rdi)\n"
        "    movq %r13, 0x58(%rdi)\n"
        "    movq %r14, 0x60(%rdi)\n"
        "    movq %r15, 0x68(%rdi)\n"

        "    fnstcw  0x70(%rdi)\n"
        "    stmxcsr 0x72(%rdi)\n"

        "    xor %eax, %eax\n"
        "    ret\n"

        ".global _sceFiberLongJmp\n"
        "_sceFiberLongJmp:\n"
        /* MXCSR = (MXCSR & 0x3f) ^ (ctx->mxcsr & ~0x3f) */
        "    stmxcsr -0x4(%rsp)\n"
        "    movl 0x72(%rdi), %eax\n"
        "    andl $0xffffffc0, %eax\n"
        "    movl -0x4(%rsp), %ecx\n"
        "    andl $0x3f, %ecx\n");

__asm__("    xorl %eax, %ecx\n"
        "    movl %ecx, -0x4(%rsp)\n"
        "    ldmxcsr -0x4(%rsp)\n"

        "    movq 0x00(%rdi), %rax\n"
        "    movq 0x08(%rdi), %rcx\n"
        "    movq 0x10(%rdi), %rdx\n"
        "    movq 0x18(%rdi), %rbx\n"
        "    movq 0x20(%rdi), %rsp\n"
        "    movq 0x28(%rdi), %rbp\n"

        "    movq 0x30(%rdi), %r8\n"
        "    movq 0x38(%rdi), %r9\n"
        "    movq 0x40(%rdi), %r10\n"
        "    movq 0x48(%rdi), %r11\n"
        "    movq 0x50(%rdi), %r12\n");

__asm__("    movq 0x58(%rdi), %r13\n"
        "    movq 0x60(%rdi), %r14\n"
        "    movq 0x68(%rdi), %r15\n"

        "    fldcw 0x70(%rdi)\n"

        /* Make the jump and return 1 */
        "    movq %rdx, 0x00(%rsp)\n"
        "    movl $0x1, %eax\n"
        "    ret\n"

        ".global _sceFiberSwitchEntry\n"
        "_sceFiberSwitchEntry:\n"
        "    mov %rdi, %r11\n"

        /* Set stack address to provided stack */
        "    movq 0x18(%r11), %rsp\n"
        "    xorl %ebp, %ebp\n"

        "    movq 0x20(%r11), %r10\n" /* data->state */

        /* Set previous fiber state to Idle */
        "    test %r10, %r10\n"
        "    jz .clear_regs\n");

__asm__("    movl $2, (%r10)\n"

        ".clear_regs:\n"
        "    test %esi, %esi\n"
        "    jz .skip_fpu_regs\n"

        "    ldmxcsr 0x2c(%r11)\n"
        "    fldcw 0x28(%r11)\n"

        ".skip_fpu_regs:\n"
        "    movq 0x08(%r11), %rdi\n" /* data->arg_on_initialize */
        "    movq 0x10(%r11), %rsi\n" /* data->arg_on_run_to */
        "    movq 0x00(%r11), %r11\n" /* data->entry */

        "    xorl %eax, %eax\n"
        "    xorl %ebx, %ebx\n"
        "    xorl %ecx, %ecx\n");

__asm__("    xorl %edx, %edx\n"
        "    xorq %r8, %r8\n"
        "    xorq %r9, %r9\n"
        "    xorq %r10, %r10\n"
        "    xorq %r12, %r12\n"
        "    xorq %r13, %r13\n"
        "    xorq %r14, %r14\n"
        "    xorq %r15, %r15\n"
        "    pxor %mm0, %mm0\n"
        "    pxor %mm1, %mm1\n"
        "    pxor %mm2, %mm2\n"
        "    pxor %mm3, %mm3\n"
        "    pxor %mm4, %mm4\n"
        "    pxor %mm5, %mm5\n"
        "    pxor %mm6, %mm6\n"
        "    pxor %mm7, %mm7\n"
        "    emms\n"
        "    vzeroall\n");

__asm__(/* Call the fiber's entry function: entry(arg_on_initialize, arg_on_run_to) */
        "    call *%r11\n"

        /* Fiber returned, not good */
        "    movl $1, %edi\n"
        "    call _sceFiberForceQuit\n"
        "    ret\n");

#endif
