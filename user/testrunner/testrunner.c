#include "test_harness.h"

static int total = 0;
static int passed = 0;

char tmpdir[256];

#define TEST(name)                                                                                 \
    do {                                                                                           \
        extern int test_##name(void);                                                              \
        total++;                                                                                   \
        fprintf(stderr, "  TEST %-30s ", #name);                                                   \
        if (test_##name()) {                                                                       \
            passed++;                                                                              \
            fprintf(stderr, "PASS\n");                                                             \
        } else {                                                                                   \
            fprintf(stderr, "FAIL\n");                                                             \
        }                                                                                          \
    } while (0)

/* ================================================================== */
/*  Existing tests (inherited from original testrunner)                */
/* ================================================================== */

static int test_pipe_dup2_exec(void)
{
    int p[2];
    if (pipe(p) < 0)
        return 0;

    pid_t pid = fork();
    if (pid < 0)
        return 0;

    if (pid == 0) {
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
        close(p[1]);
        execlp("ls", "ls", "/bin", NULL);
        _exit(127);
    }

    close(p[1]);
    char buf[256];
    ssize_t tot = 0, n;
    while ((n = read(p[0], buf + tot, sizeof(buf) - tot)) > 0)
        tot += n;
    close(p[0]);

    int status;
    waitpid(pid, &status, 0);

    return tot > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int test_ls_grep_pipeline(void)
{
    int p1[2], p2[2];
    if (pipe(p1) < 0)
        return 0;
    if (pipe(p2) < 0) {
        close(p1[0]);
        close(p1[1]);
        return 0;
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);
        return 0;
    }

    if (pid1 == 0) {
        close(p1[0]);
        close(p2[0]);
        close(p2[1]);
        dup2(p1[1], STDOUT_FILENO);
        close(p1[1]);
        execlp("ls", "ls", "/bin", NULL);
        _exit(127);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);
        return 0;
    }

    if (pid2 == 0) {
        close(p1[1]);
        close(p2[0]);
        dup2(p1[0], STDIN_FILENO);
        close(p1[0]);
        dup2(p2[1], STDOUT_FILENO);
        close(p2[1]);
        execlp("grep", "grep", "fetch", NULL);
        _exit(127);
    }

    close(p1[0]);
    close(p1[1]);
    close(p2[1]);

    char buf[256];
    ssize_t n = read(p2[0], buf, sizeof(buf) - 1);
    close(p2[0]);
    buf[n > 0 ? n : 0] = '\0';

    int status;
    waitpid(pid2, &status, 0);
    waitpid(pid1, &status, 0);

    return strstr(buf, "fetch") != NULL;
}

static int test_grep_o(void)
{
    int p1[2], p2[2];
    if (pipe(p1) < 0 || pipe(p2) < 0)
        return 0;

    pid_t pid1 = fork();
    if (pid1 < 0)
        return 0;
    if (pid1 == 0) {
        close(p1[0]);
        close(p2[0]);
        close(p2[1]);
        dup2(p1[1], STDOUT_FILENO);
        close(p1[1]);
        execlp("echo", "echo", "fetch", NULL);
        _exit(127);
    }

    pid_t pid2 = fork();
    if (pid2 < 0)
        return 0;
    if (pid2 == 0) {
        close(p1[1]);
        close(p2[0]);
        dup2(p1[0], STDIN_FILENO);
        close(p1[0]);
        dup2(p2[1], STDOUT_FILENO);
        close(p2[1]);
        execlp("grep", "grep", "-o", "fetch", NULL);
        _exit(127);
    }

    close(p1[0]);
    close(p1[1]);
    close(p2[1]);

    char buf[64];
    ssize_t tot = 0, n;
    while ((n = read(p2[0], buf + tot, sizeof(buf) - 1 - tot)) > 0)
        tot += n;
    close(p2[0]);
    buf[tot > 0 ? tot : 0] = '\0';

    int status;
    waitpid(pid2, &status, 0);
    waitpid(pid1, &status, 0);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && strcmp(buf, "fetch\n") == 0;
}

static int test_tiocgwinsz(void)
{
    struct winsize ws;
    int fd = open("/dev/tty", O_RDWR);
    if (fd < 0)
        return 0;
    int ret = ioctl(fd, TIOCGWINSZ, &ws);
    close(fd);
    if (ret < 0)
        return 0;
    return ws.ws_row > 0 && ws.ws_col > 0;
}

