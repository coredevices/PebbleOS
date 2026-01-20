/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/new_timer/new_timer.h"
#include "system/passert.h"

#include "bf0_hal.h"

#define RC10K_DEFAULT_FREQ_HZ 9700UL
#define RC10K_CAL_PERIOD_MS 15000U

static TimerID s_rc10k_cal_timer;

static void prv_rc10k_cal_timer_cb(void *data) {
  uint8_t lp_cycle;

  lp_cycle = HAL_RC_CAL_GetLPCycle();
  HAL_RC_CAL_update_reference_cycle_on_48M(lp_cycle);
}

void rc10k_init(void) {
  prv_rc10k_cal_timer_cb(NULL);

  s_rc10k_cal_timer = new_timer_create();
  PBL_ASSERTN(s_rc10k_cal_timer != TIMER_INVALID_ID);

  bool success = new_timer_start(s_rc10k_cal_timer, RC10K_CAL_PERIOD_MS, prv_rc10k_cal_timer_cb,
                                 NULL, TIMER_START_FLAG_REPEATING);
  PBL_ASSERTN(success);
}

uint32_t rc10k_get_freq_hz(void) {
  uint32_t cycle;
  
  cycle = HAL_RC_CAL_get_average_cycle_on_48M();
  if (cycle == 0UL) {
    return RC10K_DEFAULT_FREQ_HZ;
  } else {
    return (48000000ULL * HAL_RC_CAL_GetLPCycle()) / cycle;
  }
}