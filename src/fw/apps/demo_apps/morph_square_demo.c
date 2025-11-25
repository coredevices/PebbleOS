/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "morph_square_demo.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/kino/kino_layer.h"
#include "applib/ui/kino/kino_reel/morph_square.h"
#include "applib/ui/kino/kino_reel/transform.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"

typedef struct {
  Window window;
  KinoLayer icon_layer;
  KinoReel *icon_reel;
} MorphSquareDemoData;

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  MorphSquareDemoData *data = context;
  kino_player_rewind(kino_layer_get_player(&data->icon_layer));
  kino_layer_play(&data->icon_layer);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_set_click_context(BUTTON_ID_UP, context);
  window_set_click_context(BUTTON_ID_SELECT, context);
  window_set_click_context(BUTTON_ID_DOWN, context);
}

static void prv_window_load(Window *window) {
  MorphSquareDemoData *data = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(window);

  kino_layer_init(&data->icon_layer, &window_layer->bounds);

  KinoReel *from_image = kino_reel_create_with_resource(RESOURCE_ID_NOTIFICATION_GENERIC_LARGE);
  KinoReel *to_image = kino_reel_create_with_resource(RESOURCE_ID_GENERIC_CONFIRMATION_LARGE);

  KinoReel *icon_reel = kino_reel_morph_square_create(from_image, true);
  kino_reel_transform_set_to_reel(icon_reel, to_image, true);
  kino_reel_transform_set_transform_duration(icon_reel, 10000);
  kino_layer_set_reel(&data->icon_layer, icon_reel, true);

  layer_add_child(window_layer, (Layer *)&data->icon_layer);
}

static void prv_window_appear(Window *window) {
  MorphSquareDemoData *data = window_get_user_data(window);
  kino_layer_play(&data->icon_layer);
}

static void prv_window_unload(Window *window) {
  MorphSquareDemoData *data = window_get_user_data(window);
  kino_layer_deinit(&data->icon_layer);
}

static void prv_init(void) {
  MorphSquareDemoData *data = app_zalloc_check(sizeof(MorphSquareDemoData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Morph Square Demo"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
    .unload = prv_window_unload,
  });

  window_set_click_config_provider_with_context(window, prv_click_config_provider, data);

  const bool animated = true;
  app_window_stack_push(window, animated);
}

static void prv_deinit(void) {
  MorphSquareDemoData *data = app_state_get_user_data();
  app_free(data);
}

///////////////////////////
// App boilerplate
///////////////////////////

static void s_main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd *morph_square_demo_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common = {
      .main_func = s_main,
      // UUID: 6447c83d-52b7-4579-8817-8c7ec5927cbe
      .uuid = {0x64, 0x47, 0xc8, 0x3d, 0x52, 0xb7, 0x45, 0x79,
               0x88, 0x17, 0x8c, 0x7e, 0xc5, 0x92, 0x7c, 0xbe},
    },
    .name = "Morph Square Demo",
  };

  return (const PebbleProcessMd*) &s_app_info;
}
