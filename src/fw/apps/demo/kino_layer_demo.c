/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kino_layer_demo.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/kino/kino_layer.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "util/size.h"

typedef struct {
  Window window;
  KinoLayer kino_layer;
  int resource_index;
} KinoLayerDemoData;

uint32_t resources[] = {
  RESOURCE_ID_RESULT_SENT_LARGE,
  RESOURCE_ID_GENERIC_QUESTION_LARGE,
  RESOURCE_ID_VOICE_MICROPHONE_LARGE,
};

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  KinoLayerDemoData *data = context;
  kino_layer_pause(&data->kino_layer);
  int index = ++data->resource_index % ARRAY_LENGTH(resources);
  kino_layer_set_reel_with_resource(&data->kino_layer, resources[index]);
  kino_layer_play(&data->kino_layer);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_set_click_context(BUTTON_ID_SELECT, context);
}

static void prv_window_appear(Window *window) {
  KinoLayerDemoData *data = window_get_user_data(window);
  kino_layer_play(&data->kino_layer);
}

static void prv_window_load(Window *window) {
  KinoLayerDemoData *data = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  // init the kino layer
  kino_layer_init(&data->kino_layer, &root->bounds);
  layer_add_child(root, &data->kino_layer.layer);
  // create the first kino reel
  kino_layer_set_reel_with_resource(&data->kino_layer, resources[0]);
}

static void prv_init(void) {
  KinoLayerDemoData *data = app_malloc_check(sizeof(KinoLayerDemoData));
  memset(data, 0, sizeof(KinoLayerDemoData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Kino Layer"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
  });

  window_set_click_config_provider_with_context(window, prv_click_config_provider, data);
  app_window_stack_push(window, true /*animated*/);
}

static void prv_deinit(void) {
  KinoLayerDemoData *data = app_state_get_user_data();
  kino_layer_deinit(&data->kino_layer);
  app_free(data);
}

///////////////////////////
// app boilerplate
///////////////////////////

static void s_main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd *kino_layer_demo_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common = {
      .main_func = s_main,
      // UUID: 67a32d95-ef69-46d4-a0b9-854cc62f97fa
      .uuid = {0x12, 0xa3, 0x2d, 0x95, 0xef, 0x69, 0x46, 0xd4,
               0xa0, 0xb9, 0x85, 0x4c, 0xc6, 0x2f, 0x97, 0xfa},
    },
    .name = "KinoLayer Demo",
  };

  return (const PebbleProcessMd*) &s_app_info;
}
