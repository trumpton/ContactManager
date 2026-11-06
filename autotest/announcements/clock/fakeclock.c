/*
 * fakeclock.so: the wall clock of the announcements bench (autotest/announcements).
 *
 * Preloaded into the program (LD_PRELOAD).  The program's wall clock runs at FAKE_SPEED times real time from FAKE_START:
 *
 *   FAKE_START  the fake time when the process starts, in seconds since 1970 (decimal, may have a fraction)
 *   FAKE_SPEED  the speed multiplier (360 for the announcement tests: one real second is six minutes)
 *
 * Only the realtime clock is changed (clock_gettime(CLOCK_REALTIME), gettimeofday() and time()).  The monotonic clock is
 * left at real speed, so the program's timers (its one-second check) run at real speed: the check runs once per real second,
 * which is once per six fake minutes at 360 times (specification 0.7).
 *
 * Test build only.  It is never linked into the program, and it is not used outside the bench.
 *
 * Build: gcc -shared -fPIC -O2 -o fakeclock.so fakeclock.c -ldl
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

typedef int (*clock_gettime_fn)(clockid_t, struct timespec *);

static int initialised = 0;
static double speed = 1.0;
static double fake_start = 0.0;
static struct timespec mono_start;

static clock_gettime_fn real_clock_gettime(void)
{
    static clock_gettime_fn fn = NULL;
    if (!fn) fn = (clock_gettime_fn)dlsym(RTLD_NEXT, "clock_gettime");
    return fn;
}

static void initialise(void)
{
    const char *s;
    if (initialised) return;
    initialised = 1;
    s = getenv("FAKE_SPEED");
    if (s) speed = atof(s);
    s = getenv("FAKE_START");
    if (s) fake_start = atof(s);
    real_clock_gettime()(CLOCK_MONOTONIC, &mono_start);
}

/* The fake time now, in seconds since 1970 */
static double fake_now(void)
{
    struct timespec mono;
    double elapsed;
    initialise();
    real_clock_gettime()(CLOCK_MONOTONIC, &mono);
    elapsed = (double)(mono.tv_sec - mono_start.tv_sec) + (double)(mono.tv_nsec - mono_start.tv_nsec) / 1e9;
    return fake_start + elapsed * speed;
}

int clock_gettime(clockid_t clk, struct timespec *ts)
{
    if (clk == CLOCK_REALTIME) {
        double t = fake_now();
        ts->tv_sec = (time_t)t;
        ts->tv_nsec = (long)((t - (double)ts->tv_sec) * 1e9);
        return 0;
    }
    return real_clock_gettime()(clk, ts);
}

int gettimeofday(struct timeval *tv, void *tz)
{
    double t = fake_now();
    (void)tz;
    tv->tv_sec = (time_t)t;
    tv->tv_usec = (suseconds_t)((t - (double)tv->tv_sec) * 1e6);
    return 0;
}

time_t time(time_t *out)
{
    time_t t = (time_t)fake_now();
    if (out) *out = t;
    return t;
}
