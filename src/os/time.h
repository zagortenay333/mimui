#pragma once

#include "base/core.h"

istruct (Date) {
    U32 year;
    U32 month;
    U32 day;
    U32 week;
};

istruct (Time) {
    U32 hours;
    U32 minutes;
    U32 seconds;
    U32 mseconds;
};

U32  os_days_in_month (U32 year, U32 month);
Time os_get_wall_time ();
U64  os_get_time_ms   ();
Void os_sleep_ms      (U64 msec);
