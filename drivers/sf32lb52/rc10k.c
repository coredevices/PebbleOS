/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/regular_timer.h>

#include <bf0_hal.h>

PBL_LOG_MODULE_DECLARE(driver_rtc_sf32lb, CONFIG_DRIVER_RTC_LOG_LEVEL);

#define RC10K_DEFAULT_FREQ_HZ 10000UL
// One sample per minute; 25 of them keep the average over the last ~25 minutes.
#define RC10K_CAL_AVE_WINDOW 25

static void prv_rc10k_cal_timer_cb(void *data) {
  uint8_t lp_cycle;

  lp_cycle = HAL_RC_CAL_GetLPCycle();
  // A dropped sample leaves the RTC running on a stale reference; the LCPU also
  // drives RC calibration here, so losing the mailbox is expected but must not
  // be silent - a run of these means the clock is drifting uncorrected.
  const int rv = HAL_RC_CAL_update_reference_cycle_on_48M_ex(lp_cycle, 0, RC10K_CAL_AVE_WINDOW);
  if (rv != 0) {
    PBL_LOG_WRN("RC10K calibration failed: %d", rv);
  }
}

void rc10k_init(void) {
  static RegularTimerInfo s_cal_timer = {.cb = prv_rc10k_cal_timer_cb};

  prv_rc10k_cal_timer_cb(nullptr);
  regular_timer_add_minutes_callback(&s_cal_timer);
}

uint32_t rc10k_get_freq_hz(void) {
  uint32_t hxt48_cyc;

  hxt48_cyc = HAL_RC_CAL_get_average_cycle_on_48M();
  if (hxt48_cyc == 0UL) {
    return RC10K_DEFAULT_FREQ_HZ;
  } else {
    return (48000000ULL * HAL_RC_CAL_GetLPCycle()) / hxt48_cyc;
  }
}

uint32_t rc10k_cyc_to_milli_ticks(uint32_t rc10k_cyc) {
  uint32_t hxt48_cyc;

  hxt48_cyc = HAL_RC_CAL_get_average_cycle_on_48M();
  if (hxt48_cyc == 0UL) {
    return (1000ULL * RTC_TICKS_HZ * rc10k_cyc) / RC10K_DEFAULT_FREQ_HZ;
  } else {
    return (1000ULL * RTC_TICKS_HZ * rc10k_cyc * hxt48_cyc) /
           (48000000ULL * HAL_RC_CAL_GetLPCycle());
  }
}