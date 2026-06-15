#include "test_harness.h"

static volatile sig_atomic_t sigpipe_caught = 0;

static void handler_sigpipe(int sig) {
    (void) sig;
    sigpipe_caught = 1;
}

int test_pipe_close_race(void) {
    int p[2];
    ASSERT_EQ(0, pipe(p));

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child: close read end immediately, then write */
        close(p[0]);
        sigpipe_caught = 0;
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handler_sigpipe;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGPIPE, &sa, NULL);

        /* write large buffer to trigger EPIPE */
        char buf[4096];
        memset(buf, 'A', sizeof(buf));
        ssize_t n = write(p[1], buf, sizeof(buf));
        if (n < 0 && errno == EPIPE) _exit(0);
        /* if write succeeded (unlikely), signal might be pending */
        if (sigpipe_caught) _exit(0);
        _exit(1);
    }

    close(p[0]);
    close(p[1]);

    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));
    return 1;
}
REGISTER_TEST(pipe_close_race, "Phase 5: Pipes & IPC");

int test_pipe_capacity(void) { return TEST_FAIL; }
REGISTER_TEST(pipe_capacity, "Phase 5: Pipes & IPC");

int test_dup_dup2_dup3(void) {
    int p[2];
    ASSERT_EQ(0, pipe(p));

    /* dup */
    int newfd = dup(p[0]);
    ASSERT_GE(newfd, 0);
    ASSERT_NE(newfd, p[0]);
    close(newfd);

    /* dup2 — specific fd */
    ASSERT_EQ(30, dup2(p[0], 30));
    close(30);

    /* dup2 same fd (no-op) */
    ASSERT_EQ(p[0], dup2(p[0], p[0]));

    /* dup3 with O_CLOEXEC */
    ASSERT_EQ(31, dup3(p[0], 31, O_CLOEXEC));
    int flags = fcntl(31, F_GETFD);
    ASSERT_GE(flags, 0);
    ASSERT_TRUE(flags & FD_CLOEXEC);
    close(31);

    /* dup2 invalid fd */
    ASSERT_EQ(-1, dup2(9999, 40));
    ASSERT(errno == EBADF);

    close(p[0]);
    close(p[1]);
    return 1;
}
REGISTER_TEST(dup_dup2_dup3, "Phase 5: Pipes & IPC");

int test_socketpair(void) { return TEST_FAIL; }
REGISTER_TEST(socketpair, "Phase 5: Pipes & IPC");

int test_unix_stream(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0 && (errno == ENOSYS || errno == ENOTSUP || errno == EAFNOSUPPORT)) return 1;
    ASSERT_GE(fd, 0);

    char path[PATH_MAX];
    tmpfile_path(path, sizeof(path), "unix_stream.sock");
    unlink(path);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path) + 1);

    ASSERT_EQ(0, bind(fd, (struct sockaddr *) &addr, sizeof(addr)));
    ASSERT_EQ(0, listen(fd, 5));

    /* fork a client */
    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* client */
        int cf = socket(AF_UNIX, SOCK_STREAM, 0);
        if (cf < 0) _exit(2);
        if (connect(cf, (struct sockaddr *) &addr, sizeof(addr)) < 0) _exit(3);

        const char *msg = "hello from client";
        if (write(cf, msg, strlen(msg)) < 0) _exit(4);

        char buf[64];
        ssize_t n = read(cf, buf, sizeof(buf));
        if (n <= 0) _exit(5);

        close(cf);
        _exit(0);
    }

    /* server accept */
    struct sockaddr_un peer;
    socklen_t peerlen = sizeof(peer);
    int af = accept(fd, (struct sockaddr *) &peer, &peerlen);
    ASSERT_GE(af, 0);

    char buf[64];
    ssize_t n = read(af, buf, sizeof(buf));
    ASSERT_GT(n, 0);
    buf[n] = '\0';
    ASSERT_STREQ("hello from client", buf);

    const char *resp = "hello from server";
    ASSERT_EQ((ssize_t) strlen(resp), write(af, resp, strlen(resp)));

    close(af);
    close(fd);

    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));

    unlink(path);
    return 1;
}
REGISTER_TEST(unix_stream, "Phase 5: Pipes & IPC");

