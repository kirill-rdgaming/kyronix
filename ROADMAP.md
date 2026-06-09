# Kyronix — roadmap

Target: x86-64 kernel that can run statically linked userspace binaries
via POSIX syscalls.

- [ ] = not done
- [x] = done

---

## Phase 1 — Segments & interrupts

- [x] GDT: null, kernel code/data (ring 0), user code/data (ring 3), TSS
- [x] IDT: 256 vectors, load with `lidt`
- [x] ISR dispatch: asm wrapper → `cpu_state_t` → C handler table
- [x] PIC 8259: remap master (0x20) / slave (0x28), mask all
- [x] Exception stubs: #PF, #GP, #DE, #SS — log + halt

## Phase 2 — Memory management

- [x] PMM: bitmap + free-stack frame allocator
- [x] VMM: map/unmap, page alloc, higher-half takeover from Limine
- [x] Heap: `kmalloc`/`kfree` on top of page allocator

## Phase 3 — Timer

- [ ] PIT at 1000 Hz (or HPET)
- [ ] IRQ0 → tick counter → scheduler entry
- [ ] (WIP) `sleep()` / `block_for()` primitives (nanosleep stub only)

## Phase 4 — Scheduler & kernel threads

- [x] `struct proc` (regs, stack, state, page table pointer)
- [x] Context switch (callee-saved regs, RSP, CR3)
- [x] Cooperative `sched_yield_blocking()`
- [ ] Preemptive scheduler (timer IRQ)
- [ ] Kernel threads

## Phase 5 — Userspace & syscall entry

- [x] `sysretq` to ring 3 (user SS/RSP/RFLAGS/CS/RIP on stack)
- [x] User page tables (U/S bit)
- [x] `syscall`/`sysret` MSRs (STAR, LSTAR, SF_MASK)
- [x] Syscall dispatch table (RAX → handler, 60+ syscalls)
- [x] SSE context restore on syscall entry/exit

## Phase 6 — ELF loader

- [x] ELF64 parser (magic, program headers, segment mapping)
- [x] `process_exec`: buffer → VMM mappings → ring 3 entry
- [x] User stack setup with argv/envp/aux vectors
- [x] Test binary (`user/init.S`) run as `/init`

## Phase 7 — VFS & initramfs

- [x] `struct vfs_node`, `struct vfs_file`, chr-dev callbacks
- [ ] Mount table (single ramfs root only)
- [x] initramfs: embedded cpio (newc format) → ramfs root
- [x] Device nodes: `/dev/null`, `/dev/zero`, `/dev/tty`, `/dev/fd`
- [x] `/dev/stdin`, `/dev/stdout`, `/dev/stderr`

## Phase 8 — POSIX syscalls

### Process control
- [x] `exit` / `exit_group`
- [x] `fork` (full address space clone)
- [x] `execve` (ELF load + argv/envp)
- [x] `wait4` (blocking child reaping)
- [x] `getpid` / `getppid` / `getpgrp` / `setsid`

### File I/O
- [x] `open` / `openat`
- [x] `close`
- [x] `read` / `write` / `pread64`
- [x] `lseek`
- [x] `dup` / `dup2` / `dup3`
- [x] `fcntl`
- [x] `ioctl` (TIOCGWINSZ, TCGETS/TCSETS, TIOCGPGRP/SPGRP)
- [x] `pipe` / `pipe2`
- [x] `stat` / `fstat` / `lstat`
- [x] `getdents64`
- [x] `readlink`
- [x] `chdir` / `getcwd`

### Memory
- [x] `brk`
- [x] `mmap` (anonymous, MAP_FIXED, MAP_ANON)
- [ ] (WIP) `munmap` (unmaps but does not free physical pages)
- [ ] (WIP) `mprotect` (no-op)

### Signals
- [x] `rt_sigaction`
- [x] `rt_sigprocmask`
- [x] `rt_sigreturn`
- [x] `kill` / `tkill` / `tgkill`
- [x] Signal delivery frame (`rt_sigframe_t` on user stack)
- [x] Default actions (fatal with core, ignore, stop/cont)
- [ ] `sigaltstack` (no-op)

### Other
- [x] `uname` (Linux-compatible fields)
- [x] `arch_prctl` (ARCH_SET_FS/GS, ARCH_GET_FS/GS)
- [x] `getrandom`
- [x] `umask`
- [ ] (WIP) `nanosleep` (no-op)
- [ ] (WIP) `futex` (FUTEX_WAIT/WEAKE stubs, no blocking)
- [ ] (WIP) `gettimeofday` / `clock_gettime` / `times` (return zero)
- [ ] (WIP) `getrlimit` / `prlimit64` (return unlimited)
- [ ] (WIP) `statfs` / `fstatfs` (return zeroed data)
- [ ] (WIP) `select` / `pselect6` / `poll` / `ppoll` (no-op / always-ready)
- [ ] `socket` / networking

## Not in scope yet

- SMP / APIC
- PCI / AHCI / NVMe
- Networking
- Dynamic linking
- Full permission model / procfs
- Arbitrary executable path (currently load from VFS only)
