/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "boot_tests.h"

#include "board/board.h"
#include "drivers/button.h"
#include "drivers/flash.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/rtc_registers.h"
#include "util/misc.h"

#include "stm32f4xx.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

static const int STUCK_BUTTON_THRESHOLD = 5;

bool is_button_stuck(void) {
  // We store how many times each button has been pressed on previous boots in this
  // rtc backup register. Every time when we boot without that button pressed that
  // counter gets cleared. If the byte reaches 5, return a failure.

  uint32_t button_counter_register = RTC_ReadBackupRegister(STUCK_BUTTON_REGISTER);
  uint8_t* button_counter = (uint8_t*) (&button_counter_register);
  bool result = false;

  for (int button_id = 0; button_id < NUM_BUTTONS; button_id++) {
    if (!button_is_pressed(button_id)) {
      button_counter[button_id] = 0;
      continue;
    }

    if (button_counter[button_id] >= STUCK_BUTTON_THRESHOLD) {
      dbgserial_putstr("Stuck button register is invalid, clearing.");
      char buffer[32];
      itoa(button_counter_register, buffer, sizeof(buffer));
      dbgserial_putstr(buffer);

      RTC_WriteBackupRegister(STUCK_BUTTON_REGISTER, 0);
      return false;
    }

    button_counter[button_id] += 1;

    if (button_counter[button_id] >= STUCK_BUTTON_THRESHOLD) {
      PBL_LOG(LOG_LEVEL_ERROR, "Button id %u is stuck!", button_id);
      result = true;
    }
  }

  if (button_counter_register != 0) {
    dbgserial_putstr("Button is pushed at boot");
    char buffer[32];
    itoa(button_counter_register, buffer, sizeof(buffer));
    dbgserial_putstr(buffer);
  }

  RTC_WriteBackupRegister(STUCK_BUTTON_REGISTER, button_counter_register);
  return result;
}

bool is_flash_broken(void) {
  return !flash_sanity_check();
}
