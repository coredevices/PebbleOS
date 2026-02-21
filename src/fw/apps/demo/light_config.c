/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/number_window.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "services/common/light.h"

#include <stdint.h>

static void selected_pwm_percentage(NumberWindow *nw, void *ctx) {
  uint8_t val = number_window_get_value(nw);
  backlight_set_intensity_percent(val);
}

static void handle_init(void) {
  NumberWindow *light_num_window = number_window_create("Light Config",
      (NumberWindowCallbacks) { .selected = selected_pwm_percentage },
      NULL);
  app_state_set_user_data(light_num_window);

  uint8_t scale_granularity = 5; // 5 percent at a time
  uint8_t curr_percent = scale_granularity * ((backlight_get_intensity_percent() +
      scale_granularity - 1) / scale_granularity);

  number_window_set_value(light_num_window, curr_percent);
  number_window_set_max(light_num_window, 100);
  number_window_set_min(light_num_window, 0);
  number_window_set_step_size(light_num_window, scale_granularity);

  app_window_stack_push(number_window_get_window(light_num_window), true);
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

const PebbleProcessMd* light_config_get_info() {
  static const PebbleProcessMdSystem s_accel_config_info = {
    .common.main_func = s_main,
    .name = "Light Config"
  };
  return (const PebbleProcessMd*) &s_accel_config_info;
}
