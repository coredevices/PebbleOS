/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weather_app_splash_screen.h"

#include "applib/app.h"
#include "applib/ui/kino/kino_layer.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_ids.auto.h"

typedef struct SplashScreenData {
  Window window;
  KinoLayer logo_layer;
  AppTimer *timer;
  uint32_t timeout_ms;
} SplashScreenData;

static void prv_splash_screen_finished_callback(void *cb_data) {
  SplashScreenData *data = cb_data;
  data->timer = NULL;
  const bool animated = false;
  app_window_stack_remove(&data->window, animated);
}

static void prv_window_unload(Window *window) {
  SplashScreenData *data = window_get_user_data(window);
  kino_layer_deinit(&data->logo_layer);
  // Execute conditional only if the user presses back while the splash screen is showing
  if (data->timer) {
    app_timer_cancel(data->timer);
    const bool animated = true;
    app_window_stack_pop_all(animated);
  }
  app_free(data);
}

static void prv_window_load(Window *window) {
  SplashScreenData *data = window_get_user_data(window);
  Layer *window_root_layer = &window->layer;
  KinoLayer *logo_layer = &data->logo_layer;
  kino_layer_init(logo_layer, &window_root_layer->bounds);
  kino_layer_set_reel_with_resource(logo_layer,
                                    RESOURCE_ID_WEATHER_CHANNEL_LOGO);
  layer_add_child(window_root_layer,
                  kino_layer_get_layer(logo_layer));

  data->timer = app_timer_register(data->timeout_ms,
                                   prv_splash_screen_finished_callback,
                                   data);
}

void weather_app_splash_screen_push(uint32_t timeout_ms) {
  SplashScreenData *data = app_zalloc_check(sizeof(SplashScreenData));
  data->timeout_ms = timeout_ms;

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Weather - Splash Screen"));

  const GColor background_color = PBL_IF_COLOR_ELSE(GColorBlue, GColorBlack);
  window_set_background_color(window, background_color);

  const WindowHandlers window_handlers = {
    .load = prv_window_load,
    .unload = prv_window_unload,
  };
  window_set_window_handlers(window, &window_handlers);
  window_set_user_data(window, data);

  const bool animated = false;
  app_window_stack_push(window, animated);
}
