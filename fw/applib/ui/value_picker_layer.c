/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "value_picker_layer.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "board/display.h"

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

const ValuePickerStyle *value_picker_layer_default_style(void) {
  static const ValuePickerStyle s_style = {
    .background_color = GColorWhite,
    .accent_color = GColorBlack,
    .value_color = GColorBlack,
    .metadata_color = GColorBlack,
    .neighbor_color = GColorDarkGray,
    .value_font_key = FONT_KEY_BITHAM_34_MEDIUM_NUMBERS,
    .unit_font_key = FONT_KEY_GOTHIC_18_BOLD,
    .neighbor_font_key = FONT_KEY_GOTHIC_24_BOLD,
    .title_y = TITLE_Y,
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
                              const ValuePickerStyle *style, int64_t value) {
  if (value < content->min_value || value > content->max_value) {
    return;
  }
  char text[16];
  prv_format(content, text, sizeof(text), (int32_t)value);
  graphics_draw_text(ctx, text, fonts_get_system_font(style->neighbor_font_key), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_current_value(GContext *ctx, const GRect *content_bounds,
                                   const ValuePickerContent *content, const ValuePickerStyle *style,
                                   int16_t y) {
  char value[16];
  prv_format(content, value, sizeof(value), content->value);
  const char *unit = content->unit ? content->unit : "";
  GFont value_font = fonts_get_system_font(style->value_font_key);
  GFont unit_font = fonts_get_system_font(style->unit_font_key);
  const int16_t unit_y_offset = (unit_font == value_font) ? 0 : 16;
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
    graphics_draw_text(
        ctx, unit, unit_font,
        GRect(value_x + value_width + spacing, y + unit_y_offset, unit_width, 48 - unit_y_offset),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void prv_draw(GContext *ctx, const GRect *bounds, const ValuePickerContent *content,
                     const ValuePickerStyle *style) {
  graphics_context_set_fill_color(ctx, style->background_color);
  graphics_fill_rect(ctx, bounds);

  const GRect content_bounds = GRect(0, 0, bounds->size.w, bounds->size.h);
  GRect frame = GRect(0, style->title_y, content_bounds.size.w, 30);
  if (content->title) {
    graphics_context_set_text_color(ctx, style->metadata_color);
    graphics_draw_text(ctx, content->title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, style->neighbor_color);
  frame.origin.y = TOP_Y;
  frame.size.h = 34;
  prv_draw_neighbor(ctx, frame, content, style, (int64_t)content->value + content->step);

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
  prv_draw_neighbor(ctx, frame, content, style, (int64_t)content->value - content->step);
}

static int32_t prv_clamp_value(const ValuePickerContent *content, int64_t value) {
  if (value < content->min_value) {
    return content->min_value;
  }
  if (value > content->max_value) {
    return content->max_value;
  }
  return value;
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  _Static_assert(offsetof(ValuePickerLayer, layer) == 0, "");
  ValuePickerLayer *picker_layer = (ValuePickerLayer *)layer;
  prv_draw(ctx, &layer->bounds, &picker_layer->content, &picker_layer->style);
}

bool value_picker_layer_step(ValuePickerLayer *picker_layer, int direction) {
  ValuePickerContent *content = &picker_layer->content;
  if (picker_layer->callbacks.can_step &&
      !picker_layer->callbacks.can_step(picker_layer, direction, picker_layer->callback_context)) {
    return false;
  }
  const int32_t value =
      prv_clamp_value(content, (int64_t)content->value + direction * (int64_t)content->step);
  if (value == content->value) {
    return false;
  }
  content->value = value;
  layer_mark_dirty(&picker_layer->layer);
  if (picker_layer->callbacks.changed) {
    picker_layer->callbacks.changed(picker_layer, direction, picker_layer->callback_context);
  }
  return true;
}

static void prv_touch_step(int direction, void *context) {
  value_picker_layer_step(context, direction);
}

void value_picker_layer_init(ValuePickerLayer *picker_layer, const GRect *frame,
                             const ValuePickerContent *content, const ValuePickerStyle *style,
                             ValuePickerLayerCallbacks callbacks, void *callback_context) {
  *picker_layer = (ValuePickerLayer){
    .content = *content,
    .style = style ? *style : *value_picker_layer_default_style(),
    .callbacks = callbacks,
    .callback_context = callback_context,
  };
  picker_layer->content.value = prv_clamp_value(&picker_layer->content, content->value);
  layer_init(&picker_layer->layer, frame);
  layer_set_update_proc(&picker_layer->layer, prv_update_proc);
}

void value_picker_layer_enable_touch(ValuePickerLayer *picker_layer) {
  if (picker_layer->touch_enabled) {
    return;
  }
  picker_touch_init(&picker_layer->touch, &picker_layer->layer,
                    (PickerTouchCallbacks){
                      .step = prv_touch_step,
                    },
                    picker_layer);
  picker_layer->touch_enabled = true;
}

void value_picker_layer_disable_touch(ValuePickerLayer *picker_layer) {
  if (!picker_layer->touch_enabled) {
    return;
  }
  picker_touch_deinit(&picker_layer->touch);
  picker_layer->touch_enabled = false;
}

void value_picker_layer_deinit(ValuePickerLayer *picker_layer) {
  value_picker_layer_disable_touch(picker_layer);
  layer_deinit(&picker_layer->layer);
}

void value_picker_layer_set_content(ValuePickerLayer *picker_layer,
                                    const ValuePickerContent *content) {
  picker_layer->content = *content;
  picker_layer->content.value = prv_clamp_value(&picker_layer->content, content->value);
  layer_mark_dirty(&picker_layer->layer);
}

int32_t value_picker_layer_get_value(const ValuePickerLayer *picker_layer) {
  return picker_layer->content.value;
}
