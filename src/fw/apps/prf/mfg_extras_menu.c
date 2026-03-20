/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "apps/prf/mfg_hrm.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "system/bootbits.h"
#include "system/reset.h"
#include "util/size.h"

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
} MfgExtrasMenuAppData;

static bool s_relaunch_menu = false;

//! Callback to launch app and return to extras menu
static void prv_launch_app_and_return_cb(void *data) {
  s_relaunch_menu = true;
  app_manager_launch_new_app(&(AppLaunchConfig) { .md = data });
}

#ifdef CONFIG_HRM
static void prv_select_hrm(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_hrm_app_get_info());
}
#endif

#ifdef MANUFACTURING_FW
static void prv_load_prf_confirmed(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *confirmation_dialog = (ConfirmationDialog *)context;
  confirmation_dialog_pop(confirmation_dialog);

  bool confirmed = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  if (confirmed) {
    boot_bit_set(BOOT_BIT_FORCE_PRF);
    system_reset();
  }
}

static void prv_load_prf_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_load_prf_confirmed);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_load_prf_confirmed);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_load_prf_confirmed);
}

static void prv_select_load_prf(int index, void *context) {
  ConfirmationDialog *confirmation_dialog = confirmation_dialog_create("Load PRF");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation_dialog);

  dialog_set_text(dialog, "Load PRF?\n\nThis action cannot be undone!");
  dialog_set_background_color(dialog, GColorOrange);
  dialog_set_text_color(dialog, GColorWhite);

  confirmation_dialog_set_click_config_provider(confirmation_dialog, prv_load_prf_click_config);

  ActionBarLayer *action_bar = confirmation_dialog_get_action_bar(confirmation_dialog);
  action_bar_layer_set_context(action_bar, confirmation_dialog);

  app_confirmation_dialog_push(confirmation_dialog);
}
#endif

static void prv_window_load(Window *window) {
  MfgExtrasMenuAppData *data = app_state_get_user_data();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  const SimpleMenuItem menu_items[] = {
#ifdef CONFIG_HRM
    { .title = "Test HRM",          .callback = prv_select_hrm },
#endif
#ifdef MANUFACTURING_FW
    { .title = "Load PRF",          .callback = prv_select_load_prf },
#endif
  };

  SimpleMenuItem *items = app_malloc(sizeof(menu_items));
  memcpy(items, menu_items, sizeof(menu_items));

  data->menu_section = (SimpleMenuSection) {
    .num_items = ARRAY_LENGTH(menu_items),
    .items = items
  };

  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));
}

bool mfg_extras_menu_should_relaunch(void) {
  if (s_relaunch_menu) {
    s_relaunch_menu = false;
    return true;
  }
  return false;
}

static void s_main(void) {
  MfgExtrasMenuAppData *data = app_malloc_check(sizeof(MfgExtrasMenuAppData));
  *data = (MfgExtrasMenuAppData){};

  app_state_set_user_data(data);

  data->window = window_create();
  window_init(data->window, "Extras");
  window_set_window_handlers(data->window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  window_set_fullscreen(data->window, true);
  app_window_stack_push(data->window, true);

  app_event_loop();
}

const PebbleProcessMd* mfg_extras_menu_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 2c9f1a8e-4b7d-3e6f-9a1c-5d8e2b4f7a3e
    .common.uuid = { 0x2c, 0x9f, 0x1a, 0x8e, 0x4b, 0x7d, 0x3e, 0x6f,
                     0x9a, 0x1c, 0x5d, 0x8e, 0x2b, 0x4f, 0x7a, 0x3e },
    .name = "MfgExtrasMenu",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
