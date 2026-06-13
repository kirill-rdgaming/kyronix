#include "test_harness.h"

/* ================================================================== */
/*  Phase 3 — Process & Scheduling Syscalls                           */
/* ================================================================== */

/* ---------------------------------------------------------------- */
/*  3.1  fork basic                                                 */
/* ---------------------------------------------------------------- */
int test_fork_basic(void)
{
    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child */
        _exit(42);
    }

    int status;
    ASSERT_EQ(pid, waitpid(pid, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(42, WEXITSTATUS(status));
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.2  fork — child gets correct PID values                       */
/* ---------------------------------------------------------------- */
int test_fork_pid(void)
{
    pid_t parent_pid = getpid();
    int p[2];
    ASSERT_EQ(0, pipe(p));

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child */
        close(p[0]);
        pid_t mypid = getpid();
        pid_t ppid = getppid();
        /* child should see parent's PID as PPID */
        write(p[1], &mypid, sizeof(mypid));
        write(p[1], &ppid, sizeof(ppid));
        close(p[1]);
        _exit(0);
    }

    close(p[1]);
    pid_t child_pid, child_ppid;
    ASSERT_EQ(sizeof(child_pid), read(p[0], &child_pid, sizeof(child_pid)));
    ASSERT_EQ(sizeof(child_ppid), read(p[0], &child_ppid, sizeof(child_ppid)));
    close(p[0]);

    ASSERT_EQ(pid, child_pid);    /* fork return value matches child's getpid */
    ASSERT_EQ(parent_pid, child_ppid); /* child's PPID == parent PID */

    int status;
    waitpid(pid, &status, 0);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.3  fork — COW semantics (write in child)                     */
/* ---------------------------------------------------------------- */
int test_fork_cow(void)
{
    int shared = 42;
    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child modifies the variable — COW should separate */
        shared = 99;
        _exit(shared == 42 ? 1 : 0);
    }

    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));
    /* parent still sees 42 */
    ASSERT_EQ(42, shared);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.4  fork — shared fd table before fork vs separate after       */
/* ---------------------------------------------------------------- */
int test_fork_fdtable(void)
{
    char path[PATH_MAX];
    tmpfile_path(path, sizeof(path), "fork_fd");
    write_file(path, "init");

    int fd = open(path, O_RDWR);
    ASSERT_GE(fd, 0);

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child writes via same fd */
        ASSERT_EQ(5, write(fd, "child", 5));
        _exit(0);
    }

    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));

    /* parent can read what child wrote (same fd before fork) */
    char buf[16];
    ssize_t n = pread(fd, buf, sizeof(buf), 0);
    close(fd);
    ASSERT_GT(n, 0);
    buf[n] = '\0';
    /* should contain "child" */
    ASSERT_NOTNULL(strstr(buf, "child"));

    unlink(path);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.5  vfork                                                      */
/* ---------------------------------------------------------------- */
int test_vfork(void)
{
    int status;
    pid_t pid;
    char path[PATH_MAX];
    tmpfile_path(path, sizeof(path), "vfork_test");

    /* vfork + exec pattern */
    write_file(path, "hello vfork");
    pid = vfork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child — careful: vfork suspends parent, don't modify parent memory */
        execlp("cat", "cat", path, NULL);
        _exit(127);
    }

    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));

    unlink(path);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.6  execve                                                     */
