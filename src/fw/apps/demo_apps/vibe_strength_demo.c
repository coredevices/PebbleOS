/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/number_window.h"
#include "drivers/vibe.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"

#include <stdint.h>

static void selected_pwm_percentage(NumberWindow *nw, void *ctx) {
  static bool on = true;
  int8_t val = number_window_get_value(nw);
  vibe_set_strength(val);
  vibe_ctl(on);
  on = !on;
}

static void handle_init(void) {
  NumberWindow *vibe_num_window = number_window_create("Vibe Strength",
      (NumberWindowCallbacks) { .selected = selected_pwm_percentage },
      NULL);
  app_state_set_user_data(vibe_num_window);

  uint8_t scale_granularity = 5; // 5 percent at a time
  uint8_t curr_percent = VIBE_STRENGTH_MAX;

  number_window_set_value(vibe_num_window, curr_percent);
  number_window_set_max(vibe_num_window, VIBE_STRENGTH_MAX);
  number_window_set_min(vibe_num_window, VIBE_STRENGTH_MIN);
  number_window_set_step_size(vibe_num_window, scale_granularity);

  app_window_stack_push(number_window_get_window(vibe_num_window), true);
}

static void handle_deinit(void) {
  NumberWindow *data = app_state_get_user_data();
  number_window_destroy(data);
}

static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

const PebbleProcessMd* vibe_strength_demo_get_info() {
  static const PebbleProcessMdSystem s_vibe_strength_info = {
    .common.main_func = s_main,
    .name = "Vibe Strength"
  };
  return (const PebbleProcessMd*) &s_vibe_strength_info;
}
