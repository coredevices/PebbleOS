/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "accessory_manager.h"

#include "accessory_idle_mode.h"
#include "accessory_imaging.h"
#include "accessory_mfg_mode.h"

#include "drivers/accessory.h"

#include "system/logging.h"
#include "os/mutex.h"

static AccessoryInputState s_input_state = AccessoryInputStateIdle;
static PebbleMutex *s_state_mutex;

void accessory_manager_init(void) {
  s_state_mutex = mutex_create();
}

bool accessory_manager_handle_character_from_isr(char c) {
  // NOTE: THIS IS RUN WITHIN AN ISR
  switch (s_input_state) {
  case AccessoryInputStateMfg:
    return accessory_mfg_mode_handle_char(c);
  case AccessoryInputStateIdle:
    return accessory_idle_mode_handle_char(c);
  case AccessoryInputStateImaging:
    return accessory_imaging_handle_char(c);
  case AccessoryInputStateMic:
    // fallthrough
  default:
    break;
  }
  return false;
}

bool accessory_manager_handle_break_from_isr(void) {
  // NOTE: THIS IS RUN WITHIN AN ISR
  switch (s_input_state) {
  case AccessoryInputStateIdle:
  case AccessoryInputStateMic:
  case AccessoryInputStateMfg:
  case AccessoryInputStateImaging:
    // fallthrough
  default:
    break;
  }
  return false;
}

// Valid state transitions are:
//               +-----+
//               | IMG |
//               +-----+
//                  ^
//                  |
//                  v
//   +------+    +-----+    +-----+
//   | Idle |<-->| MFG |<-->| MIC |
//   +------+    +-----+    +-----+
static bool prv_is_valid_state_transition(AccessoryInputState new_state) {
  if (s_input_state == AccessoryInputStateIdle) {
    return new_state == AccessoryInputStateMfg;
  } else if (s_input_state == AccessoryInputStateMfg) {
    return (new_state == AccessoryInputStateIdle) ||
           (new_state == AccessoryInputStateImaging) ||
           (new_state == AccessoryInputStateMic);
  } else if (s_input_state == AccessoryInputStateImaging) {
    return new_state == AccessoryInputStateMfg;
  } else if (s_input_state == AccessoryInputStateMic) {
    return new_state == AccessoryInputStateMfg;
  }
  return false;
}

// The accessory state is used to differentiate between different consumers of the accessory port.
// Before a consumer uses the accessory port, it must set its state and return the state to idle
// once it has finished. No other consumer will be permitted to use the accessory port until the
// state is returned to idle.
bool accessory_manager_set_state(AccessoryInputState state) {
  mutex_lock(s_state_mutex);

  if (!prv_is_valid_state_transition(state)) {
    // the state is already set by somebody else
    mutex_unlock(s_state_mutex);
    return false;
  }

  s_input_state = state;
  switch (s_input_state) {
  case AccessoryInputStateMfg:
    accessory_enable_input();
    accessory_set_baudrate(AccessoryBaud115200);
    accessory_set_power(false);
    accessory_mfg_mode_start();
    break;
  case AccessoryInputStateIdle:
    // restore accessory to default state
    accessory_enable_input();
    accessory_set_baudrate(AccessoryBaud115200);
    accessory_set_power(false);
    break;
  case AccessoryInputStateImaging:
    accessory_enable_input();
    accessory_set_baudrate(AccessoryBaud921600);
    accessory_set_power(false);
    break;
  case AccessoryInputStateMic:
    // fallthrough
  default:
    break;
  }

  mutex_unlock(s_state_mutex);
  PBL_LOG(LOG_LEVEL_DEBUG, "Setting accessory state to %u", state);
  return true;
}
