/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "input_buttons.h"

#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>

#include <kernel/events.h>

static const uint16_t s_codes[NUM_BUTTONS] = {
  [BUTTON_ID_BACK] = PBL_INPUT_KEY_BACK,
  [BUTTON_ID_UP] = PBL_INPUT_KEY_UP,
  [BUTTON_ID_SELECT] = PBL_INPUT_KEY_SELECT,
  [BUTTON_ID_DOWN] = PBL_INPUT_KEY_DOWN,
};

uint16_t input_buttons_code(ButtonId id) {
  return s_codes[id];
}

static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  if (evt->type != PBL_INPUT_EV_KEY) {
    return;
  }

  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    if (s_codes[id] != evt->code) {
      continue;
    }

    PebbleEvent e = {
      .type = evt->value ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT,
      .button.button_id = id,
    };

    if (pbl_in_isr()) {
      event_put_isr(&e);
    } else {
      event_put(&e);
    }
    return;
  }
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, NULL);
