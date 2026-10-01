/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "value_picker_window.h"

#include "kernel/ui/system_icons.h"

#define BUTTON_REPEAT_INTERVAL_MS 50

static void prv_step(int direction, void *context) {
  ValuePickerWindow *picker_window = context;
  value_picker_layer_step(&picker_window->picker_layer, direction);
}

static void prv_changed(ValuePickerLayer *picker_layer, int direction, void *context) {
  ValuePickerWindow *picker_window = context;
  if (picker_window->callbacks.changed) {
    picker_window->callbacks.changed(picker_window, picker_window->callback_context);
  }
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_step(1, context);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_step(-1, context);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  ValuePickerWindow *picker_window = context;
  if (picker_window->callbacks.selected) {
    picker_window->callbacks.selected(picker_window, picker_window->callback_context);
  }
}

static void prv_click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, BUTTON_REPEAT_INTERVAL_MS,
                                          prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, BUTTON_REPEAT_INTERVAL_MS,
                                          prv_down_click_handler);
  // Multi-click keeps the action bar's pressed segment visible briefly as feedback.
  window_multi_click_subscribe(BUTTON_ID_SELECT, 1, 2, 25, true, prv_select_click_handler);
}

static void prv_load(Window *window) {
  ValuePickerWindow *picker_window = window_get_user_data(window);
  value_picker_layer_enable_touch(&picker_window->picker_layer);
  ActionBarLayer *action_bar = &picker_window->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_set_context(action_bar, picker_window);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, &s_bar_icon_up_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, &s_bar_icon_down_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, &s_bar_icon_check_bitmap);
  action_bar_layer_add_to_window(action_bar, window);
  action_bar_layer_set_click_config_provider(action_bar, prv_click_config_provider);
}

static void prv_unload(Window *window) {
  ValuePickerWindow *picker_window = window_get_user_data(window);
  value_picker_layer_deinit(&picker_window->picker_layer);
  action_bar_layer_deinit(&picker_window->action_bar);
  window_deinit(window);
  if (picker_window->callbacks.unload) {
    picker_window->callbacks.unload(picker_window, picker_window->callback_context);
  }
}

void value_picker_window_init(ValuePickerWindow *picker_window, const ValuePickerContent *content,
                              const ValuePickerStyle *style, ValuePickerWindowCallbacks callbacks,
                              void *callback_context) {
  *picker_window = (ValuePickerWindow){
    .callbacks = callbacks,
    .callback_context = callback_context,
  };

  Window *window = &picker_window->window;
  window_init(window, WINDOW_NAME("Value Picker"));
  window_set_user_data(window, picker_window);
  const GRect picker_frame =
      GRect(0, 0, window->layer.bounds.size.w - ACTION_BAR_WIDTH, window->layer.bounds.size.h);
  value_picker_layer_init(&picker_window->picker_layer, &picker_frame, content, style,
                          (ValuePickerLayerCallbacks){
                            .changed = prv_changed,
                          },
                          picker_window);
  layer_add_child(&window->layer, &picker_window->picker_layer.layer);
  window_set_background_color(window, picker_window->picker_layer.style.background_color);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_load,
                                       .unload = prv_unload,
                                     });
}

int32_t value_picker_window_get_value(const ValuePickerWindow *picker_window) {
  return value_picker_layer_get_value(&picker_window->picker_layer);
}
