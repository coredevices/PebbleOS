/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "value_picker.h"

#include "action_bar_layer.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "board/display.h"

#ifdef CONFIG_TOUCH
#include "applib/ui/recognizer/recognizer_manager.h"
#include "applib/ui/recognizer/touch_nav.h"
#include "kernel/pebble_tasks.h"

struct TouchNavState *app_state_get_touch_nav_state(void);
struct TouchNavState *modal_manager_get_touch_nav_state(void);
#endif

#include <inttypes.h>
#include <stdio.h>

#define Y_OFFSET ((DISP_ROWS - LEGACY_2X_DISP_ROWS) / 2)

#if DISP_ROWS <= LEGACY_2X_DISP_ROWS
#define TITLE_Y   4
#define TOP_Y     32
#define CURRENT_Y 68
#define BOTTOM_Y  120
#else
#define TITLE_Y   (16 + Y_OFFSET / 3)
#define TOP_Y     (30 + Y_OFFSET)
#define CURRENT_Y (72 + Y_OFFSET)
#define BOTTOM_Y  (124 + Y_OFFSET)
#endif

#define TOUCH_PIXELS_PER_STEP 18

const ValuePickerStyle *value_picker_default_style(void) {
  static const ValuePickerStyle s_style = {
    .background_color = GColorWhite,
    .accent_color = GColorBlack,
    .value_color = GColorBlack,
    .metadata_color = GColorBlack,
    .neighbor_color = GColorDarkGray,
    .value_font_key = FONT_KEY_BITHAM_34_MEDIUM_NUMBERS,
  };
  return &s_style;
}

static void prv_format(const ValuePickerContent *content, char *buffer, size_t buffer_size,
                       int32_t value) {
  if (content->format) {
    content->format(buffer, buffer_size, value);
  } else {
    snprintf(buffer, buffer_size, "%" PRId32, value);
  }
}

