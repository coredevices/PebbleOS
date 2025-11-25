/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

typedef struct AppData {
  Window *window;
  TextLayer *result_text;
  char *result;
  DictationSession *session;
  bool confirm;
} AppData;

static void prv_result_handler(DictationSession *session, DictationSessionStatus result,
                               char *transcription, void *context) {
  AppData *app_data = context;
  free(app_data->result);
  if (result == DictationSessionStatusSuccess) {
    size_t size = strlen(transcription) + 11;
    app_data->result = malloc(size);
    if (!app_data->result) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocated memory for transcription");
      result = 99;
    } else {
      snprintf(app_data->result, size, "You said:\n%s", transcription);
      text_layer_set_text(app_data->result_text, app_data->result);
      return;
    }
  }
  app_data->result = malloc(100);
  snprintf(app_data->result, 100, "Welp, that didn't work (Error: %u).\n Try again.", result);
  text_layer_set_text(app_data->result_text, app_data->result);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *app_data = context;
  dictation_session_start(app_data->session);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *app_data = context;
  dictation_session_destroy(app_data->session);
  app_data->session = NULL;
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *app_data = context;
  app_data->confirm = !app_data->confirm;
  dictation_session_enable_confirmation(app_data->session, app_data->confirm);
}

static void prv_click_config_provider(void *context) {
  window_set_click_context(BUTTON_ID_SELECT, context);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_set_click_context(BUTTON_ID_DOWN, context);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_set_click_context(BUTTON_ID_UP, context);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
}

static void prv_window_load(Window *window) {
  AppData *app_data = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(app_data->window);
  GRect bounds = layer_get_bounds(window_layer);

  app_data->result_text = text_layer_create((GRect) {
    .origin = { .x = 10, .y = 10},
    .size = { .w = bounds.size.w - 20, .h = bounds.size.h - 20 }
  });

  text_layer_set_text(app_data->result_text, "Press SELECT to start");
  text_layer_set_overflow_mode(app_data->result_text, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(app_data->result_text, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(app_data->result_text));
}

static void prv_window_unload(Window *window) {
  AppData *app_data = window_get_user_data(window);
  text_layer_destroy(app_data->result_text);
}

static void init(AppData *app_data) {
  app_data->session = dictation_session_create(1024, prv_result_handler, app_data);
  if (!app_data->session) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create dictation session");
  }

  app_data->confirm = true;

  app_data->window = window_create();
  window_set_click_config_provider_with_context(app_data->window, prv_click_config_provider,
      app_data);
  window_set_window_handlers(app_data->window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_set_user_data(app_data->window, app_data);

  const bool animated = true;
  window_stack_push(app_data->window, animated);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", app_data->window);
}

static void deinit(AppData *app_data) {
  dictation_session_destroy(app_data->session);
  window_destroy(app_data->window);
}

int main(void) {
  AppData *app_data = calloc(1, sizeof(AppData));
  init(app_data);

  app_event_loop();

  deinit(app_data);
}
