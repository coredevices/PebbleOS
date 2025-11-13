/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mfg_test_aging_app.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "board/board.h"
#include "drivers/accel.h"
#include "drivers/ambient_light.h"
#include "drivers/audio.h"
#include "drivers/led_controller.h"
#include "drivers/mag.h"
#include "drivers/vibe.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "services/common/light.h"

#include <stdio.h>

#define STATUS_STRING_LEN 128
#define TEST_DURATION_SEC 10

static const int16_t sine_wave_4k[] = {
  0, 32767, 0, -32768, 0, 32767, 0, -32768,
  0, 32767, 0, -32768, 0, 32767, 0, -32768,
};

typedef enum {
  Duration_2Hours = 0,
  Duration_4Hours,
  Duration_Unlimited,
  NumDurations
} TestDuration;

typedef enum {
  TestState_Menu = 0,
  TestState_Accel,
  TestState_Mag,
  TestState_BacklightWhite,
  TestState_BacklightRed,
  TestState_BacklightGreen,
  TestState_BacklightBlue,
  TestState_Audio,
  TestState_ALS,
  TestState_Vibe,
  NumTestStates
} TestState;

typedef struct {
  Window window;

  TextLayer menu_text;
  TextLayer title;
  TextLayer status;
  char status_string[STATUS_STRING_LEN];

  TestState current_test;
  TestDuration duration;
  TestDuration selected_duration;

  uint32_t test_elapsed_sec;
  uint32_t total_elapsed_sec;
  uint32_t max_duration_sec;

  uint32_t cycle_count;

  bool running;
  bool menu_active;
  bool audio_playing;
  uint32_t saved_backlight_color;
} AppData;

static void prv_stop_test(void);
static void prv_start_tests(TestDuration duration);
static void prv_update_menu_display(AppData *data);

// Audio transmission handler
static void prv_audio_trans_handler(uint32_t *free_size) {
  uint32_t available_size = *free_size;
  while (available_size > sizeof(sine_wave_4k)) {
    available_size = audio_write(AUDIO, (void*)&sine_wave_4k[0], sizeof(sine_wave_4k));
  }
}

// Helper to format time as HH:MM:SS
static void prv_format_time(char *buffer, size_t size, uint32_t seconds) {
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;
  sniprintf(buffer, size, "%02"PRIu32":%02"PRIu32":%02"PRIu32, hours, minutes, secs);
}

// Test implementations
static void prv_test_accel(AppData *data) {
  AccelDriverSample sample;
  int ret = accel_peek(&sample);

  char time_str[16];
  prv_format_time(time_str, sizeof(time_str), data->total_elapsed_sec);

  if (ret == 0) {
    sniprintf(data->status_string, sizeof(data->status_string),
              "ACCEL TEST\nCycle: %"PRIu32"\nTime: %s\n\nX: %"PRIi16"\nY: %"PRIi16"\nZ: %"PRIi16,
              data->cycle_count, time_str, sample.x, sample.y, sample.z);
  } else {
    sniprintf(data->status_string, sizeof(data->status_string),
              "ACCEL TEST\nCycle: %"PRIu32"\nTime: %s\n\nERROR: %d",
              data->cycle_count, time_str, ret);
  }
  text_layer_set_text(&data->status, data->status_string);
}

static void prv_test_mag(AppData *data) {
  MagData mag_sample;
  MagReadStatus status = mag_read_data(&mag_sample);

  char time_str[16];
  prv_format_time(time_str, sizeof(time_str), data->total_elapsed_sec);

  if (status == MagReadSuccess) {
    sniprintf(data->status_string, sizeof(data->status_string),
              "MAG TEST\nCycle: %"PRIu32"\nTime: %s\n\nX: %"PRIi16"\nY: %"PRIi16"\nZ: %"PRIi16,
              data->cycle_count, time_str, mag_sample.x, mag_sample.y, mag_sample.z);
  } else {
    sniprintf(data->status_string, sizeof(data->status_string),
              "MAG TEST\nCycle: %"PRIu32"\nTime: %s\n\nERROR: %d",
              data->cycle_count, time_str, status);
  }
  text_layer_set_text(&data->status, data->status_string);
}

static void prv_test_backlight(AppData *data, const char *color_name, uint32_t color) {
  char time_str[16];
  prv_format_time(time_str, sizeof(time_str), data->total_elapsed_sec);

  led_controller_rgb_set_color(color);

  sniprintf(data->status_string, sizeof(data->status_string),
            "BACKLIGHT TEST\nCycle: %"PRIu32"\nTime: %s\n\nColor: %s",
            data->cycle_count, time_str, color_name);
  text_layer_set_text(&data->status, data->status_string);
}

