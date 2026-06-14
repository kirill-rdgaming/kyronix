#include "test_harness.h"

/* ------------------------------------------------------------------ */
/*  Global counters                                                    */
/* ------------------------------------------------------------------ */

static int total = 0;
static int passed = 0;
static int failed = 0;
static int skipped = 0;

char tmpdir[256];

/* ------------------------------------------------------------------ */
/*  Failure pipe & log (child→parent)                                 */
/* ------------------------------------------------------------------ */

int failure_pipe[2] = { -1, -1 };

#define MAX_FAILURES 64

static struct {
    const char *name;
    char detail[256];
} failures[MAX_FAILURES];
static int nfailures = 0;

/* ------------------------------------------------------------------ */
/*  Test registry (populated by constructor REGISTER_TEST calls)      */
/* ------------------------------------------------------------------ */

test_entry_t test_registry[MAX_TESTS];
int test_count = 0;

/* ------------------------------------------------------------------ */
/*  Phase tracking                                                     */
/* ------------------------------------------------------------------ */

static const char *cur_phase = NULL;
static int ph_total = 0;
static int ph_passed = 0;
static int ph_failed = 0;
static int ph_skipped = 0;

static void phase_summary(void) {
    fprintf(stderr, "  " ANSI_CYAN "--- %s: %d/%d PASS, %d FAIL, %d SKIP" ANSI_RESET "\n",
            cur_phase, ph_passed, ph_total, ph_failed, ph_skipped);
}

static void phase_begin(const char *phase) {
    if (cur_phase) phase_summary();
    cur_phase = phase;
    ph_total = ph_passed = ph_failed = ph_skipped = 0;
    fprintf(stderr, "\n" ANSI_CYAN "[%s]" ANSI_RESET "\n", phase);
}

/* ------------------------------------------------------------------ */
/*  Sandboxed test runner (fork + alarm per test)                      */
/* ------------------------------------------------------------------ */

static int run_sandbox(const char *test_name, int (*test_fn)(void)) {
    int p[2];
    if (pipe(p) < 0) {
        p[0] = -1;
        p[1] = -1;
    }
    failure_pipe[0] = p[0];
    failure_pipe[1] = p[1];

    pid_t pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        failure_pipe[0] = failure_pipe[1] = -1;
        if (!setup_tmpdir()) return TEST_FAIL;
        int r = test_fn();
        cleanup_tmpdir();
        return r;
    }

    if (pid == 0) {
        close(p[0]);
        signal(SIGALRM, SIG_DFL);
        if (!setup_tmpdir()) _exit(TEST_FAIL);

        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) {
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        alarm(5);
        int r = test_fn();
        alarm(0);
        cleanup_tmpdir();
        close(p[1]);
        _exit(r);
    }

    close(p[1]);
    failure_pipe[1] = -1;

    int status;
    waitpid(pid, &status, 0);

    char buf[256];
    ssize_t n = read(p[0], buf, sizeof(buf) - 1);
    close(p[0]);
    failure_pipe[0] = -1;

    if (n > 0) {
        buf[n] = '\0';
        if (nfailures < MAX_FAILURES) {
            failures[nfailures].name = test_name;
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
            size_t cp = len < sizeof(failures[nfailures].detail) - 1 ?
                            len :
                            sizeof(failures[nfailures].detail) - 1;
            memcpy(failures[nfailures].detail, buf, cp);
            failures[nfailures].detail[cp] = '\0';
            nfailures++;
        }
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return TEST_FAIL;
}

/* ================================================================== */
/*  Existing tests (inherited from original testrunner)                */
/* ================================================================== */

