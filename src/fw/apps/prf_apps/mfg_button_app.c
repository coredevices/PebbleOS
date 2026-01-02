/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "board/display.h"
#include "util/trig.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/path_layer.h"
#include "applib/ui/text_layer.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_info.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "util/bitset.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// How long after test pass / fail to wait before popping the window
#define WINDOW_POP_TIME_S (3)

typedef struct {
  Window window;

  PathLayer arrows[NUM_BUTTONS];

  //! bitset of buttons pressed so far
  uint32_t buttons_pressed;

  TextLayer title;
  TextLayer status;
  char status_string[35];

  uint32_t seconds_remaining;
  bool test_complete;
} AppData;

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  if (data->test_complete) {
    if (--data->seconds_remaining == 0) {
      app_window_stack_pop(true);
    }
    return;
  }

  const bool test_passed = (data->buttons_pressed == 0x0f);
  if (data->seconds_remaining == 0 || test_passed) {
    data->test_complete = true;

#if MFG_INFO_RECORDS_TEST_RESULTS
    mfg_info_write_test_result(MfgTest_Buttons, test_passed);
#endif

    if (test_passed) {
      // pass
      sniprintf(data->status_string, sizeof(data->status_string), "PASS!");
    } else {
      // fail
      sniprintf(data->status_string, sizeof(data->status_string), "FAIL!");
    }
    data->seconds_remaining = WINDOW_POP_TIME_S;
  } else {
    sniprintf(data->status_string, sizeof(data->status_string),
              "TIME REMAINING: %"PRIu32"s", data->seconds_remaining);
    data->seconds_remaining--;
  }

  text_layer_set_text(&data->status, data->status_string);
}

static void prv_button_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  ButtonId button_id_pressed = click_recognizer_get_button_id(recognizer);
  bitset32_set(&app_data->buttons_pressed, button_id_pressed);
  layer_set_hidden((struct Layer*)&app_data->arrows[button_id_pressed], true);

  if (app_data->test_complete) {
    app_window_stack_remove(&app_data->window, false /* Animated */);
  }
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_button_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_button_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_button_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_button_click_handler);
}

