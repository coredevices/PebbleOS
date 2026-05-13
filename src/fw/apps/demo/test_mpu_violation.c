/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_mpu_violation.h"

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/ui.h"
#include "font_resource_keys.auto.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"

// This demo app deliberately tries to write to memory that the MPU should
// deny the unprivileged App task (Worker RAM is mapped priv-only). If the
// MPU is enforcing access protection the App task will MemManage fault and
// be killed by the kernel; we will never see the "MPU FAILED" text on the
// display. If we *do* see it, the MPU is not blocking the access.
//
// __WORKER_RAM__ is exported by the linker script (src/fw/fw_common.ld).
extern const uint32_t __WORKER_RAM__[];

typedef struct {
  Window window;
  TextLayer text;
} AppData;

static void prv_attempt_violation(void *data) {
  AppData *app_data = data;

  volatile uint32_t *forbidden = (volatile uint32_t *)__WORKER_RAM__;
  *forbidden = 0xDEADBEEF;  // Expected to MemManage-fault and kill the app.

  // Reaching this point means the MPU did NOT block the unprivileged write
  // to Worker RAM. Surface the failure so the screenshot makes it obvious.
  text_layer_set_text(&app_data->text, "MPU FAILED");
  layer_mark_dirty(text_layer_get_layer(&app_data->text));
}

static void prv_window_load(Window *window) {
  AppData *app_data = window_get_user_data(window);

  GRect frame;
  layer_get_frame(window_get_root_layer(window), &frame);
  GRect text_frame = { .size.h = 64, .size.w = frame.size.w };
  grect_align(&text_frame, &frame, GAlignCenter, false);

  text_layer_init(&app_data->text, &text_frame);
  text_layer_set_font(&app_data->text, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(&app_data->text, GTextAlignmentCenter);
  text_layer_set_text(&app_data->text, "POKING\nKERNEL...");
  layer_add_child(window_get_root_layer(window), (Layer *)&app_data->text);

  // Let the window paint first, then attempt the forbidden write.
  app_timer_register(500, prv_attempt_violation, app_data);
}

static void prv_handle_init(void) {
  AppData *app_data = app_malloc_check(sizeof(AppData));
  app_state_set_user_data(app_data);

  window_init(&app_data->window, WINDOW_NAME("test_mpu_violation"));
  window_set_user_data(&app_data->window, app_data);
  window_set_window_handlers(&app_data->window, &(WindowHandlers) {
      .load = prv_window_load,
  });

  app_window_stack_push(&app_data->window, true /* animated */);
}

static void prv_main(void) {
  prv_handle_init();
  app_event_loop();
}

const PebbleProcessMd* test_mpu_violation_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common.main_func = prv_main,
    // System apps default to privileged; this one must run unprivileged
    // for the MPU to actually evaluate access against the App task regions.
    .common.is_unprivileged = true,
    .name = "Test MPU violation",
  };
  return (const PebbleProcessMd *)&s_info;
}
