/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "text_clipping.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

typedef enum {
  SelectIndexPixels,
  SelectIndexDirection,
  SelectIndexOverflow,
  // Add new selection criteria above
  SelectIndexMax
} SelectIndex;

typedef struct AppState {
  Window window;
  Layer canvas;
  GSize canvas_size;
  TextLayer text_layer;
  TextLayer direction_layer;
  TextLayer word_wrap_layer;
  SelectIndex select_index;
  bool up_down_direction; // True = move up or down; False = move left or right
  bool word_wrap;         // True = word wrap; False = don't word wrap
} AppState;

static const char* text_buffer = "Text Clipping";

static void init_text_layer(AppState *data, GRect frame) {
  text_layer_init(&data->text_layer, &GRect(frame.origin.x, frame.origin.y,
                                            frame.size.w, frame.size.h));
  text_layer_set_background_color(&data->text_layer, GColorWhite);
  text_layer_set_text_color(&data->text_layer, GColorBlack);
  text_layer_set_text(&data->text_layer, text_buffer);
  GFont gothic_24_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  text_layer_set_font(&data->text_layer, gothic_24_bold);
  text_layer_set_text_alignment(&data->text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(&data->text_layer, GTextOverflowModeTrailingEllipsis);
}

static void click_handler(ClickRecognizerRef recognizer, Window *window) {
  AppState *data = window_get_user_data(window);
  ButtonId button = click_recognizer_get_button_id(recognizer);
  GRect frame = data->text_layer.layer.frame;

  if (button == BUTTON_ID_UP) {
    if (data->select_index == SelectIndexPixels) {
      // Do inverse
      if (data->up_down_direction) {
        frame.origin.y--;
      } else {
        frame.origin.x--;
      }
    } else if (data->select_index == SelectIndexDirection) {
      data->up_down_direction = !(data->up_down_direction);
    } else if (data->select_index == SelectIndexOverflow) {
      data->word_wrap = !(data->word_wrap);
    }
  } else if (button == BUTTON_ID_SELECT) {
    data->select_index = (data->select_index + 1) % SelectIndexMax;
  } else if (button == BUTTON_ID_DOWN) {
    if (data->select_index == SelectIndexPixels) {
      // Do inverse
      if (data->up_down_direction) {
        frame.origin.y++;
      } else {
        frame.origin.x++;
      }
    } else if (data->select_index == SelectIndexDirection) {
      data->up_down_direction = !(data->up_down_direction);
    } else if (data->select_index == SelectIndexOverflow) {
      data->word_wrap = !(data->word_wrap);
    }
  }

  text_layer_set_text_color(&data->direction_layer, GColorBlack);
  text_layer_set_background_color(&data->direction_layer, GColorWhite);
  text_layer_set_text_color(&data->word_wrap_layer, GColorBlack);
  text_layer_set_background_color(&data->word_wrap_layer, GColorWhite);

  if (data->select_index == SelectIndexDirection) {
    text_layer_set_text_color(&data->direction_layer, GColorWhite);
    text_layer_set_background_color(&data->direction_layer, GColorBlack);
  } else if (data->select_index == SelectIndexOverflow) {
    text_layer_set_text_color(&data->word_wrap_layer, GColorWhite);
    text_layer_set_background_color(&data->word_wrap_layer, GColorBlack);
  }

  if (data->up_down_direction) {
    text_layer_set_text(&data->direction_layer, "Direction: Up/Down");
  } else {
    text_layer_set_text(&data->direction_layer, "Direction: Left/Right");
  }

  if (data->word_wrap) {
    frame.size.w = 72;
    frame.size.h = 60;
  } else {
    frame.size.w = 72;
    frame.size.h = 32;
  }

  init_text_layer(data, frame);

  if (data->word_wrap) {
    text_layer_set_text(&data->word_wrap_layer, "Overflow: Word Wrap");
    text_layer_set_overflow_mode(&data->text_layer, GTextOverflowModeWordWrap);
  } else {
    text_layer_set_text(&data->word_wrap_layer, "Overflow: Ellipsis");
    text_layer_set_overflow_mode(&data->text_layer, GTextOverflowModeTrailingEllipsis);
  }
}

static void config_provider(Window *window) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, (ClickHandler)click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_SELECT, 100, (ClickHandler)click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, (ClickHandler)click_handler);
  (void)window;
}

static void update_window(Layer *layer, GContext *ctx) {
  // Clear parent layer first
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &GRect(39, 39, 82, 42));

  // Draw box just outside the clipping area
  graphics_draw_rect(ctx, &GRect(39, 39, 82, 42));
}

static void prv_window_load(Window *window) {
  AppState *data = window_get_user_data(window);

  data->select_index = SelectIndexPixels;
  data->up_down_direction = true;
  data->word_wrap = false;

  // Init canvas (i.e. clipping box)
  data->canvas_size = GSize(80, 40);

  layer_init(&data->canvas, &GRect(40, 40, data->canvas_size.w, data->canvas_size.h));
  layer_add_child(&data->window.layer, &data->canvas);

  // Init text layer
  init_text_layer(data, GRect(4, 4, 72, 32));

  // Init direction layer
  text_layer_init(&data->direction_layer, &GRect(5, 100, 135, 20));
  text_layer_set_background_color(&data->direction_layer, GColorWhite);
  text_layer_set_text_color(&data->direction_layer, GColorBlack);
  text_layer_set_text(&data->direction_layer, "Direction: Up/Down");
  GFont gothic_14_bold = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  text_layer_set_font(&data->direction_layer, gothic_14_bold);
  text_layer_set_text_alignment(&data->direction_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(&data->direction_layer, GTextOverflowModeTrailingEllipsis);

  // Init word wrap layer
  text_layer_init(&data->word_wrap_layer, &GRect(5, 130, 135, 20));
  text_layer_set_background_color(&data->word_wrap_layer, GColorWhite);
  text_layer_set_text_color(&data->word_wrap_layer, GColorBlack);
  text_layer_set_text(&data->word_wrap_layer, "Overflow: Ellipsis");
  text_layer_set_font(&data->word_wrap_layer, gothic_14_bold);
  text_layer_set_text_alignment(&data->word_wrap_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(&data->word_wrap_layer, GTextOverflowModeTrailingEllipsis);

  // Setup children for clipping canvas
  layer_add_child(&data->canvas, &data->text_layer.layer);

  // Setup children for main window canvas
  layer_add_child(&window->layer, &data->direction_layer.layer);
  layer_add_child(&window->layer, &data->word_wrap_layer.layer);

  // Setup update proc to draw clipping box
  layer_set_update_proc(&window->layer, update_window);
}

static void push_window(struct AppState *data) {
  Window* window = &data->window;
  window_init(window, WINDOW_NAME("Text Clipping"));
  window_set_user_data(window, data);
  window_set_click_config_provider(window, (ClickConfigProvider) config_provider);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}


////////////////////
// App boilerplate

static void handle_init(void) {
  struct AppState* data = app_malloc_check(sizeof(struct AppState));

  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit(void) {
  struct AppState* data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* text_clipping_app_get_info() {
  static const PebbleProcessMdSystem text_spacing_info = {
    .common.main_func = &s_main,
    .name = "Text Clipping"
  };
  return (const PebbleProcessMd*) &text_spacing_info;
}
