#ifndef TIME_H
#define TIME_H
#include <sys/types.h>

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct timeval {
    time_t tv_sec;
    long   tv_usec;
};

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1

int clock_gettime(int clock_id, struct timespec *tp);
time_t time(time_t *tloc);

#endif
