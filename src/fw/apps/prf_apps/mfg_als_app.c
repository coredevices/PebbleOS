/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_als_app.h"

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window_private.h"
#include "kernel/pbl_malloc.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "services/common/light.h"
#include "system/logging.h"

#include <stdint.h>
#include <stdio.h>

// ALS pass/fail range (adjust these values based on your test requirements)
#if PLATFORM_OBELIX
#define ALS_MIN_VALUE 100
#define ALS_MAX_VALUE 250
#elif PLATFORM_GETAFIX
// FIXME(GETAFIX): Adjust to narrower range once validated
#define ALS_MIN_VALUE 5000
#define ALS_MAX_VALUE 20000
#else
#define ALS_MIN_VALUE 0
#define ALS_MAX_VALUE 65535
#endif

// Test parameters
#define COUNTDOWN_SECONDS 5
#define SAMPLE_DURATION_SECONDS 5

typedef enum {
  ALSStateWaitForStart = 0,
  ALSStateCountdown,
  ALSStateSampling,
  ALSStatePass,
  ALSStateFail,
} ALSTestState;

#define AMBIENT_READING_STR_LEN 64
typedef struct {
  Window *window;
  TextLayer *status_text_layer;
  TextLayer *reading_text_layer;
  char status_text[AMBIENT_READING_STR_LEN];
  char ambient_reading[AMBIENT_READING_STR_LEN];

  ALSTestState test_state;
  uint32_t countdown_remaining;
  uint32_t samples_remaining;
  uint64_t als_sum;
  uint32_t als_sample_count;
  uint32_t als_average;
} AmbientLightAppData;

static void prv_update_display(AmbientLightAppData *data) {
  switch (data->test_state) {
    case ALSStateWaitForStart:
      snprintf(data->status_text, AMBIENT_READING_STR_LEN, "ALS Test\nPress CENTER\nto start");
      snprintf(data->ambient_reading, AMBIENT_READING_STR_LEN, " ");
      break;

    case ALSStateCountdown:
      snprintf(data->status_text, AMBIENT_READING_STR_LEN, "Place in\nlight box");
      snprintf(data->ambient_reading, AMBIENT_READING_STR_LEN, "Starting in: %"PRIu32"s",
               data->countdown_remaining);
      break;

    case ALSStateSampling: {
      uint32_t current_level = ambient_light_get_light_level();
      snprintf(data->status_text, AMBIENT_READING_STR_LEN, "Sampling...");
      snprintf(data->ambient_reading, AMBIENT_READING_STR_LEN,
               "Time: %"PRIu32"s\nCurrent: %"PRIu32"\nSamples: %"PRIu32,
               data->samples_remaining, current_level, data->als_sample_count);
      break;
    }

    case ALSStatePass:
      snprintf(data->status_text, AMBIENT_READING_STR_LEN, "PASS");
      snprintf(data->ambient_reading, AMBIENT_READING_STR_LEN,
               "Average: %"PRIu32"\nRange: %d-%d",
               data->als_average, ALS_MIN_VALUE, ALS_MAX_VALUE);
      break;

    case ALSStateFail:
      snprintf(data->status_text, AMBIENT_READING_STR_LEN, "FAIL");
      snprintf(data->ambient_reading, AMBIENT_READING_STR_LEN,
               "Average: %"PRIu32"\nRange: %d-%d",
               data->als_average, ALS_MIN_VALUE, ALS_MAX_VALUE);
      break;
  }

  text_layer_set_text(data->status_text_layer, data->status_text);
  text_layer_set_text(data->reading_text_layer, data->ambient_reading);
}

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AmbientLightAppData *data = app_state_get_user_data();

  switch (data->test_state) {
    case ALSStateCountdown:
      if (data->countdown_remaining > 0) {
        data->countdown_remaining--;
        if (data->countdown_remaining == 0) {
          // Start sampling
          data->test_state = ALSStateSampling;
          data->samples_remaining = SAMPLE_DURATION_SECONDS;
          data->als_sum = 0;
          data->als_sample_count = 0;
          PBL_LOG(LOG_LEVEL_INFO, "ALS sampling started");
        }
      }
      break;

    case ALSStateSampling: {
      // Take a sample
      uint32_t level = ambient_light_get_light_level();
      data->als_sum += level;
      data->als_sample_count++;
      PBL_LOG(LOG_LEVEL_INFO, "ALS sample %"PRIu32": %"PRIu32,
              data->als_sample_count, level);

      if (data->samples_remaining > 0) {
        data->samples_remaining--;
        if (data->samples_remaining == 0) {
          // Calculate average and determine pass/fail
          data->als_average = (uint32_t)(data->als_sum / data->als_sample_count);

          PBL_LOG(LOG_LEVEL_INFO, "ALS test complete - Average: %"PRIu32" (samples: %"PRIu32")",
                  data->als_average, data->als_sample_count);

          if (data->als_average >= ALS_MIN_VALUE && data->als_average <= ALS_MAX_VALUE) {
            data->test_state = ALSStatePass;
            PBL_LOG(LOG_LEVEL_INFO, "ALS test PASSED");
          } else {
            data->test_state = ALSStateFail;
            PBL_LOG(LOG_LEVEL_ERROR, "ALS test FAILED - Average %"PRIu32" outside range %d-%d",
                    data->als_average, ALS_MIN_VALUE, ALS_MAX_VALUE);
          }
        }
      }
      break;
    }

    case ALSStateWaitForStart:
    case ALSStatePass:
    case ALSStateFail:
    default:
      // No action needed
      break;
  }

  prv_update_display(data);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  AmbientLightAppData *data = app_state_get_user_data();

  if (data->test_state == ALSStateWaitForStart) {
    // Turn off backlight before starting test
    light_enable(false);

    // Start countdown
    data->test_state = ALSStateCountdown;
    data->countdown_remaining = COUNTDOWN_SECONDS;

    PBL_LOG(LOG_LEVEL_INFO, "ALS test started - countdown %d seconds", COUNTDOWN_SECONDS);
    prv_update_display(data);
  } else if (data->test_state == ALSStatePass || data->test_state == ALSStateFail) {
    // Exit app on second press
    app_window_stack_pop(true);
  }
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  app_window_stack_pop(true);
}

