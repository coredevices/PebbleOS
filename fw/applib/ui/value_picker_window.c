/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "value_picker_window.h"

#include "kernel/ui/system_icons.h"
#include "pbl/util/math.h"

#define BUTTON_REPEAT_INTERVAL_MS 50

static void prv_step(int direction, void *context) {
  ValuePickerWindow *picker_window = context;
  ValuePickerContent *content = &picker_window->content;
  const int32_t value =
      CLIP(content->value + direction * content->step, content->min_value, content->max_value);
  if (value == content->value) {
    return;
  }
  content->value = value;
  layer_mark_dirty(&picker_window->window.layer);
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

static void prv_update_proc(Layer *layer, GContext *ctx) {
  ValuePickerWindow *picker_window = window_get_user_data(layer_get_window(layer));
  value_picker_draw(ctx, &layer->bounds, &picker_window->content, picker_window->style);
}

static void prv_load(Window *window) {
  ValuePickerWindow *picker_window = window_get_user_data(window);
  value_picker_touch_init(&picker_window->touch, &window->layer, prv_step, picker_window);
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
  value_picker_touch_deinit(&picker_window->touch);
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
    .content = *content,
    .style = style ? style : value_picker_default_style(),
    .callbacks = callbacks,
    .callback_context = callback_context,
  };
  picker_window->content.value = CLIP(content->value, content->min_value, content->max_value);

  Window *window = &picker_window->window;
  window_init(window, WINDOW_NAME("Value Picker"));
  window_set_user_data(window, picker_window);
  window_set_background_color(window, picker_window->style->background_color);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .load = prv_load,
                                       .unload = prv_unload,
                                     });
  layer_set_update_proc(&window->layer, prv_update_proc);
}

int32_t value_picker_window_get_value(const ValuePickerWindow *picker_window) {
  return picker_window->content.value;
}
