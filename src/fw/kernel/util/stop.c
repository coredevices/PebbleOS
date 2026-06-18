/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/dbgserial.h"
#include "console/dbgserial_input.h"
#include "drivers/flash.h"
#include "drivers/rtc.h"
#include "drivers/task_watchdog.h"
#include "os/tick.h"
#include "kernel/util/stop.h"
#include "mcu/interrupts.h"
#include "pbl/services/analytics/analytics.h"
#include "system/passert.h"
#include "console/dbgserial_input.h"

#include <cmsis_core.h>

#include <stdbool.h>
#include <inttypes.h>

static int s_num_items_disallowing_stop_mode = 0;

typedef struct {
  uint32_t active_count;
  RtcTicks ticks_when_stop_mode_disabled;
  RtcTicks total_ticks_while_disabled;
} InhibitorTickProfile;

// Note: These variables should be protected within a critical section since
// they are read and modified by multiple threads
static InhibitorTickProfile s_inhibitor_profile[InhibitorNumItems];

void stop_mode_disable( StopModeInhibitor inhibitor ) {
  portENTER_CRITICAL();
  ++s_num_items_disallowing_stop_mode;

  ++s_inhibitor_profile[inhibitor].active_count;
  // TODO: We should probably check if s_inhibitor_profile.active_count == 1
  // before doing this assignment. We don't seem to ever run into this case
  // yet (i.e. active_count is never > 1), but when we do, this code would
  // report the wrong number of nostop ticks.
  s_inhibitor_profile[inhibitor].ticks_when_stop_mode_disabled = rtc_get_ticks();
  portEXIT_CRITICAL();
}

void stop_mode_enable( StopModeInhibitor inhibitor ) {
  portENTER_CRITICAL();
  PBL_ASSERTN(s_num_items_disallowing_stop_mode != 0);
  PBL_ASSERTN(s_inhibitor_profile[inhibitor].active_count != 0);

  --s_num_items_disallowing_stop_mode;
  --s_inhibitor_profile[inhibitor].active_count;
  if (s_inhibitor_profile[inhibitor].active_count == 0) {
    s_inhibitor_profile[inhibitor].total_ticks_while_disabled += rtc_get_ticks() -
        s_inhibitor_profile[inhibitor].ticks_when_stop_mode_disabled;
  }
  portEXIT_CRITICAL();
}

bool stop_mode_is_allowed(void) {
#ifdef CONFIG_NOSTOP
  return false;
#else
  return s_num_items_disallowing_stop_mode == 0;
#endif
}