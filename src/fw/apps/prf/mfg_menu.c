/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "applib/app.h"
#include "applib/battery_state_service.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "apps/prf/mfg_adv.h"
#include "apps/prf/mfg_test_aging.h"
#include "apps/prf/mfg_info_qr.h"
#include "apps/prf/mfg_test_menu.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/standby.h"
#include "mfg/mfg_info.h"
#include "mfg/mfg_serials.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/common/bluetooth/pairability.h"
#include "system/bootbits.h"
#include "system/reset.h"
#include "util/size.h"

#include <string.h>

// "XXXXXX - 100%"
#define DEVICE_INFO_SUBTITLE_SIZE (MFG_SERIAL_NUMBER_SIZE + 8)

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
  char device_info_subtitle[DEVICE_INFO_SUBTITLE_SIZE];
} MfgMenuAppData;

static uint16_t s_menu_position = 0;
static const PebbleProcessMd *s_last_test_menu = NULL;

//! Callback to run from the kernel main task
static void prv_launch_app_cb(void *data) {
  app_manager_launch_new_app(&(AppLaunchConfig) { .md = data });
}

static void prv_select_info_qr(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_info_qr_app_get_info());
}

static void prv_select_tests_sf(int index, void *context) {
  s_last_test_menu = mfg_test_menu_semi_finished_app_get_info();
  launcher_task_add_callback(prv_launch_app_cb, (void*) s_last_test_menu);
}

static void prv_select_tests_fi(int index, void *context) {
  s_last_test_menu = mfg_test_menu_finished_app_get_info();
  launcher_task_add_callback(prv_launch_app_cb, (void*) s_last_test_menu);
}

static void prv_select_aging(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_test_aging_app_get_info());
}

static void prv_select_ble_adv(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_adv_app_get_info());
}

static void prv_select_reset(int index, void *context) {
  system_reset();
}

static void prv_select_shutdown(int index, void *context) {
  enter_standby(RebootReasonCode_ShutdownMenuItem);
}

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

static void prv_update_device_info_subtitle(MfgMenuAppData *data, uint8_t charge_percent) {
  char serial[MFG_SERIAL_NUMBER_SIZE + 1];
  mfg_info_get_serialnumber(serial, sizeof(serial));
  sniprintf(data->device_info_subtitle, sizeof(data->device_info_subtitle),
            "%s - %"PRIu8"%%", serial, charge_percent);
}

static void prv_battery_state_handler(BatteryChargeState charge) {
  MfgMenuAppData *data = app_state_get_user_data();
  prv_update_device_info_subtitle(data, charge.charge_percent);
  layer_mark_dirty(simple_menu_layer_get_layer(data->menu_layer));
}

//! @param[out] out_items
static size_t prv_create_menu_items(SimpleMenuItem** out_menu_items) {

  // Define a const blueprint on the stack.
  const SimpleMenuItem s_menu_items[] = {
    { .title = "Device Info",       .callback = prv_select_info_qr },
    { .title = "Semi-finished Tests", .callback = prv_select_tests_sf },
    { .title = "Finished Tests",   .callback = prv_select_tests_fi },
    { .title = "Aging Test",        .callback = prv_select_aging },
    { .title = "BLE Advertising",   .callback = prv_select_ble_adv },
    { .title = "Shutdown",          .callback = prv_select_shutdown },
    { .title = "Reset",             .callback = prv_select_reset },
#ifdef MANUFACTURING_FW
    { .title = "Load PRF",          .callback = prv_select_load_prf },
#endif
  };

  // Copy it into the heap so we can modify it.
  *out_menu_items = app_malloc(sizeof(s_menu_items));
  memcpy(*out_menu_items, s_menu_items, sizeof(s_menu_items));

  size_t num_items = ARRAY_LENGTH(s_menu_items);

  return num_items;
}

static void prv_window_load(Window *window) {
  MfgMenuAppData *data = app_state_get_user_data();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  SimpleMenuItem* menu_items;
  size_t num_items = prv_create_menu_items(&menu_items);

  // Set initial subtitle with serial and battery %
  BatteryChargeState charge = battery_state_service_peek();
  prv_update_device_info_subtitle(data, charge.charge_percent);
  menu_items[0].subtitle = data->device_info_subtitle;

  data->menu_section = (SimpleMenuSection) {
    .num_items = num_items,
    .items = menu_items
  };

  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));

  // Set the menu layer back to it's previous highlight position
  simple_menu_layer_set_selected_index(data->menu_layer, s_menu_position, false);

  battery_state_service_subscribe(prv_battery_state_handler);
}

static void s_main(void) {
  // If returning from a submenu item, relaunch the appropriate submenu
  if (mfg_test_menu_should_relaunch() && s_last_test_menu) {
    launcher_task_add_callback(prv_launch_app_cb, (void*) s_last_test_menu);
  }

  bt_pairability_use();

  MfgMenuAppData *data = app_malloc_check(sizeof(MfgMenuAppData));
  *data = (MfgMenuAppData){};

  app_state_set_user_data(data);

  data->window = window_create();
  window_init(data->window, "");
  window_set_window_handlers(data->window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  window_set_overrides_back_button(data->window, true);
  window_set_fullscreen(data->window, true);
  app_window_stack_push(data->window, true /*animated*/);

  app_event_loop();

  bt_pairability_release();

  s_menu_position = simple_menu_layer_get_selected_index(data->menu_layer);
}

const PebbleProcessMd* mfg_menu_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: ddfdf403-664e-47dd-a620-b1a14ce2b59b
    .common.uuid = { 0xdd, 0xfd, 0xf4, 0x03, 0x66, 0x4e, 0x47, 0xdd,
                     0xa6, 0x20, 0xb1, 0xa1, 0x4c, 0xe2, 0xb5, 0x9b },
    .name = "MfgMenu",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

