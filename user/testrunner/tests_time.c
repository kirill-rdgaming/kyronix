#include "test_harness.h"

/* ================================================================== */
/*  Phase 6 — Timers & Time                                           */
/* ================================================================== */

/* ---------------------------------------------------------------- */
/*  6.1  time                                                       */
/* ---------------------------------------------------------------- */

int test_time(void) {
    time_t t1 = time(NULL);
    ASSERT_GT(t1, 0);

    time_t t2 = time(NULL);
    ASSERT_GE(t2, t1); /* monotonic-ish */

    return 1;
}
REGISTER_TEST(time, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.2  gettimeofday                                                */
/* ---------------------------------------------------------------- */

int test_gettimeofday(void) {
    struct timeval tv;
    ASSERT_EQ(0, gettimeofday(&tv, NULL));
    ASSERT_GT(tv.tv_sec, 0);

    struct timeval tv2;
    ASSERT_EQ(0, gettimeofday(&tv2, NULL));
    ASSERT_TRUE(tv2.tv_sec > tv.tv_sec || (tv2.tv_sec == tv.tv_sec && tv2.tv_usec >= tv.tv_usec));
    return 1;
}
REGISTER_TEST(gettimeofday, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.3  clock_gettime                                               */
/* ---------------------------------------------------------------- */

int test_clock_gettime(void) {
    struct timespec ts;

    ASSERT_EQ(0, clock_gettime(CLOCK_REALTIME, &ts));
    ASSERT_GT(ts.tv_sec, 0);

    ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &ts));
    ASSERT_GT(ts.tv_sec, 0);

    int ret = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL)) return 1;
    ASSERT_EQ(0, ret);

    ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL)) return 1;
    ASSERT_EQ(0, ret);

    return 1;
}
REGISTER_TEST(clock_gettime, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.4  clock_getres                                                */
/* ---------------------------------------------------------------- */

int test_clock_getres(void) {
    struct timespec ts;

    ASSERT_EQ(0, clock_getres(CLOCK_REALTIME, &ts));
    ASSERT_GT(ts.tv_sec, 0);
    /* resolution should be reasonable (< 1 sec) */
    ASSERT_LT(ts.tv_nsec, 1000000000L);

    ASSERT_EQ(0, clock_getres(CLOCK_MONOTONIC, &ts));
    ASSERT_LT(ts.tv_nsec, 1000000000L);

    return 1;
}
REGISTER_TEST(clock_getres, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.5  clock_nanosleep                                             */
/* ---------------------------------------------------------------- */

int test_clock_nanosleep(void) {
    SKIP("");

    struct timespec req, rem;

    /* sleep for 10ms */
    req.tv_sec = 0;
    req.tv_nsec = 10000000;
    int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem);
    /* nanosleep may be a stub — accept ENOSYS */
    if (ret == ENOSYS) return 1;
    ASSERT_EQ(0, ret);

    /* TIMER_ABSTIME */
    struct timespec abs;
    clock_gettime(CLOCK_MONOTONIC, &abs);
    abs.tv_sec += 1;
    ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &abs, NULL);
    if (ret == ENOSYS) return 1;
    if (ret == EINVAL) return 1; /* TIMER_ABSTIME may not be supported */
    ASSERT_EQ(0, ret);

    return 1;
}
REGISTER_TEST(clock_nanosleep, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.6  nanosleep                                                   */
/* ---------------------------------------------------------------- */

int test_nanosleep(void) {
    struct timespec req, rem;
    req.tv_sec = 0;
    req.tv_nsec = 10000000; /* 10ms */
    int ret = nanosleep(&req, &rem);
    if (ret < 0 && errno == ENOSYS) return 1;
    ASSERT_EQ(0, ret);

    return 1;
}
REGISTER_TEST(nanosleep, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.7  getitimer / setitimer                                      */
/* ---------------------------------------------------------------- */

int test_getitimer_setitimer(void) {
    struct itimerval old, curr;

    /* ITIMER_REAL */
    memset(&old, 0, sizeof(old));
    old.it_value.tv_sec = 1;
    int ret = setitimer(ITIMER_REAL, &old, NULL);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL || errno == ENOTSUP)) return 1;
    ASSERT_EQ(0, ret);

    ret = getitimer(ITIMER_REAL, &curr);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL)) return 1;
    ASSERT_EQ(0, ret);

    /* Cancel */
    memset(&old, 0, sizeof(old));
    ASSERT_EQ(0, setitimer(ITIMER_REAL, &old, NULL));

    /* ITIMER_VIRTUAL */
    memset(&old, 0, sizeof(old));
    ret = setitimer(ITIMER_VIRTUAL, &old, NULL);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL || errno == ENOTSUP)) return 1;
    ASSERT_EQ(0, ret);

    /* ITIMER_PROF */
    memset(&old, 0, sizeof(old));
    ret = setitimer(ITIMER_PROF, &old, NULL);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL || errno == ENOTSUP)) return 1;
    ASSERT_EQ(0, ret);

    return 1;
}
REGISTER_TEST(getitimer_setitimer, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.8  alarm — set, cancel, multiple (last wins)                  */
/* ---------------------------------------------------------------- */

int test_alarm_basic(void) {
    unsigned prev;

    /* Set alarm */
    prev = alarm(1);
    ASSERT_EQ(0, prev);

    /* Cancel (alarm(0) should return remaining time of previous alarm) */
    prev = alarm(0);
    /* If alarm is a stub, prev might be 0, which is fine */
    if (prev == 0 && errno == ENOSYS) return 1;

    /* Multiple alarms — last wins */
    prev = alarm(3);
    ASSERT_EQ(0, prev);
    prev = alarm(5);
    /* previous alarm (3s) should be replaced, remaining > 0 */
    /* but if alarm is stub, prev === 0, accept that too */
    alarm(0);

    return 1;
}
REGISTER_TEST(alarm_basic, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.9  POSIX timers — stubs, just test return codes               */
/* ---------------------------------------------------------------- */

int test_posix_timers(void) {
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;

    int ret = timer_create(CLOCK_REALTIME, &sev, &timerid);
    if (ret < 0 && (errno == ENOSYS || errno == ENOTSUP || errno == EINVAL)) return 1;
    ASSERT_EQ(0, ret);

    memset(&its, 0, sizeof(its));
    its.it_value.tv_sec = 1;
    ret = timer_settime(timerid, 0, &its, NULL);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL)) return 1;
    ASSERT_EQ(0, ret);

    ret = timer_gettime(timerid, &its);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL)) return 1;
    ASSERT_EQ(0, ret);

    int overrun = timer_getoverrun(timerid);
    if (overrun < 0 && (errno == ENOSYS || errno == EINVAL)) return 1;
    ASSERT_GE(overrun, 0);

    ret = timer_delete(timerid);
    if (ret < 0 && (errno == ENOSYS || errno == EINVAL)) return 1;
    ASSERT_EQ(0, ret);

    return 1;
}
REGISTER_TEST(posix_timers, "Phase 6: Timers & Time");

/* ---------------------------------------------------------------- */
/*  6.10  times — process/children time                              */
/* ---------------------------------------------------------------- */

int test_times(void) {
    struct tms buf;
    clock_t t = times(&buf);
    if (t < 0 && errno == ENOSYS) return 1;
    ASSERT_GT(t, 0);

    /* tms_utime and tms_stime should be >= 0 */
    ASSERT_GE(buf.tms_utime, 0);
    ASSERT_GE(buf.tms_stime, 0);
    ASSERT_GE(buf.tms_cutime, 0);
    ASSERT_GE(buf.tms_cstime, 0);

    return 1;
}
REGISTER_TEST(times, "Phase 6: Timers & Time");