static int test_exec_fail(void)
{
    pid_t pid = fork();
    if (pid < 0)
        return 0;

    if (pid == 0) {
        execlp("nonexistent-binary", "nonexistent-binary", NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    return WIFEXITED(status) && WEXITSTATUS(status) == 127;
}

static int test_basic_syscalls(void)
{
    pid_t pid = getpid();
    pid_t ppid = getppid();
    uid_t uid = getuid();
    return pid > 0 && ppid >= 0 && uid == 0;
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(void)
{
    int fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
            close(fd);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGALRM, SIG_IGN);

    setenv("PATH", "/bin", 1);
    setenv("HOME", "/root", 1);

    if (!setup_tmpdir()) {
        fprintf(stderr, "FAIL: could not create tmpdir\n");
        return 1;
    }

    fprintf(stderr, "Kyronix Test Runner\n");
    fprintf(stderr, "--------------------\n\n");

    /* ── Existing tests ── */
    fprintf(stderr, "[Existing Tests]\n");
    TEST(pipe_dup2_exec);
    TEST(ls_grep_pipeline);
    TEST(grep_o);
    TEST(tiocgwinsz);
    TEST(exec_fail);
    TEST(basic_syscalls);

    /* ── Phase 2: File System ── */
    fprintf(stderr, "\n[Phase 2: File System]\n");
    TEST(open_close);
    TEST(read_write);
    TEST(lseek);
    TEST(stat_fstat_lstat);
    TEST(access);
    TEST(creat);
    TEST(truncate);
    TEST(link_unlink);
    TEST(symlink_readlink);
    TEST(rename);
    TEST(chdir_getcwd);
    TEST(mkdir_rmdir);
    TEST(getdents);
    TEST(chmod_fchmod);
    TEST(chown_fchown);
    TEST(umask);
    TEST(fcntl);
    TEST(mknod);
    TEST(statfs);
    TEST(openat_mkdirat);
    TEST(fstatat_unlinkat);
    TEST(renameat_linkat);
    TEST(symlinkat_readlinkat);
    TEST(fchmodat_faccessat);
    TEST(pread_pwrite);
    TEST(readv_writev);
    TEST(copy_file_range);
    TEST(memfd_create);
    TEST(sendfile);
    TEST(flock);
    TEST(fsync_fdatasync);
    TEST(fallocate);
    TEST(utime_utimensat);
    TEST(statx);
    TEST(pipe2);
    TEST(sendfile_noffset);

    /* ── Phase 3: Process & Scheduling ── */
    fprintf(stderr, "\n[Phase 3: Process & Scheduling]\n");
    TEST(fork_basic);
    TEST(fork_pid);
    TEST(fork_cow);
    TEST(fork_fdtable);
    TEST(vfork);
    TEST(execve);
    TEST(execve_bad);
    TEST(exit_status);
    TEST(wait_nohang);
    TEST(wait_specific);
    TEST(wait_echild);
    TEST(getpid_getppid);
    TEST(getuid_getgid);
    TEST(setuid_setgid);
    TEST(setreuid_setregid);
    TEST(setresuid_setresgid);
    TEST(setpgid_getpgid);
    TEST(setsid_getsid);
    TEST(getgroups_setgroups);
    TEST(setfsuid_setfsgid);
    TEST(sched_yield);
    TEST(prctl);
    TEST(arch_prctl);
    TEST(uname);
    TEST(sysinfo);
    TEST(brk);
    TEST(mmap_munmap);
    TEST(mprotect);
    TEST(mremap);
    TEST(msync_mincore);
    TEST(madvise);
    TEST(iopl_ioperm);

    fprintf(stderr, "\nRESULT: %d/%d passed\n", passed, total);

    cleanup_tmpdir();

    if (passed == total) {
        fprintf(stderr, "ALL TESTS PASSED\n");
        fflush(stderr);
        __asm__ volatile("mov $169, %%rax; xor %%rdi, %%rdi; syscall" ::: "rax", "rdi");
    } else {
        fprintf(stderr, "SOME TESTS FAILED\n");
        fflush(stderr);
    }

    for (;;)
        pause();
}
