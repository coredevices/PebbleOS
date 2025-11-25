/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "movable_line.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/text.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

static Window *s_window;

typedef enum PixelBit {
  PIXEL_BIT_BOTH,
  PIXEL_BIT_MSB,
  PIXEL_BIT_LSB,
  PIXEL_BIT_COUNT
} PixelBit;

typedef enum LineHue {
  HUE_RED,
  HUE_GREEN,
  HUE_BLUE,
  HUE_WHITE,
  HUE_COUNT
} LineHue;

typedef enum LineAttribute {
  ATTRIBUTE_HUE,
  ATTRIBUTE_PIXEL_BIT,
  ATTRIBUTE_X,
  ATTRIBUTE_Y,
  ATTRIBUTE_COUNT
} LineAttribute;


typedef struct AppData {
  Layer *canvas_layer;

  // UI state
  LineAttribute selection;

  // Line attributes
  PixelBit pixel_bit;
  LineHue hue;
  GPoint intersection;
} AppData;

static void up_handler(ClickRecognizerRef recognizer, void *context) {
  // Increment the selected attribute
  AppData *data = window_get_user_data(s_window);
  switch (data->selection) {
    case ATTRIBUTE_HUE:
      if (data->hue <= 0) {
        data->hue = HUE_COUNT - 1;
      } else {
        --data->hue;
      }
      break;
    case ATTRIBUTE_PIXEL_BIT:
      if (data->pixel_bit <= 0) {
        data->pixel_bit = PIXEL_BIT_COUNT - 1;
      } else {
        --data->pixel_bit;
      }
      break;
    case ATTRIBUTE_X:
      if (data->intersection.x > 0) {
        --data->intersection.x;
      }
      break;
    case ATTRIBUTE_Y:
      if (data->intersection.y > 0) {
        --data->intersection.y;
      }
      break;
    default:
      break;
  }
  layer_mark_dirty(data->canvas_layer);
}

static void down_handler(ClickRecognizerRef recognizer, void *context) {
  // Decrement the selected attribute
  AppData *data = window_get_user_data(s_window);
  GRect bounds = data->canvas_layer->bounds;
  switch (data->selection) {
    case ATTRIBUTE_HUE:
      ++data->hue;
      if (data->hue >= HUE_COUNT) {
        data->hue = 0;
      }
      break;
    case ATTRIBUTE_PIXEL_BIT:
      ++data->pixel_bit;
      if (data->pixel_bit >= PIXEL_BIT_COUNT) {
        data->pixel_bit = 0;
      }
      break;
    case ATTRIBUTE_X:
      if (data->intersection.x < bounds.size.w - 1) {
        ++data->intersection.x;
      }
      break;
    case ATTRIBUTE_Y:
      if (data->intersection.y < bounds.size.h - 1) {
        ++data->intersection.y;
      }
      break;
    default:
      break;
  }
  layer_mark_dirty(data->canvas_layer);
}

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  // Cycle through the attributes
  AppData *data = window_get_user_data(s_window);
  ++data->selection;
  if (data->selection >= ATTRIBUTE_COUNT) {
    data->selection = 0;
  }
  layer_mark_dirty(data->canvas_layer);
}


static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_SELECT, 100, select_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_handler);
}

