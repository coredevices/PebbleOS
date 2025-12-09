/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/graphics/bitblt.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "apps/prf_apps/mfg_accel_app.h"
#include "apps/prf_apps/mfg_als_app.h"
#include "apps/prf_apps/mfg_bt_device_name_app.h"
#include "apps/prf_apps/mfg_charge_app.h"
#include "apps/prf_apps/mfg_backlight_app.h"
#include "apps/prf_apps/mfg_button_app.h"
#include "apps/prf_apps/mfg_discharge_app.h"
#include "apps/prf_apps/mfg_display_app.h"
#include "apps/prf_apps/mfg_hrm_app.h"
#include "apps/prf_apps/mfg_hrm_ctr_leakage_obelix_app.h"
#include "apps/prf_apps/mfg_mic_asterix_app.h"
#include "apps/prf_apps/mfg_mic_obelix_app.h"
#include "apps/prf_apps/mfg_program_color_app.h"
#include "apps/prf_apps/mfg_info_qr_app.h"
#include "apps/prf_apps/mfg_speaker_asterix_app.h"
#include "apps/prf_apps/mfg_speaker_obelix_app.h"
#include "apps/prf_apps/mfg_test_aging_app.h"
#include "apps/prf_apps/mfg_touch_app.h"
#include "apps/prf_apps/mfg_vibration_app.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/standby.h"
#include "mfg/mfg_info.h"
#include "mfg/mfg_serials.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "services/common/bluetooth/local_id.h"
#include "services/common/bluetooth/pairability.h"
#include "system/bootbits.h"
#include "system/firmware_storage.h"
#include "system/reset.h"
#include "util/size.h"

#if PBL_ROUND
#include "apps/prf_apps/mfg_display_calibration_app.h"
#endif

#include <string.h>

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
} MfgMenuAppData;

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
} ExtrasMenuData;

static uint16_t s_menu_position = 0;
static uint16_t s_extras_menu_position = 0;
static bool s_show_extras_on_launch = false;

#if MFG_INFO_RECORDS_TEST_RESULTS
static GBitmap *s_menu_icons[2];
#define ICON_IDX_CHECK 0
#define ICON_IDX_X     1
#endif

//! Callback to run from the kernel main task
static void prv_launch_app_cb(void *data) {
  app_manager_launch_new_app(&(AppLaunchConfig) { .md = data });
}

//! Callback to launch app and return to extras menu
static void prv_launch_app_from_extras_cb(void *data) {
  s_show_extras_on_launch = true;
  app_manager_launch_new_app(&(AppLaunchConfig) { .md = data });
}

static void prv_select_bt_device_name(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_bt_device_name_app_get_info());
}

static void prv_select_info_qr(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_info_qr_app_get_info());
}

#if PBL_ROUND
static void prv_select_calibrate_display(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_display_calibration_app_get_info());
}
#endif

static void prv_select_accel(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_accel_app_get_info());
}

static void prv_select_button(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_button_app_get_info());
}

static void prv_select_display(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_display_app_get_info());
}

#if PLATFORM_OBELIX
static void prv_select_backlight(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_backlight_app_get_info());
}
#endif

static void prv_select_charge(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_charge_app_get_info());
}

static void prv_select_vibration(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_vibration_app_get_info());
}

static void prv_select_als(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_als_app_get_info());
}

static void prv_select_speaker(int index, void *context) {
#if PLATFORM_ASTERIX
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_speaker_asterix_app_get_info());
#elif PLATFORM_OBELIX
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_speaker_obelix_app_get_info());
#endif
}

static void prv_select_mic(int index, void *context) {
#if PLATFORM_ASTERIX
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_mic_asterix_app_get_info());
#elif PLATFORM_OBELIX
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_mic_obelix_app_get_info());
#endif
}

#if CAPABILITY_HAS_BUILTIN_HRM
static void prv_select_hrm(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_hrm_app_get_info());
}
#endif

#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
static void prv_select_hrm_ctr_leakage_obelix(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_hrm_ctr_leakage_obelix_app_get_info());
}
#endif

