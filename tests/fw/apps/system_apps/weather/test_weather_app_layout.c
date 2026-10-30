/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/services/timeline/timeline_resources.h>
#include <pbl/util/buffer.h>
#include <pbl/util/hash.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <applib/graphics/framebuffer.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/raw_image.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/content_indicator.h>
#include <applib/ui/content_indicator_private.h>
#include <applib/ui/dialogs/simple_dialog.h>
#include <applib/ui/text_layer.h>
#include <applib/ui/window_private.h>
#include <clar.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

// Fakes
/////////////////////

#include <fake_content_indicator.h>
#include <fake_pbl_std.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>
#include <fixtures/load_test_resources.h>

// Stubs
/////////////////////

// Helper Functions
/////////////////////

#include <fw/graphics/test_graphics.h>
#include <fw/graphics/util.h>

// Included LAST: this drags in pebble_compat.h, whose function-like macros
// (SDK->firmware name remaps) would otherwise rewrite the stub headers above.
// Undo the libc remaps the harness itself needs.
#include <apps/system/weather/weather_app_layout.h>
#undef malloc
#undef calloc
#undef free
#undef time
#undef localtime
#undef resource_size
#undef resource_get_handle
#undef resource_load

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer *fb = nullptr;

KinoReel *kino_reel_morph_square_create(KinoReel *from_reel, bool take_ownership) {
  return from_reel;
}

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

static UnitsDistance s_units_distance;
static UnitsWind s_units_wind;

UnitsDistance shell_prefs_get_units_distance(void) {
  return s_units_distance;
}

UnitsWind shell_prefs_get_units_wind(void) {
  return s_units_wind;
}

void test_weather_app_layout__initialize(void) {
  s_units_distance = UnitsDistance_Miles;
  s_units_wind = UnitsWind_Mph;

  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize){DISP_COLS, DISP_ROWS});

  const GContextInitializationMode context_init_mode = GContextInitializationMode_System;
  graphics_context_init(&s_ctx, fb, context_init_mode);
  // The layout measures text via app_graphics_text_layout_get_content_size,
  // which draws its context from app_state.
  s_app_state_get_graphics_context = &s_ctx;

  framebuffer_clear(fb);

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  pbl_fs_format("/");
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);

  resource_init();

  ContentIndicatorsBuffer *buffer = content_indicator_get_current_buffer();
  content_indicator_init_buffer(buffer);
}

void test_weather_app_layout__cleanup(void) {
  free(fb);
}

// Helpers
//////////////////////

// The data source mirrors the synced current_* fields into the current-hour
// fields when a record carries no hourly block — the layout expects both.
static void prv_mirror_current_hour(WeatherLocationForecast *forecast) {
  forecast->current_type_now = forecast->current_weather_type;
  forecast->current_temp_now = forecast->current_temp;
  forecast->current_phrase_now = forecast->current_weather_phrase;
}

static void prv_create_layout_and_render(WeatherLocationForecast *today,
                                         WeatherLocationForecast *next) {
  prv_mirror_current_hour(today);
  if (next) {
    prv_mirror_current_hour(next);
  }

  Window window;
  WeatherAppLayout layout = {};
  window_init(&window, WINDOW_NAME("Weather"));
  weather_app_layout_init(&layout, &s_ctx.dest_bitmap.bounds);
  weather_app_layout_set_data(&layout, today, next);
  window_set_user_data(&window, &layout);

  Layer *window_root_layer = window_get_root_layer(&window);
  layer_add_child(window_root_layer, layout.root_layer);
  window_set_on_screen(&window, true, true);
  window_render(&window, &s_ctx);
}

// Tests
//////////////////////

