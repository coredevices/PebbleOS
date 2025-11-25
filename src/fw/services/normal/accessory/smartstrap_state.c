/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "mcu/interrupts.h"
#include "services/normal/accessory/smartstrap_profiles.h"
#include "services/normal/accessory/smartstrap_state.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "os/mutex.h"
#include "system/logging.h"
#include "system/passert.h"

//! The current FSM state
static volatile SmartstrapState s_fsm_state = SmartstrapStateUnsubscribed;
//! Whether or not we're connected to a smartstrap
static bool s_is_connected = false;
//! The smartstrap state lock
static PebbleMutex *s_state_lock;
//! The maximum number of services we could have connected
static uint32_t s_max_services;
//! The services we are currently connected to
static uint16_t *s_connected_services;
//! The number of connected services
static uint32_t s_num_connected_services = 0;
static PebbleMutex *s_services_lock;


void smartstrap_state_init(void) {
  s_state_lock = mutex_create();
  s_services_lock = mutex_create();
  s_max_services = smartstrap_profiles_get_max_services();
  s_connected_services = kernel_zalloc_check(s_max_services * sizeof(uint16_t));
}

static void prv_assert_valid_fsm_transition(SmartstrapState prev_state, SmartstrapState new_state) {
  if (new_state == SmartstrapStateUnsubscribed) {
    // we can go to SmartstrapStateUnsubscribed from any state
    PBL_ASSERTN(!mcu_state_is_isr());
  } else if ((prev_state == SmartstrapStateUnsubscribed) &&
             (new_state == SmartstrapStateReadReady)) {
    PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  } else if ((prev_state == SmartstrapStateReadReady) &&
             (new_state == SmartstrapStateNotifyInProgress)) {
    PBL_ASSERTN(mcu_state_is_isr());
  } else if ((prev_state == SmartstrapStateReadReady) &&
             (new_state == SmartstrapStateReadDisabled)) {
    PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  } else if ((prev_state == SmartstrapStateNotifyInProgress) &&
             (new_state == SmartstrapStateReadComplete)) {
    PBL_ASSERTN(mcu_state_is_isr() || (pebble_task_get_current() == PebbleTask_NewTimers));
  } else if ((prev_state == SmartstrapStateReadDisabled) &&
             (new_state == SmartstrapStateReadInProgress)) {
    PBL_ASSERTN(mcu_state_is_isr() || (pebble_task_get_current() == PebbleTask_KernelBackground));
  } else if ((prev_state == SmartstrapStateReadDisabled) &&
             (new_state == SmartstrapStateReadReady)) {
    PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  } else if ((prev_state == SmartstrapStateReadInProgress) &&
             (new_state == SmartstrapStateReadComplete)) {
    PBL_ASSERTN(mcu_state_is_isr() || (pebble_task_get_current() == PebbleTask_NewTimers));
  } else if ((prev_state == SmartstrapStateReadComplete) &&
             (new_state == SmartstrapStateReadReady)) {
    PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  } else {
    // all other transitions are invalid
    WTF;
  }
}

bool smartstrap_fsm_state_test_and_set(SmartstrapState expected_state, SmartstrapState next_state) {
  const bool did_set = __atomic_compare_exchange_n(&s_fsm_state, &expected_state, next_state, false,
                                                   __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  if (did_set) {
    prv_assert_valid_fsm_transition(expected_state, next_state);
  }
  return did_set;
}

void smartstrap_fsm_state_set(SmartstrapState next_state) {
  prv_assert_valid_fsm_transition(s_fsm_state, next_state);
  s_fsm_state = next_state;
}

void smartstrap_fsm_state_reset(void) {
  // we should only force an update to the FSM state in a critical region
  PBL_ASSERTN(portIN_CRITICAL());
  s_fsm_state = SmartstrapStateReadReady;
}

SmartstrapState smartstrap_fsm_state_get(void) {
  return s_fsm_state;
}

//! NOTE: the caller must hold s_services_lock
static int prv_find_connected_service(uint16_t service_id) {
  mutex_assert_held_by_curr_task(s_services_lock, true);
  for (uint32_t i = 0; i < s_num_connected_services; i++) {
    if (s_connected_services[i] == service_id) {
      return i;
    }
  }
  return -1;
}

//! NOTE: the caller must hold s_services_lock
static bool prv_remove_connected_service(uint16_t service_id) {
  mutex_assert_held_by_curr_task(s_services_lock, true);
  int index = prv_find_connected_service(service_id);
  if (index == -1) {
    return false;
  }
  PBL_ASSERTN(s_num_connected_services > 0);
  // move the last entry into this slot to remove this entry from the array
  s_num_connected_services--;
  s_connected_services[index] = s_connected_services[s_num_connected_services];
  return true;
}

//! NOTE: the caller must hold s_services_lock
static void prv_set_service_connected(uint16_t service_id, bool connected) {
  mutex_assert_held_by_curr_task(s_services_lock, true);
  if (connected) {
    if (prv_find_connected_service(service_id) != -1) {
      // already connected
      return;
    }
    // insert the service_id
    PBL_ASSERTN(s_num_connected_services < s_max_services);
    s_connected_services[s_num_connected_services++] = service_id;
  } else if (!prv_remove_connected_service(service_id)) {
    // we weren't previously connected
    return;
  }

  PBL_LOG(LOG_LEVEL_INFO, "Connection state for service (0x%x) changed to %d", service_id,
          connected);
  PebbleEvent event = {
    .type = PEBBLE_SMARTSTRAP_EVENT,
    .smartstrap = {
      .type = SmartstrapConnectionEvent,
      .result = connected ? SmartstrapResultOk : SmartstrapResultServiceUnavailable,
      .service_id = service_id
    },
  };
  event_put(&event);
}

void smartstrap_connection_state_set_by_service(uint16_t service_id, bool connected) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  mutex_lock(s_services_lock);
  prv_set_service_connected(service_id, connected);
  mutex_unlock(s_services_lock);
}

void smartstrap_connection_state_set(bool connected) {
  if (connected == s_is_connected) {
    return;
  }
  // if we're disconnecting, disconnect the services first
  if (s_is_connected) {
    mutex_lock(s_services_lock);
    while (s_num_connected_services) {
      const uint16_t service_id = s_connected_services[s_num_connected_services - 1];
      prv_set_service_connected(service_id, false);
    }
    s_num_connected_services = 0;
    mutex_unlock(s_services_lock);
  }
  s_is_connected = connected;
  smartstrap_profiles_handle_connection_event(connected);
}

DEFINE_SYSCALL(bool, sys_smartstrap_is_service_connected, uint16_t service_id) {
  if (!smartstrap_is_connected()) {
    return false;
  }
  mutex_lock(s_services_lock);
  bool result = prv_find_connected_service(service_id) != -1;
  mutex_unlock(s_services_lock);
  return result;
}

bool smartstrap_is_connected(void) {
  return (s_fsm_state != SmartstrapStateUnsubscribed) && s_is_connected;
}

void smartstrap_state_lock(void) {
  mutex_lock(s_state_lock);
}

void smartstrap_state_unlock(void) {
  mutex_unlock(s_state_lock);
}

void smartstrap_state_assert_locked_by_current_task(void) {
  mutex_assert_held_by_curr_task(s_state_lock, true);
}