static void draw_ui_element(GContext *ctx, GRect bounds, const char *text,
                            bool chosen, bool selected) {
  GFont font = fonts_get_system_font(chosen || selected?
                                       FONT_KEY_GOTHIC_14_BOLD
                                     : FONT_KEY_GOTHIC_14);
  if (chosen && selected) {
    // Draw rectangle behind text, invert text color
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_round_rect(ctx, &bounds, 2, GCornersAll);
    graphics_context_set_text_color(ctx, GColorBlack);
  } else {
    graphics_context_set_text_color(ctx, GColorWhite);
  }
  graphics_draw_text(ctx, text, font, bounds, GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static void canvas_update_proc(Layer *layer, GContext* ctx) {
  AppData *data = window_get_user_data(s_window);
  GRect bounds = layer->bounds;

  // Fill background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, &bounds);

  // Draw UI
  draw_ui_element(ctx, GRect(30, 80, 20, 20), "R", data->hue == HUE_RED,
                  data->selection == ATTRIBUTE_HUE);
  draw_ui_element(ctx, GRect(50, 80, 20, 20), "G", data->hue == HUE_GREEN,
                  data->selection == ATTRIBUTE_HUE);
  draw_ui_element(ctx, GRect(70, 80, 20, 20), "B", data->hue == HUE_BLUE,
                  data->selection == ATTRIBUTE_HUE);
  draw_ui_element(ctx, GRect(90, 80, 20, 20), "W", data->hue == HUE_WHITE,
                  data->selection == ATTRIBUTE_HUE);

  draw_ui_element(ctx, GRect(30, 100, 35, 20), "Both",
                  data->pixel_bit == PIXEL_BIT_BOTH,
                  data->selection == ATTRIBUTE_PIXEL_BIT);
  draw_ui_element(ctx, GRect(65, 100, 30, 20), "MSB",
                  data->pixel_bit == PIXEL_BIT_MSB,
                  data->selection == ATTRIBUTE_PIXEL_BIT);
  draw_ui_element(ctx, GRect(95, 100, 30, 20), "LSB",
                  data->pixel_bit == PIXEL_BIT_LSB,
                  data->selection == ATTRIBUTE_PIXEL_BIT);

  char text[6];
  snprintf(text, sizeof(text), "x=%"PRId16, data->intersection.x);
  draw_ui_element(ctx, GRect(30, 120, 40, 20), text,
                  data->selection == ATTRIBUTE_X,
                  data->selection == ATTRIBUTE_X);
  snprintf(text, sizeof(text), "y=%"PRId16, data->intersection.y);
  draw_ui_element(ctx, GRect(70, 120, 40, 20), text,
                  data->selection == ATTRIBUTE_Y,
                  data->selection == ATTRIBUTE_Y);

  // Draw the lines
  uint8_t saturation;
  if (data->pixel_bit == PIXEL_BIT_MSB) {
    saturation = 0b10101010;
  } else if (data->pixel_bit == PIXEL_BIT_LSB) {
    saturation = 0b01010101;
  } else {
    saturation = 0b11111111;
  }
  uint8_t r = 0, g = 0, b = 0;
  if (data->hue == HUE_RED) {
    r = 1;
  } else if (data->hue == HUE_GREEN) {
    g = 1;
  } else if (data->hue == HUE_BLUE) {
    b = 1;
  } else if (data->hue == HUE_WHITE) {
    r = 1;
    g = 1;
    b = 1;
  }
  GColor line_color = GColorFromRGB(r * saturation, g * saturation,
                                    b * saturation);
  graphics_context_set_stroke_color(ctx, line_color);
  graphics_draw_line(ctx, GPoint(0, data->intersection.y),
                     GPoint(bounds.size.w, data->intersection.y));
  graphics_draw_line(ctx, GPoint(data->intersection.x, 0),
                     GPoint(data->intersection.x, bounds.size.h));
}

static void main_window_load(Window *window) {
  AppData *data = window_get_user_data(s_window);
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = window_layer->bounds;

  data->canvas_layer = layer_create(GRect(0, 0, window_bounds.size.w,
                                          window_bounds.size.h));
  layer_set_update_proc(data->canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, data->canvas_layer);
}

static void main_window_unload(Window *window) {
  AppData *data = window_get_user_data(s_window);
  layer_destroy(data->canvas_layer);
}

static void init(void) {
  AppData *data = task_zalloc(sizeof(AppData));
  if (!data) {
    return;
  }

  s_window = window_create();
  window_set_user_data(s_window, data);
  window_set_fullscreen(s_window, true);
  window_set_window_handlers(s_window, &(WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });

  window_set_click_config_provider(s_window, click_config_provider);

  const bool animated = true;
  app_window_stack_push(s_window, animated);
}

static void deinit(void) {
  AppData *data = window_get_user_data(s_window);
  task_free(data);
  window_destroy(s_window);
}

static void s_main(void) {
  init();
  app_event_loop();
  deinit();
}

const PebbleProcessMd* movable_line_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = s_main,
    .name = "Movable Line"
  };
  return (const PebbleProcessMd*) &s_app_info;
}