#if CAPABILITY_HAS_TOUCHSCREEN
static void prv_select_touch(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_touch_app_get_info());
}
#endif

static void prv_select_program_color(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_program_color_app_get_info());
}

#if PLATFORM_OBELIX
static void prv_select_test_aging(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_cb, (void*) mfg_test_aging_app_get_info());
}
#endif

static void prv_extras_select_accel(int index, void *context) {
  // Launch app and mark to return to extras menu when it exits
  launcher_task_add_callback(prv_launch_app_from_extras_cb, (void*) mfg_accel_app_get_info());
}

static void prv_extras_select_discharge(int index, void *context) {
  // Launch app and mark to return to extras menu when it exits
  launcher_task_add_callback(prv_launch_app_from_extras_cb, (void*) mfg_discharge_app_get_info());
}

#if PLATFORM_OBELIX
static void prv_extras_select_test_aging(int index, void *context) {
  // Launch app and mark to return to extras menu when it exits
  launcher_task_add_callback(prv_launch_app_from_extras_cb, (void*) mfg_test_aging_app_get_info());
}
#endif

static void prv_extras_window_load(Window *window) {
  ExtrasMenuData *data = window_get_user_data(window);

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  const SimpleMenuItem extras_menu_items[] = {
    { .title = "Test Accel",        .callback = prv_extras_select_accel },
#if CAPABILITY_HAS_BUILTIN_HRM
    { .title = "Test HRM",          .callback = prv_select_hrm },
#endif
    { .title = "Test Discharge", .callback = prv_extras_select_discharge },
#if PLATFORM_OBELIX
    { .title = "Test Aging",        .callback = prv_extras_select_test_aging },
#endif
  };

  SimpleMenuItem *menu_items = app_malloc(sizeof(extras_menu_items));
  memcpy(menu_items, extras_menu_items, sizeof(extras_menu_items));

  data->menu_section = (SimpleMenuSection) {
    .num_items = ARRAY_LENGTH(extras_menu_items),
    .items = menu_items
  };

  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));

  // Restore the previous selection position
  simple_menu_layer_set_selected_index(data->menu_layer, s_extras_menu_position, false);
}

static void prv_extras_window_unload(Window *window) {
  ExtrasMenuData *data = window_get_user_data(window);

  // Save the current selection position
  s_extras_menu_position = simple_menu_layer_get_selected_index(data->menu_layer);

  simple_menu_layer_destroy(data->menu_layer);
  app_free(data);
}

static void prv_select_extras(int index, void *context) {
  ExtrasMenuData *data = app_malloc_check(sizeof(ExtrasMenuData));
  *data = (ExtrasMenuData){};

  data->window = window_create();
  window_init(data->window, "Extras");
  window_set_user_data(data->window, data);
  window_set_window_handlers(data->window, &(WindowHandlers) {
    .load = prv_extras_window_load,
    .unload = prv_extras_window_unload,
  });
  window_set_fullscreen(data->window, true);

  app_window_stack_push(data->window, true);
}