static void init_arrow_layer_for_button(AppData *data, ButtonId id) {
  static GPoint ARROW_PATH_POINTS[] =
      {{0,  7}, {14,  7}, {14, 0}, {26, 12}, {14, 24}, {14, 17}, {0, 17}};

  static const GPathInfo ARROW_PATH_INFO = {
    .num_points = ARRAY_LENGTH(ARROW_PATH_POINTS),
    .points = ARROW_PATH_POINTS
  };

// Original arrow dimensions
#define ARROW_W 26
#define ARROW_H 24
// The arrow path center (used as rotation pivot)
#define ARROW_CENTER_X 13
#define ARROW_CENTER_Y 12
// Layer size must be large enough to contain rotated arrow (diagonal of arrow)
#define ARROW_LAYER_SIZE 36
#define ARROW_SIZE {ARROW_W, ARROW_H}

  PathLayer *arrow = &data->arrows[id];
  path_layer_init(arrow, &ARROW_PATH_INFO);
  path_layer_set_fill_color(arrow, GColorBlack);
  path_layer_set_stroke_color(arrow, GColorBlack);

#if PBL_ROUND
  // Position arrows along the display circle
  // Angles are in Pebble trig format (0 at 3 o'clock, counter-clockwise)
  // Up: -30 deg from top, Down: +30 deg from top, Back: +30 deg from left
#define ARROW_MARGIN 5
  const int16_t center_x = DISP_COLS / 2;
  const int16_t center_y = DISP_ROWS / 2;
  const int16_t radius = center_x - ARROW_MARGIN - ARROW_LAYER_SIZE / 2;

  // Angles: 0 = 3 o'clock, 90 = 12 o'clock (TRIG_MAX_ANGLE/4), etc.
  // Back button: top-left, 30 deg from horizontal = 180 - 30 = 150 deg
  // Up button: top-right, 30 deg from horizontal = 30 deg
  // Select button: center right = 0 deg
  // Down button: bottom-right, 30 deg from horizontal = -30 deg
  const int32_t BUTTON_ANGLES[] = {
    // BACK: top-left, 150 deg
    DEG_TO_TRIGANGLE(150),
    // UP: top-right, 30 deg
    DEG_TO_TRIGANGLE(30),
    // SELECT: center right, 0 deg
    DEG_TO_TRIGANGLE(0),
    // DOWN: bottom-right, -30 deg
    DEG_TO_TRIGANGLE(-30),
  };

  int32_t angle = BUTTON_ANGLES[id];
  // Position the layer so its center is at the desired edge position
  int16_t x = center_x + (cos_lookup(angle) * radius / TRIG_MAX_RATIO) - ARROW_LAYER_SIZE / 2;
  int16_t y = center_y - (sin_lookup(angle) * radius / TRIG_MAX_RATIO) - ARROW_LAYER_SIZE / 2;

  // Use larger layer to accommodate rotated arrow without clipping
  layer_set_frame(&arrow->layer, &GRect(x, y, ARROW_LAYER_SIZE, ARROW_LAYER_SIZE));

  // Rotate arrow to point outward (away from center)
  // Arrow graphic points right (0 deg), so rotate by -angle to point outward
  int32_t rotation = -angle;
  gpath_rotate_to(&arrow->path, rotation);
  // Move the path so its center aligns with the layer center
  // The path rotates around origin, so we need to offset by the layer center
  // minus the rotated position of the original arrow center
  int32_t cos_rot = cos_lookup(rotation);
  int32_t sin_rot = sin_lookup(rotation);
  int16_t rotated_center_x = (ARROW_CENTER_X * cos_rot - ARROW_CENTER_Y * sin_rot) / TRIG_MAX_RATIO;
  int16_t rotated_center_y = (ARROW_CENTER_Y * cos_rot + ARROW_CENTER_X * sin_rot) / TRIG_MAX_RATIO;
  gpath_move_to(&arrow->path, GPoint(ARROW_LAYER_SIZE / 2 - rotated_center_x,
                                     ARROW_LAYER_SIZE / 2 - rotated_center_y));
#else
#define ARROW_LR_MARGIN 5
#define ARROW_TB_MARGIN 30
  const GRect ARROW_RECTS[] = {
    // BACK
    {{ARROW_LR_MARGIN,                       ARROW_TB_MARGIN}, ARROW_SIZE},
    // UP
    {{DISP_COLS - ARROW_LR_MARGIN - ARROW_W, ARROW_TB_MARGIN}, ARROW_SIZE},
    // SELECT
    {{DISP_COLS - ARROW_LR_MARGIN - ARROW_W, (DISP_ROWS - ARROW_H) / 2}, ARROW_SIZE},
    // DOWN
    {{DISP_COLS - ARROW_LR_MARGIN - ARROW_W, DISP_ROWS - ARROW_TB_MARGIN - ARROW_H }, ARROW_SIZE},
  };

  layer_set_frame(&arrow->layer, &ARROW_RECTS[id]);

  if (id == BUTTON_ID_BACK) {
    gpath_rotate_to(&arrow->path, (TRIG_MAX_ANGLE / 2));
    gpath_move_to(&arrow->path, GPoint(26, 24));
  }
#endif

  layer_add_child(&data->window.layer, &arrow->layer);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .seconds_remaining = 10,
    .buttons_pressed = 0,
    .test_complete = false,
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_overrides_back_button(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "BUTTON TEST");
  layer_add_child(&window->layer, &title->layer);

  TextLayer *status = &data->status;
  text_layer_init(status,
                  &GRect(5, 110,
                         window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 110));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  layer_add_child(&window->layer, &status->layer);

  for (ButtonId id = 0; id < NUM_BUTTONS; ++id) {
    init_arrow_layer_for_button(data, id);
  }

  app_window_stack_push(window, true /* Animated */);

  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd* mfg_button_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: eed03647-fa9e-4bae-9254-608aa297e4e4
    .common.uuid = { 0xee, 0xd0, 0x36, 0x47, 0xfa, 0x9e, 0x4b, 0xae,
                     0x92, 0x54, 0x60, 0x8a, 0xa2, 0x97, 0xe4, 0xe4},
    .name = "MfgButton",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

