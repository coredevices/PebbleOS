/* SPDX-FileCopyrightText: 2024 Google LLC */
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
#include "drivers/accel.h"
#include "drivers/rtc.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "services/common/evented_timer.h"
#include "util/bitset.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define STATUS_STRING_LEN 200

#define RANGE_MIN -1050
#define RANGE_MAX -950
#define PREPARE_TIME_MS 3000
#define SAMPLE_TIME_MS 5000
#define SAMPLE_INTERVAL_MS 100

typedef enum {
  STATE_IDLE,
  STATE_PREPARE_FLAT,
  STATE_MEASURE_FLAT,
  STATE_RESULT_FLAT,
  STATE_PREPARE_RIGHT,
  STATE_MEASURE_RIGHT,
  STATE_RESULT_RIGHT,
  STATE_PREPARE_DOWN,
  STATE_MEASURE_DOWN,
  STATE_RESULT_DOWN,
} TestState;

static EventedTimerID s_timer;

typedef struct {
  Window window;

  TextLayer title;
  TextLayer status;
  char status_string[STATUS_STRING_LEN];

  TestState state;
  RtcTicks state_start_time;
  int32_t sum;
  int32_t avg;
  uint8_t cnt;
  bool pass;
} AppData;

static void prv_start_test(AppData *data);

static void prv_update_display(void *context) {
  AppData *data = context;

  AccelDriverSample sample;
  int ret = accel_peek(&sample);

  if (ret != 0) {
    sniprintf(data->status_string, sizeof(data->status_string),
              "ACCEL ERROR:\n%d", ret);
    text_layer_set_text(&data->status, data->status_string);
    return;
  }

  uint32_t elapsed = (uint32_t)(rtc_get_ticks() - data->state_start_time);

  switch (data->state) {
    case STATE_IDLE:
      sniprintf(data->status_string, sizeof(data->status_string),
                "X: %"PRIi16"\nY: %"PRIi16"\nZ: %"PRIi16"\n\nPress SEL to start test",
                sample.x, sample.y, sample.z);
      break;

    case STATE_PREPARE_FLAT:
      sniprintf(data->status_string, sizeof(data->status_string),
                "Place FLAT\n\nStarting in %"PRIu32" sec",
                (PREPARE_TIME_MS - elapsed) / 1000 + 1);
      if (elapsed >= PREPARE_TIME_MS) {
        data->state = STATE_MEASURE_FLAT;
        data->state_start_time = rtc_get_ticks();
        data->sum = 0;
        data->cnt = 0;
      }
      break;

    case STATE_MEASURE_FLAT:
      data->sum += sample.z;
      data->cnt++;
      sniprintf(data->status_string, sizeof(data->status_string),
                "Measuring FLAT\n\nZ: %"PRIi16"\n%"PRIu32" sec remaining",
                sample.z, (SAMPLE_TIME_MS - elapsed) / 1000 + 1);
      if (elapsed >= SAMPLE_TIME_MS) {
        data->avg = data->sum / (int32_t)data->cnt;
        data->pass = (data->avg >= RANGE_MIN && data->avg <= RANGE_MAX);
        data->state = STATE_RESULT_FLAT;
      }
      break;

    case STATE_RESULT_FLAT: {
      sniprintf(data->status_string, sizeof(data->status_string),
                "FLAT: %s\n\nZ avg: %"PRId32"\nExpected: %d to %d\n\nPress SEL",
                data->pass ? "PASS" : "FAIL", data->avg, RANGE_MIN, RANGE_MAX);
      break;
    }

    case STATE_PREPARE_RIGHT:
      sniprintf(data->status_string, sizeof(data->status_string),
                "Turn RIGHT\n\nStarting in %"PRIu32" sec",
                (PREPARE_TIME_MS - elapsed) / 1000 + 1);
      if (elapsed >= PREPARE_TIME_MS) {
        data->state = STATE_MEASURE_RIGHT;
        data->state_start_time = rtc_get_ticks();
        data->sum = 0;
        data->cnt = 0;
      }
      break;

    case STATE_MEASURE_RIGHT:
      data->sum += sample.x;
      data->cnt++;
      sniprintf(data->status_string, sizeof(data->status_string),
                "Measuring RIGHT\n\nX: %"PRIi16"\n%"PRIu32" sec remaining",
                sample.x, (SAMPLE_TIME_MS - elapsed) / 1000 + 1);
      if (elapsed >= SAMPLE_TIME_MS) {
        data->avg = data->sum / (int32_t)data->cnt;
        data->pass = (data->avg >= RANGE_MIN && data->avg <= RANGE_MAX);
        data->state = STATE_RESULT_RIGHT;
      }
      break;

    case STATE_RESULT_RIGHT: {
      sniprintf(data->status_string, sizeof(data->status_string),
                "RIGHT: %s\n\nX avg: %"PRId32"\nExpected: %d to %d\n\nPress SEL",
                data->pass ? "PASS" : "FAIL", data->avg, RANGE_MIN, RANGE_MAX);
      break;
    }

    case STATE_PREPARE_DOWN:
      sniprintf(data->status_string, sizeof(data->status_string),
                "Turn DOWNWARDS\n\nStarting in %"PRIu32" sec",
                (PREPARE_TIME_MS - elapsed) / 1000 + 1);
      if (elapsed >= PREPARE_TIME_MS) {
        data->state = STATE_MEASURE_DOWN;
        data->state_start_time = rtc_get_ticks();
        data->sum = 0;
        data->cnt = 0;
      }
      break;

    case STATE_MEASURE_DOWN:
      data->sum += sample.y;
      data->cnt++;
      sniprintf(data->status_string, sizeof(data->status_string),
                "Measuring DOWN\n\nY: %"PRIi16"\n%"PRIu32" sec remaining",
                sample.y, (SAMPLE_TIME_MS - elapsed) / 1000 + 1);
      if (elapsed >= SAMPLE_TIME_MS) {
        data->avg = data->sum / (int32_t)data->cnt;
        data->pass = (data->avg >= RANGE_MIN && data->avg <= RANGE_MAX);
        data->state = STATE_RESULT_DOWN;
      }
      break;

    case STATE_RESULT_DOWN: {
      sniprintf(data->status_string, sizeof(data->status_string),
                "DOWN: %s\n\nY avg: %"PRId32"\nExpected: %d to %d\n\nPress SEL",
                data->pass ? "PASS" : "FAIL", data->avg, RANGE_MIN, RANGE_MAX);
      break;
    }
  }

  text_layer_set_text(&data->status, data->status_string);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = context;

  switch (data->state) {
    case STATE_IDLE:
      prv_start_test(data);
      break;

    case STATE_RESULT_FLAT:
      data->state = STATE_PREPARE_RIGHT;
      data->state_start_time = rtc_get_ticks();
      break;

    case STATE_RESULT_RIGHT:
      data->state = STATE_PREPARE_DOWN;
      data->state_start_time = rtc_get_ticks();
      break;

    case STATE_RESULT_DOWN:
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

static void prv_start_test(AppData *data) {
  data->state = STATE_PREPARE_FLAT;
  data->state_start_time = rtc_get_ticks();
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {};

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "ACCEL TEST");
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

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  evented_timer_cancel(s_timer);
}

const PebbleProcessMd* mfg_accel_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: ED2E214A-D4B5-4360-B5EC-612B9E49FB95
    .common.uuid = { 0xED, 0x2E, 0x21, 0x4A, 0xD4, 0xB5, 0x43, 0x60,
                     0xB5, 0xEC, 0x61, 0x2B, 0x9E, 0x49, 0xFB, 0x95,
    },
    .name = "MfgAccel",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
