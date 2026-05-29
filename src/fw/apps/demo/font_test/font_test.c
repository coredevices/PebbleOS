/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "font_test.h"

#include <stdio.h>

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "util/size.h"

#define FIRST_GLYPH 0x20  // space
#define LAST_GLYPH  0x7E  // tilde

typedef struct {
  const char *key;
  const char *label;
} FontEntry;

static const FontEntry s_fonts[] = {
  { FONT_KEY_LECO_20_BOLD_NUMBERS,         "LECO_20_BOLD" },
  { FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,   "LECO_26_BOLD" },
  { FONT_KEY_LECO_28_LIGHT_NUMBERS,        "LECO_28_LIGHT" },
  { FONT_KEY_LECO_32_BOLD_NUMBERS,         "LECO_32_BOLD" },
  { FONT_KEY_LECO_36_BOLD_NUMBERS,         "LECO_36_BOLD" },
  { FONT_KEY_LECO_38_BOLD_NUMBERS,         "LECO_38_BOLD" },
  { FONT_KEY_LECO_42_NUMBERS,              "LECO_42" },
  { FONT_KEY_LECO_60_NUMBERS_AM_PM,        "LECO_60" },
  { FONT_KEY_LECO_60_BOLD_NUMBERS_AM_PM,   "LECO_60_BOLD" },
};

typedef struct {
  Window window;
  TextLayer header_layer;
  TextLayer info_layer;
  TextLayer glyph_layer;
  GFont label_font;
  unsigned int font_index;
  uint8_t codepoint;
  char header_buf[32];
  char info_buf[16];
  char glyph_buf[2];
} AppState;

static void prv_refresh(AppState *data) {
  sniprintf(data->header_buf, sizeof(data->header_buf), "%u/%u %s",
           data->font_index + 1, (unsigned int)ARRAY_LENGTH(s_fonts),
           s_fonts[data->font_index].label);
  sniprintf(data->info_buf, sizeof(data->info_buf), "U+%04X '%c'",
           data->codepoint, data->codepoint);
  data->glyph_buf[0] = (char)data->codepoint;
  data->glyph_buf[1] = '\0';

  text_layer_set_font(&data->glyph_layer,
                      fonts_get_system_font(s_fonts[data->font_index].key));

  layer_mark_dirty(&data->header_layer.layer);
  layer_mark_dirty(&data->info_layer.layer);
  layer_mark_dirty(&data->glyph_layer.layer);
}

static void prv_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppState *data = context;
  ButtonId button = click_recognizer_get_button_id(recognizer);
  if (button == BUTTON_ID_UP) {
    data->codepoint = (data->codepoint == FIRST_GLYPH)
                          ? LAST_GLYPH
                          : (uint8_t)(data->codepoint - 1);
  } else if (button == BUTTON_ID_DOWN) {
    data->codepoint = (data->codepoint == LAST_GLYPH)
                          ? FIRST_GLYPH
                          : (uint8_t)(data->codepoint + 1);
  } else if (button == BUTTON_ID_SELECT) {
    data->font_index = (data->font_index + 1) % ARRAY_LENGTH(s_fonts);
  }
  prv_refresh(data);
}

static void prv_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, prv_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click_handler);
}

static void prv_window_load(Window *window) {
  AppState *data = window_get_user_data(window);
  const GRect bounds = window->layer.bounds;

  data->label_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

  text_layer_init(&data->header_layer, &GRect(0, 2, bounds.size.w, 18));
  text_layer_set_background_color(&data->header_layer, GColorWhite);
  text_layer_set_text_color(&data->header_layer, GColorBlack);
  text_layer_set_text_alignment(&data->header_layer, GTextAlignmentCenter);
  text_layer_set_font(&data->header_layer, data->label_font);
  text_layer_set_text(&data->header_layer, data->header_buf);
  layer_add_child(&window->layer, &data->header_layer.layer);

  text_layer_init(&data->info_layer, &GRect(0, 22, bounds.size.w, 18));
  text_layer_set_background_color(&data->info_layer, GColorWhite);
  text_layer_set_text_color(&data->info_layer, GColorBlack);
  text_layer_set_text_alignment(&data->info_layer, GTextAlignmentCenter);
  text_layer_set_font(&data->info_layer, data->label_font);
  text_layer_set_text(&data->info_layer, data->info_buf);
  layer_add_child(&window->layer, &data->info_layer.layer);

  // Big glyph fills the rest of the screen.
  const int16_t glyph_y = 44;
  text_layer_init(&data->glyph_layer,
                  &GRect(0, glyph_y, bounds.size.w, bounds.size.h - glyph_y));
  text_layer_set_background_color(&data->glyph_layer, GColorWhite);
  text_layer_set_text_color(&data->glyph_layer, GColorBlack);
  text_layer_set_text_alignment(&data->glyph_layer, GTextAlignmentCenter);
  text_layer_set_text(&data->glyph_layer, data->glyph_buf);
  layer_add_child(&window->layer, &data->glyph_layer.layer);

  prv_refresh(data);
}

static void prv_push_window(AppState *data) {
  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Font Test"));
  window_set_user_data(window, data);
  window_set_background_color(window, GColorWhite);
  window_set_click_config_provider_with_context(window, prv_config_provider, data);
  window_set_window_handlers(window, &(WindowHandlers){
    .load = prv_window_load,
  });
  app_window_stack_push(window, true);
}

static void prv_handle_init(void) {
  AppState *data = app_zalloc_check(sizeof(AppState));
  data->font_index = 0;
  data->codepoint = '0';
  app_state_set_user_data(data);
  prv_push_window(data);
}

static void prv_handle_deinit(void) {
  AppState *data = app_state_get_user_data();
  app_free(data);
}

static void prv_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd *font_test_app_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common.main_func = &prv_main,
    .name = "Font Test",
  };
  return (const PebbleProcessMd *)&s_info;
}
