/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "serial_console.h"

#include "console/dbgserial_input.h"
#include "console/pulse_protocol_impl.h"
#include "console_internal.h"
#include "prompt.h"
#include "ui_nudge.h"

#include "console/pulse_internal.h"
#include "drivers/mic.h"
#include "drivers/watchdog.h"
#include "kernel/util/stop.h"
#include "os/tick.h"
#include "system/logging.h"
#include "system/passert.h"

#include <bluetooth/bt_test.h>

SerialConsoleState s_serial_console_state = SERIAL_CONSOLE_STATE_LOGGING;
static bool s_serial_console_initialized;

static bool s_prompt_enabled = false;

static void logging_handle_character(char c, bool* should_context_switch) {
#ifdef DISABLE_PROMPT
  return;
#endif
  // Remember, you're in an interrupt here!

  if (c == 0x3) { // CTRL-C
    if (!s_prompt_enabled) {
      PBL_LOG(LOG_LEVEL_DEBUG, "Ignoring prompt request, not yet ready!");
      return;
    }
    console_switch_to_prompt();
  }
}

void serial_console_init(void) {
  if (s_serial_console_initialized) {
    return;
  }

  dbgserial_register_character_callback(logging_handle_character);

  s_serial_console_state = SERIAL_CONSOLE_STATE_LOGGING;
  s_serial_console_initialized = true;
}

bool serial_console_is_prompt_enabled(void) {
  if (!s_serial_console_initialized) {
    return false;
  }

  return (s_serial_console_state == SERIAL_CONSOLE_STATE_PROMPT);
}

bool serial_console_is_logging_enabled(void) {
  if (!s_serial_console_initialized) {
    return true;
  }

  return s_serial_console_state == SERIAL_CONSOLE_STATE_LOGGING ||
         s_serial_console_state == SERIAL_CONSOLE_STATE_PULSE;
}

void serial_console_enable_prompt(void) {
  s_prompt_enabled = true;
}

void serial_console_write_log_message(const char* msg) {
  while (*msg) {
    dbgserial_putchar(*(msg++));
  }
}

void serial_console_set_state(SerialConsoleState new_state) {
  PBL_ASSERTN(s_serial_console_initialized);
  PBL_ASSERTN(new_state < SERIAL_CONSOLE_NUM_STATES);

  // This function is called from the USART3 IRQ, the new timer thread,
  // and the system task. It thus needs a critical section.
  portENTER_CRITICAL();

  if (new_state == s_serial_console_state) {
    portEXIT_CRITICAL();
    return;
  }

#if !PULSE_EVERYWHERE
  if (new_state == SERIAL_CONSOLE_STATE_LOGGING) {
    stop_mode_enable(InhibitorDbgSerial);
    dbgserial_enable_rx_exti();
  } else if (s_serial_console_state == SERIAL_CONSOLE_STATE_LOGGING) {
    stop_mode_disable(InhibitorDbgSerial);
  }
#endif

  s_serial_console_state = new_state;

  switch (s_serial_console_state) {
#if !DISABLE_PROMPT
    case SERIAL_CONSOLE_STATE_PROMPT:
      dbgserial_register_character_callback(prompt_handle_character);
      dbgserial_set_rx_dma_enabled(false);
      break;
#endif
    case SERIAL_CONSOLE_STATE_LOGGING:
      dbgserial_register_character_callback(logging_handle_character);
      dbgserial_set_rx_dma_enabled(false);
      break;
#ifdef UI_DEBUG
    case SERIAL_CONSOLE_STATE_LAYER_NUDGING:
      dbgserial_register_character_callback(layer_debug_nudging_handle_character);
      dbgserial_set_rx_dma_enabled(false);
      break;
#endif
    case SERIAL_CONSOLE_STATE_HCI_PASSTHROUGH:
      dbgserial_register_character_callback(bt_driver_test_handle_hci_passthrough_character);
      dbgserial_set_rx_dma_enabled(false);
      break;
    case SERIAL_CONSOLE_STATE_PULSE:
      dbgserial_register_character_callback(pulse_handle_character);
      dbgserial_set_rx_dma_enabled(true);
      break;
    default:
      WTF; // Don't know this state
  }

  portEXIT_CRITICAL();
}

SerialConsoleState serial_console_get_state(void) {
  SerialConsoleState state = __atomic_load_n(&s_serial_console_state, __ATOMIC_RELAXED);

  return state;
}

