/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "number_window.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/applib_malloc.auto.h"
#include "kernel/ui/kernel_ui.h"
#include "kernel/ui/system_icons.h"
#include "pbl/util/size.h"
#ifdef CONFIG_TOUCH
#include "value_picker_layer.h"
#endif

#include <limits.h>
#include <stdio.h>

#if defined(CONFIG_RECOVERY_FW) || defined(CONFIG_MFG)
#define NUMBER_FONT_KEY FONT_KEY_GOTHIC_24_BOLD
#else
#define NUMBER_FONT_KEY FONT_KEY_BITHAM_34_MEDIUM_NUMBERS
#endif

#ifdef CONFIG_TOUCH
static ValuePickerLayer *prv_get_picker_layer(const NumberWindow *number_window) {
  return number_window->private_data;
}

static ValuePickerContent prv_get_picker_content(const NumberWindow *number_window) {
  return (ValuePickerContent){
    .title = number_window->label,
    .value = number_window->value,
    .min_value = number_window->min_val,
    .max_value = number_window->max_val,
    .step = number_window->step_size,
  };
}
#endif

// updates the textual output value of the numberwindow to match the actual value
static void update_output_value(NumberWindow *nf) {
#ifdef CONFIG_TOUCH
  ValuePickerLayer *picker_layer = prv_get_picker_layer(nf);
  if (picker_layer) {
    const ValuePickerContent content = prv_get_picker_content(nf);
    value_picker_layer_set_content(picker_layer, &content);
    return;
  }
#endif
  layer_mark_dirty(&nf->window.layer);
}

static void up_click_handler(ClickRecognizerRef recognizer, NumberWindow *nf) {
#ifdef CONFIG_TOUCH
  ValuePickerLayer *picker_layer = prv_get_picker_layer(nf);
  if (picker_layer) {
    value_picker_layer_step(picker_layer, 1);
    return;
  }
#endif
  bool is_increased = false;
  int32_t new_val = nf->value + nf->step_size;
  if (new_val <= nf->max_val && new_val > nf->value) {
    nf->value = new_val;
    is_increased = true;
  }
  if (is_increased) {
    if (nf->callbacks.incremented != NULL) {
      nf->callbacks.incremented(nf, nf->callback_context);
    }
    update_output_value(nf);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, NumberWindow *nf) {
#ifdef CONFIG_TOUCH
  ValuePickerLayer *picker_layer = prv_get_picker_layer(nf);
  if (picker_layer) {
    value_picker_layer_step(picker_layer, -1);
    return;
  }
#endif
  bool is_decreased = false;
  int32_t new_val = nf->value - nf->step_size;
  if (new_val >= nf->min_val && new_val < nf->value) {
    nf->value = new_val;
    is_decreased = true;
  }
  if (is_decreased) {
    if (nf->callbacks.decremented != NULL) {
      nf->callbacks.decremented(nf, nf->callback_context);
    }
    update_output_value(nf);
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, NumberWindow *nf) {
  if (nf->callbacks.selected != NULL) {
    nf->callbacks.selected(nf, nf->callback_context);
  }
}

static void click_config_provider(NumberWindow *nf) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 50, (ClickHandler)up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 50, (ClickHandler)down_click_handler);

  // Work-around: by using a multi-click setup for the select button,
  // the handler will get fired with a very short delay, so the inverted segment of
  // the action bar is visible for a short period of time as to give visual
  // feedback of the button press.
  window_multi_click_subscribe(BUTTON_ID_SELECT, 1, 2, 25, true,
                               (ClickHandler)select_click_handler);
}

#ifdef CONFIG_TOUCH
static bool prv_can_step(ValuePickerLayer *picker_layer, int direction, void *context) {
  (void)picker_layer;
  const NumberWindow *number_window = context;
  if (number_window->step_size <= 0) {
    return false;
  }
  const int64_t value =
      (int64_t)number_window->value + direction * (int64_t)number_window->step_size;
  return value >= number_window->min_val && value <= number_window->max_val;
}

static void prv_changed(ValuePickerLayer *picker_layer, int direction, void *context) {
  NumberWindow *number_window = context;
  number_window->value = value_picker_layer_get_value(picker_layer);
  if (direction > 0) {
    if (number_window->callbacks.incremented) {
      number_window->callbacks.incremented(number_window, number_window->callback_context);
    }
  } else if (direction < 0 && number_window->callbacks.decremented) {
    number_window->callbacks.decremented(number_window, number_window->callback_context);
  }
}
#endif

static GRect prv_get_text_frame(Layer *window_layer) {
  const int16_t x_margin = 5;
  const int16_t label_y_offset = PBL_IF_ROUND_ELSE(40, 16);
  const GEdgeInsets insets =
      PBL_IF_ROUND_ELSE(GEdgeInsets(ACTION_BAR_WIDTH + x_margin),
                        GEdgeInsets(0, ACTION_BAR_WIDTH + x_margin, 0, x_margin));
  GRect frame = grect_inset(window_layer->bounds, insets);
  frame.origin.y = label_y_offset;
  return frame;
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  _Static_assert(offsetof(Window, layer) == 0, "");
  _Static_assert(offsetof(NumberWindow, window) == 0, "");
  NumberWindow *nw = (NumberWindow *)layer;

#ifdef CONFIG_TOUCH
  if (prv_get_picker_layer(nw)) {
    return;
  }
#endif

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &layer->bounds);
  graphics_context_set_text_color(ctx, GColorBlack);

  GRect frame = prv_get_text_frame(layer);
  frame.size.h = 54;

  TextLayoutExtended cached_label_layout = {};
  graphics_draw_text(ctx, nw->label, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     (TextLayout *)&cached_label_layout);

  char value_output_buffer[12];
  snprintf(value_output_buffer, ARRAY_LENGTH(value_output_buffer), "%" PRId32, nw->value);

  frame.origin.y += cached_label_layout.max_used_size.h;
