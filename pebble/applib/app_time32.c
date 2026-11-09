/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// App ABI versions of time syscalls apps call directly.

#include <pbl/services/clock.h>

#include <applib/pbl_std/pbl_std.h>
#include <syscall/syscall.h>

pbl_time32_t time_start_of_today_time32(void) {
  return pbl_time_to_time32(sys_time_start_of_today());
}

pbl_time32_t clock_to_timestamp_time32(WeekDay day, int hour, int minute) {
  return pbl_time_to_time32(clock_to_timestamp(day, hour, minute));
}