static void prv_load_prf_confirmed(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *confirmation_dialog = (ConfirmationDialog *)context;
  confirmation_dialog_pop(confirmation_dialog);

  bool confirmed = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  if (confirmed) {
#if PLATFORM_OBELIX
    // On Obelix MFG, we invalidate all slots so it will boot into PRF next time
    firmware_storage_invalidate_firmware_slot(0);
    firmware_storage_invalidate_firmware_slot(1);
#else
    boot_bit_set(BOOT_BIT_FORCE_PRF);
#endif
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

static void prv_select_reset(int index, void *context) {
  system_reset();
}

static void prv_select_shutdown(int index, void *context) {
  enter_standby(RebootReasonCode_ShutdownMenuItem);
}

static GBitmap * prv_get_icon_for_test(MfgTest test) {
#if MFG_INFO_RECORDS_TEST_RESULTS
  const bool passed = mfg_info_get_test_result(test);
  if (passed) {
    return s_menu_icons[ICON_IDX_CHECK];
  }
  return s_menu_icons[ICON_IDX_X];
#else
  return NULL;
#endif
}

static void prv_load_icons(void) {
#if MFG_INFO_RECORDS_TEST_RESULTS
  // The icons in resources are black boxes with either a white checkmark or X.
  // In order to make them look correct in the way we are using them, we want to
  // invert the icons so that they are black icon on a white background.
  //
  // To do this, load each resource temporarily and then create two new bitmaps.
  // Then bitblt the original resource into the new bitmap using GCompOpAssignInverted.

  const uint32_t icon_id[] = { RESOURCE_ID_ACTION_BAR_ICON_CHECK, RESOURCE_ID_ACTION_BAR_ICON_X };

  for (unsigned i = 0; i < ARRAY_LENGTH(icon_id); ++i) {
    GBitmap tmp;
    gbitmap_init_with_resource(&tmp, icon_id[i]);

    GBitmap *icon = gbitmap_create_blank(tmp.bounds.size, tmp.info.format);
    bitblt_bitmap_into_bitmap(icon, &tmp, GPointZero, GCompOpAssignInverted, GColorBlack);

    s_menu_icons[i] = icon;
    gbitmap_deinit(&tmp);
  }
#endif
}

//! @param[out] out_items
static size_t prv_create_menu_items(SimpleMenuItem** out_menu_items) {
  prv_load_icons();

  // Define a const blueprint on the stack.
  const SimpleMenuItem s_menu_items[] = {
    { .title = "BT Device Name",    .callback = prv_select_bt_device_name },
    { .title = "Device Info",       .callback = prv_select_info_qr },
    { .icon = prv_get_icon_for_test(MfgTest_Buttons),
      .title = "Test Buttons",      .callback = prv_select_button },
#if PBL_ROUND
    { .title = "Calibrate Display", .callback = prv_select_calibrate_display },
#endif
      { .icon = prv_get_icon_for_test(MfgTest_Display),
      .title = "Test Display",      .callback = prv_select_display },
#if CAPABILITY_HAS_TOUCHSCREEN
    { .title = "Test Touch",        .callback = prv_select_touch },
#endif
#if PLATFORM_OBELIX
    { .title = "Test Backlight",    .callback = prv_select_backlight },
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX
    { .title = "Test Speaker",      .callback = prv_select_speaker },
    { .title = "Test Microphone",   .callback = prv_select_mic },
#endif
    { .icon = prv_get_icon_for_test(MfgTest_ALS),
      .title = "Test ALS",          .callback = prv_select_als },
    { .icon = prv_get_icon_for_test(MfgTest_Vibe),
      .title = "Test Vibration",    .callback = prv_select_vibration },
#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
    { .title = "Test HRM CTR/L",          .callback = prv_select_hrm_ctr_leakage_obelix },
#endif
    { .title = "Program Color",     .callback = prv_select_program_color },
    { .title = "Test Charge",       .callback = prv_select_charge },
    { .title = "Load PRF",          .callback = prv_select_load_prf },
    { .title = "Reset",             .callback = prv_select_reset },
    { .title = "Shutdown",          .callback = prv_select_shutdown },
    { .title = "Extras",            .callback = prv_select_extras },
  };

  // Copy it into the heap so we can modify it.
  *out_menu_items = app_malloc(sizeof(s_menu_items));
  memcpy(*out_menu_items, s_menu_items, sizeof(s_menu_items));

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

  // We've now populated out_menu_items with the correct data. Return the number of items by
  // looking at the original list of menu items.
  return ARRAY_LENGTH(s_menu_items);
}

static void prv_window_load(Window *window) {
  MfgMenuAppData *data = app_state_get_user_data();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;
#ifdef PLATFORM_SPALDING
  bounds.origin.x += 25;
  bounds.origin.y += 25;
  bounds.size.w -= 50;
  bounds.size.h -= 25;
#endif

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

  // If returning from an app launched from extras menu, open extras menu
  if (s_show_extras_on_launch) {
    s_show_extras_on_launch = false;
    prv_select_extras(0, NULL);
  }

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

