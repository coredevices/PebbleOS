/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "text_layout.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include <limits.h>

struct AppState {
  Window window;
  ScrollLayer scroll_layer;
  TextLayer text;
};

static GFont s_system_font;
static GFont s_fonts[8];
static int s_font_selection = 0;

static const char* ALL_CODEPOINTS =
" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ¡¢£¤¥¦§¨©ª«¬®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿıŁłŒœŠšŸŽžƒˆˇ˘˙˚˛˜˝π–—‘’‚“”„†‡•…‰‹›⁄€™Ω∂∆∏∑−√∞∫≈≠≤≥◊ﬁﬂ";

static void select_click_handler(ClickRecognizerRef recognizer, void* callback_param) {
  (void) recognizer;
  struct AppState* data = (struct AppState*) callback_param;

  PBL_LOG(LOG_LEVEL_DEBUG, "I should be changing the font!");
  if (++s_font_selection > 7) {
    s_font_selection = 0;
  }
  ScrollLayer* scroll_layer = &data->scroll_layer;
  TextLayer* text = &data->text;

  text_layer_set_font(text, s_fonts[s_font_selection]);
  GSize max_size = graphics_text_layout_get_max_used_size(app_state_get_graphics_context(),
                              text->text, text->font,
                              GRect(0, 0, text->layer.bounds.size.w, SHRT_MAX),
                              text->overflow_mode, text->text_alignment, NULL);

  text_layer_set_size(text, max_size);
  static const int vert_scroll_padding = 4;
  scroll_layer_set_content_size(scroll_layer, GSize(144, max_size.h + vert_scroll_padding));
}

#if 0
static void select_long_click_handler(ClickRecognizerRef recognizer, struct AppState* data) {
  (void) data;
  (void) recognizer;

  PBL_LOG(LOG_LEVEL_DEBUG, "SELECT loooong clicked!");
}
#endif

static void click_config_provider(struct AppState* data) {
  // The config that gets passed in, has already the UP and DOWN buttons configured
  // to scroll up and down. It's possible to override that here, if needed.

  // Configure how the SELECT button should behave:
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, (ClickHandler) select_click_handler, NULL);
  (void)data;
}

static void prv_window_load(Window *window) {
  struct AppState* data = window_get_user_data(window);

  ScrollLayer* scroll_layer = &data->scroll_layer;
  scroll_layer_init(scroll_layer, &window->layer.bounds);
  scroll_layer_set_click_config_onto_window(scroll_layer, window);
  scroll_layer_set_callbacks(scroll_layer, (ScrollLayerCallbacks) {
    .click_config_provider = (ClickConfigProvider) click_config_provider,
  });
  scroll_layer_set_context(scroll_layer, data);
  scroll_layer_set_content_size(scroll_layer, GSize(144, 672));

  const GRect max_text_bounds = GRect(0, 0, 144, 672);
  TextLayer* text = &data->text;
  text_layer_init(text, &max_text_bounds);
  text_layer_set_font(text, s_system_font);
  text_layer_set_text(text, ALL_CODEPOINTS);

  // Trim text layer and scroll content to fit text box
  GSize max_size = text_layer_get_content_size(app_state_get_graphics_context(), text);
  text_layer_set_size(text, max_size);
  static const int vert_scroll_padding = 4;
  scroll_layer_set_content_size(scroll_layer, GSize(144, max_size.h + vert_scroll_padding));

  scroll_layer_add_child(scroll_layer, &text->layer);
  layer_add_child(&window->layer, &scroll_layer->layer);
}

static void push_window(struct AppState *data) {
  Window* window = &data->window;
  window_init(window, WINDOW_NAME("Text Layout Demo"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}


////////////////////
// App boilerplate

static void handle_init(void) {
  s_fonts[0] = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_fonts[1] = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_fonts[2] = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  s_fonts[3] = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_fonts[4] = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  s_fonts[5] = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_fonts[6] = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  s_fonts[7] = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  s_system_font = s_fonts[0];
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

const PebbleProcessMd* text_layout_get_info() {
  static const PebbleProcessMdSystem text_layout_info = {
    .common.main_func = &s_main,
    .name = "\xF3\xBE\x87\x8A Code Points Overflow This!" // The first 4 bytes is a UTF-8 codepoint for the hamster emoji.
  };
  return (const PebbleProcessMd*) &text_layout_info;
}