void test_weather_app_layout__render_palo_alto(void) {
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = 68,
    .today_high = 68,
    .today_low = 58,
    .today_uv = 5,
    .today_uv_now = 5,
    .today_precip_mm = 20,
    .today_wind_mph = 8,
    .today_wind_dir_deg = 270,
    .today_feels = 66,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "Sunny",
  };
  WeatherLocationForecast next = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = 62,
    .today_high = 62,
    .today_low = 52,
    .current_weather_type = WeatherType_PartlyCloudy,
    .current_weather_phrase = "Partly Cloudy",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_wind_meters_per_second(void) {
  s_units_distance = UnitsDistance_KM;
  s_units_wind = UnitsWind_MetersPerSecond;
  WeatherLocationForecast today = {
    .location_name = "BARCELONA",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = 21,
    .today_high = 24,
    .today_low = 17,
    .today_uv = 5,
    .today_uv_now = 5,
    .today_precip_mm = 20,
    .today_wind_mph = 22,
    .today_wind_dir_deg = 135,
    .today_feels = 21,
    .current_weather_type = WeatherType_PartlyCloudy,
    .current_weather_phrase = "Partly Cloudy",
  };
  WeatherLocationForecast next = {
    .location_name = "BARCELONA",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = 19,
    .today_high = 22,
    .today_low = 16,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "Sunny",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

static void prv_render_long_strings_test(bool is_current_location) {
  WeatherLocationForecast today = {
    .location_name = "QWERTYUIO ASEDDFFGHHJ",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .is_current_location = is_current_location,
    .current_temp = 68,
    .today_high = 68,
    .today_low = 58,
    .current_weather_type = WeatherType_PartlyCloudy,
    .current_weather_phrase = "Cloudy with 90% chance of meatballs",
  };
  WeatherLocationForecast next = {
    .location_name = "QWERTYUIO ASEDDFFGHHJ",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .is_current_location = is_current_location,
    .current_temp = 62,
    .today_high = 62,
    .today_low = 52,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "Sunny",
  };

  prv_create_layout_and_render(&today, &next);
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
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "Sunny",
  };
  WeatherLocationForecast next = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -99,
    .today_high = -99,
    .today_low = -99,
    .current_weather_type = WeatherType_PartlyCloudy,
    .current_weather_phrase = "Partly Cloudy",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_cloudy_light_snow(void) {
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_CloudyDay,
    .current_weather_phrase = "Cloudy",
  };
  WeatherLocationForecast next = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -99,
    .today_high = -99,
    .today_low = -99,
    .current_weather_type = WeatherType_LightSnow,
    .current_weather_phrase = "Light Snow",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_light_rain_heavy_rain(void) {
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_LightRain,
    .current_weather_phrase = "Light Rain",
  };
  WeatherLocationForecast next = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -99,
    .today_high = -99,
    .today_low = -99,
    .current_weather_type = WeatherType_HeavyRain,
    .current_weather_phrase = "Heavy Rain",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_generic_generic(void) {
  WeatherLocationForecast today = {
    .location_name = "HOUSTON",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = 110,
    .today_high = 120,
    .today_low = 85,
    .current_weather_type = WeatherType_Generic,
    .current_weather_phrase = "Humid AF",
  };
  WeatherLocationForecast next = {
    .location_name = "HOUSTON",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = 500,
    .today_high = 500,
    .today_low = 100,
    .current_weather_type = WeatherType_Generic,
    .current_weather_phrase = "Humid AF",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_heavy_snow_rain_snow(void) {
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -88,
    .today_high = -88,
    .today_low = -88,
    .current_weather_type = WeatherType_HeavySnow,
    .current_weather_phrase = "Heavy Snow",
  };
  WeatherLocationForecast next = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = -99,
    .today_high = -99,
    .today_low = -99,
    .current_weather_type = WeatherType_RainAndSnow,
    .current_weather_phrase = "Rain and Snow",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_no_next_forecast(void) {
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = 68,
    .today_high = 68,
    .today_low = 58,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "Sunny",
  };

  prv_create_layout_and_render(&today, nullptr);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_all_unknown_values(void) {
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_high = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_low = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_uv = -1,
    .today_uv_now = -1,
    .today_precip_mm = -1,
    .today_wind_mph = -1,
    .today_wind_dir_deg = -1,
    .today_feels = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .current_weather_type = WeatherType_Unknown,
    .current_weather_phrase = "",
  };
  WeatherLocationForecast next = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_high = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_low = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .current_weather_type = WeatherType_Unknown,
    .current_weather_phrase = "",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_some_unknown_values(void) {
  WeatherLocationForecast today = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_high = 99,
    .today_low = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_uv = -1,
    .today_precip_mm = -1,
    .today_wind_mph = -1,
    .current_weather_type = WeatherType_Sun,
    .current_weather_phrase = "",
  };
  WeatherLocationForecast next = {
    .location_name = "PALO ALTO",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .current_temp = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_high = WEATHER_SERVICE_LOCATION_FORECAST_UNKNOWN_TEMP,
    .today_low = -99,
    .current_weather_type = WeatherType_Unknown,
    .current_weather_phrase = "",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_current_location(void) {
  WeatherLocationForecast today = {
    .location_name = "PHILADELPHIA",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .is_current_location = true,
    .current_temp = 13,
    .today_high = 15,
    .today_low = -2,
    .current_weather_type = WeatherType_HeavySnow,
    .current_weather_phrase = "Heavy Snow",
  };
  WeatherLocationForecast next = {
    .location_name = "PHILADELPHIA",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .is_current_location = true,
    .current_temp = 26,
    .today_high = 26,
    .today_low = 3,
    .current_weather_type = WeatherType_RainAndSnow,
    .current_weather_phrase = "Rain and Snow",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_weather_app_layout__render_long_current_location_name_pbl_38049(void) {
  WeatherLocationForecast today = {
    .location_name = "DA'AN DISTRICT",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .is_current_location = true,
    .current_temp = 30,
    .today_high = 33,
    .today_low = 26,
    .current_weather_type = WeatherType_CloudyDay,
    .current_weather_phrase = "M Cloudy",
  };
  WeatherLocationForecast next = {
    .location_name = "DA'AN DISTRICT",
    .today_wmo = -1,
    .today_humidity = -1,
    .today_visibility_m = -1,
    .today_precip_sum_mm = -1,
    .is_current_location = true,
    .current_temp = 34,
    .today_high = 34,
    .today_low = 26,
    .current_weather_type = WeatherType_HeavyRain,
    .current_weather_phrase = "Heavy Rain",
  };

  prv_create_layout_and_render(&today, &next);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
