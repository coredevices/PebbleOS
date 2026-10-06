/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/touch/touch_sensor.h>
#include <pbl_arch_posix.h>

#include "pbl/services/system_task.h"
#include "pbl/services/touch/touch.h"
#include "board/board.h"
#include "touch_sdl_bottom.h"

static bool s_enabled;
static bool s_pressed;
static int16_t s_x;
static int16_t s_y;
static bool s_callback_scheduled;

static void prv_process_touch_update(void *unused) {
  s_callback_scheduled = false;
  touch_handle_update(s_pressed ? TouchState_FingerDown : TouchState_FingerUp, s_x, s_y);
}

struct prv_touch_event {
  bool pressed;
  int16_t x;
  int16_t y;
};

static void prv_touch_isr(void *arg) {
  const struct prv_touch_event *e = arg;
  s_pressed = e->pressed;
  s_x = e->x;
  s_y = e->y;
  if (!s_enabled || s_callback_scheduled) {
    return;
  }
  if (system_task_add_callback_from_isr(prv_process_touch_update, NULL)) {
    s_callback_scheduled = true;
  }
}

void touch_sdl_changed(bool pressed, int x, int y) {
  struct prv_touch_event e = {.pressed = pressed, .x = (int16_t)x, .y = (int16_t)y};
  pbl_posix_irq_run(prv_touch_isr, &e);
}

void touch_sensor_init(void) {
  touch_sdl_bottom_init(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT);
  s_enabled = true;
}

void touch_sensor_set_enabled(bool enabled) {
  s_enabled = enabled;
}
