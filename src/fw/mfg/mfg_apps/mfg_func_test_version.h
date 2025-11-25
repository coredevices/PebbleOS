/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/backlight.h"
#include "drivers/button.h"
#include "applib/fonts/fonts.h"
#include "git_version.auto.h"
#include "applib/graphics/utf8.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"
#include "system/version.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

typedef struct {
  MfgFuncTestData *app_data;
  TextLayer label;
} VersionData;

static int s_click_count;
static char* s_version_str;

static void version_window_button_up(ClickRecognizerRef recognizer, Window *window) {
  // The button-up event from launching the QC app will propagate to
  // this window, so instead we wait for two button-up events before
  // proceeding
  if (s_click_count++ > 0) {
    const bool animated = false;
    window_stack_pop(animated);
  }
}

static void version_window_click_config_provider(void *context) {
  for (ButtonId button_id = BUTTON_ID_BACK; button_id < NUM_BUTTONS; ++button_id) {
    window_raw_click_subscribe(button_id, NULL, (ClickHandler) version_window_button_up, context);
  }
}


static void version_get_version_str(char* buf, int size) {
  FirmwareMetadata normal_fw;
  FirmwareMetadata recovery_fw;

  version_copy_running_fw_metadata(&normal_fw);
  version_copy_recovery_fw_metadata(&recovery_fw);

  ResourceVersion system_res = resource_get_system_version();

  uint32_t bootloader_version = boot_version_read();
  uint32_t system_res_version = system_res.crc;

  char* normal_version = (char*) normal_fw.version_short;
  if (!utf8_is_valid_string(normal_version)) {
    normal_version = "???";
  }

  char* recovery_version = (char*) recovery_fw.version_short;
  if (!utf8_is_valid_string(recovery_version)) {
    recovery_version = "???";
  }

  sniprintf(buf, size,
            "n:%s\nr:%s\nb:0x%"PRIx32"\ns:0x%"PRIx32,
            normal_version,
            recovery_version,
            bootloader_version,
            system_res_version);
}

static void version_window_load(Window *window) {
  VersionData *data = window_get_user_data(window);
  Layer *root = &window->layer;

  s_version_str = (char*) malloc(64);
  version_get_version_str(s_version_str, 64);

  TextLayer *label = &data->label;
  text_layer_init(label, GRect(2, 2, 142, 164));
  text_layer_set_background_color(label, GColorClear);
  text_layer_set_text_color(label, GColorBlack);
  text_layer_set_text(label, s_version_str);
  text_layer_set_font(label, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(root, &label->layer);

  s_click_count = 0;
}

static void version_window_unload(Window* window) {
  free(s_version_str);
  (void)window;
}

static void push_version_window(MfgFuncTestData *app_data) {
  static VersionData s_version_data;
  s_version_data.app_data = app_data;

  Window* version_window = &app_data->version_window;
  window_init(version_window, WINDOW_NAME("Mfg Func Test Version"));
  window_set_overrides_back_button(version_window, true);
  window_set_click_config_provider_with_context(version_window,
      (ClickConfigProvider) version_window_click_config_provider, version_window);
  window_set_window_handlers(version_window, &(WindowHandlers) {
      .load = version_window_load,
      .unload = version_window_unload
  });
  window_set_user_data(version_window, &s_version_data);
  window_set_fullscreen(version_window, true);

  const bool animated = false;
  window_stack_push(version_window, animated);
}

