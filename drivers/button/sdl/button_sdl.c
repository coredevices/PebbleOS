/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "button_sdl_bottom.h"

#include <pbl/drivers/button.h>
#include <pbl/drivers/debounced_button.h>

#include <kernel/events.h>
#include <pbl_arch_posix.h>

static const ButtonId s_buttons[] = {
  [BUTTON_SDL_BACK] = BUTTON_ID_BACK,
  [BUTTON_SDL_UP] = BUTTON_ID_UP,
  [BUTTON_SDL_SELECT] = BUTTON_ID_SELECT,
  [BUTTON_SDL_DOWN] = BUTTON_ID_DOWN,
};

static bool s_initialized;
static uint8_t s_state;

struct prv_button_event {
  ButtonId id;
  bool pressed;
};

static void prv_button_isr(void *arg) {
  const struct prv_button_event *e = arg;
  if (e->pressed) {
    s_state |= 1u << e->id;
  } else {
    s_state &= ~(1u << e->id);
  }
  if (!s_initialized) {
    return;
  }
  PebbleEvent event = {
    .type = e->pressed ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT,
    .button.button_id = e->id,
  };
  event_put_isr(&event);
}

void button_sdl_changed(int button, bool pressed) {
  struct prv_button_event e = {.id = s_buttons[button], .pressed = pressed};
  pbl_posix_irq_run(prv_button_isr, &e);
}

void button_init(void) {
  s_initialized = true;
}

bool button_is_pressed(ButtonId id) {
  return id < NUM_BUTTONS && (s_state & (1u << id)) != 0;
}

uint8_t button_get_state_bits(void) {
  return s_state;
}

void button_set_rotated(bool rotated) {
}

void debounced_button_init(void) {
  button_init();
}
