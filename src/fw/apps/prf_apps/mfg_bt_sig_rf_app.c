/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "services/common/bluetooth/bt_compliance_tests.h"
#include "system/logging.h"

typedef struct {
  Window window;

  TextLayer title;

  //! How many times we've vibrated
  int vibe_count;
} AppData;

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .vibe_count = 0
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "BT_SIG_RF\nTEST");
  layer_add_child(&window->layer, &title->layer);

  app_window_stack_push(window, true /* Animated */);

  // Enter the bluetooth test mode
  if (!bt_test_bt_sig_rf_test_mode()) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to enter bt_sig_rf!");
  }
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  // Bring us out of test mode. Do this on the kernel main thread as this app is currently
  // closing and if we take too long we'll get force-killed.
  bt_ctl_reset_bluetooth();
}

const PebbleProcessMd* mfg_bt_sig_rf_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 278f66e0-11a1-4139-a5f4-fceb64efcf55
    .common.uuid = { 0x27, 0x8f, 0x66, 0xe0, 0x11, 0xa1, 0x41, 0x39,
                     0xa5, 0xf4, 0xfc, 0xeb, 0x64, 0xef, 0xcf, 0x55 },
    .name = "MfgBtSigRf",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