#if PBL_RECT
  const int16_t output_offset_from_label = 15;
  frame.origin.y += output_offset_from_label;
#endif
  frame.size.h = 48;

  graphics_draw_text(ctx, value_output_buffer, fonts_get_system_font(NUMBER_FONT_KEY), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void number_window_set_label(NumberWindow *nw, const char *label) {
  nw->label = label;
  update_output_value(nw);
}

void number_window_set_max(NumberWindow *nf, int32_t max) {
  nf->max_val = max;
  if (nf->value > max) {
    nf->value = max;
  }
  if (nf->min_val > max) {
    nf->min_val = max;
  }
  update_output_value(nf);
}

void number_window_set_min(NumberWindow *nf, int32_t min) {
  nf->min_val = min;
  if (nf->value < min) {
    nf->value = min;
  }
  if (nf->max_val < min) {
    nf->max_val = min;
  }
  update_output_value(nf);
}

void number_window_set_value(NumberWindow *nf, int32_t value) {
  nf->value = value;
  if (nf->value > nf->max_val) {
    nf->value = nf->max_val;
  }
  if (nf->value < nf->min_val) {
    nf->value = nf->min_val;
  }
  update_output_value(nf);
}

void number_window_set_step_size(NumberWindow *nf, int32_t step) {
  nf->step_size = step;
  update_output_value(nf);
}

int32_t number_window_get_value(const NumberWindow *nf) {
  return nf->value;
}

static void number_window_load(NumberWindow *nw) {
  ActionBarLayer *action_bar = &nw->action_bar;
  action_bar_layer_set_context(action_bar, nw);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, &s_bar_icon_up_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, &s_bar_icon_down_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, &s_bar_icon_check_bitmap);
  action_bar_layer_add_to_window(action_bar, &nw->window);
  action_bar_layer_set_click_config_provider(action_bar,
                                             (ClickConfigProvider)click_config_provider);
}

void number_window_init(NumberWindow *nw, const char *label, NumberWindowCallbacks callbacks,
                        void *callback_context) {
  *nw = (NumberWindow){
    .label = label,
    .value = 0,
    .max_val = INT_MAX,
    .min_val = INT_MIN,
    .step_size = 1,
    .callbacks = callbacks,
    .callback_context = callback_context
  };

  window_init(&nw->window, WINDOW_NAME(label));
  window_set_window_handlers(&nw->window, &(WindowHandlers){
                                            .load = (WindowHandler)number_window_load,
                                          });
  layer_set_update_proc(&nw->window.layer, prv_update_proc);

  ActionBarLayer *action_bar = &nw->action_bar;
  action_bar_layer_init(action_bar);

#ifdef CONFIG_TOUCH
  ValuePickerLayer *picker_layer = applib_malloc(sizeof(*picker_layer));
  nw->private_data = picker_layer;
  if (picker_layer) {
    ValuePickerStyle style = *value_picker_layer_default_style();
    style.value_font_key = NUMBER_FONT_KEY;
    const ValuePickerContent content = prv_get_picker_content(nw);
    const GRect frame = GRect(0, 0, nw->window.layer.bounds.size.w - ACTION_BAR_WIDTH,
                              nw->window.layer.bounds.size.h);
    value_picker_layer_init(picker_layer, &frame, &content, &style,
                            (ValuePickerLayerCallbacks){
                              .changed = prv_changed,
                              .can_step = prv_can_step,
                            },
                            nw);
    layer_add_child(&nw->window.layer, &picker_layer->layer);
    value_picker_layer_enable_touch(picker_layer);
    window_set_background_color(&nw->window, style.background_color);
  }
#endif
}

NumberWindow *number_window_create(const char *label, NumberWindowCallbacks callbacks,
                                   void *callback_context) {
  NumberWindow *window = applib_type_malloc(NumberWindow);
  if (window) {
    number_window_init(window, label, callbacks, callback_context);
  }
  return window;
}

static void number_window_deinit(NumberWindow *number_window) {
#ifdef CONFIG_TOUCH
  ValuePickerLayer *picker_layer = prv_get_picker_layer(number_window);
  if (picker_layer) {
    value_picker_layer_deinit(picker_layer);
    applib_free(picker_layer);
    number_window->private_data = NULL;
  }
#endif
  action_bar_layer_deinit(&number_window->action_bar);
  window_deinit(&number_window->window);
}

void number_window_destroy(NumberWindow *number_window) {
  if (number_window == NULL) {
    return;
  }
  number_window_deinit(number_window);
  applib_free(number_window);
}

Window *number_window_get_window(NumberWindow *numberwindow) {
  return (&numberwindow->window);
}
