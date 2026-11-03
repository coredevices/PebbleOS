/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rtc_sync.h"

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/services/clock.h>
#include <pbl/services/regular_timer.h>

PBL_LOG_MODULE_DECLARE(service_clock, CONFIG_SERVICE_CLOCK_LOG_LEVEL);

// January 1, 2010 00:00:00 UTC. An RTC that lost its time reads earlier: snap forward so that the
// time is not that far off.
#define MIN_VALID_TIME 1262304000

static RegularTimerInfo s_save_timer;

void clock_restore_time(void) {
  time_t t = 0;
  int ret;

  ret = pbl_device_init(&RTC->dev);
  if (ret == 0) {
    ret = pbl_rtc_get_time(RTC, &t);
  }
  if (ret != 0) {
    PBL_LOG_ERR("RTC read failed (%d)", ret);
  }

  if (t < MIN_VALID_TIME) {
    t = MIN_VALID_TIME;
    (void)pbl_rtc_set_time(RTC, t);
  }

  pbl_realtime_set(t);
}

void rtc_sync_save(void) {
  int ret;

  // The RTC counts whole seconds from when it is set.
  ret = pbl_rtc_set_time(RTC, (time_t)((pbl_realtime_ms() + 500U) / 1000U));
  if (ret != 0) {
    PBL_LOG_WRN("RTC write failed (%d)", ret);
  }
}

static void prv_save_cb(void *data) {
  rtc_sync_save();
}

static void prv_sys_off(enum pbl_sys_off_mode mode, void *user_data) {
  rtc_sync_save();
}

// Last, so that the time saved is the latest.
PBL_SYS_OFF_HANDLER_DEFINE(rtc_sync_restart, PBL_SYS_OFF_RESTART_PREPARE, PBL_SYS_OFF_PRIO_LOW,
                           prv_sys_off, nullptr);
PBL_SYS_OFF_HANDLER_DEFINE(rtc_sync_power_off, PBL_SYS_OFF_POWER_OFF_PREPARE, PBL_SYS_OFF_PRIO_LOW,
                           prv_sys_off, nullptr);

void rtc_sync_start(void) {
  s_save_timer = (RegularTimerInfo){.cb = prv_save_cb};
  regular_timer_add_multisecond_callback(&s_save_timer, CONFIG_SERVICE_CLOCK_RTC_SAVE_INTERVAL);
}