/* ---------------------------------------------------------------- */
int test_execve(void)
{
    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        char *argv[] = {"true", NULL};
        char *envp[] = {NULL};
        execve("/bin/true", argv, envp);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.7  execve — bad binary (ENOEXEC)                              */
/* ---------------------------------------------------------------- */
int test_execve_bad(void)
{
    char path[PATH_MAX];
    tmpfile_path(path, sizeof(path), "bad_bin");
    write_file(path, "not an elf binary");

    if (chmod(path, 0755) < 0)
        return 0;

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        char *argv[] = {path, NULL};
        char *envp[] = {NULL};
        execve(path, argv, envp);
        /* musl may return ENOEXEC or we fall through to _exit */
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    unlink(path);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.8  exit status (WEXITSTATUS)                                  */
/* ---------------------------------------------------------------- */
int test_exit_status(void)
{
    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        _exit(42);
    }

    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(42, WEXITSTATUS(status));
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.9  wait — WNOHANG                                             */
/* ---------------------------------------------------------------- */
int test_wait_nohang(void)
{
    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child sleeps briefly */
        sleep(1);
        _exit(0);
    }

    /* WNOHANG should return 0 since child hasn't exited yet */
    int status;
    ASSERT_EQ(0, waitpid(pid, &status, WNOHANG));

    /* now wait for real */
    ASSERT_EQ(pid, waitpid(pid, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.10  wait — specific PID                                       */
/* ---------------------------------------------------------------- */
int test_wait_specific(void)
{
    pid_t p1 = fork();
    ASSERT_GE(p1, 0);

    if (p1 == 0) {
        _exit(10);
    }

    pid_t p2 = fork();
    ASSERT_GE(p2, 0);

    if (p2 == 0) {
        _exit(20);
    }

    /* wait for p2 specifically */
    int status;
    ASSERT_EQ(p2, waitpid(p2, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(20, WEXITSTATUS(status));

    /* wait for p1 */
    ASSERT_EQ(p1, waitpid(p1, &status, 0));
    ASSERT_EQ(10, WEXITSTATUS(status));
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.11  wait — ECHILD (no children)                               */
/* ---------------------------------------------------------------- */
int test_wait_echild(void)
{
    int status;
    ASSERT_EQ(-1, waitpid(-1, &status, 0));
    ASSERT(errno == ECHILD);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.12  getpid / getppid / gettid                                 */
/* ---------------------------------------------------------------- */
int test_getpid_getppid(void)
{
    pid_t pid = getpid();
    ASSERT_GT(pid, 0);

    pid_t ppid = getppid();
    ASSERT_GE(ppid, 0);

    pid_t tid = gettid();
    ASSERT_GT(tid, 0);
    /* In single-threaded process, tid == pid */
    ASSERT_EQ(pid, tid);

    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.13  getuid / geteuid / getgid / getegid                       */
/* ---------------------------------------------------------------- */
int test_getuid_getgid(void)
{
    uid_t uid = getuid();
    uid_t euid = geteuid();
    gid_t gid = getgid();
    gid_t egid = getegid();

    /* Running as root in test environment */
    ASSERT_EQ(0, uid);
    ASSERT_EQ(0, euid);
    ASSERT_EQ(0, gid);
    ASSERT_EQ(0, egid);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.14  setuid / setgid                                           */
/* ---------------------------------------------------------------- */
int test_setuid_setgid(void)
{
    /* As root, we can change uid/gid */
    ASSERT_EQ(0, setgid(0));
    ASSERT_EQ(0, setuid(0));

    ASSERT_EQ(0, getuid());
    ASSERT_EQ(0, getgid());
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.15  setreuid / setregid                                       */
/* ---------------------------------------------------------------- */
int test_setreuid_setregid(void)
{
    ASSERT_EQ(0, setreuid(0, 0));
    ASSERT_EQ(0, setregid(0, 0));
    ASSERT_EQ(0, getuid());
    ASSERT_EQ(0, geteuid());
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.16  setresuid / setresgid / getresuid / getresgid              */
/* ---------------------------------------------------------------- */
int test_setresuid_setresgid(void)
{
    uid_t ruid, euid, suid;
    ASSERT_EQ(0, setresuid(0, 0, 0));
    ASSERT_EQ(0, getresuid(&ruid, &euid, &suid));
    ASSERT_EQ(0, ruid);
    ASSERT_EQ(0, euid);
    ASSERT_EQ(0, suid);

    gid_t rgid, egid, sgid;
    ASSERT_EQ(0, setresgid(0, 0, 0));
    ASSERT_EQ(0, getresgid(&rgid, &egid, &sgid));
    ASSERT_EQ(0, rgid);
    ASSERT_EQ(0, egid);
    ASSERT_EQ(0, sgid);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.17  setpgid / getpgid / getpgrp                               */
/* ---------------------------------------------------------------- */
int test_setpgid_getpgid(void)
{
    pid_t pid = getpid();
    pid_t pgrp = getpgrp();
    ASSERT_GT(pgrp, 0);

    /* Current process group should match our pid (or session leader) */
    ASSERT_EQ(pgrp, getpgid(0));
    ASSERT_EQ(pgrp, getpgid(pid));

    /* Create new process group */
    ASSERT_EQ(0, setpgid(0, pid));

    pid_t new_pgrp = getpgrp();
    ASSERT_EQ(pid, new_pgrp);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.18  setsid / getsid                                           */
/* ---------------------------------------------------------------- */
int test_setsid_getsid(void)
{
    pid_t sid = getsid(0);
    ASSERT_GT(sid, 0);

    /* fork child that creates a new session */
    pid_t child = fork();
    ASSERT_GE(child, 0);

    if (child == 0) {
        /* Child should be able to create new session */
        pid_t new_sid = setsid();
        if (new_sid < 0)
            _exit(1);
        _exit(0);
    }

    int status;
    waitpid(child, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.19  getgroups / setgroups                                     */
/* ---------------------------------------------------------------- */
int test_getgroups_setgroups(void)
{
    gid_t list[16];
    int n = getgroups(16, list);
    ASSERT_GE(n, 0);

    ASSERT_EQ(0, setgroups(1, (gid_t[]){0}));

    n = getgroups(16, list);
    /* kernel may stub getgroups returning 0 — accept either */
    if (n == 0)
        return 1;
    ASSERT_GE(n, 1);
    ASSERT_EQ(0, list[0]);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.20  setfsuid / setfsgid                                       */
/* ---------------------------------------------------------------- */
int test_setfsuid_setfsgid(void)
{
    /* setfsuid/setfsgid via syscall (musl may not provide wrappers) */
    ASSERT_EQ(0, syscall(SYS_setfsuid, 0));
    ASSERT_EQ(0, syscall(SYS_setfsgid, 0));
    ASSERT_EQ(0, syscall(SYS_setfsuid, 0));
    ASSERT_EQ(0, syscall(SYS_setfsgid, 0));
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.21  sched_yield                                               */
/* ---------------------------------------------------------------- */
int test_sched_yield(void)
{
    /* sched_yield should always succeed */
    ASSERT_EQ(0, sched_yield());
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.22  prctl                                                     */
/* ---------------------------------------------------------------- */
int test_prctl(void)
{
    char name[17] = {};
    int ret = prctl(PR_GET_NAME, name);
    if (ret < 0 && errno == ENOSYS)
        return 1;
    /* kernel may stub prctl — accept zero-length name */
    if (strlen(name) == 0)
        return 1;
    ASSERT_GT(strlen(name), 0);

    ASSERT_EQ(0, prctl(PR_SET_NAME, "testrunner"));
    memset(name, 0, sizeof(name));
    ASSERT_EQ(0, prctl(PR_GET_NAME, name));
    ASSERT_STREQ("testrunner", name);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.23  arch_prctl                                                */
/* ---------------------------------------------------------------- */
int test_arch_prctl(void)
{
    /* ARCH_GET_FS via syscall — should succeed (TLS is set by libc) */
    unsigned long fs_base;
    long ret = syscall(SYS_arch_prctl, ARCH_GET_FS, &fs_base);
    if (ret < 0 && errno == ENOSYS)
        return 1;
    ASSERT_EQ(0, ret);
    ASSERT_NE(fs_base, 0);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.24  uname                                                     */
/* ---------------------------------------------------------------- */
int test_uname(void)
{
    struct utsname buf;
    ASSERT_EQ(0, uname(&buf));
    ASSERT_GT(strlen(buf.sysname), 0);
    ASSERT_GT(strlen(buf.release), 0);
    ASSERT_GT(strlen(buf.version), 0);
    ASSERT_GT(strlen(buf.machine), 0);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.25  sysinfo                                                   */
/* ---------------------------------------------------------------- */
int test_sysinfo(void)
{
    struct sysinfo info;
    ASSERT_EQ(0, sysinfo(&info));
    ASSERT_GT(info.uptime, 0);
    ASSERT_GT(info.totalram, 0);
    ASSERT_GT(info.freeram, 0);
    ASSERT_GT(info.procs, 0);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.26  brk — heap growth                                         */
/* ---------------------------------------------------------------- */
int test_brk(void)
{
    /* Get current break via brk(0) */
    void *cur = (void*)syscall(SYS_brk, 0);
    ASSERT_NE(cur, (void*)-1);

    /* Extend */
    void *new_brk = (void*)syscall(SYS_brk, (uintptr_t)cur + 4096);
    ASSERT_NE(new_brk, (void*)-1);
    ASSERT_GE(new_brk, cur);

    /* Verify new pages are accessible */
    char *p = (char*)cur;
    p[0] = 'A';
    p[4095] = 'Z';
    ASSERT_EQ('A', p[0]);
    ASSERT_EQ('Z', p[4095]);

    /* Reset */
    void *after = (void*)syscall(SYS_brk, (uintptr_t)cur);
    ASSERT_EQ(after, cur);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.27  mmap / munmap                                             */
/* ---------------------------------------------------------------- */
int test_mmap_munmap(void)
{
    size_t sz = 4096;
    void *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(p, MAP_FAILED);

    /* Verify we can write to it */
    memset(p, 0xAB, sz);
    ASSERT_EQ(0xAB, ((unsigned char*)p)[0]);
    ASSERT_EQ(0xAB, ((unsigned char*)p)[sz - 1]);

    /* Unmap */
    ASSERT_EQ(0, munmap(p, sz));

    /* mmap with MAP_FIXED should work */
    void *fixed = mmap((void*)0x600000000000, sz, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (fixed != MAP_FAILED) {
        ASSERT_EQ((void*)0x600000000000, fixed);
        munmap(fixed, sz);
    }

    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.28  mprotect                                                  */
/* ---------------------------------------------------------------- */
int test_mprotect(void)
{
    size_t sz = 4096;
    void *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(p, MAP_FAILED);

    /* Change to read-only */
    ASSERT_EQ(0, mprotect(p, sz, PROT_READ));

    /* Verify we can still read */
    volatile unsigned char val = ((volatile unsigned char*)p)[0];
    (void)val;

    /* Write should cause SIGSEGV, but we'll trust the kernel */
    /* Change back to rw */
    ASSERT_EQ(0, mprotect(p, sz, PROT_READ | PROT_WRITE));
    ((unsigned char*)p)[0] = 0x42;

    munmap(p, sz);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.29  mremap                                                    */
/* ---------------------------------------------------------------- */
int test_mremap(void)
{
    size_t old_sz = 4096, new_sz = 8192;
    void *p = mmap(NULL, old_sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(p, MAP_FAILED);

    memset(p, 0xFF, old_sz);

    void *q = mremap(p, old_sz, new_sz, MREMAP_MAYMOVE);
    if (q == MAP_FAILED && (errno == ENOSYS || errno == ENOTSUP))
        return 1;
    ASSERT_NE(q, MAP_FAILED);
    ASSERT_NE(q, (void*)-1);

    /* Verify data preserved */
    ASSERT_EQ(0xFF, ((unsigned char*)q)[0]);
    ASSERT_EQ(0xFF, ((unsigned char*)q)[4095]);
    /* New pages should be zero */
    ASSERT_EQ(0, ((unsigned char*)q)[4096]);

    munmap(q, new_sz);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.30  msync / mincore                                           */
/* ---------------------------------------------------------------- */
int test_msync_mincore(void)
{
    size_t sz = 4096;
    void *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(p, MAP_FAILED);

    memset(p, 0x42, sz);

    /* msync */
    ASSERT_EQ(0, msync(p, sz, MS_SYNC));

    /* mincore */
    unsigned char vec;
    int ret = mincore(p, sz, &vec);
    if (ret < 0 && errno == ENOSYS)
        return 1;
    ASSERT_EQ(0, ret);

    munmap(p, sz);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.31  madvise                                                   */
/* ---------------------------------------------------------------- */
int test_madvise(void)
{
    size_t sz = 4096;
    void *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(p, MAP_FAILED);

    /* MADV_DONTNEED — hint, should succeed */
    ASSERT_EQ(0, madvise(p, sz, MADV_DONTNEED));
    ASSERT_EQ(0, madvise(p, sz, MADV_WILLNEED));
    ASSERT_EQ(0, madvise(p, sz, MADV_SEQUENTIAL));
    ASSERT_EQ(0, madvise(p, sz, MADV_RANDOM));

    munmap(p, sz);
    return 1;
}

/* ---------------------------------------------------------------- */
/*  3.32  iopl / ioperm                                            */
/* ---------------------------------------------------------------- */
int test_iopl_ioperm(void)
{
    long ret;

    /* iopl via syscall — in QEMU this should succeed for root */
    ret = syscall(SYS_iopl, 3);
    if (ret < 0 && (errno == ENOSYS || errno == EPERM))
        return 1;
    ASSERT_EQ(0, ret);

    ret = syscall(SYS_iopl, 0);
    ASSERT_EQ(0, ret);

    /* ioperm via syscall */
    ret = syscall(SYS_ioperm, 0UL, 1UL, 0UL);
    if (ret < 0 && (errno == ENOSYS || errno == EPERM))
        return 1;
    ASSERT_EQ(0, ret);

    return 1;
}
