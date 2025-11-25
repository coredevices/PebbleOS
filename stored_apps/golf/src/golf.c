/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! The app is driven by pebble protocol app_messages, used indirectly through app_sync.
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#include <sys/types.h>
#include <pebble.h>

#include "golf_resources.h"

//! TODO: Fixme once i18n support is available for 3rd party apps
#define i18n_get(a, b) a
#define i18n_free_all(data)

enum {
  GOLF_FRONT_KEY = 0x0, // TUPLE_CSTRING
  GOLF_MID_KEY = 0x1, // TUPLE_CSTRING
  GOLF_BACK_KEY = 0x2, // TUPLE_CSTRING
  GOLF_HOLE_KEY = 0x3, // TUPLE_CSTRING
  GOLF_PAR_KEY = 0x4, // TUPLE_CSTRING
  GOLF_CMD_KEY = 0x5, // TUPLE_INTEGER
};

enum {
  CMD_PREV = 0x01,
  CMD_NEXT = 0x02,
  CMD_SELECT = 0x03,
};

typedef enum {
  TextBack = 0,
  TextMid,
  TextFront,
  TextParLabel,
  TextPar,
  TextHoleLabel,
  TextHole,
  NumTextIdx
} TextIdx;

const int KEY_TO_TEXT_IDX[] = {
  [GOLF_FRONT_KEY] = TextFront,
  [GOLF_MID_KEY] = TextMid,
  [GOLF_BACK_KEY] = TextBack,
  [GOLF_HOLE_KEY] = TextHole,
  [GOLF_PAR_KEY] = TextPar
};

typedef struct {
  Window *window;
  ActionBarLayer *action_bar;
  StatusBarLayer *status_layer;
  GBitmap *up_bitmap;
  GBitmap *down_bitmap;
  GBitmap *click_bitmap;
  Layer *background;
  TextLayer *text_layers[NumTextIdx];
  TextLayer *disconnected_text;
  uint8_t sync_buffer[60];
  AppSync sync;
} AppData;

static AppData s_data;

static void bluetooth_status_callback(bool connected) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Golf bluetooth connection status: %d", connected);

  AppData *data = &s_data;
  TextLayer **text = &data->text_layers[0];

#if PBL_ROUND
  layer_set_hidden(data->background, !connected);
  layer_set_hidden(action_bar_layer_get_layer(data->action_bar), !connected);
#endif
  layer_set_hidden(text_layer_get_layer(data->disconnected_text), connected);

  if (!connected) {
    // blank out text if we have no up-to-date data
    text_layer_set_text(text[TextBack], NULL);
    text_layer_set_text(text[TextMid], NULL);
    text_layer_set_text(text[TextFront], NULL);
    text_layer_set_text(text[TextPar], "-");
    text_layer_set_text(text[TextHole], "-");
  } else {
    // Return text to normal size. Display '...' while waiting for updated data.
    text_layer_set_text(text[TextMid], "...");
  }
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error,
                                void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Golf sync error! dict: %u, app msg: %u", dict_error,
          app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple *new_tuple,
                                        const Tuple *old_tuple, void *context) {
  AppData *data = context;
  TextLayer **text = &data->text_layers[0];
  switch (key) {
  case GOLF_BACK_KEY:
  case GOLF_MID_KEY:
  case GOLF_FRONT_KEY:
  case GOLF_HOLE_KEY:
  case GOLF_PAR_KEY:
    text_layer_set_text(text[KEY_TO_TEXT_IDX[key]], new_tuple->value->cstring);
  default:
    // Unknown key
    return;
  }
}

static void send_golf_cmd(uint8_t cmd) {
  Tuplet value = TupletInteger(GOLF_CMD_KEY, cmd);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL)
    return;

  dict_write_tuplet(iter, &value);
  dict_write_end(iter);

  app_message_outbox_send();
}

static void up_click_handler(ClickRecognizerRef recognizer, AppData *data) {
  send_golf_cmd(CMD_PREV);
}

static void down_click_handler(ClickRecognizerRef recognizer, AppData *data) {
  send_golf_cmd(CMD_NEXT);
}

static void select_click_handler(ClickRecognizerRef recognizer, AppData *data) {
  send_golf_cmd(CMD_SELECT);
}

