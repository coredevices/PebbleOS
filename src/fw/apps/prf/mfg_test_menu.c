/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include "applib/app.h"
#include "applib/graphics/bitblt.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "apps/prf/mfg_accel.h"
#ifdef CONFIG_MAG
#include "apps/prf/mfg_mag.h"
#endif
#include "apps/prf/mfg_als.h"
#include "apps/prf/mfg_backlight.h"
#include "apps/prf/mfg_button.h"
#include "apps/prf/mfg_charge.h"
#include "apps/prf/mfg_discharge.h"
#include "apps/prf/mfg_display.h"
#include "apps/prf/mfg_hrm_ctr_leakage_obelix.h"
#include "apps/prf/mfg_mic_asterix.h"
#include "apps/prf/mfg_mic_getafix.h"
#include "apps/prf/mfg_mic_obelix.h"
#include "apps/prf/mfg_program_color.h"
#include "apps/prf/mfg_speaker_asterix.h"
#include "apps/prf/mfg_speaker_obelix.h"
#include "apps/prf/mfg_test_aging.h"
#include "apps/prf/mfg_touch.h"
#include "apps/prf/mfg_vibration.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_info.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "util/size.h"

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
} MfgTestMenuAppData;

#if MFG_INFO_RECORDS_TEST_RESULTS
static GBitmap *s_menu_icons[2];
#define ICON_IDX_CHECK 0
#define ICON_IDX_X     1
#endif

static bool s_relaunch_menu = false;

//! Callback to launch app and return to tests menu
static void prv_launch_app_and_return_cb(void *data) {
  s_relaunch_menu = true;
  app_manager_launch_new_app(&(AppLaunchConfig) { .md = data });
}

static void prv_select_button(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_button_app_get_info());
}

static void prv_select_display(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_display_app_get_info());
}

#if PLATFORM_OBELIX
static void prv_select_backlight(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_backlight_app_get_info());
}
#endif

static void prv_select_accel(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_accel_app_get_info());
}

#ifdef CONFIG_MAG
static void prv_select_mag(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_mag_app_get_info());
}
#endif

static void prv_select_vibration(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_vibration_app_get_info());
}

static void prv_select_als(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_als_app_get_info());
}

static void prv_select_speaker(int index, void *context) {
#if PLATFORM_ASTERIX
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_speaker_asterix_app_get_info());
#elif PLATFORM_OBELIX
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_speaker_obelix_app_get_info());
#endif
}

static void prv_select_mic(int index, void *context) {
#if PLATFORM_ASTERIX
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_mic_asterix_app_get_info());
#elif PLATFORM_OBELIX
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_mic_obelix_app_get_info());
#elif PLATFORM_GETAFIX
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_mic_getafix_app_get_info());
#endif
}

#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
static void prv_select_hrm_ctr_leakage_obelix(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_hrm_ctr_leakage_obelix_app_get_info());
}
#endif

#ifdef CONFIG_TOUCH
static void prv_select_touch(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_touch_app_get_info());
}
#endif

static void prv_select_program_color(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_program_color_app_get_info());
}

static void prv_select_test_aging(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_test_aging_app_get_info());
}

static void prv_select_charge(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_charge_app_get_info());
}

static void prv_select_discharge(int index, void *context) {
  launcher_task_add_callback(prv_launch_app_and_return_cb, (void*) mfg_discharge_app_get_info());
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

static void prv_window_load(Window *window) {
  MfgTestMenuAppData *data = app_state_get_user_data();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  prv_load_icons();

  const SimpleMenuItem menu_items[] = {
    { .icon = prv_get_icon_for_test(MfgTest_Buttons),
      .title = "Test Buttons",      .callback = prv_select_button },
    { .icon = prv_get_icon_for_test(MfgTest_Display),
      .title = "Test Display",      .callback = prv_select_display },
#ifdef CONFIG_TOUCH
    { .title = "Test Touch",        .callback = prv_select_touch },
#endif
#if PLATFORM_OBELIX
    { .title = "Test Backlight",    .callback = prv_select_backlight },
#endif
    { .title = "Test Accelerometer", .callback = prv_select_accel },
#ifdef CONFIG_MAG
    { .title = "Test Magnetometer", .callback = prv_select_mag },
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX
    { .title = "Test Speaker",      .callback = prv_select_speaker },
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX || PLATFORM_GETAFIX
    { .title = "Test Microphone",   .callback = prv_select_mic },
#endif
    { .icon = prv_get_icon_for_test(MfgTest_ALS),
      .title = "Test ALS",          .callback = prv_select_als },
    { .icon = prv_get_icon_for_test(MfgTest_Vibe),
      .title = "Test Vibration",    .callback = prv_select_vibration },
#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
    { .title = "Test HRM CTR/L",    .callback = prv_select_hrm_ctr_leakage_obelix },
#endif
    { .title = "Program Color",     .callback = prv_select_program_color },
    { .title = "Test Aging",        .callback = prv_select_test_aging },
    { .title = "Test Charge",       .callback = prv_select_charge },
    { .title = "Test Discharge",    .callback = prv_select_discharge },
  };

  SimpleMenuItem *items = app_malloc(sizeof(menu_items));
  memcpy(items, menu_items, sizeof(menu_items));

  size_t num_items = ARRAY_LENGTH(menu_items);

  // Add index numbers to each menu entry
  for (size_t i = 0; i < num_items; i++) {
    const char *original_title = items[i].title;
    size_t new_title_len = snprintf(NULL, 0, "%zu. %s", i + 1, original_title) + 1;
    char *new_title = app_malloc(new_title_len);
    snprintf(new_title, new_title_len, "%zu. %s", i + 1, original_title);
    items[i].title = new_title;
  }

  data->menu_section = (SimpleMenuSection) {
    .num_items = num_items,
    .items = items
  };

  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));
}

bool mfg_test_menu_should_relaunch(void) {
  if (s_relaunch_menu) {
    s_relaunch_menu = false;
    return true;
  }
  return false;
}

static void s_main(void) {
  MfgTestMenuAppData *data = app_malloc_check(sizeof(MfgTestMenuAppData));
  *data = (MfgTestMenuAppData){};

  app_state_set_user_data(data);

  data->window = window_create();
  window_init(data->window, "Tests");
  window_set_window_handlers(data->window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  window_set_fullscreen(data->window, true);
  app_window_stack_push(data->window, true);

  app_event_loop();
}

const PebbleProcessMd* mfg_test_menu_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 8a3f6c1e-9b2d-4f5a-a7c3-1d4e6b8f9a2c
    .common.uuid = { 0x8a, 0x3f, 0x6c, 0x1e, 0x9b, 0x2d, 0x4f, 0x5a,
                     0xa7, 0xc3, 0x1d, 0x4e, 0x6b, 0x8f, 0x9a, 0x2c },
    .name = "MfgTestMenu",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
