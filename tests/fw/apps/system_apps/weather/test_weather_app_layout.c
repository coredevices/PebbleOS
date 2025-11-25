/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "apps/system_apps/weather/weather_app_layout.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/content_indicator.h"
#include "applib/ui/content_indicator_private.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window_private.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/timeline_resources.h"
#include "shell/system_theme.h"
#include "util/buffer.h"
#include "util/graphics.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/size.h"

#include "clar.h"

#include <stdio.h>

// Fakes
/////////////////////

#include "fake_content_indicator.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_animation_timing.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_bootbits.h"
#include "stubs_click.h"
#include "stubs_i18n.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_process_info.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_status_bar_layer.h"
#include "stubs_syscalls.h"
#include "stubs_system_theme.h"
#include "stubs_task_watchdog.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

// Helper Functions
/////////////////////

#include "fw/graphics/test_graphics.h"
#include "fw/graphics/util.h"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer *fb = NULL;

KinoReel *kino_reel_morph_square_create(KinoReel *from_reel, bool take_ownership) {
  return from_reel;
}

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_weather_app_layout__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});

  const GContextInitializationMode context_init_mode = GContextInitializationMode_System;
  graphics_context_init(&s_ctx, fb, context_init_mode);

  framebuffer_clear(fb);

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false /* is_next */);

  resource_init();

  ContentIndicatorsBuffer *buffer = content_indicator_get_current_buffer();
  content_indicator_init_buffer(buffer);
}

void test_weather_app_layout__cleanup(void) {
  free(fb);
}

// Helpers
//////////////////////

static void prv_create_layout_for_forecast(const WeatherLocationForecast *forecast,
                                           WeatherAppLayout *layout, Window *window) {
  window_init(window, WINDOW_NAME("Weather"));
  weather_app_layout_init(layout, &s_ctx.dest_bitmap.bounds);
  weather_app_layout_set_data(layout, forecast);
  window_set_user_data(window, layout);

  Layer *window_root_layer = window_get_root_layer(window);
  layer_add_child(window_root_layer, &layout->root_layer);
  window_set_on_screen(window, true, true);
}

static void prv_create_layout_for_forecast_and_render(const WeatherLocationForecast *forecast) {
  Window window;
  WeatherAppLayout layout = {};
  prv_create_layout_for_forecast(forecast, &layout, &window);
  window_render(&window, &s_ctx);
}

static void prv_create_layout_for_forecast_and_render_with_down_arrow_indicator(
      const WeatherLocationForecast *forecast) {
  Window window;
  WeatherAppLayout layout = {};
  prv_create_layout_for_forecast(forecast, &layout, &window);
  weather_app_layout_set_down_arrow_visible(&layout, true);
  window_render(&window, &s_ctx);
}

// Tests
//////////////////////

void test_weather_app_layout__render_palo_alto(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = 68,
    .today_high = 68,
    .today_low = 58,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "Sunny",
    .tomorrow_high = 62,
    .tomorrow_low = 52,
    .tomorrow_weather_type = WeatherType_PartlyCloudy,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

static void prv_render_long_strings_test(bool is_current_location) {
  const WeatherLocationForecast forecast = {
      .location_name = "QWERTYUIO ASEDDFFGHHJ",
      .is_current_location = is_current_location,
      .current_temp = 68,
      .today_high = 68,
      .today_low = 58,
      .current_weather_type = WeatherType_PartlyCloudy,
      .current_weather_phrase = "Cloudy with 90% chance of meatballs",
      .tomorrow_high = 62,
      .tomorrow_low = 52,
      .tomorrow_weather_type = WeatherType_Sun,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
}

void test_weather_app_layout__render_longer_strings(void) {
  const bool is_current_location = false;
  prv_render_long_strings_test(is_current_location);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_longer_strings_for_current_location(void) {
  const bool is_current_location = true;
  prv_render_long_strings_test(is_current_location);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_large_numbers(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "Sunny",
    .tomorrow_high = -99,
    .tomorrow_low = -99,
    .tomorrow_weather_type = WeatherType_PartlyCloudy,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_cloudy_light_snow(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_CloudyDay,
    .current_weather_phrase = "Cloudy",
    .tomorrow_high = -99,
    .tomorrow_low = -99,
    .tomorrow_weather_type = WeatherType_LightSnow,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_light_rain_heavy_rain(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_LightRain,
    .current_weather_phrase = "Light Rain",
    .tomorrow_high = -99,
    .tomorrow_low = -99,
    .tomorrow_weather_type = WeatherType_HeavyRain,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_generic_generic(void) {
  const WeatherLocationForecast forecast = {
      .location_name = "HOUSTON",
      .current_temp = 110,
      .today_high = 120,
      .today_low = 85,
      .current_weather_type = WeatherType_Generic,
      .current_weather_phrase = "Humid AF",
      .tomorrow_high = 500,
      .tomorrow_low = 100,
      .tomorrow_weather_type = WeatherType_Generic,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_heavy_snow_rain_snow(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_HeavySnow,
    .current_weather_phrase = "Heavy Snow",
    .tomorrow_high = -99,
    .tomorrow_low = -99,
    .tomorrow_weather_type = WeatherType_RainAndSnow,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_down_arrow(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_HeavySnow,
    .current_weather_phrase = "Heavy Snow",
    .tomorrow_high = -99,
    .tomorrow_low = -99,
    .tomorrow_weather_type = WeatherType_RainAndSnow,
  };

  prv_create_layout_for_forecast_and_render_with_down_arrow_indicator(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_all_unknown_values(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_high = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_low = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .current_weather_type = WeatherType_Unknown,
    .current_weather_phrase = "",
    .tomorrow_high = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .tomorrow_low = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .tomorrow_weather_type = WeatherType_Unknown,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_some_unknown_values(void) {
  const WeatherLocationForecast forecast = {
    .location_name = "PALO ALTO",
    .current_temp = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_high = 99,
    .today_low = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "",
    .tomorrow_high = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .tomorrow_low = -99,
    .tomorrow_weather_type = WeatherType_Unknown,
  };

  prv_create_layout_for_forecast_and_render(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_current_location(void) {
  const WeatherLocationForecast forecast = {
      .location_name = "PHILADELPHIA",
      .is_current_location = true,
      .current_temp = 13,
      .today_high = 15,
      .today_low = -2,
      .current_weather_type = WeatherType_HeavySnow,
      .current_weather_phrase = "Heavy Snow",
      .tomorrow_high = 26,
      .tomorrow_low = 3,
      .tomorrow_weather_type = WeatherType_RainAndSnow,
  };

  prv_create_layout_for_forecast_and_render_with_down_arrow_indicator(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_long_current_location_name_pbl_38049(void) {
  const WeatherLocationForecast forecast = {
      .location_name = "DA'AN DISTRICT",
      .is_current_location = true,
      .current_temp = 30,
      .today_high = 33,
      .today_low = 26,
      .current_weather_type = WeatherType_CloudyDay,
      .current_weather_phrase = "M Cloudy",
      .tomorrow_high = 34,
      .tomorrow_low = 26,
      .tomorrow_weather_type = WeatherType_HeavyRain,
  };

  prv_create_layout_for_forecast_and_render_with_down_arrow_indicator(&forecast);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

// renders a blank image
void test_weather_app_layout__render_empty_view(void) {
  prv_create_layout_for_forecast_and_render(NULL);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
