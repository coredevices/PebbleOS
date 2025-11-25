/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if PLATFORM_SPALDING

#include "settings_display_calibration.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "util/trig.h"
#include "applib/ui/window.h"
#include "applib/ui/window_stack.h"

#include "kernel/pbl_malloc.h"
#include "resource/resource_ids.auto.h"
#include "services/common/analytics/analytics.h"
#include "services/common/i18n/i18n.h"
#include "services/common/light.h"
#include "shell/prefs.h"
#include "system/passert.h"
#include "util/math.h"

static const int16_t MAX_OFFSET_MAGNITUDE = 10;

typedef enum {
  DisplayCalibrationState_X_Adjust,
  DisplayCalibrationState_Y_Adjust,
  DisplayCalibrationState_Confirm
} DisplayCalibrationState;

static const DisplayCalibrationState INITIAL_STATE = DisplayCalibrationState_X_Adjust;

typedef struct {
  Window window;
  Layer layer;

  DisplayCalibrationState state;
  GPoint offset;
  GBitmap arrow_down;
  GBitmap arrow_left;
  GBitmap arrow_up;
  GBitmap arrow_right;
} DisplayCalibrationData;

static void prv_draw_text(Layer *layer, GContext *ctx) {
  DisplayCalibrationData *data = window_get_user_data(layer_get_window(layer));
  graphics_context_set_text_color(ctx, GColorWhite);

  const char *titles[] = {
    [DisplayCalibrationState_X_Adjust] = i18n_noop("Horizontal Alignment"),
    [DisplayCalibrationState_Y_Adjust] = i18n_noop("Vertical Alignment"),
    [DisplayCalibrationState_Confirm] = i18n_noop("Confirm Alignment")
  };
  const char *instructions[] = {
    [DisplayCalibrationState_X_Adjust] = i18n_noop("Up/Down to adjust\nSelect to proceed"),
    [DisplayCalibrationState_Y_Adjust] = i18n_noop("Up/Down to adjust\nSelect to proceed"),
    [DisplayCalibrationState_Confirm] = i18n_noop("Select to confirm alignment changes")
  };

  const char *title_text = i18n_get(titles[data->state], data);
  const char *instruction_text = i18n_get(instructions[data->state], data);

  const GTextOverflowMode overflow_mode = GTextOverflowModeTrailingEllipsis;
  const GTextAlignment text_alignment = GTextAlignmentCenter;
  const GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const GFont instruction_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  const int16_t title_line_height = fonts_get_font_height(title_font);
  const int16_t text_margin_x = 16;
  const int16_t text_margin_y = 32;
  const GRect max_text_container_frame = grect_inset_internal(layer->bounds,
                                                              text_margin_x, text_margin_y);

  GRect title_frame = (GRect) {
    .size = GSize(max_text_container_frame.size.w, title_line_height)
  };
  GRect instruction_frame = (GRect) {
    .size = GSize(max_text_container_frame.size.w,
                  max_text_container_frame.size.h - title_line_height)
  };
  instruction_frame.size = graphics_text_layout_get_max_used_size(ctx, instruction_text,
                                                                   instruction_font,
                                                                   instruction_frame,
                                                                   overflow_mode, text_alignment,
                                                                   NULL);
  GRect text_container_frame = (GRect) {
    .size = GSize(max_text_container_frame.size.w, title_frame.size.h + instruction_frame.size.h)
  };

  const bool clips = true;
  grect_align(&text_container_frame, &max_text_container_frame, GAlignCenter, clips);
  grect_align(&title_frame, &text_container_frame, GAlignTop, clips);
  grect_align(&instruction_frame, &text_container_frame, GAlignBottom, clips);

  const int16_t title_vertical_adjust_px = -fonts_get_font_cap_offset(title_font);
  title_frame.origin.y += title_vertical_adjust_px;

  graphics_draw_text(ctx, title_text, title_font, title_frame,
                     overflow_mode, text_alignment, NULL);
  graphics_draw_text(ctx, instruction_text, instruction_font,
                     instruction_frame, overflow_mode, text_alignment, NULL);
}

