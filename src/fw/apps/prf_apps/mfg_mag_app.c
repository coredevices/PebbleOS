/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "util/trig.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/path_layer.h"
#include "applib/ui/text_layer.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "drivers/mag.h"
#include "drivers/rtc.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "services/common/evented_timer.h"
#include "util/bitset.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define STATUS_STRING_LEN 200

// Typical Earth's magnetic field is ~250-650 mG (25-65 µT)
#define MIN_FIELD_STRENGTH_MG 200    // Minimum Earth field magnitude (mG)
#define MAX_FIELD_STRENGTH_MG 700    // Maximum Earth field magnitude (mG)

#define CALIBRATION_TIME_MS 30000 // 30 seconds of calibration
#define SAMPLE_TIME_MS 10000      // 10 seconds of sampling
#define SAMPLE_INTERVAL_MS 100    // Sample every 100ms

typedef enum {
  STATE_IDLE,
  STATE_CALIBRATING,
  STATE_MEASURING,
  STATE_RESULT,
} TestState;

static EventedTimerID s_timer;

typedef struct {
  Window window;

  TextLayer title;
  TextLayer status;
  char status_string[STATUS_STRING_LEN];

  TestState state;
  RtcTicks state_start_time;

  // Calibration data
  bool calibrated;
  // Hard iron offsets (constant bias)
  int16_t offset_x;
  int16_t offset_y;
  int16_t offset_z;
  // Soft iron correction (axis scaling)
  float scale_x;
  float scale_y;
  float scale_z;
  // Calibration raw data
  int16_t min_x, max_x;
  int16_t min_y, max_y;
  int16_t min_z, max_z;

  // Sample accumulation
  int64_t sum_magnitude;
  uint32_t sample_count;

  // Statistics
  int32_t avg_magnitude;

  bool test_passed;
} AppData;

// Calculate magnitude of 3D vector: sqrt(x^2 + y^2 + z^2)
static int32_t prv_calculate_magnitude(int16_t x, int16_t y, int16_t z) {
  int32_t x_sq = (int32_t)x * x;
  int32_t y_sq = (int32_t)y * y;
  int32_t z_sq = (int32_t)z * z;

  // Using integer sqrt approximation
  int64_t sum = x_sq + y_sq + z_sq;

  // Newton-Raphson integer square root
  if (sum == 0) return 0;

  int64_t x0 = sum;
  int64_t x1 = (x0 + sum / x0) / 2;

  while (x1 < x0) {
    x0 = x1;
    x1 = (x0 + sum / x0) / 2;
  }

  return (int32_t)x0;
}

// Apply calibration corrections to raw sample
static void prv_apply_calibration(AppData *data, MagData *sample,
                                   int16_t *out_x, int16_t *out_y, int16_t *out_z) {
  if (data->calibrated) {
    // Apply hard iron correction (offset removal)
    *out_x = (int16_t)((sample->x - data->offset_x) * data->scale_x);
    *out_y = (int16_t)((sample->y - data->offset_y) * data->scale_y);
    *out_z = (int16_t)((sample->z - data->offset_z) * data->scale_z);
  } else {
    *out_x = sample->x;
    *out_y = sample->y;
    *out_z = sample->z;
  }
}

