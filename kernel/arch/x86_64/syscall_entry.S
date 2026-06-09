.code64
.section .text

#define CPU_KERNEL_RSP   0
#define CPU_USER_RSP     8

.global syscall_entry
syscall_entry:
    swapgs                              /* GS.base -> cpu_local_t *           */
    movq    %rsp, %gs:CPU_USER_RSP      /* save user RSP                     */
    movq    %gs:CPU_KERNEL_RSP, %rsp    /* sweitch to kernel stack            */

    pushq   %r15
    pushq   %r14
    pushq   %r13
    pushq   %r12
    pushq   %r11        /* user RFLAGS */
    pushq   %r10
    pushq   %r9
    pushq   %r8
    pushq   %rbp
    pushq   %rdi
    pushq   %rsi
    pushq   %rdx
    pushq   %rcx        /* user RIP */
    pushq   %rbx
    pushq   %rax

    movq    %rsp, %rdi
    call    syscall_dispatch

    popq    %rax
    popq    %rbx
    popq    %rcx        /* user RIP */
    popq    %rdx
    popq    %rsi
    popq    %rdi
    popq    %rbp
    popq    %r8
    popq    %r9
    popq    %r10
    popq    %r11        /* user RFLAGS */
    popq    %r12
    popq    %r13
    popq    %r14
    popq    %r15

    movq    %gs:CPU_USER_RSP, %rsp
    swapgs
    sysretq

.global enter_userspace
enter_userspace:
    movq    %rdi, %rcx
    movq    %rdx, %r11
    movq    %rsi, %rsp

    xorq    %rax, %rax
    xorq    %rbx, %rbx
    xorq    %rdx, %rdx
    xorq    %rdi, %rdi
    xorq    %rsi, %rsi
    xorq    %rbp, %rbp
    xorq    %r8,  %r8
    xorq    %r9,  %r9
    xorq    %r10, %r10
    xorq    %r12, %r12
    xorq    %r13, %r13
    xorq    %r14, %r14
    xorq    %r15, %r15

    sysretq

.global enter_userspace_exec
enter_userspace_exec:
    movq    %rdi, %rcx
    movq    %rdx, %r11
    movq    %rsi, %rsp

    xorq    %rax, %rax
    xorq    %rbx, %rbx
    xorq    %rdx, %rdx
    xorq    %rdi, %rdi
    xorq    %rsi, %rsi
    xorq    %rbp, %rbp
    xorq    %r8,  %r8
    xorq    %r9,  %r9
    xorq    %r10, %r10
    xorq    %r12, %r12
    xorq    %r13, %r13
    xorq    %r14, %r14
    xorq    %r15, %r15

    swapgs
    sysretq