void prv_draw_border_stripe(Layer *layer, GContext *ctx, GAlign alignment) {
  const int16_t stripe_inset = 6;
  const int16_t stripe_width = 2;

  const GRect *layer_bounds = &layer->bounds;
  const bool is_horizontal = ((alignment == GAlignTop) || (alignment == GAlignBottom));
  GRect rect = (GRect) {
    .size = (is_horizontal) ? GSize(layer_bounds->size.w, stripe_width)
                            : GSize(stripe_width, layer_bounds->size.h)
  };

  for (int i = stripe_inset - stripe_width; i >= -MAX_OFFSET_MAGNITUDE; i -= stripe_width) {
    // alternate yellow and red stripes
    graphics_context_set_stroke_color(ctx, (i % (2 * stripe_width)) ? GColorRed : GColorYellow);
    const GRect outer_bounds = grect_inset_internal(*layer_bounds, i, i);
    grect_align(&rect, &outer_bounds, alignment, false);
    graphics_draw_rect(ctx, &rect);
  }
}

static void prv_draw_border_stripes(Layer *layer, GContext *ctx) {
  DisplayCalibrationData *data = window_get_user_data(layer_get_window(layer));

  if ((data->state == DisplayCalibrationState_X_Adjust) ||
      (data->state == DisplayCalibrationState_Confirm)) {
    prv_draw_border_stripe(layer, ctx, GAlignLeft);
    prv_draw_border_stripe(layer, ctx, GAlignRight);
  }
  if ((data->state == DisplayCalibrationState_Y_Adjust) ||
      (data->state == DisplayCalibrationState_Confirm)) {
    prv_draw_border_stripe(layer, ctx, GAlignTop);
    prv_draw_border_stripe(layer, ctx, GAlignBottom);
  }
}

static void prv_draw_arrow(Layer *layer, GContext *ctx, GBitmap *arrow_bitmap, GAlign alignment) {
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  const int16_t margin = 8;
  const GRect bounds = grect_inset_internal(layer->bounds, margin, margin);
  GRect box = arrow_bitmap->bounds;
  grect_align(&box, &bounds, alignment, true);
  graphics_draw_bitmap_in_rect(ctx, arrow_bitmap, &box);
}

static void prv_draw_arrows(Layer *layer, GContext *ctx) {
  DisplayCalibrationData *data = window_get_user_data(layer_get_window(layer));

  switch (data->state) {
    case DisplayCalibrationState_X_Adjust:
      if (data->offset.x > -MAX_OFFSET_MAGNITUDE) {
        prv_draw_arrow(layer, ctx, &data->arrow_left, GAlignLeft);
      }
      if (data->offset.x < MAX_OFFSET_MAGNITUDE) {
        prv_draw_arrow(layer, ctx, &data->arrow_right, GAlignRight);
      }
      break;
    case DisplayCalibrationState_Y_Adjust:
      if (data->offset.y > -MAX_OFFSET_MAGNITUDE) {
        prv_draw_arrow(layer, ctx, &data->arrow_up, GAlignTop);
      }
      if (data->offset.y < MAX_OFFSET_MAGNITUDE) {
        prv_draw_arrow(layer, ctx, &data->arrow_down, GAlignBottom);
      }
      break;
    case DisplayCalibrationState_Confirm:
      break;
  }
}

static void prv_layer_update_proc(Layer *layer, GContext *ctx) {
  DisplayCalibrationData *data = window_get_user_data(layer_get_window(layer));

  ctx->draw_state.drawing_box.origin.x += data->offset.x;
  ctx->draw_state.drawing_box.origin.y += data->offset.y;

  prv_draw_border_stripes(layer, ctx);
  prv_draw_text(layer, ctx);
  prv_draw_arrows(layer, ctx);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  DisplayCalibrationData *data = context;
  if (data->state == DisplayCalibrationState_Confirm) {
    // set a new user offset
    shell_prefs_set_display_offset(data->offset);
    analytics_inc(ANALYTICS_DEVICE_METRIC_DISPLAY_OFFSET_MODIFIED_COUNT, AnalyticsClient_System);
    window_stack_remove(&data->window, true /* animated */);
    return;
  }

  data->state++;
  layer_mark_dirty(&data->window.layer);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  DisplayCalibrationData *data = context;
  if (data->state == INITIAL_STATE) {
    // exit the calibration window without changing the prefs
    window_stack_remove(&data->window, true /* animated */);
    return;
  }

  data->state--;
  layer_mark_dirty(&data->window.layer);
}