static void config_provider(AppData *data) {
  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) select_click_handler);
}

static void window_unload(Window *window) {
  AppData *data = &s_data;
  app_sync_deinit(&data->sync);
}

// Used to draw lines to contain 'hole' and 'par' sections.
static void draw_dotted_line(GContext *ctx, GPoint p0, uint16_t length, bool is_vertical) {
  bool even = (p0.x + p0.y) % 2 == 0;
  const GPoint delta = is_vertical ? GPoint(0, 1) : GPoint(1, 0);
  while (length >= 1) {
    if (even) {
      graphics_draw_pixel(ctx, p0);
    }
    even = !even;
    p0.x += delta.x;
    p0.y += delta.y;
    --length;
  }
}

static void background_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Draw lines to contain 'hole' and 'par' sections.
  // Magic numbers measured from design spec
  const int16_t vertical_divider_height = PBL_IF_ROUND_ELSE(107, 50);
  const int16_t horizontal_divider_width = PBL_IF_ROUND_ELSE(51, bounds.size.w - ACTION_BAR_WIDTH);
  const int16_t vertical_divider_x_offset = PBL_IF_ROUND_ELSE(72, horizontal_divider_width / 2);
  const int16_t horizontal_divider_x_offset = PBL_IF_ROUND_ELSE(vertical_divider_x_offset, 0);
  const int16_t vertical_divider_y_offset = PBL_IF_ROUND_ELSE(
      37, bounds.size.h - vertical_divider_height);
  const uint16_t horizontal_divider_y_offset = PBL_IF_ROUND_ELSE(
      vertical_divider_y_offset + (vertical_divider_height / 2), vertical_divider_y_offset);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  draw_dotted_line(ctx, GPoint(horizontal_divider_x_offset, horizontal_divider_y_offset),
                   horizontal_divider_width, false /* is_vertical */);
  draw_dotted_line(ctx, GPoint(vertical_divider_x_offset, vertical_divider_y_offset),
                   vertical_divider_height, true /* is_vertical */);
}

static void prv_setup_text_layer(TextLayer *text_layer, const char *font_key, const char *text,
                                 GTextAlignment alignment) {
  text_layer_set_font(text_layer, fonts_get_system_font(font_key));
  text_layer_set_text(text_layer, text);
  text_layer_set_text_alignment(text_layer, alignment);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_background_color(text_layer, GColorClear);
}

