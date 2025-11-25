/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/accessory.h"
#include "services/normal/accessory/accessory_manager.h"
#include "services/normal/accessory/smartstrap_attribute.h"
#include "services/normal/accessory/smartstrap_comms.h"
#include "services/normal/accessory/smartstrap_connection.h"
#include "services/normal/accessory/smartstrap_profiles.h"
#include "services/normal/accessory/smartstrap_state.h"
#include "system/logging.h"
#include "os/mutex.h"

static AccessoryInputState s_input_state = AccessoryInputStateIdle;
static PebbleMutex *s_state_mutex;

void accessory_manager_init(void) {
  s_state_mutex = mutex_create();

  // initialize consumers of the accessory port
  smartstrap_attribute_init();
  smartstrap_comms_init();
  smartstrap_state_init();
  smartstrap_connection_init();
  smartstrap_profiles_init();
}

bool accessory_manager_handle_character_from_isr(char c) {
  // NOTE: THIS IS RUN WITHIN AN ISR
  switch (s_input_state) {
  case AccessoryInputStateSmartstrap:
    return smartstrap_handle_data_from_isr((uint8_t)c);
  case AccessoryInputStateIdle:
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
  case AccessoryInputStateSmartstrap:
    return smartstrap_handle_break_from_isr();
  case AccessoryInputStateIdle:
  case AccessoryInputStateMic:
    // fallthrough
  default:
    break;
  }
  return false;
}

// The accessory state is used to differentiate between different consumers of the accessory port.
// Before a consumer uses the accessory port, it must set its state and return the state to idle
// once it has finished. No other consumer will be permitted to use the accessory port until the
// state is returned to idle.
bool accessory_manager_set_state(AccessoryInputState state) {
  mutex_lock(s_state_mutex);

  // Setting the state is only allowed if we are currently in the Idle state or we are
  // moving to the Idle state.
  if (state != AccessoryInputStateIdle && s_input_state != AccessoryInputStateIdle) {
    // the state is already set by somebody else
    mutex_unlock(s_state_mutex);
    return false;
  }

  accessory_use_dma(false);
  s_input_state = state;
  switch (s_input_state) {
  case AccessoryInputStateIdle:
    // restore accessory to default state
    accessory_enable_input();
    accessory_set_baudrate(AccessoryBaud115200);
    accessory_set_power(false);
    break;
  case AccessoryInputStateSmartstrap:
  case AccessoryInputStateMic:
    // fallthrough
  default:
    break;
  }

  mutex_unlock(s_state_mutex);
  PBL_LOG(LOG_LEVEL_DEBUG, "Setting accessory state to %u", state);
  return true;
}