static void prv_up_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  DisplayCalibrationData *data = context;

  int to_add = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP) ? -1 : 1;

  switch (data->state) {
    case DisplayCalibrationState_X_Adjust:
      data->offset.x = CLIP(data->offset.x + to_add, -MAX_OFFSET_MAGNITUDE, MAX_OFFSET_MAGNITUDE);
      break;
    case DisplayCalibrationState_Y_Adjust:
      data->offset.y = CLIP(data->offset.y + to_add, -MAX_OFFSET_MAGNITUDE, MAX_OFFSET_MAGNITUDE);
      break;
    case DisplayCalibrationState_Confirm:
      break;
  }

  layer_mark_dirty(&data->window.layer);
}

static void prv_config_provider(void *data) {
  const uint16_t interval_ms = 50;
  window_single_repeating_click_subscribe(BUTTON_ID_UP, interval_ms, prv_up_down_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, interval_ms, prv_up_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

void prv_window_unload(struct Window *window) {
  DisplayCalibrationData *data = window_get_user_data(window);

  light_reset_user_controlled();

  gbitmap_deinit(&data->arrow_down);
  gbitmap_deinit(&data->arrow_left);
  gbitmap_deinit(&data->arrow_up);
  gbitmap_deinit(&data->arrow_right);

  // reinitialize display offset now that values may have changed
  shell_prefs_display_offset_init();

  layer_deinit(&data->layer);
  i18n_free_all(data);
  task_free(data);
}

static void prv_init_arrow_bitmap(GBitmap *bitmap, uint32_t resource_id) {
  gbitmap_init_with_resource(bitmap, resource_id);

  // tint cyan
  PBL_ASSERTN(bitmap->info.format == GBitmapFormat2BitPalette);
  unsigned int palette_size = gbitmap_get_palette_size(bitmap->info.format);
  GColor *palette = task_zalloc_check(sizeof(GColor) * palette_size);
  for (unsigned int i = 0; i < palette_size; i++) {
    palette[i].argb = (bitmap->palette[i].argb & 0xc0) | (GColorCyan.argb & 0x3f);
  }
  gbitmap_set_palette(bitmap, palette, true /* free on destroy */);
}

void settings_display_calibration_push(WindowStack *window_stack) {
  DisplayCalibrationData *data = task_zalloc_check(sizeof(DisplayCalibrationData));
  *data = (DisplayCalibrationData) {
    .offset = shell_prefs_get_display_offset()
  };

  shell_prefs_set_should_prompt_display_calibration(false);
  display_set_offset(GPointZero);
  light_enable(true);

  Window *window = &data->window;
  window_init(window, "SettingsDisplayCalibration");
  window_set_click_config_provider_with_context(window, prv_config_provider, data);
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) { .unload = prv_window_unload });
  window_set_background_color(window, GColorBlack);

  Layer *root_layer = window_get_root_layer(window);
  Layer *layer = &data->layer;
  layer_init(layer, &root_layer->bounds);
  layer_set_update_proc(layer, prv_layer_update_proc);
  layer_add_child(root_layer, layer);

  prv_init_arrow_bitmap(&data->arrow_down, RESOURCE_ID_ARROW_DOWN);
  prv_init_arrow_bitmap(&data->arrow_left, RESOURCE_ID_ARROW_LEFT);
  prv_init_arrow_bitmap(&data->arrow_up, RESOURCE_ID_ARROW_UP);
  prv_init_arrow_bitmap(&data->arrow_right, RESOURCE_ID_ARROW_RIGHT);

  window_stack_push(window_stack, window, true /* animated */);
}

#endif
