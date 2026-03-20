/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "apps/prf/mfg_bt_device_name.h"
#include "apps/prf/mfg_extras_menu.h"
#include "apps/prf/mfg_info_qr.h"
#include "apps/prf/mfg_test_menu.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/standby.h"
#include "mfg/mfg_info.h"
#include "mfg/mfg_serials.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/common/bluetooth/local_id.h"
#include "pbl/services/common/bluetooth/pairability.h"
#include "system/reset.h"
#include "util/size.h"

#include <string.h>

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
} MfgMenuAppData;

static uint16_t s_menu_position = 0;

//! Callback to run from the kernel main task
static void prv_launch_app_cb(void *data) {
  app_manager_launch_new_app(&(AppLaunchConfig) { .md = data });
}

static void prv_select_bt_device_name(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_bt_device_name_app_get_info());
}

static void prv_select_info_qr(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_info_qr_app_get_info());
}

static void prv_select_tests(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_test_menu_app_get_info());
}

static void prv_select_extras(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_extras_menu_app_get_info());
}

static void prv_select_reset(int index, void *context) {
  system_reset();
}

static void prv_select_shutdown(int index, void *context) {
  enter_standby(RebootReasonCode_ShutdownMenuItem);
}

//! @param[out] out_items
static size_t prv_create_menu_items(SimpleMenuItem** out_menu_items) {

  // Define a const blueprint on the stack.
  const SimpleMenuItem s_menu_items[] = {
    { .title = "BT Device Name",    .callback = prv_select_bt_device_name },
    { .title = "Device Info",       .callback = prv_select_info_qr },
    { .title = "Tests",             .callback = prv_select_tests },
    { .title = "Reset",             .callback = prv_select_reset },
    { .title = "Shutdown",          .callback = prv_select_shutdown },
    { .title = "Extras",            .callback = prv_select_extras },
  };

  // Copy it into the heap so we can modify it.
  *out_menu_items = app_malloc(sizeof(s_menu_items));
  memcpy(*out_menu_items, s_menu_items, sizeof(s_menu_items));

  size_t num_items = ARRAY_LENGTH(s_menu_items);

  // Now we're going to modify the first two elements in the menu to include data available only
  // at runtime. If it was available at compile time we could have just shoved it in the
  // s_menu_items array but it's not. Note that we allocate a few buffers here that we never
  // bother freeing for simplicity. It's all on the app heap so it will automatically get cleaned
  // up on app exit.

  // Poke in the bluetooth name
  int buffer_size = BT_DEVICE_NAME_BUFFER_SIZE;
  char *bt_dev_name = app_malloc(buffer_size);
  bt_local_id_copy_device_name(bt_dev_name, false);

  (*out_menu_items)[0].subtitle = bt_dev_name;

  // Poke in the serial number
  buffer_size = MFG_SERIAL_NUMBER_SIZE + 1;
  char *device_serial = app_malloc(buffer_size);
  mfg_info_get_serialnumber(device_serial, buffer_size);

  (*out_menu_items)[1].subtitle = device_serial;

  return num_items;
}

static void prv_window_load(Window *window) {
  MfgMenuAppData *data = app_state_get_user_data();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  SimpleMenuItem* menu_items;
  size_t num_items = prv_create_menu_items(&menu_items);

  data->menu_section = (SimpleMenuSection) {
    .num_items = num_items,
    .items = menu_items
  };

  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));

  // Set the menu layer back to it's previous highlight position
  simple_menu_layer_set_selected_index(data->menu_layer, s_menu_position, false);
}

static void s_main(void) {
  // If returning from a submenu item, relaunch the appropriate submenu
  if (mfg_test_menu_should_relaunch()) {
    launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_test_menu_app_get_info());
  } else if (mfg_extras_menu_should_relaunch()) {
    launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_extras_menu_app_get_info());
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

