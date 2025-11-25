/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum SerialConsoleState {
  SERIAL_CONSOLE_STATE_PROMPT,
  SERIAL_CONSOLE_STATE_LOGGING,
#ifdef UI_DEBUG
  SERIAL_CONSOLE_STATE_LAYER_NUDGING,
#endif
  SERIAL_CONSOLE_STATE_HCI_PASSTHROUGH,
  SERIAL_CONSOLE_STATE_ACCESSORY_PASSTHROUGH,
  SERIAL_CONSOLE_STATE_PROFILER,
  SERIAL_CONSOLE_STATE_PULSE,
  SERIAL_CONSOLE_NUM_STATES
} SerialConsoleState;

// This function cannot be called in a > systick priority IRQ
void serial_console_set_state(SerialConsoleState new_state);

SerialConsoleState serial_console_get_state(void);