static void window_load(Window *window) {
  AppData *data = &s_data;

  // Action bar icon bitmaps.
  data->up_bitmap = gbitmap_create_from_png_data(s_golf_api_up_icon_png_data,
                                                 sizeof(s_golf_api_up_icon_png_data));
  data->down_bitmap = gbitmap_create_from_png_data(s_golf_api_down_icon_png_data,
                                                   sizeof(s_golf_api_down_icon_png_data));
  data->click_bitmap = gbitmap_create_from_png_data(s_golf_api_click_icon_png_data,
                                                    sizeof(s_golf_api_click_icon_png_data));

  // Set up UI here
  Layer *window_layer = window_get_root_layer(window);
  TextLayer **text = &data->text_layers[0];
  const GRect window_bounds = layer_get_bounds(window_layer);
  const int16_t background_width = window_bounds.size.w - PBL_IF_RECT_ELSE(ACTION_BAR_WIDTH, 0);

  data->background = layer_create(window_bounds);
  Layer *background = data->background;
  layer_set_update_proc(background, &background_update_proc);

  layer_add_child(window_layer, background);

  // Set up the action bar.
  data->action_bar = action_bar_layer_create();
  action_bar_layer_set_context(data->action_bar, data);
  action_bar_layer_set_icon(data->action_bar, BUTTON_ID_UP, data->up_bitmap);
  action_bar_layer_set_icon(data->action_bar, BUTTON_ID_SELECT, data->click_bitmap);
  action_bar_layer_set_icon(data->action_bar, BUTTON_ID_DOWN, data->down_bitmap);
  action_bar_layer_set_click_config_provider(data->action_bar,
                                             (ClickConfigProvider) config_provider);
  action_bar_layer_set_icon_press_animation(data->action_bar,
                                            BUTTON_ID_UP,
                                            ActionBarLayerIconPressAnimationMoveUp);
  action_bar_layer_set_icon_press_animation(data->action_bar,
                                            BUTTON_ID_DOWN,
                                            ActionBarLayerIconPressAnimationMoveDown);
  action_bar_layer_add_to_window(data->action_bar, data->window);

  data->status_layer = status_bar_layer_create();
  // Change the status bar width to make space for the action bar
  const GRect status_frame = GRect(0, 0, background_width, STATUS_BAR_LAYER_HEIGHT);
  layer_set_frame(status_bar_layer_get_layer(data->status_layer), status_frame);
  status_bar_layer_set_colors(data->status_layer, GColorClear, GColorBlack);
#if PBL_RECT
  status_bar_layer_set_separator_mode(data->status_layer, StatusBarLayerSeparatorModeDotted);
#endif
  layer_add_child(background, status_bar_layer_get_layer(data->status_layer));

  // labels
  const char * const font_key_label = FONT_KEY_GOTHIC_09;
  // back, mid, front numbers
  const char * const font_key_small_numbers = PBL_IF_ROUND_ELSE(FONT_KEY_LECO_20_BOLD_NUMBERS,
                                                                FONT_KEY_LECO_28_LIGHT_NUMBERS);
  const char * const font_key_accent_numbers = PBL_IF_ROUND_ELSE(FONT_KEY_LECO_20_BOLD_NUMBERS,
                                                                 FONT_KEY_LECO_38_BOLD_NUMBERS);
  // hole, par numbers
  const char * const font_key_large_numbers = PBL_IF_ROUND_ELSE(FONT_KEY_LECO_32_BOLD_NUMBERS,
                                                                FONT_KEY_LECO_38_BOLD_NUMBERS);
  // "disconnected" text
  const char * const font_key_disconnected = FONT_KEY_GOTHIC_24_BOLD;

  static const GTextAlignment distance_text_alignment = PBL_IF_ROUND_ELSE(GTextAlignmentRight,
                                                                          GTextAlignmentCenter);

  // text heights only used for setting text box height, not for layout
  const int16_t label_height = 10;
  const int16_t small_numbers_height = 30;
  const int16_t accent_numbers_height = 40;
  const int16_t large_numbers_height = 40;
  const int16_t disconnected_text_height = 24;

  // magic numbers measured from design spec
  const int16_t distance_column_x_offset = 0;
  const int16_t distance_column_width = PBL_IF_ROUND_ELSE(63, background_width);
  const int16_t back_value_y_offset = STATUS_BAR_LAYER_HEIGHT + PBL_IF_ROUND_ELSE(24, 0);
  const int16_t mid_value_y_offset = back_value_y_offset + PBL_IF_ROUND_ELSE(30, 26);
  const int16_t front_value_y_offset = mid_value_y_offset + PBL_IF_ROUND_ELSE(30, 40);
  const int16_t disconnected_text_y_offset = mid_value_y_offset + PBL_IF_ROUND_ELSE(-5, 8);

  const int16_t stroke_box_width = PBL_IF_ROUND_ELSE(54, background_width / 2);
#if PBL_ROUND
  const int16_t stroke_box_height = PBL_IF_ROUND_ELSE(53, 50);
#endif
  const int16_t hole_box_x_offset = PBL_IF_ROUND_ELSE(73, 0);
  const int16_t hole_label_y_offset = STATUS_BAR_LAYER_HEIGHT + PBL_IF_ROUND_ELSE(18, 104);
  const int16_t hole_value_y_offset = hole_label_y_offset + PBL_IF_ROUND_ELSE(5, 2);
  const int16_t par_box_x_offset = hole_box_x_offset + PBL_IF_ROUND_ELSE(0, stroke_box_width);
  const int16_t par_label_y_offset = hole_label_y_offset + PBL_IF_ROUND_ELSE(stroke_box_height, 0);
  const int16_t par_value_y_offset = hole_value_y_offset + PBL_IF_ROUND_ELSE(stroke_box_height, 0);

  // Hole label.
  text[TextHoleLabel] = text_layer_create(GRect(hole_box_x_offset, hole_label_y_offset,
                                                stroke_box_width, label_height));
  prv_setup_text_layer(text[TextHoleLabel], font_key_label,
                       i18n_get("HOLE", data), GTextAlignmentCenter);
  layer_add_child(background, text_layer_get_layer(text[TextHoleLabel]));

  // Hole value.
  text[TextHole] = text_layer_create(GRect(hole_box_x_offset, hole_value_y_offset,
                                           stroke_box_width, large_numbers_height));
  prv_setup_text_layer(text[TextHole], font_key_large_numbers, NULL, GTextAlignmentCenter);
  layer_add_child(background, text_layer_get_layer(text[TextHole]));

  // Par label.
  text[TextParLabel] = text_layer_create(GRect(par_box_x_offset, par_label_y_offset,
                                               stroke_box_width, label_height));
  prv_setup_text_layer(text[TextParLabel], font_key_label, i18n_get("PAR", data),
                       GTextAlignmentCenter);
  layer_add_child(background, text_layer_get_layer(text[TextParLabel]));

  // Par value.
  text[TextPar] = text_layer_create(GRect(par_box_x_offset, par_value_y_offset,
                                          stroke_box_width, large_numbers_height));
  prv_setup_text_layer(text[TextPar], font_key_large_numbers, NULL, GTextAlignmentCenter);
  layer_add_child(background, text_layer_get_layer(text[TextPar]));

  // Back value.
  text[TextBack] = text_layer_create(GRect(distance_column_x_offset, back_value_y_offset,
                                           distance_column_width, small_numbers_height));
  prv_setup_text_layer(text[TextBack], font_key_small_numbers, NULL, distance_text_alignment);
  layer_add_child(background, text_layer_get_layer(text[TextBack]));

  // Mid value.
  text[TextMid] = text_layer_create(GRect(distance_column_x_offset, mid_value_y_offset,
                                          distance_column_width, accent_numbers_height));
  prv_setup_text_layer(text[TextMid], font_key_accent_numbers, NULL, distance_text_alignment);
  layer_add_child(background, text_layer_get_layer(text[TextMid]));

  // Front value.
  text[TextFront] = text_layer_create(GRect(distance_column_x_offset, front_value_y_offset,
                                            distance_column_width, small_numbers_height));
  prv_setup_text_layer(text[TextFront], font_key_small_numbers, NULL, distance_text_alignment);
  layer_add_child(background, text_layer_get_layer(text[TextFront]));

  // Disconnected text.
  data->disconnected_text = text_layer_create(GRect(0, disconnected_text_y_offset,
                                                    background_width, disconnected_text_height));
  prv_setup_text_layer(data->disconnected_text, font_key_disconnected,
                       i18n_get("Disconnected", data), GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(data->disconnected_text));
  layer_set_hidden(text_layer_get_layer(data->disconnected_text), true);

  // Sync setup:
  Tuplet initial_values[] = {
    TupletCString(GOLF_PAR_KEY, "0"),
    TupletCString(GOLF_HOLE_KEY, "0"),
    TupletCString(GOLF_BACK_KEY, "000"),
    TupletCString(GOLF_MID_KEY, "000"),
    TupletCString(GOLF_FRONT_KEY, "000"),
  };
  app_sync_init(&data->sync, data->sync_buffer, sizeof(data->sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback, sync_error_callback,
                data);
}

static void push_window(AppData *data) {
  data->window = window_create();
  Window *window = data->window;

  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_set_click_config_provider_with_context(window, (ClickConfigProvider) config_provider,
                                                data);
  window_set_background_color(window, PBL_IF_COLOR_ELSE(GColorMintGreen, GColorWhite));
  window_stack_push(window, true);
}

static void handle_init(void) {
  app_message_open(64, 16);
  push_window(&s_data);

  // overall reduce the sniff-mode latency at the expense of some power...
  app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

  ConnectionHandlers handlers = {
    .pebble_app_connection_handler = NULL,
    .pebblekit_connection_handler = bluetooth_status_callback
  };
  connection_service_subscribe(handlers);
}

////////////////////
// App boilerplate

int main(void) {
  handle_init();
  app_event_loop();
}
