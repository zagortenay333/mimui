#include <time.h>
#include <unistd.h>
#include "base/core.h"
#include "os/time.h"

Void os_sleep_ms (U64 msec) {
    struct timespec ts_sleep = { msec/1000, (msec % 1000) * 1000000 };
    nanosleep(&ts_sleep, NULL);
}

U64 os_get_time_ms () {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return cast(U64, (ts.tv_sec * 1000) + (ts.tv_nsec / 1'000'000));
}

Time os_get_wall_time () {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm local;
    localtime_r(&ts.tv_sec, &local);

    Time t;
    t.hours    = cast(U32, local.tm_hour);
    t.minutes  = cast(U32, local.tm_min);
    t.seconds  = cast(U32, local.tm_sec);
    t.mseconds = cast(U32, ts.tv_nsec / 1000000);

    return t;
}