static void prv_update_display(void *context) {
  AppData *data = context;

  MagData sample;
  MagReadStatus ret = mag_read_data(&sample);

  if (ret != MagReadSuccess) {
    sniprintf(data->status_string, sizeof(data->status_string),
              "MAG ERROR:\n%d", ret);
    text_layer_set_text(&data->status, data->status_string);
    return;
  }

  uint32_t elapsed = (uint32_t)(rtc_get_ticks() - data->state_start_time);

  switch (data->state) {
    case STATE_IDLE: {
      int16_t display_x, display_y, display_z;
      prv_apply_calibration(data, &sample, &display_x, &display_y, &display_z);

      int32_t magnitude = prv_calculate_magnitude(display_x, display_y, display_z);
      sniprintf(data->status_string, sizeof(data->status_string),
                "X: %"PRIi16" mG\nY: %"PRIi16" mG\nZ: %"PRIi16" mG\nMag: %"PRId32" mG\n\n%s\n\nPress SEL",
                display_x, display_y, display_z, magnitude,
                data->calibrated ? "Calibrated" : "Uncalibrated");
      break;
    }

    case STATE_CALIBRATING: {
      // Track min/max for each axis
      if (data->sample_count == 0) {
        data->min_x = data->max_x = sample.x;
        data->min_y = data->max_y = sample.y;
        data->min_z = data->max_z = sample.z;
      } else {
        if (sample.x < data->min_x) data->min_x = sample.x;
        if (sample.x > data->max_x) data->max_x = sample.x;
        if (sample.y < data->min_y) data->min_y = sample.y;
        if (sample.y > data->max_y) data->max_y = sample.y;
        if (sample.z < data->min_z) data->min_z = sample.z;
        if (sample.z > data->max_z) data->max_z = sample.z;
      }
      data->sample_count++;

      sniprintf(data->status_string, sizeof(data->status_string),
                "Calibrating...\nRotate device\n\n%"PRIu32" sec remaining",
                (CALIBRATION_TIME_MS - elapsed) / 1000 + 1);

      if (elapsed >= CALIBRATION_TIME_MS) {
        // Calculate hard iron offsets as midpoint of min/max range
        data->offset_x = (data->min_x + data->max_x) / 2;
        data->offset_y = (data->min_y + data->max_y) / 2;
        data->offset_z = (data->min_z + data->max_z) / 2;

        // Calculate soft iron correction (axis scaling)
        // Scale each axis range to match the average range across all axes
        int16_t range_x = data->max_x - data->min_x;
        int16_t range_y = data->max_y - data->min_y;
        int16_t range_z = data->max_z - data->min_z;
        float avg_range = (range_x + range_y + range_z) / 3.0f;

        // Scale factors normalize each axis to the average range
        data->scale_x = avg_range / (range_x > 0 ? range_x : 1.0f);
        data->scale_y = avg_range / (range_y > 0 ? range_y : 1.0f);
        data->scale_z = avg_range / (range_z > 0 ? range_z : 1.0f);

        data->calibrated = true;

        // Automatically transition to measuring stage
        data->state = STATE_MEASURING;
        data->state_start_time = rtc_get_ticks();
        data->sample_count = 0;
        data->sum_magnitude = 0;
      }
      break;
    }

    case STATE_MEASURING: {
      int16_t corrected_x, corrected_y, corrected_z;
      prv_apply_calibration(data, &sample, &corrected_x, &corrected_y, &corrected_z);

      int32_t magnitude = prv_calculate_magnitude(corrected_x, corrected_y, corrected_z);

      // Accumulate samples
      data->sum_magnitude += magnitude;
      data->sample_count++;

      sniprintf(data->status_string, sizeof(data->status_string),
                "Measuring...\nRotate device\n\nMag: %"PRId32" mG\n%"PRIu32" sec remaining",
                magnitude, (SAMPLE_TIME_MS - elapsed) / 1000 + 1);

      if (elapsed >= SAMPLE_TIME_MS) {
        // Calculate average magnitude
        data->avg_magnitude = (int32_t)(data->sum_magnitude / data->sample_count);

        // Test passes if average magnitude is within expected Earth field range
        data->test_passed = (data->avg_magnitude >= MIN_FIELD_STRENGTH_MG &&
                            data->avg_magnitude <= MAX_FIELD_STRENGTH_MG);

        data->state = STATE_RESULT;
        data->state_start_time = rtc_get_ticks();
      }
      break;
    }

    case STATE_RESULT: {
      sniprintf(data->status_string, sizeof(data->status_string),
                "MAG: %s\n\nAvg: %"PRId32" mG\n\nPress SEL",
                data->test_passed ? "PASS" : "FAIL",
                data->avg_magnitude);
      break;
    }
  }

  text_layer_set_text(&data->status, data->status_string);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = context;

  switch (data->state) {
    case STATE_IDLE:
      // Start calibration
      data->state = STATE_CALIBRATING;
      data->state_start_time = rtc_get_ticks();
      data->sample_count = 0;
      break;

    case STATE_RESULT:
      data->state = STATE_IDLE;
      data->state_start_time = rtc_get_ticks();
      break;

    default:
      break;
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}


static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {};

  app_state_set_user_data(data);

  // Initialize magnetometer
  mag_start_sampling();
  mag_change_sample_rate(MagSampleRate20Hz);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "MAG TEST");
  layer_add_child(&window->layer, &title->layer);

  TextLayer *status = &data->status;
  text_layer_init(status,
                  &GRect(5, 40,
                         window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 40));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  layer_add_child(&window->layer, &status->layer);

  window_set_click_config_provider_with_context(window, prv_click_config_provider, data);

  data->state = STATE_IDLE;
  data->state_start_time = rtc_get_ticks();

  app_window_stack_push(window, true /* Animated */);

  s_timer = evented_timer_register(SAMPLE_INTERVAL_MS, true /* repeating */, prv_update_display, data);
}

static void prv_handle_deinit(void) {
  evented_timer_cancel(s_timer);
  mag_release();
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  prv_handle_deinit();
}

const PebbleProcessMd* mfg_mag_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 3F4C8A2E-1B6D-4F9E-A3C5-7D8E9F0A1B2C
    .common.uuid = { 0x3F, 0x4C, 0x8A, 0x2E, 0x1B, 0x6D, 0x4F, 0x9E,
                     0xA3, 0xC5, 0x7D, 0x8E, 0x9F, 0x0A, 0x1B, 0x2C,
    },
    .name = "MfgMag",
  };
  return (const PebbleProcessMd*) &s_app_info;
}