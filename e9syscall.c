/*
 *        ___                          _ _ 
 *   ___ / _ \ ___ _   _ ___  ___ __ _| | |
 *  / _ \ (_) / __| | | / __|/ __/ _` | | |
 * |  __/\__, \__ \ |_| \__ \ (_| (_| | | |
 *  \___|  /_/|___/\__, |___/\___\__,_|_|_|
 *                 |___/ 
 *
 * Copyright (C) 2020 National University of Singapore
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "e9syscall.h"

#define STRING(x)   STRING_2(x)
#define STRING_2(x) #x

/*
 * System call implementation.
 */
asm
(
    ".globl syscall\n"
    "syscall:\n"

    // Disallow syscalls that MUST execute in the original context:
    "cmp $" STRING(__NR_rt_sigreturn) ",%eax\n"
    "je .Lno_sys\n"
    "cmp $" STRING(__NR_clone) ",%eax\n"
    "je .Lno_sys\n"

    // Convert SYSV -> SYSCALL ABI:
    "mov %edi,%eax\n"
    "mov %rsi,%rdi\n"
    "mov %rdx,%rsi\n"
    "mov %rcx,%rdx\n"
    "mov %r8,%r10\n"
    "mov %r9,%r8\n"
    "mov 0x8(%rsp),%r9\n"

    // Execute the system call:
    "syscall\n"

    "retq\n"

    // Handle errors:
    ".Lno_sys:\n"
    "mov $-" STRING(ENOSYS) ",%eax\n"
    "retq\n"
);

/*
 * Entry point.
 */
asm
(
    // Arguments:
    // %rdi       = old %rsp
    // %rsi       = address of the next instruction.
    // 0x10(%rsp) = old %rsi
    // 0x08(%rsp) = old %rdi

    ".globl intercept\n"
    "intercept:\n"

    // Save the callno:
    "push %rax\n"

    // Save the old %rsp and next address:
    "push %rdi\n"   // Push old %rsp
    "push %rsi\n"   // Push next

    // Save state:
    "push %rdx\n"
    "push %r8\n"
    "push %r10\n"

    // Setup hook() arguments:
    "sub $8,%rsp\n"
    "push %rsp\n"
    "push %r9\n"
    "mov %r8,%r9\n"
    "mov %r10,%r8\n"
    "mov %rdx,%rcx\n"
    "mov 0x10+9*8(%rsp),%rdx\n"
    "mov 0x08+9*8(%rsp),%rsi\n"
    "mov %rax,%rdi\n"

    // Call hook():
    "callq hook\n"

    // Restore state:
    "mov 0x10+9*8(%rsp),%rsi\n"
    "mov 0x08+9*8(%rsp),%rdi\n"
    "pop %r9\n"
    "add $16,%rsp\n"
    "pop %r10\n"
    "pop %r8\n"
    "pop %rdx\n"

    // Load the old %rsp and next address into %rcx/%r11
    // (%rcx, %r11 are registers that syscall can clobber)
    "pop %rcx\n"    // Pop next
    "pop %r11\n"    // Pop old %rsp

    // Test if we need to execute the original syscall:
    "test %eax,%eax\n"
    "jne .Lsyscall\n"

    // The original syscall has been replaced:
    "mov -6*8(%rsp),%rax\n"
    "mov %r11,%rsp\n"
    "jmpq *%rcx\n"
    "ud2\n"

    // The original syscall should be executed:
    ".Lsyscall:\n"
    "pop %rax\n"    // Restore the callno
    "retq\n"
);

