.code64
.section .text

#define PROC_SPACE      16
#define PROC_KSTACK_TOP 32
#define PROC_KSTACK_RSP 40
#define PROC_USER_RSP   48

#define CPU_KERNEL_RSP  0
#define CPU_USER_RSP    8

.global sched_switch
sched_switch:
    pushq   %rbx
    pushq   %rbp
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    movq    g_current_proc(%rip), %rax
    movq    %rsp,            PROC_KSTACK_RSP(%rax)

    movq    %gs:CPU_USER_RSP, %rcx
    movq    %rcx,            PROC_USER_RSP(%rax)

    movq    %rdi,            g_current_proc(%rip)

    movq    PROC_USER_RSP(%rdi),    %rcx
    movq    %rcx,                   %gs:CPU_USER_RSP

    movq    PROC_KSTACK_TOP(%rdi),  %rcx
    movq    %rcx,                   %gs:CPU_KERNEL_RSP

    movq    PROC_SPACE(%rdi),       %rdi
    call    vmm_switch

    /* rax clobbered by vmm_switch */
    movq    g_current_proc(%rip),   %rax
    movq    PROC_KSTACK_RSP(%rax),  %rsp

    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbp
    popq    %rbx
    retq                            /* retjrn into next's context */


.global proc_resume_frame
proc_resume_frame:
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