int test_pipe_dup2_exec(void) {
    int p[2];
    if (pipe(p) < 0) return 0;

    pid_t pid = fork();
    if (pid < 0) return 0;

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
    while ((n = read(p[0], buf + tot, sizeof(buf) - tot)) > 0) tot += n;
    close(p[0]);

    int status;
    waitpid(pid, &status, 0);

    return tot > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
REGISTER_TEST(pipe_dup2_exec, "Infrastructure");

int test_ls_grep_pipeline(void) {
    int p1[2], p2[2];
    if (pipe(p1) < 0) return 0;
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
REGISTER_TEST(ls_grep_pipeline, "Infrastructure");

int test_grep_o(void) {
    int p1[2], p2[2];
    if (pipe(p1) < 0 || pipe(p2) < 0) return 0;

    pid_t pid1 = fork();
    if (pid1 < 0) return 0;
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
    if (pid2 < 0) return 0;
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
    while ((n = read(p2[0], buf + tot, sizeof(buf) - 1 - tot)) > 0) tot += n;
    close(p2[0]);
    buf[tot > 0 ? tot : 0] = '\0';

    int status;
    waitpid(pid2, &status, 0);
    waitpid(pid1, &status, 0);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && strcmp(buf, "fetch\n") == 0;
}
REGISTER_TEST(grep_o, "Infrastructure");

int test_tiocgwinsz(void) {
    struct winsize ws;
    int fd = open("/dev/tty", O_RDWR);
    if (fd < 0) return 0;
    int ret = ioctl(fd, TIOCGWINSZ, &ws);
    close(fd);
    if (ret < 0) return 0;
    return ws.ws_row > 0 && ws.ws_col > 0;
}
REGISTER_TEST(tiocgwinsz, "Infrastructure");

int test_exec_fail(void) {
    pid_t pid = fork();
    if (pid < 0) return 0;

    if (pid == 0) {
        execlp("nonexistent-binary", "nonexistent-binary", NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    return WIFEXITED(status) && WEXITSTATUS(status) == 127;
}
REGISTER_TEST(exec_fail, "Infrastructure");

int test_basic_syscalls(void) {
    pid_t pid = getpid();
    pid_t ppid = getppid();
    uid_t uid = getuid();
    return pid > 0 && ppid >= 0 && uid == 0;
}
REGISTER_TEST(basic_syscalls, "Infrastructure");

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(void) {
    int fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGALRM, SIG_IGN);

    setenv("PATH", "/bin", 1);
    setenv("HOME", "/root", 1);

    fprintf(stderr, ANSI_CYAN "Kyronix Test Runner" ANSI_RESET "\n");
    fprintf(stderr, "--------------------\n");

    for (int i = 0; i < test_count; i++) {
        test_entry_t *e = &test_registry[i];

        if (!cur_phase || strcmp(cur_phase, e->phase) != 0) phase_begin(e->phase);

        fflush(stderr);
        fprintf(stderr, "  %-30s ", e->name);
        fflush(stderr);

        int result = run_sandbox(e->name, e->func);

        if (result == TEST_PASS) {
            passed++;
            ph_passed++;
            fprintf(stderr, ANSI_GREEN "PASS" ANSI_RESET "\n");
        } else if (result == TEST_SKIP) {
            skipped++;
            ph_skipped++;
            fprintf(stderr, ANSI_YELLOW "SKIP" ANSI_RESET "\n");
        } else {
            failed++;
            ph_failed++;
            fprintf(stderr, ANSI_RED "FAIL" ANSI_RESET "\n");
        }
        total++;
    }

    if (cur_phase) phase_summary();

    fprintf(stderr, "\n" ANSI_CYAN "RESULT:" ANSI_RESET " %d/%d PASS, %d FAIL, %d SKIP\n", passed,
            total, failed, skipped);

    if (nfailures > 0) {
        fprintf(stderr, "\n" ANSI_RED "FAILED TESTS:" ANSI_RESET "\n");
        for (int i = 0; i < nfailures; i++)
            fprintf(stderr, "  %-28s %s\n", failures[i].name, failures[i].detail);
    }

    cleanup_tmpdir();

    if (failed == 0 && skipped == 0) {
        fprintf(stderr, ANSI_GREEN "ALL TESTS PASSED" ANSI_RESET "\n");
    } else if (failed == 0) {
        fprintf(stderr, ANSI_YELLOW "ALL TESTS PASSED (with skips)" ANSI_RESET "\n");
    } else {
        fprintf(stderr, ANSI_RED "SOME TESTS FAILED" ANSI_RESET "\n");
    }

    fflush(stderr);
    __asm__ volatile("mov $169, %%rax; xor %%rdi, %%rdi; syscall" ::: "rax", "rdi");

    for (;;) pause();
}