static void prv_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void prv_handle_init(void) {
  AmbientLightAppData *data = task_zalloc_check(sizeof(AmbientLightAppData));

  data->window = window_create();
  window_set_fullscreen(data->window, true);

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  // Status text layer (top)
  data->status_text_layer = text_layer_create(GRect(0, 30, bounds.size.w, 80));
  text_layer_set_font(data->status_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(data->status_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(data->status_text_layer));

  // Reading text layer (bottom)
  data->reading_text_layer = text_layer_create(GRect(0, 110, bounds.size.w, 80));
  text_layer_set_font(data->reading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(data->reading_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(data->reading_text_layer));

  // Initialize state
  data->test_state = ALSStateWaitForStart;
  data->countdown_remaining = 0;
  data->samples_remaining = 0;
  data->als_sum = 0;
  data->als_sample_count = 0;
  data->als_average = 0;

  prv_update_display(data);

  // Set up click handlers
  window_set_click_config_provider(data->window, prv_config_provider);

  app_state_set_user_data(data);
  app_window_stack_push(data->window, true);

  // Subscribe to second tick for timing
  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);

  PBL_LOG(LOG_LEVEL_INFO, "ALS test initialized - range: %d-%d", ALS_MIN_VALUE, ALS_MAX_VALUE);
}

static void prv_handle_deinit(void) {
  AmbientLightAppData *data = app_state_get_user_data();

  // Unsubscribe from tick timer
  tick_timer_service_unsubscribe();

  text_layer_destroy(data->status_text_layer);
  text_layer_destroy(data->reading_text_layer);
  window_destroy(data->window);
  task_free(data);
}

static void prv_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd* mfg_als_app_get_info(void) {
  static const PebbleProcessMdSystem s_ambient_light_info = {
    .common.main_func = prv_main,
    .name = "MfgALS"
  };
  return (const PebbleProcessMd*) &s_ambient_light_info;
}
