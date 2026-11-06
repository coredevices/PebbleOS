/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch_sdl_bottom.h"

#include <pbl/drivers/touch.h>
#include <pbl/drivers/touch/sdl.h>
#include <pbl/services/system_task.h>
#include <pbl/util/math.h>

#include <pbl_arch_posix.h>

struct prv_touch_event {
  bool pressed;
  int x;
  int y;
};

static bool s_enabled;
static bool s_work_pending;
static struct prv_touch_event s_last;

static void prv_work(void *arg) {
  s_work_pending = false;

  pbl_touch_report(SDL_TOUCH, s_last.pressed, s_last.x, s_last.y);
}

static void prv_touch_isr(void *arg) {
  const struct prv_touch_event *e = arg;

  s_last = (struct prv_touch_event){
    .pressed = e->pressed,
    .x = CLIP(e->x, 0, CONFIG_TOUCH_SDL_SIZE_X - 1),
    .y = CLIP(e->y, 0, CONFIG_TOUCH_SDL_SIZE_Y - 1),
  };

  if (!s_enabled || s_work_pending) {
    return;
  }

  s_work_pending = system_task_add_callback_from_isr(prv_work, nullptr);
}

void touch_sdl_changed(bool pressed, int x, int y) {
  struct prv_touch_event e = {.pressed = pressed, .x = x, .y = y};

  pbl_posix_irq_run(prv_touch_isr, &e);
}

static int prv_init(const struct pbl_touch *touch) {
  touch_sdl_bottom_init(CONFIG_TOUCH_SDL_SIZE_X, CONFIG_TOUCH_SDL_SIZE_Y);

  return 0;
}

static int prv_enable(const struct pbl_touch *touch) {
  s_enabled = true;

  return 0;
}

static int prv_disable(const struct pbl_touch *touch) {
  s_enabled = false;

  return 0;
}

static const struct pbl_touch_ops s_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};

PBL_TOUCH_STATE_DEFINE(pbl_touch_sdl);
const struct pbl_touch pbl_touch_sdl =
    PBL_TOUCH_INIT(pbl_touch_sdl, "touch", nullptr, &s_ops, .size_x = CONFIG_TOUCH_SDL_SIZE_X,
                   .size_y = CONFIG_TOUCH_SDL_SIZE_Y);
PBL_DEVICE_REGISTER(pbl_touch_sdl, &pbl_touch_sdl.dev);
