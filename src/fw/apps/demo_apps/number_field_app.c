/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "number_field_app.h"

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include "system/passert.h"

typedef struct {
  NumberWindow num;
} AppData;

static void selected(NumberWindow *nw, void *ctx) {
  PBL_LOG(LOG_LEVEL_DEBUG, "selected: %"PRId32, number_window_get_value(nw));

  const bool animated = true;
  app_window_stack_pop(animated);

  (void)ctx;
}

static void handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  number_window_init(&data->num, "Some Number",
                    (NumberWindowCallbacks) { .selected = selected },
                    data);

  number_window_set_min(&data->num, 10);
  number_window_set_max(&data->num, 100);
  number_window_set_step_size(&data->num, 5);

  const bool animated = true;
  app_window_stack_push(&data->num.window, animated);
}

static void handle_deinit(void) {
  AppData *data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* number_field_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "NumberField Demo"
  };
  return (const PebbleProcessMd*) &s_app_info;
}

