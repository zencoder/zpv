#ifdef __MACH__

// clock_gettime is not implemented on OSX, so stub it out.
#include <sys/time.h>

int clock_gettime(int clk_id, struct timespec* t) {
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}

// We also need to define the clock type.
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

#endif
