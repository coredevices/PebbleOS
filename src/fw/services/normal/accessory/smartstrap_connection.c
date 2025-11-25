/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/accessory.h"
#include "services/common/new_timer/new_timer.h"
#include "services/normal/accessory/accessory_manager.h"
#include "services/normal/accessory/smartstrap_attribute.h"
#include "services/normal/accessory/smartstrap_comms.h"
#include "services/normal/accessory/smartstrap_connection.h"
#include "services/normal/accessory/smartstrap_link_control.h"
#include "services/normal/accessory/smartstrap_state.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#include <stddef.h>

//! How long to wait after failing to acquire the accessory in ms before trying again
static const uint32_t ACCESSORY_ACQUIRE_INTERVAL = 5000;
//! The backoff before trying to detect a smartstrap again in ms
static const uint32_t DETECTION_BACKOFF = 200;
//! The maximum interval between detection attempts in ms
static const uint32_t DETECTION_MAX_INTERVAL = 10000;
//! When we expect something will kick us, we'll use this value as a timeout just in-case.
static const uint32_t KICK_TIMEOUT_INTERVAL = 2000;
//! If we hit bus contention during sending, we should wait this number of milliseconds.
static const uint32_t BUS_CONTENTION_INTERVAL = 100;

//! Subscriber information
static int s_subscriber_count;
//! Timer used for monitoring the connection and sending pending requests
static TimerID s_monitor_timer = TIMER_INVALID_ID;
//! The last time we got valid data from the smartstrap
static time_t s_last_data_time = 0;


void smartstrap_connection_init(void) {
  s_monitor_timer = new_timer_create();
}

static bool prv_acquire_accessory(void) {
  if (accessory_manager_set_state(AccessoryInputStateSmartstrap)) {
    // enable the accessory port
    accessory_set_baudrate(AccessoryBaud9600);
    accessory_set_power(true);
    smartstrap_comms_set_enabled(true);
    return true;
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "The accessory is already in use");
    return false;
  }
}

static void prv_release_accessory(void) {
  PBL_ASSERTN(!s_subscriber_count);

  smartstrap_fsm_state_set(SmartstrapStateUnsubscribed);
  PBL_LOG(LOG_LEVEL_DEBUG, "Disconnecting from smartstrap");
  smartstrap_link_control_disconnect();
  smartstrap_comms_set_enabled(false);
  new_timer_stop(s_monitor_timer);
  // stop any in-progress write
  accessory_send_stream_stop();
  // release the accessory port
  PBL_ASSERTN(accessory_manager_set_state(AccessoryInputStateIdle));
}

static void prv_monitor_timer_cb(void *context);

static void prv_monitor_system_task_cb(void *context) {
  static uint32_t s_monitor_interval = 0;
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  smartstrap_state_lock();
  if (s_subscriber_count == 0) {
    prv_release_accessory();
    smartstrap_state_unlock();
    return;
  }

  if (smartstrap_fsm_state_get() == SmartstrapStateUnsubscribed) {
    if (prv_acquire_accessory()) {
      // we will now start to attempt to connect to the smartstrap
      smartstrap_fsm_state_set(SmartstrapStateReadReady);
      s_monitor_interval = 0;
    } else {
      // try again in a little while to acquire the accessory
      s_monitor_interval = ACCESSORY_ACQUIRE_INTERVAL;
    }
  }

  bool can_send = false;
  if (smartstrap_fsm_state_get() == SmartstrapStateReadReady) {
    if (accessory_is_present() || smartstrap_is_connected()) {
      // If the accessory is present and we are connected then we can send data freely. If the
      // accessory is present but we're not connected, we'll try to connected. If we are connected
      // but the accessory is not present, we'll disconnect.
      can_send = true;
    } else {
      // back off a bit and check again for an accessory to be present
      s_monitor_interval = MIN(s_monitor_interval + DETECTION_BACKOFF, DETECTION_MAX_INTERVAL);
    }
  } else if (smartstrap_fsm_state_get() != SmartstrapStateUnsubscribed) {
    // There is a request in progress. We'll get kicked when it's completed.
    s_monitor_interval = KICK_TIMEOUT_INTERVAL;
  }

  smartstrap_state_unlock();

  if (can_send) {
    // We should attempt to send control messages first, followed by pending attributes.
    bool did_send = false;
    if (smartstrap_profiles_send_control()) {
      did_send = true;
    } else if (smartstrap_attribute_send_pending()) {
      did_send = true;
    }
    if (did_send && smartstrap_fsm_state_get() == SmartstrapStateReadReady) {
      if (accessory_bus_contention_detected()) {
        // There was bus contention during the send which caused it to fail. Set a short interval
        // before trying to send to allow the bus contention to clear.
        s_monitor_interval = BUS_CONTENTION_INTERVAL;
      } else {
        // We sent a write request, so are ready to send another request right away.
        s_monitor_interval = 0;
      }
    } else {
      // Either we are now waiting for a response, at which point we'll get kicked, or there was
      // nothing to send, in which case we'll get kicked when there is.
      s_monitor_interval = KICK_TIMEOUT_INTERVAL;
    }
  }

  if (s_monitor_interval) {
    // run the monitor again after the set interval
    new_timer_start(s_monitor_timer, s_monitor_interval, prv_monitor_timer_cb, NULL, 0);
  } else {
    // custom fast-path for 0ms timeout
    prv_monitor_timer_cb(NULL);
  }
}

static void prv_monitor_timer_cb(void *context) {
  // we need to run from KernelBG so schedule a callback
  system_task_add_callback(prv_monitor_system_task_cb, NULL);
}

void smartstrap_connection_kick_monitor(void) {
  // queue up the system task immediately
  prv_monitor_timer_cb(NULL);
}

void smartstrap_connection_got_valid_data(void) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  s_last_data_time = rtc_get_time();
}

time_t smartstrap_connection_get_time_since_valid_data(void) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  return rtc_get_time() - s_last_data_time;
}

#if !RELEASE
#include "console/prompt.h"

void command_smartstrap_status(void) {
  char buf[80];
  prompt_send_response_fmt(buf, sizeof(buf), "present=%d, connected=%d", accessory_is_present(),
                           smartstrap_is_connected());
}
#endif

// Subscription functions
////////////////////////////////////////////////////////////////////////////////

bool smartstrap_connection_has_subscriber(void) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  return s_subscriber_count > 0;
}

DEFINE_SYSCALL(void, sys_smartstrap_subscribe, void) {
  smartstrap_state_lock();
  s_subscriber_count++;
  if (s_subscriber_count == 1) {
    // kick the connection monitor
    smartstrap_connection_kick_monitor();
  }
  smartstrap_state_unlock();
}

DEFINE_SYSCALL(void, sys_smartstrap_unsubscribe, void) {
  smartstrap_state_lock();
  s_subscriber_count--;
  if (s_subscriber_count == 0) {
    // Disconnect directly from here rather than waiting for the monitor in order to ensure it
    // happens synchronously.
    prv_release_accessory();
  }
  PBL_ASSERTN(s_subscriber_count >= 0);
  smartstrap_state_unlock();
}
