# Testrunner ROADMAP: Maximum Userspace Test Coverage

## Overview

Current coverage: **6 tests** (pipe_dup2_exec, ls_grep_pipeline, grep_o, tiocgwinsz, exec_fail, basic_syscalls).

The test framework runs as PID 1 in QEMU. Tests are C functions returning 1 (PASS) or 0 (FAIL). Available utilities in `test_rootfs` must be explicitly listed in `Makefile` (currently: `ls`, `grep`, `echo`, `fetch`, `reboot`).

Goals:
- Cover every kernel syscall (350+ syscalls, many are stubs — document each)
- Cover every kyrobox utility
- Cover the shell (ksh) features
- Cover edge cases and error paths

---

## Phase 1: Infrastructure Improvements

| # | Task | Effort | Priority |
|---|------|--------|----------|
| 1.1 | Add `cat`, `sort`, `wc`, `cut`, `head`, `tail`, `tee` to test_rootfs | small | high |
| 1.2 | Add `mkdir`, `rm`, `rmdir`, `touch`, `cp`, `mv`, `ln`, `chmod` to test_rootfs | small | high |
| 1.3 | Add `sed`, `tr`, `uniq`, `test`, `printf`, `true`, `false` to test_rootfs | small | high |
| 1.4 | Add `kill`, `sleep`, `sync`, `pwd`, `which`, `env` to test_rootfs | small | medium |
| 1.5 | Add `ksh` (shell) to test_rootfs for shell-related tests | medium | medium |
| 1.6 | Add `vi` to test_rootfs for editor tests | large | low |
| 1.7 | Create test helper library: `test_harness.h` (shared setup, assertions, temp files, process helpers) | medium | high |
| 1.8 | Add `setenv("HOME", "/root", 1)` in main() — some programs need a home dir | small | high |
| 1.9 | Implement a `TMPDIR`-based temp file system using ramfs (`/tmp`) for file operation tests | small | high |

---

## Phase 2: File System Syscalls

| # | Syscall / Feature | Test Scenarios | Effort |
|---|-------------------|----------------|--------|
| 2.1 | `open` / `close` | open existing, open nonexistent (ENOENT), open O_RDONLY/O_WRONLY/O_RDWR/O_CREAT/O_TRUNC/O_APPEND, open with mode, close invalid fd (EBADF) | small |
| 2.2 | `read` / `write` | read from file, read past EOF (=0), write to file, write to read-only fd (EBADF), write NULL buf (EFAULT) | small |
| 2.3 | `lseek` | SEEK_SET, SEEK_CUR, SEEK_END, SEEK_DATA, SEEK_HOLE, invalid whence (EINVAL) | small |
| 2.4 | `stat` / `fstat` / `lstat` | stat file, stat dir, stat symlink, stat nonexistent (ENOENT), fstat fd | small |
| 2.5 | `access` | F_OK, R_OK, W_OK, X_OK, nonexistent (ENOENT) | small |
| 2.6 | `creat` | create new file, overwrite existing, error on dir | small |
| 2.7 | `truncate` / `ftruncate` | truncate to smaller size, enlarge (zero-fill), truncate read-only (EINVAL) | small |
| 2.8 | `link` / `unlink` | hard link, unlink regular, unlink nonexistent (ENOENT), unlink dir (EPERM) | small |
| 2.9 | `symlink` / `readlink` | symlink create, readlink, follow vs nofollow in stat | small |
| 2.10 | `rename` | rename file, rename across dirs, rename nonexistent (ENOENT), rename to existing (overwrite) | small |
| 2.11 | `chdir` / `fchdir` / `getcwd` | chdir to existing, chdir nonexistent (ENOENT), fchdir, getcwd buffer size | small |
| 2.12 | `mkdir` / `rmdir` | mkdir, mkdir existing (EEXIST), rmdir empty, rmdir non-empty (ENOTEMPTY), rmdir nonexistent (ENOENT) | small |
| 2.13 | `getdents64` | read directory entries, nonexistent fd (EBADF) | small |
| 2.14 | `chmod` / `fchmod` | change mode bits, fchmod on fd | small |
| 2.15 | `chown` / `fchown` / `lchown` | change owner/group, fchown, lchown (no follow) | medium |
| 2.16 | `umask` | get/set umask, verify effect on creat | small |
| 2.17 | `fcntl` | F_DUPFD, F_GETFD, F_SETFD, F_GETFL, F_SETFL | small |
| 2.18 | `mknod` | create device node (EPERM? ramfs support?) | small |
| 2.19 | `statfs` / `fstatfs` | filesystem info, fstatfs | small |
| 2.20 | `openat` / `mkdirat` / `fstatat` / `unlinkat` / `renameat` / `linkat` / `symlinkat` / `readlinkat` / `fchmodat` / `faccessat` | modern *at variants, AT_FDCWD usage, dirfd-based ops | medium |
| 2.21 | `pread64` / `pwrite64` | positional read/write without changing file offset | small |
| 2.22 | `readv` / `writev` | vectored I/O, iovcnt=0, iov with NULL base (EFAULT) | medium |
| 2.23 | `copy_file_range` | copy between file descriptors, infd=outfd (EINVAL) | medium |
| 2.24 | `memfd_create` | anonymous memory fd, read/write/mmap on memfd, sealing | medium |
| 2.25 | `sendfile` | zero-copy between fds | small |
| 2.26 | `flock` | LOCK_SH, LOCK_EX, LOCK_UN, LOCK_NB | small |
| 2.27 | `fsync` / `fdatasync` | sync file (no-op on ramfs but should succeed) | small |
| 2.28 | `fallocate` | preallocate (no-op but check return) | small |
| 2.29 | `utime` / `utimes` / `utimensat` | set file timestamps (no-op but check return) | small |
| 2.30 | `statx` | modern stat interface, STATX_ALL, STATX_BASIC_STATS | medium |