static void prv_test_audio(AppData *data) {
  char time_str[16];
  prv_format_time(time_str, sizeof(time_str), data->total_elapsed_sec);

  sniprintf(data->status_string, sizeof(data->status_string),
            "AUDIO TEST\nCycle: %"PRIu32"\nTime: %s\n\nPlaying...",
            data->cycle_count, time_str);
  text_layer_set_text(&data->status, data->status_string);

  // Start audio playback if not already playing
  if (!data->audio_playing) {
    audio_init(AUDIO);
    audio_start(AUDIO, prv_audio_trans_handler);
    audio_set_volume(AUDIO, 100);
    data->audio_playing = true;
  }
}

static void prv_test_als(AppData *data) {
  uint32_t level = ambient_light_get_light_level();

  char time_str[16];
  prv_format_time(time_str, sizeof(time_str), data->total_elapsed_sec);

  sniprintf(data->status_string, sizeof(data->status_string),
            "ALS TEST\nCycle: %"PRIu32"\nTime: %s\n\nLevel: %"PRIu32,
            data->cycle_count, time_str, level);
  text_layer_set_text(&data->status, data->status_string);
}

static void prv_test_vibe(AppData *data) {
  char time_str[16];
  prv_format_time(time_str, sizeof(time_str), data->total_elapsed_sec);

  sniprintf(data->status_string, sizeof(data->status_string),
            "VIBE TEST\nCycle: %"PRIu32"\nTime: %s\n\nVibrating...",
            data->cycle_count, time_str);
  text_layer_set_text(&data->status, data->status_string);

  vibes_long_pulse();
}

static void prv_advance_test(AppData *data) {
  data->test_elapsed_sec = 0;

  // Move to next test
  if (data->current_test == TestState_Menu) {
    data->current_test = TestState_Accel;
  } else {
    data->current_test++;
  }

  // If we've completed all tests, start a new cycle
  if (data->current_test >= NumTestStates) {
    data->current_test = TestState_Accel;
    data->cycle_count++;
  }

  // Check if we've reached max duration
  if (data->duration != Duration_Unlimited &&
      data->total_elapsed_sec >= data->max_duration_sec) {
    // Clean up current test before showing finished
    if (data->current_test == TestState_Mag) {
      mag_release();
    }
    if (data->current_test >= TestState_BacklightWhite &&
        data->current_test <= TestState_BacklightBlue) {
      led_controller_rgb_set_color(data->saved_backlight_color);
      light_enable(false);
    }
    if (data->audio_playing) {
      audio_stop(AUDIO);
      data->audio_playing = false;
    }

    data->running = false;

    tick_timer_service_unsubscribe();

    return;
  }

  // Start magnetometer if needed
  if (data->current_test == TestState_Mag) {
    mag_start_sampling();
  }

  // Enable backlight for backlight tests
  if (data->current_test >= TestState_BacklightWhite &&
      data->current_test <= TestState_BacklightBlue) {
    light_enable(true);
  }
}

static void prv_update_display(AppData *data) {
  if (!data->running) {
    // Display the finished message
    char time_str[16];
    prv_format_time(time_str, sizeof(time_str), data->total_elapsed_sec);
    sniprintf(data->status_string, sizeof(data->status_string),
              "FINISHED\n\nTotal Time: %s\nCycles: %"PRIu32,
              time_str, data->cycle_count);
    text_layer_set_text(&data->status, data->status_string);
    return;
  }

  switch (data->current_test) {
    case TestState_Menu:
      break;
    case TestState_Accel:
      prv_test_accel(data);
      break;
    case TestState_Mag:
      prv_test_mag(data);
      break;
    case TestState_BacklightWhite:
      prv_test_backlight(data, "WHITE", 0xD0D0D0);
      break;
    case TestState_BacklightRed:
      prv_test_backlight(data, "RED", 0xFF0000);
      break;
    case TestState_BacklightGreen:
      prv_test_backlight(data, "GREEN", 0x00FF00);
      break;
    case TestState_BacklightBlue:
      prv_test_backlight(data, "BLUE", 0x0000FF);
      break;
    case TestState_Audio:
      prv_test_audio(data);
      break;
    case TestState_ALS:
      prv_test_als(data);
      break;
    case TestState_Vibe:
      prv_test_vibe(data);
      break;
    default:
      break;
  }
}

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  if (!data->running) {
    return;
  }

  data->test_elapsed_sec++;
  data->total_elapsed_sec++;

  // Update display
  prv_update_display(data);

  // Check if current test duration has elapsed
  if (data->test_elapsed_sec >= TEST_DURATION_SEC) {
    // Clean up current test
    if (data->current_test == TestState_Mag) {
      mag_release();
    }
    if (data->current_test >= TestState_BacklightWhite &&
        data->current_test <= TestState_BacklightBlue) {
      led_controller_rgb_set_color(data->saved_backlight_color);
      light_enable(false);
    }
    if (data->current_test == TestState_Audio && data->audio_playing) {
      audio_stop(AUDIO);
      data->audio_playing = false;
    }

    prv_advance_test(data);
    prv_update_display(data);
  }
}

