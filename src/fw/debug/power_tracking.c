/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "debug/power_tracking.h"

#include "drivers/rtc.h"
#include "services/common/regular_timer.h"
#include "system/logging.h"
#include "system/passert.h"

#if !defined(SW_POWER_TRACKING)

// a few dummy empty functions that should end up being compiled out
void power_tracking_init(void) {
}

void power_tracking_start(PowerSystem system) {
  // sanitize all uses of this function when you implement it
  (void) system;
}

void power_tracking_stop(PowerSystem system) {
  // sanitize all uses of this function when you implement it
  (void) system;
}

#else // SW_POWER_TRACKING

static bool s_initialized = false;

typedef struct {
  const char* const name;
  RtcTicks start_ticks;
  RtcTicks total_ticks;
  bool dirty;
} DiscreteSystemProfile;

static DiscreteSystemProfile s_discrete_consumer_profiles[num_power_systems] = {
  [PowerSystem2v5Reg] =           { "2v5Reg", 0, 0, false},
  [PowerSystem5vReg] =            { "5vReg", 0, 0, false},
  [PowerSystemMcuCoreSleep] =     { "McuCoreSleep", 0, 0, false},
  [PowerSystemMcuCoreRun] =       { "McuCoreRun", 0, 0, false},
  [PowerSystemMcuGpioA] =         { "McuGpioA", 0, 0, false},
  [PowerSystemMcuGpioB] =         { "McuGpioB", 0, 0, false},
  [PowerSystemMcuGpioC] =         { "McuGpioC", 0, 0, false},
  [PowerSystemMcuGpioD] =         { "McuGpioD", 0, 0, false},
  [PowerSystemMcuGpioH] =         { "McuGpioH", 0, 0, false},
  [PowerSystemMcuCrc] =           { "McuCrc", 0, 0, false},
  [PowerSystemMcuPwr] =           { "McuPwr", 0, 0, false},
  [PowerSystemMcuDma1] =          { "McuDma1", 0, 0, false},
  [PowerSystemMcuDma2] =          { "McuDma2", 0, 0, false},
  [PowerSystemMcuTim1] =          { "McuTim1", 0, 0, false},
  [PowerSystemMcuTim3] =          { "McuTim3", 0, 0, false},
  [PowerSystemMcuTim4] =          { "McuTim4", 0, 0, false},
  [PowerSystemMcuUsart1] =        { "McuUsart1", 0, 0, false},
  [PowerSystemMcuUsart3] =        { "McuUsart3", 0, 0, false},
  [PowerSystemMcuI2C1] =          { "McuI2C1", 0, 0, false},
  [PowerSystemMcuI2C2] =          { "McuI2C2", 0, 0, false},
  [PowerSystemMcuSpi1] =          { "McuSpi1", 0, 0, false},
#ifdef PLATFORM_TINTIN
  [PowerSystemMcuSpi2] =          { "McuSpi2", 0, 0, false},
#else
  [PowerSystemMcuSpi6] =          { "McuSpi6", 0, 0, false},
#endif
  [PowerSystemMcuAdc1] =          { "McuAdc1", 0, 0, false},
  [PowerSystemMcuAdc2] =          { "McuAdc2", 0, 0, false},
  [PowerSystemFlashRead] =        { "FlashRead", 0, 0, false},
  [PowerSystemFlashWrite] =       { "FlashWrite", 0, 0, false},
  [PowerSystemFlashErase] =       { "FlashErase", 0, 0, false},
  [PowerSystemAccelLowPower] =    { "AccelLowPower", 0, 0, false},
  [PowerSystemAccelNormal] =      { "AccelNormal", 0, 0, false},
  [PowerSystemMfi] =              { "Mfi", 0, 0, false},
  [PowerSystemMag] =              { "Mag", 0, 0, false},
  [PowerSystemBtShutdown] =       { "BtShutdown", 0, 0, false},
  [PowerSystemBtDeepSleep] =      { "BtDeepSleep", 0, 0, false},
  [PowerSystemBtActive] =         { "BtActive", 0, 0, false},
  [PowerSystemAmbient] =          { "Ambient", 0, 0, false},
  [PowerSystemProfiling] =        { "Profiling", 0, 0, false},
};

static const uint16_t power_tracking_integration_period_s = 1;

static void power_tracking_flush(void *);

static RegularTimerInfo s_power_profile_timer = {
  .list_node = { 0, 0 },
  .cb = power_tracking_flush,
};

static void power_tracking_flush(void *null) {
  power_tracking_start(PowerSystemProfiling);

  RtcTicks log_record_time = rtc_get_ticks();
  char buffer[32];

  for (int i = 0; i<num_power_systems; ++i) {
    DiscreteSystemProfile *current_profile = &s_discrete_consumer_profiles[i];

    if (current_profile->dirty) {
      RtcTicks total_ticks = current_profile->total_ticks;
      if (current_profile->start_ticks != 0) {
        // the event is still happening - log progress so far
        RtcTicks current_ticks = rtc_get_ticks();
        total_ticks += (current_ticks - current_profile->start_ticks);
        current_profile->start_ticks = current_ticks;
      } else {
        // the event is done - clean up
        current_profile->dirty = false;
      }

      current_profile->total_ticks = 0;

      if (total_ticks != 0) {
        // dump the current ticks
        dbgserial_putstr_fmt(buffer, sizeof(buffer), ">>>PWR:%"PRIu64",%s,%"PRIu64"<", log_record_time, current_profile->name, total_ticks);
      }
    }
  }
  power_tracking_stop(PowerSystemProfiling);
}

void power_tracking_init(void) {
  regular_timer_add_multisecond_callback(&s_power_profile_timer, power_tracking_integration_period_s);
  char buffer[32];
  dbgserial_putstr_fmt(buffer, sizeof(buffer), ">>>PWR:%"PRIu64",START,%"PRIu16, rtc_get_ticks(), power_tracking_integration_period_s);
  s_initialized = true;
}

void power_tracking_start(PowerSystem system) {
  if (!s_initialized) {
    return;
  }
  PBL_ASSERTN(system < num_power_systems);

  DiscreteSystemProfile *current_profile = &s_discrete_consumer_profiles[system];

  if (current_profile->start_ticks != 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "repeat call to start ticks without stopping from %s", current_profile->name);
    // Someone was careless: two cases:
    // 1) someone forgot to call stop
    // 2) someone re-enters a function that calls start before stop is called.
    return;
  }

  current_profile->start_ticks = rtc_get_ticks();

  current_profile->dirty = true;
}

void power_tracking_stop(PowerSystem system) {
  if (!s_initialized) {
    return;
  }
  PBL_ASSERTN(system < num_power_systems);

  DiscreteSystemProfile *current_profile = &s_discrete_consumer_profiles[system];

  if (current_profile->start_ticks == 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "Stop ticks before start called: probably losing profile accuracy in %s", current_profile->name);
    // Someone was careless: two cases:
    // 1) someone forgot to call start
    // 2) someone re-entered a function that called stop already, so it is called twice.
    return;
  }

  current_profile->total_ticks += (rtc_get_ticks() - current_profile->start_ticks);

  current_profile->start_ticks = 0;
}


#endif // SW_POWER_TRACKING
