/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch_sdl_bottom.h"

#include <pbl/drivers/touch/touch_sensor.h>
#include <pbl/input/input.h>
#include <pbl/services/system_task.h>

#include <board/board.h>
#include <pbl_arch_posix.h>

static bool s_enabled;
static bool s_pressed;
static int16_t s_x;
static int16_t s_y;
static bool s_callback_scheduled;

static void prv_process_touch_update(void *unused) {
  s_callback_scheduled = false;
  pbl_input_report_key(PBL_INPUT_BTN_TOUCH, s_pressed, false);
  pbl_input_report_abs(PBL_INPUT_ABS_X, s_x, false);
  pbl_input_report_abs(PBL_INPUT_ABS_Y, s_y, true);
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