---

## Phase 3: Process & Scheduling Syscalls

| # | Syscall / Feature | Test Scenarios | Effort |
|---|-------------------|----------------|--------|
| 3.1 | `fork` | basic fork, child gets correct PID, parent/child share fd table at fork, COW semantics (write in child), fork bomb limit | small |
| 3.2 | `vfork` | basic vfork/exec pattern, vfork with blocking child | small |
| 3.3 | `clone` | CLONE_VM, CLONE_VFORK, CLONE_THREAD (if threads are supported) | medium |
| 3.4 | `execve` / `execlp` | exec valid binary, exec with argv/envp, exec with bad binary (ENOEXEC), exec with interpreter (#!) | small |
| 3.5 | `exit` | exit(0), exit(1), exit(42) — check WEXITSTATUS in parent | small |
| 3.6 | `exit_group` | exit all threads (same as exit if single thread) | small |
| 3.7 | `wait4` / `waitid` | wait for child, WNOHANG, WUNTRACED, wait for specific PID, wait for nonexistent child (ECHILD) | small |
| 3.8 | `getpid` / `getppid` / `gettid` | PID values, PPID correctness after fork, TID vs PID | small |
| 3.9 | `getuid` / `geteuid` / `getgid` / `getegid` | uid/gid values (currently root) | small |
| 3.10 | `setuid` / `setgid` / `setreuid` / `setregid` / `setresuid` / `setresgid` / `getresuid` / `getresgid` | get/set uid/gid, real/effective/saved | medium |
| 3.11 | `setpgid` / `getpgid` / `getpgrp` / `setsid` / `getsid` | process groups, session creation, orphaned group | medium |
| 3.12 | `setpgid` race: fork+setpgid in child vs parent | medium |
| 3.13 | `getgroups` / `setgroups` | supplementary groups | small |
| 3.14 | `setfsuid` / `setfsgid` | filesystem uid/gid | small |
| 3.15 | `sched_yield` | yield CPU, basic sanity | small |
| 3.16 | `prctl` | PR_SET_NAME, PR_GET_NAME | small |
| 3.17 | `arch_prctl` | ARCH_SET_FS (TLS), ARCH_GET_FS | small |
| 3.18 | `uname` | sysname, nodename, release, version, machine | small |
| 3.19 | `sysinfo` | uptime, totalram, freeram, procs | small |
| 3.20 | `brk` / `mmap` / `munmap` / `mprotect` / `mremap` / `msync` / `mincore` / `madvise` | memory mapping, heap growth, page protection, remap, mincore | medium |
| 3.21 | `iopl` / `ioperm` | I/O privilege (needs root, QEMU support) | small |

---

## Phase 4: Signal Handling

| # | Syscall / Feature | Test Scenarios | Effort |
|---|-------------------|----------------|--------|
| 4.1 | `rt_sigaction` | set handler, SIG_IGN, SIG_DFL, verify handler called, SA_RESTART | medium |
| 4.2 | `rt_sigprocmask` | SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK, verify blocked signals are pending | medium |
| 4.3 | `rt_sigpending` | get pending signal set | small |
| 4.4 | `rt_sigsuspend` | atomically set mask and wait for signal | medium |
| 4.5 | `rt_sigtimedwait` | wait for signal with timeout | medium |
| 4.6 | `rt_sigreturn` | return from signal handler (handled by libc) | small |
| 4.7 | `sigaltstack` | alternate signal stack (SS_ONSTACK) | medium |
| 4.8 | `tkill` / `tgkill` | send signal to specific thread/task | medium |
| 4.9 | `kill` | signal another process, signal self, signal 0 (probe), EPERM | small |
| 4.10 | `pause` | wait for signal, interrupted by signal | small |
| 4.11 | `alarm` | set alarm, SIGALRM delivery | small |
| 4.12 | Signal-on-child: `waitpid` in parent interrupted by signal | small |
| 4.13 | Signal safety in fork: what signals are inherited | small |
| 4.14 | SIGCHLD handling: default, ignored, caught | medium |

---

## Phase 5: Pipes & IPC

| # | Syscall / Feature | Test Scenarios | Effort |
|---|-------------------|----------------|--------|
| 5.1 | `pipe` / `pipe2` | basic pipe, pipe2(O_CLOEXEC|O_NONBLOCK), pipe with O_NONBLOCK, large write (>PIPE_BUF) | small |
| 5.2 | `pipe` close race: reader closes, writer gets SIGPIPE/EPIPE | small |
| 5.3 | `pipe` capacity: fill pipe until EAGAIN (non-block) or block (blocking) — PIPE_BUF test | medium |
| 5.4 | `dup` / `dup2` / `dup3` | dup to specific fd, dup2 same fd (no-op), dup3(O_CLOEXEC), dup invalid fd (EBADF) | small |
| 5.5 | `socketpair` | AF_UNIX stream pair, send/recv across pair, half-close | medium |
| 5.6 | Unix sockets: `socket`, `bind`, `connect`, `listen`, `accept`, `sendmsg`, `recvmsg`, `getsockname`, `getpeername`, `setsockopt`, `getsockopt`, `shutdown` | AF_UNIX stream, AF_UNIX dgram, pathname, abstract | large |
| 5.7 | `shmget` / `shmat` / `shmdt` / `shmctl` | shared memory create, attach, detach, IPC_RMID, IPC_STAT | medium |
| 5.8 | `futex` | FUTEX_WAIT, FUTEX_WAKE, basic mutex implementation, FUTEX_REQUEUE, FUTEX_CMP_REQUEUE | large |
| 5.9 | `eventfd2` (ENOSYS) | document | small |
| 5.10 | `signalfd4` (ENOSYS) | document | small |

---

## Phase 6: Timers & Time

| # | Syscall / Feature | Test Scenarios | Effort |
|---|-------------------|----------------|--------|
| 6.1 | `time` | get current time, monotonicity | small |
| 6.2 | `gettimeofday` | get tv_sec/tv_usec, check monotonic | small |
| 6.3 | `clock_gettime` | CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_PROCESS_CPUTIME_ID, CLOCK_THREAD_CPUTIME_ID | small |
| 6.4 | `clock_getres` | resolution for each clock | small |
| 6.5 | `clock_nanosleep` | sleep for interval, sleep with absolute time (TIMER_ABSTIME), interrupted by signal | medium |
| 6.6 | `nanosleep` | sleep, remaining time on interruption | medium |
| 6.7 | `getitimer` / `setitimer` | ITIMER_REAL, ITIMER_VIRTUAL, ITIMER_PROF | medium |
| 6.8 | `alarm` | set alarm, cancel (alarm(0)), multiple alarms (last wins) | small |
| 6.9 | `timer_create` / `timer_settime` / `timer_gettime` / `timer_getoverrun` / `timer_delete` | POSIX timers (stubs — test returns) | small |
| 6.10 | `times` | process/children time, tms struct | small |

---

## Phase 7: I/O Multiplexing

| # | Syscall / Feature | Test Scenarios | Effort |
|---|-------------------|----------------|--------|
| 7.1 | `poll` | poll pipe for read, POLLIN, POLLOUT, POLLHUP, timeout=0, timeout=-1, invalid fd (POLLNVAL) | medium |
| 7.2 | `ppoll` | poll with sigmask | medium |
| 7.3 | `select` / `pselect6` | similar to poll with fd_set, nfds calculation | medium |
| 7.4 | `epoll_create1` / `epoll_ctl` / `epoll_wait` | epoll with pipe, edge vs level trigger, EPOLLONESHOT, EPOLLET, EPOLLIN, EPOLLOUT | large |

---

## Phase 8: Random / Misc Syscalls

| # | Syscall / Feature | Test Scenarios | Effort |
|---|-------------------|----------------|--------|
| 8.1 | `getrandom` | get random bytes, different lengths, GRND_NONBLOCK, GRND_RANDOM | small |
| 8.2 | `set_tid_address` | clear child tid on exit | small |
| 8.3 | `set_robust_list` / `get_robust_list` | robust futex list | small |
| 8.4 | `close_range` | close a range of fds (close on exec cleanup) | medium |
| 8.5 | `prlimit64` / `getrlimit` / `setrlimit` | RLIMIT_NOFILE, RLIMIT_NPROC, RLIMIT_DATA | medium |

---

## Phase 9: kyrobox Utility Tests

Test each utility with its CLI options and edge cases. All tests use `fork`+`exec`+`wait` pattern.

| # | Utility | Test Scenarios | Effort |
|---|---------|----------------|--------|
| 9.1 | `basename` | basic, with suffix, path with trailing /, empty string | small |
| 9.2 | `cat` | single file, multiple files, stdin (no args), `-u` (unbuffered) | small |
| 9.3 | `chgrp` | change group, `-R` (if supported), nonexistent group | small |
| 9.4 | `chmod` | numeric mode, symbolic mode, `-R`, invalid mode | small |
| 9.5 | `chown` | change owner, owner:group, `-R`, `-h` (no deref) | small |
| 9.6 | `cksum` | CRC32 of file, stdin, empty file | small |
| 9.7 | `clear` | just check it doesn't crash (produces escape codes) | small |
| 9.8 | `cmp` | identical files, different files, `-l`, `-s`, file vs stdin | small |
| 9.9 | `cp` | file copy, `-f`, dest dir, src nonexistent (ENOENT) | small |
| 9.10 | `cut` | `-c` list, `-d`+`-f`, `-s`, ranges, overlapping | small |
| 9.11 | `date` | different format strings, `%s` (epoch), invalid format | small |
| 9.12 | `dd` | ibs/obs, count, skip, seek, `status=none` | medium |
| 9.13 | `dirname` | basic, path with trailing /, root, empty | small |
| 9.14 | `du` | single file, directory, `-h`, `-s`, `-b` | small |
| 9.15 | `echo` | multiple args, flags (`-n`, `-e`), escape sequences | small |
| 9.16 | `env` | list vars, `-i`, run command with env | small |
| 9.17 | `false` | exit code 1 | small |
| 9.18 | `find` | name pattern, `-type f/d`, start dir, recursive | small |
| 9.19 | `grep` | `-o`, `-v`, `-i`, `-c`, `-n`, `-l`, `-r`, multiple files, stdin, empty pattern, no match | small |
| 9.20 | `head` | default (10 lines), `-n 5`, `-c 20`, pipe | small |
| 9.21 | `hostname` | get/set (test with get only, set may affect QEMU) | small |
| 9.22 | `kill` | `-l` (list signals), kill by PID, kill by name, `-s SIGNAL`, invalid signal | small |
| 9.23 | `link` / `ln` | hard link, `-s` (symlink), `-f`, target exists | small |
| 9.24 | `ls` | `-l`, `-a`, `-R`, `-h`, non-existent path, empty dir, pipe (to check column detection) | small |
| 9.25 | `mkdir` | basic, `-p` (parents), existing dir (EEXIST), `-v` | small |
| 9.26 | `mktemp` | default, `-d` (dir), `-p` (dir prefix), template with X's | small |
| 9.27 | `mv` | rename within dir, move across dirs, overwrite | small |
| 9.28 | `printenv` | specific var, all vars, nonexistent var | small |
| 9.29 | `printf` | format strings, `%s`, `%d`, `%x`, `\\` escapes | small |
| 9.30 | `pwd` | basic, logical vs physical after chdir through symlink | small |
| 9.31 | `readlink` | symlink target, `-f` (canonicalize), `-m` (missing ok) | small |
| 9.32 | `rm` | file, `-f`, `-r` (recursive), `-rf`, nonexistent | small |
| 9.33 | `rmdir` | empty dir, non-empty (ENOTEMPTY), `-p` (parents) | small |
| 9.34 | `sed` | substitute (`s/old/new/g`), `-i` (in-place), `-n`, address range | small |
| 9.35 | `seq` | 1 arg (count), 2 args (first last), 3 args, `-s` (separator), `-w` (width) | small |
| 9.36 | `sleep` | sleep for N seconds, `m`/`h`/`d` suffixes (if supported), fractional | small |
| 9.37 | `sort` | text sort, `-r`, `-n`, `-u`, `-k`, stdin, file | small |
| 9.38 | `sync` | runs without error | small |
| 9.39 | `tail` | default (10 lines), `-n 5`, `-c 20`, `-f` (if supported, tricky) | small |
| 9.40 | `tee` | stdout only, write to file, `-a` (append), multiple files | small |
| 9.41 | `test` | `-f`, `-d`, `-e`, `-n`, `-z`, `=`/`!=`, `-eq`/`-ne`/`-lt`/`-gt`, file mode tests | small |
| 9.42 | `touch` | create file, update timestamp, multiple files, nonexistent path | small |
| 9.43 | `tr` | basic translate, `-d` (delete), `-s` (squeeze), set complement | small |
| 9.44 | `true` | exit code 0 | small |
| 9.45 | `tty` | on TTY, not TTY, `-s` (silent) | small |
| 9.46 | `uname` | `-a`, `-s`, `-n`, `-r`, `-m`, no flags, multiple flags | small |
| 9.47 | `uniq` | adjacent dedup, `-c`, `-d`, `-u`, `-i` | small |
| 9.48 | `unlink` | remove file, remove nonexistent (ENOENT) | small |
| 9.49 | `wc` | `-l`, `-w`, `-c`, multiple files, stdin, empty input | small |
| 9.50 | `which` | command in PATH, not found (exit 1), multiple args | small |
| 9.51 | `whoami` | current user (root) | small |
| 9.52 | `yes` | output, pipe to head to check | small |
| 9.53 | `vi` | open file, enter text, save, quit, `:q!`, basic motions, search | large |

---

## Phase 10: Shell (ksh) Integration Tests

Test the shell (ksh) as a binary via `ksh -c 'commands'`.

| # | Test Scenario | Effort |
|---|---------------|--------|
| 10.1 | Simple command execution: `ls /bin` | small |
| 10.2 | Pipeline: `ls /bin \| grep fetch` | small |
| 10.3 | I/O redirection: `echo hello > /tmp/out`, `cat < /tmp/out`, `>>` append, `2>` stderr | small |
| 10.4 | Here-doc: `cat << EOF` | small |
| 10.5 | Background: `sleep 1 &` | small |
| 10.6 | Variable assignment and expansion: `FOO=bar echo $FOO` | small |
| 10.7 | Environment: `export`, `env` | small |
| 10.8 | Logical operators: `true && echo yes`, `false \|\| echo no`, `;` sequencing | small |
| 10.9 | `if/then/else/fi` script via `-c` | medium |
| 10.10 | `cd` builtin + `pwd` | small |
| 10.11 | `exit` with specific code | small |
| 10.12 | `exec` builtin | small |
| 10.13 | Tab completion (hard to test non-interactively) | low |
| 10.14 | Job control: `jobs`, `fg`, `bg` (needs interactive TTY) | low |
| 10.15 | Script file execution: write `.ksh` script, run via ksh or `./` | medium |
| 10.16 | Glob expansion: `echo /*`, `echo *.c`, no match case | small |

---

## Phase 11: Integration / Multi-Scenario Tests

High-level workflows that combine multiple features.

| # | Scenario | Description | Effort |
|---|----------|-------------|--------|
| 11.1 | Pipeline stress: `cat /bin/ls \| grep -o . \| sort \| uniq -c \| wc -l` | medium |
| 11.2 | Fork/exec storm: fork 100 children, each runs `true`, wait all | medium |
| 11.3 | FD exhaustion: open many files until EMFILE, then close_range | medium |
| 11.4 | Signal stress: send SIGUSR1 to child, verify handler, repeat 1000x | medium |
| 11.5 | Pipe relay: chain of 10 processes piping to each other | medium |
| 11.6 | Mixed read/write: 2 processes write to pipe, 1 reads, verify ordering | medium |
| 11.7 | EOF handling: write to pipe, close, verify read returns 0 | small |
| 11.8 | Partial read: write 1000 bytes, read 100 at a time | small |
| 11.9 | Non-blocking I/O: O_NONBLOCK pipe read returns EAGAIN | medium |
| 11.10 | epoll + pipe event loop: epoll_wait with pipe wakeup | medium |
| 11.11 | Shared memory IPC: shmget + fork + write/read via shared memory | medium |
| 11.12 | Unix socket stream echo: server binds, client connects, sends data, receives echo | medium |

---

## Phase 12: Error Handling & Negative Tests

| # | Pattern | Examples | Effort |
|---|---------|---------|--------|
| 12.1 | Bad fd operations | read/write/close of invalid fd (EBADF) | small |
| 12.2 | Invalid pointers | EFAULT across read/write/stat/ioctl | medium |
| 12.3 | Permissions | open dir for write (EISDIR), write to read-only file, unlink dir (EPERM) | medium |
| 12.4 | Path traversal | `/../..`, `//`, `/proc/../etc` nonsense | small |
| 12.5 | Signal during syscall | EINTR on read/write/poll/nanosleep | medium |
| 12.6 | ENOMEM | mmap huge allocation to exhaust memory | small |
| 12.7 | EAGAIN/EWOULDBLOCK | non-blocking read on empty pipe, non-blocking write on full pipe | medium |
| 12.8 | execve bad binary | ELF with wrong arch, corrupt ELF, garbage file | small |
| 12.9 | execve path too long | E2BIG on large argv/envp | small |
| 12.10 | EMFILE (too many open files) | exhaust fd table, then unlink (should fail) | small |
| 12.11 | ENFILE (system-wide) | a bit harder in userspace | low |
| 12.12 | ENOENT everywhere | open, stat, chdir, rename, unlink, rmdir, chmod, chown on nonexistent paths | small |
| 12.13 | EEXIST | mkdir existing, creat existing, link existing | small |
| 12.14 | ENOTDIR | component of path is not a directory | small |
| 12.15 | EISDIR | open dir for writing | small |
| 12.16 | Loop detection | ELOOP on symlink chain (if supported) | small |

---

## Phase 13: Performance & Stress

| # | Test | Effort |
|---|------|--------|
| 13.1 | Fork throughput: forks/sec (time 1000 forks) | small |
| 13.2 | Pipe throughput: large data transfer through pipe | small |
| 13.3 | File I/O throughput: write/read large files | small |
| 13.4 | Signal delivery latency: signal round-trip time | medium |
| 13.5 | Context switch overhead: pipe ping-pong | medium |
| 13.6 | Exec latency: time `execve` of small binary | small |

---

## Phase 14: Test Runner Infrastructure Improvements

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 14.1 | Sub-test grouping | Allow multiple assertions per test with individual reporting | small |
| 14.2 | Timeout per test | Set SIGALRM per test to prevent hangs | small |
| 14.3 | Test data setup/teardown | `setUp()` / `tearDown()` pattern for temp dirs, files, env | small |
| 14.4 | Test result summary | Per-syscall or per-category pass/fail breakdown | small |
| 14.5 | Skip mechanism | `SKIP()` for not-yet-implemented or optional features | small |
| 14.6 | Test ordering/subsets | Modify Makefile to support running a subset of tests | small |
| 14.7 | QEMU test time limit | Add timeout in `test-run-log` to prevent CI hangs | small |
| 14.8 | JUnit XML output | Structured output for CI integration | medium |

---

## Summary

| Phase | Tests | Est. Tests | Est. Effort |
|-------|-------|-----------|-------------|
| Current | — | 6 | done |
| **Phase 1** Infrastructure | — | — | high |
| **Phase 2** File System | 2.1–2.30 | ~90 | large |
| **Phase 3** Process & Scheduling | 3.1–3.21 | ~40 | large |
| **Phase 4** Signal Handling | 4.1–4.14 | ~25 | large |
| **Phase 5** Pipes & IPC | 5.1–5.10 | ~20 | large |
| **Phase 6** Timers & Time | 6.1–6.10 | ~20 | medium |
| **Phase 7** I/O Multiplexing | 7.1–7.4 | ~15 | large |
| **Phase 8** Random / Misc | 8.1–8.5 | ~10 | small |
| **Phase 9** kyrobox Utilities | 9.1–9.53 | ~80 | large |
| **Phase 10** Shell (ksh) | 10.1–10.16 | ~20 | medium |
| **Phase 11** Integration | 11.1–11.12 | ~12 | medium |
| **Phase 12** Error Handling | 12.1–12.16 | ~40 | medium |
| **Phase 13** Performance | 13.1–13.6 | ~6 | small |
| **Phase 14** Test Runner Infra | 14.1–14.8 | — | medium |
| **Total** | | **~384** | |
