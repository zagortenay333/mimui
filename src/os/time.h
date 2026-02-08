#pragma once

#include "base/core.h"

istruct (Time) {
    U32 hours;
    U32 minutes;
    U32 seconds;
    U32 mseconds;
};

Time os_get_wall_time ();
U64  os_get_time_ms   ();
Void os_sleep_ms      (U64 msec);
