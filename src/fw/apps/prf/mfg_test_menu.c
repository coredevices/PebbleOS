/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include "applib/app.h"
#include "applib/ui/ui.h"
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
#include "apps/prf/mfg_test_result.h"
#include "apps/prf/mfg_vibration.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "util/size.h"

typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;
  SimpleMenuSection menu_section;
} MfgTestMenuAppData;

static bool s_relaunch_menu = false;
static int16_t s_last_selected = -1;

//! Callback to run from the kernel main task
static void prv_launch_app_cb(void *data) {
  app_manager_launch_new_app(&(AppLaunchConfig) { .md = data });
}

static void prv_launch_test(int index, const PebbleProcessMd *md) {
  s_relaunch_menu = true;
  s_last_selected = index;
  launcher_task_add_callback(prv_launch_app_cb, (void*) md);
}

static void prv_select_button(int index, void *context) {
  prv_launch_test(index, mfg_button_app_get_info());
}

static void prv_select_display(int index, void *context) {
  prv_launch_test(index, mfg_display_app_get_info());
}

#if PLATFORM_OBELIX
static void prv_select_backlight(int index, void *context) {
  prv_launch_test(index, mfg_backlight_app_get_info());
}
#endif

static void prv_select_accel(int index, void *context) {
  prv_launch_test(index, mfg_accel_app_get_info());
}

#ifdef CONFIG_MAG
static void prv_select_mag(int index, void *context) {
  prv_launch_test(index, mfg_mag_app_get_info());
}
#endif

static void prv_select_vibration(int index, void *context) {
  prv_launch_test(index, mfg_vibration_app_get_info());
}

static void prv_select_als(int index, void *context) {
  prv_launch_test(index, mfg_als_app_get_info());
}

static void prv_select_speaker(int index, void *context) {
#if PLATFORM_ASTERIX
  prv_launch_test(index, mfg_speaker_asterix_app_get_info());
#elif PLATFORM_OBELIX
  prv_launch_test(index, mfg_speaker_obelix_app_get_info());
#endif
}

static void prv_select_mic(int index, void *context) {
#if PLATFORM_ASTERIX
  prv_launch_test(index, mfg_mic_asterix_app_get_info());
#elif PLATFORM_OBELIX
  prv_launch_test(index, mfg_mic_obelix_app_get_info());
#elif PLATFORM_GETAFIX
  prv_launch_test(index, mfg_mic_getafix_app_get_info());
#endif
}

#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
static void prv_select_hrm_ctr_leakage_obelix(int index, void *context) {
  prv_launch_test(index, mfg_hrm_ctr_leakage_obelix_app_get_info());
}
#endif

#ifdef CONFIG_TOUCH
static void prv_select_touch(int index, void *context) {
  prv_launch_test(index, mfg_touch_app_get_info());
}
#endif

static void prv_select_program_color(int index, void *context) {
  prv_launch_test(index, mfg_program_color_app_get_info());
}

static void prv_select_test_aging(int index, void *context) {
  prv_launch_test(index, mfg_test_aging_app_get_info());
}

static void prv_select_charge(int index, void *context) {
  prv_launch_test(index, mfg_charge_app_get_info());
}

static void prv_select_discharge(int index, void *context) {
  prv_launch_test(index, mfg_discharge_app_get_info());
}

static const char * prv_get_status_prefix(MfgTestId test) {
  const MfgTestResult *result = mfg_test_result_get(test);
  if (!result || !result->ran) {
    return "[ ]";
  }
  return result->passed ? "[P]" : "[F]";
}

typedef struct {
  const char *title;
  SimpleMenuLayerSelectCallback callback;
  MfgTestId test_id;
} MfgTestMenuEntry;

static void prv_window_load(Window *window) {
  MfgTestMenuAppData *data = app_state_get_user_data();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  const MfgTestMenuEntry entries[] = {
    { "Buttons",       prv_select_button,      MfgTestId_Buttons },
    { "Display",       prv_select_display,      MfgTestId_Display },
#ifdef CONFIG_TOUCH
    { "Touch",         prv_select_touch,        MfgTestId_Touch },
#endif
#if PLATFORM_OBELIX
    { "Backlight",     prv_select_backlight,    MfgTestId_Backlight },
#endif
    { "Accelerometer", prv_select_accel,        MfgTestId_Accel },
#ifdef CONFIG_MAG
    { "Magnetometer",  prv_select_mag,          MfgTestId_Mag },
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX
    { "Speaker",       prv_select_speaker,      MfgTestId_Speaker },
#endif
#if PLATFORM_ASTERIX || PLATFORM_OBELIX || PLATFORM_GETAFIX
    { "Microphone",    prv_select_mic,          MfgTestId_Mic },
#endif
    { "ALS",           prv_select_als,          MfgTestId_ALS },
    { "Vibration",     prv_select_vibration,    MfgTestId_Vibration },
#if PLATFORM_OBELIX && defined(MANUFACTURING_FW)
    { "HRM CTR/L",     prv_select_hrm_ctr_leakage_obelix, MfgTestId_HrmCtrLeakage },
#endif
    { "Program Color", prv_select_program_color, MfgTestId_ProgramColor },
    { "Aging",         prv_select_test_aging,   MfgTestId_Aging },
    { "Charge",        prv_select_charge,       MfgTestId_Charge },
    { "Discharge",     prv_select_discharge,    MfgTestId_Discharge },
  };

  size_t num_items = ARRAY_LENGTH(entries);
  SimpleMenuItem *items = app_malloc(num_items * sizeof(SimpleMenuItem));

  for (size_t i = 0; i < num_items; i++) {
    const char *prefix = prv_get_status_prefix(entries[i].test_id);
    size_t len = snprintf(NULL, 0, "%zu. %s %s", i + 1, prefix, entries[i].title) + 1;
    char *title = app_malloc(len);
    snprintf(title, len, "%zu. %s %s", i + 1, prefix, entries[i].title);

    items[i] = (SimpleMenuItem) {
      .title = title,
      .callback = entries[i].callback,
    };
  }

  data->menu_section = (SimpleMenuSection) {
    .num_items = num_items,
    .items = items
  };

  data->menu_layer = simple_menu_layer_create(bounds, data->window, &data->menu_section, 1, NULL);

  // Select next entry after returning from a test, first entry otherwise
  int16_t next = s_last_selected + 1;
  if (next > 0 && (size_t)next < num_items) {
    simple_menu_layer_set_selected_index(data->menu_layer, next, false);
  }

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