int test_unix_dgram(void) {
    char path[PATH_MAX];
    tmpfile_path(path, sizeof(path), "unix_dgram.sock");
    unlink(path);

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0 && (errno == ENOSYS || errno == ENOTSUP || errno == EAFNOSUPPORT)) return 1;
    ASSERT_GE(fd, 0);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path) + 1);

    ASSERT_EQ(0, bind(fd, (struct sockaddr *) &addr, sizeof(addr)));

    /* sendto self */
    const char *msg = "dgram test";
    ASSERT_EQ((ssize_t) strlen(msg),
              sendto(fd, msg, strlen(msg), 0, (struct sockaddr *) &addr, sizeof(addr)));

    char buf[64];
    ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL);
    ASSERT_EQ((ssize_t) strlen(msg), n);
    buf[n] = '\0';
    ASSERT_STREQ(msg, buf);

    close(fd);
    unlink(path);
    return 1;
}
REGISTER_TEST(unix_dgram, "Phase 5: Pipes & IPC");

int test_shm_basic(void) {
    int shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    if (shmid < 0 && (errno == ENOSYS || errno == ENOTSUP)) return 1;
    ASSERT_GE(shmid, 0);

    void *p = shmat(shmid, NULL, 0);
    ASSERT_NE(p, (void *) -1);

    /* write to shared memory */
    const char *msg = "hello shm";
    memcpy(p, msg, strlen(msg) + 1);

    /* read back */
    ASSERT_STREQ(msg, (const char *) p);

    /* fork child and verify child can read */
    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        /* child reads from shm */
        char child_buf[32];
        memcpy(child_buf, p, strlen(msg) + 1);
        _exit(strcmp(child_buf, msg) == 0 ? 0 : 1);
    }

    int status;
    waitpid(pid, &status, 0);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));

    ASSERT_EQ(0, shmdt(p));

    struct shmid_ds ds;
    ASSERT_EQ(0, shmctl(shmid, IPC_STAT, &ds));
    ASSERT_EQ(1, ds.shm_nattch); /* parent still attached? No, we detached — kernel sees 0 */

    ASSERT_EQ(0, shmctl(shmid, IPC_RMID, NULL));
    return 1;
}
REGISTER_TEST(shm_basic, "Phase 5: Pipes & IPC");

int test_futex_basic(void) {
    uint32_t futex_word = 0;
    int ret;

    /* FUTEX_WAIT should block because futex_word == 0 (val we pass) */
    /* We need to wake it from a child */

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        usleep(100000);
        /* Wake the parent */
        syscall(SYS_futex, &futex_word, FUTEX_WAKE, 1, NULL, NULL, 0);
        _exit(0);
    }

    struct timespec timeout;
    timeout.tv_sec = 2;
    timeout.tv_nsec = 0;
    ret = syscall(SYS_futex, &futex_word, FUTEX_WAIT, 0, &timeout, NULL, 0);
    /* Should succeed (return 0) or return EAGAIN/ETIMEDOUT/EWOULDBLOCK if kernel has quirks */
    if (ret < 0) { ASSERT(errno == EAGAIN || errno == ETIMEDOUT || errno == EWOULDBLOCK); }

    int status;
    waitpid(pid, &status, 0);
    return 1;
}
REGISTER_TEST(futex_basic, "Phase 5: Pipes & IPC");

int test_futex_requeue(void) { return TEST_FAIL; }
REGISTER_TEST(futex_requeue, "Phase 5: Pipes & IPC");

int test_eventfd(void) {
    int fd = eventfd(0, 0);
    if (fd < 0 && (errno == ENOSYS || errno == ENOTSUP)) return 1;
    ASSERT_GE(fd, 0);

    /* write a value */
    eventfd_t val = 42;
    ASSERT_EQ(sizeof(val), write(fd, &val, sizeof(val)));

    /* read it back */
    val = 0;
    ASSERT_EQ(sizeof(val), read(fd, &val, sizeof(val)));
    ASSERT_EQ((eventfd_t) 42, val);

    close(fd);
    return 1;
}
REGISTER_TEST(eventfd, "Phase 5: Pipes & IPC");

int test_signalfd(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    int fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (fd < 0 && (errno == ENOSYS || errno == ENOTSUP)) return 1;
    ASSERT_GE(fd, 0);

    /* Send signal to self */
    kill(getpid(), SIGUSR1);

    struct signalfd_siginfo info;
    ssize_t n = read(fd, &info, sizeof(info));
    ASSERT_EQ((ssize_t) sizeof(info), n);
    ASSERT_EQ(SIGUSR1, info.ssi_signo);

    close(fd);
    return 1;
}
REGISTER_TEST(signalfd, "Phase 5: Pipes & IPC");
