/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/time.h>
#include <pbl/services/time.h>

#include <services/clock/rtc_sync.h>
#include <services/clock/tz_storage.h>

// Kernel wall clock and uptime
///////////////////////////////////////////////////////////////////////////////

PBL_WEAK time_t pbl_realtime(void) {
  return 0;
}

PBL_WEAK uint64_t pbl_realtime_ms(void) {
  return 0;
}

PBL_WEAK void pbl_realtime_set(time_t t) {
}

PBL_WEAK uint64_t pbl_uptime_ticks(void) {
  return 0;
}

// Timezone storage
///////////////////////////////////////////////////////////////////////////////

PBL_WEAK void tz_storage_save(const TimezoneInfo *tz_info) {
}

PBL_WEAK void tz_storage_load(TimezoneInfo *tz_info) {
}

PBL_WEAK bool tz_storage_is_set(void) {
  return false;
}

PBL_WEAK void tz_storage_clear(void) {
}

// RTC sync
///////////////////////////////////////////////////////////////////////////////

PBL_WEAK void rtc_sync_save(void) {
}

PBL_WEAK void rtc_sync_start(void) {
}
