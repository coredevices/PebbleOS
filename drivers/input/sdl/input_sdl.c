/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "input_sdl_bottom.h"

#include <pbl/device.h>
#include <pbl/input/input.h>

#include <pbl_arch_posix.h>

static const uint16_t s_codes[] = {
  [INPUT_SDL_KEY_BACK] = PBL_INPUT_KEY_BACK,
  [INPUT_SDL_KEY_UP] = PBL_INPUT_KEY_UP,
  [INPUT_SDL_KEY_SELECT] = PBL_INPUT_KEY_SELECT,
  [INPUT_SDL_KEY_DOWN] = PBL_INPUT_KEY_DOWN,
};

static bool s_ready;

struct prv_key_event {
  int key;
  bool pressed;
};

static void prv_key_isr(void *arg) {
  const struct prv_key_event *e = arg;

  if (s_ready) {
    pbl_input_report_key(s_codes[e->key], e->pressed, true);
  }
}

void input_sdl_key_changed(int key, bool pressed) {
  struct prv_key_event e = {.key = key, .pressed = pressed};
  pbl_posix_irq_run(prv_key_isr, &e);
}

static int prv_init(const struct pbl_device *dev) {
  s_ready = true;
  return 0;
}

PBL_DEVICE_STATE_DEFINE(input_sdl);
static const struct pbl_device input_sdl =
    PBL_DEVICE_INIT(input_sdl, "input_sdl", prv_init, nullptr, nullptr);
PBL_DEVICE_REGISTER(input_sdl, &input_sdl);
