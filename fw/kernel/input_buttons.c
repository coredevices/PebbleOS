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

static bool s_rotated;
// Indexed by the key's position in s_codes, whatever the rotation.
static uint8_t s_pressed;

static ButtonId prv_rotate(ButtonId id) {
  if (s_rotated && id == BUTTON_ID_UP) {
    return BUTTON_ID_DOWN;
  }
  if (s_rotated && id == BUTTON_ID_DOWN) {
    return BUTTON_ID_UP;
  }
  return id;
}

uint16_t input_buttons_code(ButtonId id) {
  return s_codes[prv_rotate(id)];
}

void input_buttons_set_rotated(bool rotated) {
  s_rotated = rotated;
}

uint8_t input_buttons_get_state(void) {
  uint8_t state = 0;

  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    if (s_pressed & (1U << id)) {
      state |= 1U << prv_rotate(id);
    }
  }

  return state;
}

static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  if (evt->type != PBL_INPUT_EV_KEY) {
    return;
  }

  for (ButtonId key = 0; key < NUM_BUTTONS; key++) {
    if (s_codes[key] != evt->code) {
      continue;
    }

    if (evt->value) {
      s_pressed |= 1U << key;
    } else {
      s_pressed &= ~(1U << key);
    }

    PebbleEvent e = {
      .type = evt->value ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT,
      .button.button_id = prv_rotate(key),
    };

    if (pbl_in_isr()) {
      event_put_isr(&e);
    } else {
      event_put(&e);
    }
    return;
  }
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);
