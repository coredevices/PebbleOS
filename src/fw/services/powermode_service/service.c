/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/powermode_service.h"

#include "drivers/cpumode.h"
#include "drivers/rtc.h"
#include "os/mutex.h"
#include "pbl/services/new_timer/new_timer.h"
#include "system/logging.h"
#include "system/passert.h"

#include <stdint.h>

PBL_LOG_MODULE_DEFINE(service_powermode_service, CONFIG_SERVICE_POWERMODE_SERVICE_LOG_LEVEL);

#define CPU_TIER_COUNT 3
#define DOWNSCALE_DELAY_MS 100
#define BOOST_MIN_MS 50
#define LAG_HOLD_MS 2000

//! Lowest tier is the light tier (48 MHz): the 24 MHz idle tier was dropped
//! because it could not draw the screen on time and its lenient work budget
//! kept the CPU parked there instead of stepping up.
static const uint32_t s_tier_mhz[CPU_TIER_COUNT] = {
    CPUMODE_FREQ_LIGHT_MHZ,
    CPUMODE_FREQ_MEDIUM_MHZ,
    CPUMODE_FREQ_HIGH_MHZ,
};

static uint32_t s_refcount;
static PebbleMutex *s_mutex;
static bool s_enabled;
static bool s_boot_complete;
static uint8_t s_current_tier_idx;
static uint8_t s_lag_tier_idx;
static RtcTicks s_boost_until_ticks;
static RtcTicks s_lag_floor_until_ticks;
static TimerID s_downscale_timer = TIMER_INVALID_ID;

static void prv_downscale_timer_cb(void *data);

static bool prv_boost_active(void) {
  return rtc_get_ticks() < s_boost_until_ticks;
}

static bool prv_lag_floor_active(void) {
  return rtc_get_ticks() < s_lag_floor_until_ticks;
}

static uint8_t prv_demand_tier_idx(void) {
  if (s_refcount > 0) {
    return CPU_TIER_COUNT - 1;
  }
  if (prv_boost_active()) {
    // Medium tier (144 MHz) for bursts of UI work so the screen draws on time.
    return 1;
  }
  return 0;
}

static uint8_t prv_target_tier_idx(void) {
  uint8_t target = prv_demand_tier_idx();
  if (prv_lag_floor_active() && s_lag_tier_idx > target) {
    target = s_lag_tier_idx;
  }
  return target;
}

static void prv_set_tier_idx(uint8_t tier_idx) {
  if (tier_idx >= CPU_TIER_COUNT) {
    tier_idx = CPU_TIER_COUNT - 1;
  }
  if (tier_idx == s_current_tier_idx) {
    return;
  }

  s_current_tier_idx = tier_idx;
  cpumode_set_freq_mhz(s_tier_mhz[tier_idx]);
  PBL_LOG_DBG("CPU %u MHz", (unsigned)s_tier_mhz[tier_idx]);
}

static void prv_schedule_demand_recheck(void) {
  uint32_t delay_ms = DOWNSCALE_DELAY_MS;

  if (s_boost_until_ticks > rtc_get_ticks()) {
    const uint64_t remaining_ticks = s_boost_until_ticks - rtc_get_ticks();
    const uint32_t boost_ms = (uint32_t)((remaining_ticks * 1000) / RTC_TICKS_HZ);
    delay_ms = boost_ms + DOWNSCALE_DELAY_MS;
  }

  new_timer_start(s_downscale_timer, delay_ms, prv_downscale_timer_cb, NULL, 0);
}

static void prv_apply_locked(void) {
  if (!s_enabled || !s_boot_complete) {
    prv_set_tier_idx(CPU_TIER_COUNT - 1);
    new_timer_stop(s_downscale_timer);
    return;
  }

  const uint8_t target = prv_target_tier_idx();

  if (target > s_current_tier_idx) {
    prv_set_tier_idx(target);
  }

  if (s_current_tier_idx > target) {
    prv_schedule_demand_recheck();
  } else if (s_refcount == 0 && (prv_boost_active() || prv_lag_floor_active())) {
    prv_schedule_demand_recheck();
  } else {
    new_timer_stop(s_downscale_timer);
  }
}