static void prv_stop_test(void) {
  AppData *data = app_state_get_user_data();

  data->running = false;

  // Clean up any active tests
  if (data->current_test == TestState_Mag) {
    mag_release();
  }
  if (data->current_test >= TestState_BacklightWhite &&
      data->current_test <= TestState_BacklightBlue) {
    led_controller_rgb_set_color(data->saved_backlight_color);
    light_enable(false);
  }
  if (data->audio_playing) {
    audio_stop(AUDIO);
    data->audio_playing = false;
  }

  tick_timer_service_unsubscribe();
}

static void prv_update_menu_display(AppData *data) {
  const char *duration_text;
  switch (data->selected_duration) {
    case Duration_2Hours:
      duration_text = "2 Hours";
      break;
    case Duration_4Hours:
      duration_text = "4 Hours";
      break;
    case Duration_Unlimited:
      duration_text = "Unlimited";
      break;
    default:
      duration_text = "2 Hours";
      break;
  }

  sniprintf(data->status_string, sizeof(data->status_string),
            "Test Duration:\n\n%s\n\nUP/DOWN: Change\nSELECT: Start\nBACK: Exit",
            duration_text);
  text_layer_set_text(&data->menu_text, data->status_string);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  if (data->menu_active) {
    if (data->selected_duration > Duration_2Hours) {
      data->selected_duration--;
    } else {
      data->selected_duration = Duration_Unlimited;
    }
    prv_update_menu_display(data);
  }
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  if (data->menu_active) {
    if (data->selected_duration < Duration_Unlimited) {
      data->selected_duration++;
    } else {
      data->selected_duration = Duration_2Hours;
    }
    prv_update_menu_display(data);
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  if (data->menu_active) {
    prv_start_tests(data->selected_duration);
  }
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  if (data->running) {
    prv_stop_test();
  }

  app_window_stack_pop(true);
}

static void prv_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void prv_start_tests(TestDuration duration) {
  AppData *data = app_state_get_user_data();

  data->duration = duration;
  data->current_test = TestState_Menu;
  data->test_elapsed_sec = 0;
  data->total_elapsed_sec = 0;
  data->cycle_count = 1;
  data->running = true;
  data->saved_backlight_color = led_controller_rgb_get_color();

  switch (duration) {
    case Duration_2Hours:
      data->max_duration_sec = 2 * 3600;
      break;
    case Duration_4Hours:
      data->max_duration_sec = 4 * 3600;
      break;
    case Duration_Unlimited:
      data->max_duration_sec = UINT32_MAX;
      break;
    default:
      data->max_duration_sec = UINT32_MAX;
      break;
  }

  // Hide menu and mark as inactive
  layer_set_hidden(&data->menu_text.layer, true);
  data->menu_active = false;

  // Show test display
  layer_set_hidden(&data->title.layer, false);
  layer_set_hidden(&data->status.layer, false);

  // Start first test
  prv_advance_test(data);
  prv_update_display(data);

  // Subscribe to tick timer
  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .running = false,
    .menu_active = true,
    .audio_playing = false,
    .selected_duration = Duration_2Hours,
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  Layer *window_layer = &window->layer;
  GRect bounds = window_layer->bounds;

  // Setup menu text layer
  TextLayer *menu_text = &data->menu_text;
  text_layer_init(menu_text, &bounds);
  text_layer_set_font(menu_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(menu_text, GTextAlignmentCenter);
  layer_add_child(window_layer, &menu_text->layer);

  // Setup title and status layers (hidden initially)
  TextLayer *title = &data->title;
  text_layer_init(title, &GRect(0, 0, bounds.size.w, 24));
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "TEST AGING");
  layer_add_child(window_layer, &title->layer);
  layer_set_hidden(&title->layer, true);

  TextLayer *status = &data->status;
  text_layer_init(status, &GRect(5, 25, bounds.size.w - 10, bounds.size.h - 25));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(status, GTextAlignmentLeft);
  layer_add_child(window_layer, &status->layer);
  layer_set_hidden(&status->layer, true);

  prv_update_menu_display(data);

  app_window_stack_push(window, true);
}

static void prv_handle_deinit(void) {
  AppData *data = app_state_get_user_data();

  if (data->running) {
    tick_timer_service_unsubscribe();
  }
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  prv_handle_deinit();
}

const PebbleProcessMd* mfg_test_aging_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 12345678-ABCD-EF01-2345-6789ABCDEF01
    .common.uuid = { 0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD, 0xEF, 0x01,
                     0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01 },
    .name = "MfgTestAging",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