static void prv_draw_neighbor(GContext *ctx, GRect frame, const ValuePickerContent *content,
                              int64_t value) {
  if (value < content->min_value || value > content->max_value) {
    return;
  }
  char text[16];
  prv_format(content, text, sizeof(text), (int32_t)value);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_current_value(GContext *ctx, const GRect *content_bounds,
                                   const ValuePickerContent *content, const ValuePickerStyle *style,
                                   int16_t y) {
  char value[16];
  prv_format(content, value, sizeof(value), content->value);
  const char *unit = content->unit ? content->unit : "";
  GFont value_font = fonts_get_system_font(style->value_font_key);
  GFont unit_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const GRect measure_frame = GRect(0, 0, content_bounds->size.w, 48);
  const int16_t value_width =
      app_graphics_text_layout_get_content_size(
          value, value_font, measure_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft)
          .w;
  const int16_t unit_width =
      app_graphics_text_layout_get_content_size(
          unit, unit_font, measure_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft)
          .w;
  const int16_t spacing = unit_width ? 4 : 0;
  const int16_t group_width = value_width + spacing + unit_width;
  const int16_t value_x = content_bounds->origin.x + (content_bounds->size.w - group_width) / 2;
  graphics_context_set_text_color(ctx, style->value_color);
  graphics_draw_text(ctx, value, value_font, GRect(value_x, y, value_width, 48),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (unit_width) {
    graphics_context_set_text_color(ctx, style->metadata_color);
    graphics_draw_text(ctx, unit, unit_font,
                       GRect(value_x + value_width + spacing, y + 16, unit_width, 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

void value_picker_draw(GContext *ctx, const GRect *bounds, const ValuePickerContent *content,
                       const ValuePickerStyle *style) {
  graphics_context_set_fill_color(ctx, style->background_color);
  graphics_fill_rect(ctx, bounds);

  const GRect content_bounds = GRect(0, 0, bounds->size.w - ACTION_BAR_WIDTH, bounds->size.h);
  GRect frame = GRect(0, TITLE_Y, content_bounds.size.w, 30);
  if (content->title) {
    graphics_context_set_text_color(ctx, style->metadata_color);
    graphics_draw_text(ctx, content->title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, style->neighbor_color);
  frame.origin.y = TOP_Y;
  frame.size.h = 34;
  prv_draw_neighbor(ctx, frame, content, (int64_t)content->value + content->step);

  const int16_t horizontal_inset = PBL_IF_ROUND_ELSE(22, 9);
  GRect selection_frame =
      GRect(horizontal_inset, CURRENT_Y - 4, content_bounds.size.w - 2 * horizontal_inset, 54);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_round_rect(ctx, &selection_frame, 10, GCornersAll);
  graphics_context_set_stroke_color(ctx, style->accent_color);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_round_rect(ctx, &selection_frame, 10);
  prv_draw_current_value(ctx, &content_bounds, content, style, CURRENT_Y);

  graphics_context_set_text_color(ctx, style->neighbor_color);
  frame.origin.y = BOTTOM_Y;
  prv_draw_neighbor(ctx, frame, content, (int64_t)content->value - content->step);
}

int16_t value_picker_steps_from_drag(int16_t delta_y) {
  const int16_t half_step = TOUCH_PIXELS_PER_STEP / 2;
  return (delta_y >= 0 ? delta_y + half_step : delta_y - half_step) / TOUCH_PIXELS_PER_STEP;
}

#ifdef CONFIG_TOUCH
_Static_assert(sizeof(((ValuePickerTouch *)0)->touch_nav_node) == sizeof(TouchNavWidgetNode),
               "ValuePickerTouch touch_nav_node must match TouchNavWidgetNode layout");

static TouchNavState *prv_task_touch_nav_state(void) {
  return (pebble_task_get_current() == PebbleTask_App) ? app_state_get_touch_nav_state()
                                                       : modal_manager_get_touch_nav_state();
}

static void prv_apply_drag(ValuePickerTouch *touch, int16_t delta_y) {
  const int16_t steps = value_picker_steps_from_drag(delta_y);
  while (touch->applied_steps != steps) {
    const int direction = (steps > touch->applied_steps) ? 1 : -1;
    touch->step(direction, touch->context);
    touch->applied_steps += direction;
  }
}

static void prv_ops_pan_started(void *w) {
  ((ValuePickerTouch *)w)->applied_steps = 0;
}

static GPointReturn prv_ops_get_base_offset(void *w) {
  return GPointZero;
}

static void prv_ops_pan_update(void *w, GPoint base, GPoint delta) {
  prv_apply_drag(w, delta.y);
}

static void prv_ops_pan_snap(void *w, GPoint base, GPoint final_delta, GPoint velocity) {
  prv_apply_drag(w, final_delta.y);
}

static void prv_ops_pan_cancel(void *w) {
}

static void prv_emit_button(ButtonId button) {
  const TouchNavState *state = prv_task_touch_nav_state();
  if (!state || !state->ops || button >= NUM_BUTTONS) {
    return;
  }
  const TouchNavOps *ops = state->ops;
  if (ops->is_animating && ops->is_animating(ops->ctx)) {
    return;
  }
  if (button == BUTTON_ID_BACK && !(ops->top_overrides_back && ops->top_overrides_back(ops->ctx))) {
    if (ops->pop_top) {
      ops->pop_top(ops->ctx);
    }
  } else if (ops->emit_button) {
    ops->emit_button(ops->ctx, button);
  }
}

static void prv_ops_swipe(void *w, SwipeDirection direction) {
  prv_emit_button(direction == SwipeDirection_Left ? BUTTON_ID_SELECT : BUTTON_ID_BACK);
}

static const TouchNavWidgetOps s_value_picker_touch_nav_ops = {
  .pan_started = prv_ops_pan_started,
  .get_base_offset = prv_ops_get_base_offset,
  .pan_update = prv_ops_pan_update,
  .pan_snap = prv_ops_pan_snap,
  .pan_cancel = prv_ops_pan_cancel,
  .swipe = prv_ops_swipe,
};
#endif

void value_picker_touch_init(ValuePickerTouch *touch, Layer *parent, ValuePickerStepHandler step,
                             void *context) {
  *touch = (ValuePickerTouch){
    .step = step,
    .context = context,
  };
  layer_init(&touch->layer, &parent->bounds);
  layer_add_child(parent, &touch->layer);
#ifdef CONFIG_TOUCH
  TouchNavState *state = prv_task_touch_nav_state();
  if (state && state->manager) {
    touch_nav_registry_add(state, TouchNavWidgetType_Scroll,
                           (TouchNavWidgetNode *)&touch->touch_nav_node, &touch->layer,
                           &s_value_picker_touch_nav_ops, touch);
  }
#endif
}

void value_picker_touch_deinit(ValuePickerTouch *touch) {
#ifdef CONFIG_TOUCH
  TouchNavState *state = prv_task_touch_nav_state();
  if (state) {
    const bool was_target = state->latched_target && state->latched_target->widget == touch;
    touch_nav_registry_remove(state, TouchNavWidgetType_Scroll,
                              (TouchNavWidgetNode *)&touch->touch_nav_node);
    if (was_target && state->manager) {
      recognizer_manager_cancel_and_reset(state->manager);
    }
  }
#endif
  layer_remove_from_parent(&touch->layer);
  layer_deinit(&touch->layer);
}