static void prv_downscale_timer_cb(void *data) {
  (void)data;

  mutex_lock(s_mutex);

  if (!s_enabled || !s_boot_complete) {
    mutex_unlock(s_mutex);
    return;
  }

  const uint8_t target = prv_target_tier_idx();

  if (s_current_tier_idx > target) {
    prv_set_tier_idx(s_current_tier_idx - 1);
    if (s_current_tier_idx > target) {
      prv_schedule_demand_recheck();
    }
  } else if (target > s_current_tier_idx) {
    prv_apply_locked();
  }

  mutex_unlock(s_mutex);
}

static uint32_t prv_lag_budget_ms(void) {
  switch (s_tier_mhz[s_current_tier_idx]) {
    case CPUMODE_FREQ_LIGHT_MHZ:
      return 16;
    case CPUMODE_FREQ_MEDIUM_MHZ:
      return 8;
    default:
      return UINT32_MAX;
  }
}

void powermode_service_init(void) {
  s_refcount = 0;
  s_mutex = mutex_create();
  s_boost_until_ticks = 0;
  s_lag_floor_until_ticks = 0;
  s_lag_tier_idx = 0;
  s_current_tier_idx = CPU_TIER_COUNT - 1;

  s_downscale_timer = new_timer_create();
}

void powermode_service_boot_complete(void) {
  mutex_lock(s_mutex);
  s_boot_complete = true;
  prv_apply_locked();
  mutex_unlock(s_mutex);
}

void powermode_service_set_enabled(bool enabled) {
  mutex_lock(s_mutex);
  s_enabled = enabled;
  prv_apply_locked();
  mutex_unlock(s_mutex);
}

void powermode_service_boost_ms(uint32_t ms) {
  if (!s_enabled) {
    return;
  }

  if (ms < BOOST_MIN_MS) {
    ms = BOOST_MIN_MS;
  }

  mutex_lock(s_mutex);

  const RtcTicks now = rtc_get_ticks();
  const RtcTicks extend = ((uint64_t)ms * RTC_TICKS_HZ) / 1000;
  const RtcTicks until = now + extend;
  if (until > s_boost_until_ticks) {
    s_boost_until_ticks = until;
  }

  prv_apply_locked();
  mutex_unlock(s_mutex);
}

void powermode_service_report_work_ms(uint32_t duration_ms) {
  if (!s_enabled || !s_boot_complete) {
    return;
  }

  mutex_lock(s_mutex);

  const uint32_t budget_ms = prv_lag_budget_ms();
  if (duration_ms > budget_ms && s_current_tier_idx < CPU_TIER_COUNT - 1) {
    const uint8_t next_tier = s_current_tier_idx + 1;
    if (s_lag_tier_idx < next_tier) {
      s_lag_tier_idx = next_tier;
    }
    s_lag_floor_until_ticks = rtc_get_ticks() + ((uint64_t)LAG_HOLD_MS * RTC_TICKS_HZ) / 1000;
    prv_apply_locked();
  }

  mutex_unlock(s_mutex);
}

void powermode_service_request_hp(void) {
  if (!s_enabled) {
    return;
  }

  mutex_lock(s_mutex);

  if (s_refcount == 0) {
    prv_set_tier_idx(CPU_TIER_COUNT - 1);
  }

  s_refcount++;

  mutex_unlock(s_mutex);
}

void powermode_service_release_hp(void) {
  if (!s_enabled) {
    return;
  }

  mutex_lock(s_mutex);

  if (s_refcount == 0) {
    mutex_unlock(s_mutex);
    return;
  }

  s_refcount--;

  if (s_refcount == 0) {
    prv_apply_locked();
  }

  mutex_unlock(s_mutex);
}
