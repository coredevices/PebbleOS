/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weight_summary_card.h"

#include "weight_detail_card.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/app_window_stack.h"
#include "board/display.h"
#include "pbl/drivers/rtc.h"
#include "pbl/services/activity/health_util.h"
#include "pbl/services/i18n/i18n.h"
#include "util/time/time.h"

#include <stdio.h>

#define HEALTH_Y_OFFSET ((DISP_ROWS - LEGACY_2X_DISP_ROWS) / 2)
#define CARD_BACKGROUND_COLOR PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite)
#define CARD_ICON_COLOR       PBL_IF_COLOR_ELSE(GColorTiffanyBlue, GColorBlack)
#define CARD_VALUE_COLOR      PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack)
#define CARD_METADATA_COLOR   PBL_IF_COLOR_ELSE(GColorMidnightGreen, GColorBlack)

typedef struct {
  HealthData *health_data;
} WeightSummaryData;

static void prv_draw_scale_icon(GContext *ctx, const GRect *bounds) {
  graphics_context_set_stroke_color(ctx, CARD_ICON_COLOR);
  graphics_context_set_stroke_width(ctx, 10);
  const GRect frame = GRect((bounds->size.w - 84) / 2, 12 + HEALTH_Y_OFFSET, 84, 84);
  graphics_draw_round_rect(ctx, &frame, 22);

  const GRect dial = GRect((bounds->size.w - 38) / 2, 36 + HEALTH_Y_OFFSET, 38, 22);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_round_rect(ctx, &dial, 5);

  const int16_t center_x = dial.origin.x + dial.size.w / 2;
  const int16_t dial_y = dial.origin.y + 11;
  graphics_draw_line(ctx, GPoint(dial.origin.x + 5, dial_y - 3),
                     GPoint(dial.origin.x + 7, dial_y - 1));
  graphics_draw_line(ctx, GPoint(dial.origin.x + 11, dial.origin.y + 4),
                     GPoint(dial.origin.x + 12, dial.origin.y + 7));
  graphics_draw_line(ctx, GPoint(center_x, dial.origin.y + 3),
                     GPoint(center_x, dial.origin.y + 6));
  graphics_draw_line(ctx, GPoint(dial.origin.x + 25, dial.origin.y + 4),
                     GPoint(dial.origin.x + 24, dial.origin.y + 7));
  graphics_draw_line(ctx, GPoint(dial.origin.x + 31, dial_y - 3),
                     GPoint(dial.origin.x + 29, dial_y - 1));
  graphics_draw_line(ctx, GPoint(center_x, dial_y + 3), GPoint(center_x + 7, dial_y - 2));
}

static void prv_draw_value(GContext *ctx, Layer *layer, WeightSummaryData *data) {
  const ActivityWeightSample *samples;
  const size_t sample_count = health_data_weight_get_samples(data->health_data, &samples);
  (void)samples;
  const uint16_t weight_dag = sample_count ? samples[0].weight_dag
                                           : health_data_weight_get_profile_dag(data->health_data);

  char value[16];
  health_util_format_weight(value, sizeof(value), weight_dag);
  const char *unit = i18n_get(health_util_get_weight_unit(), layer);
  GFont value_font = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
  GFont unit_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const GRect measure_frame = GRect(0, 0, layer->bounds.size.w, 48);
  const int16_t value_width =
      app_graphics_text_layout_get_content_size(value, value_font, measure_frame,
                                                GTextOverflowModeTrailingEllipsis,
                                                GTextAlignmentLeft)
          .w;
  const int16_t unit_width =
      app_graphics_text_layout_get_content_size(unit, unit_font, measure_frame,
                                                GTextOverflowModeTrailingEllipsis,
                                                GTextAlignmentLeft)
          .w;
  const int16_t spacing = 4;
  const int16_t value_x = (layer->bounds.size.w - value_width) / 2;
  GRect value_frame = GRect(value_x, 101 + HEALTH_Y_OFFSET, value_width, 48);
  graphics_context_set_text_color(ctx, CARD_VALUE_COLOR);
  graphics_draw_text(ctx, value, value_font, value_frame, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  GRect unit_frame =
      GRect(value_x + value_width + spacing, 115 + HEALTH_Y_OFFSET, unit_width, 24);
  graphics_context_set_text_color(ctx, CARD_METADATA_COLOR);
  graphics_draw_text(ctx, unit, unit_font, unit_frame, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  char subtitle[24];
  const char *subtitle_text;
  if (sample_count > 0) {
    const time_t now = rtc_get_time();
    const time_t sample_time = (time_t)samples[0].utc_sec;
    const uint16_t elapsed_days =
        sample_time >= now ? 0 : time_util_get_day(now) - time_util_get_day(sample_time);
    if (elapsed_days == 0) {
      subtitle_text = i18n_get("Today", layer);
    } else if (elapsed_days == 1) {
      subtitle_text = i18n_get("1 day ago", layer);
    } else {
      snprintf(subtitle, sizeof(subtitle), i18n_get("%u days ago", layer),
               (unsigned int)elapsed_days);
      subtitle_text = subtitle;
    }
  } else {
    subtitle_text = i18n_get("PROFILE WEIGHT", layer);
  }
  GRect subtitle_frame = GRect(8, 151 + HEALTH_Y_OFFSET, layer->bounds.size.w - 16, 34);
  graphics_draw_text(ctx, subtitle_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     subtitle_frame,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  WeightSummaryData *data = layer_get_data(layer);
  graphics_context_set_fill_color(ctx, CARD_BACKGROUND_COLOR);
  graphics_fill_rect(ctx, &layer->bounds);
  prv_draw_scale_icon(ctx, &layer->bounds);
  prv_draw_value(ctx, layer, data);
}

static void prv_detail_unload(Window *window) {
  health_weight_detail_card_destroy(window);
}

Layer *health_weight_summary_card_create(HealthData *health_data) {
  Layer *layer = layer_create_with_data(GRectZero, sizeof(WeightSummaryData));
  WeightSummaryData *data = layer_get_data(layer);
  data->health_data = health_data;
  layer_set_update_proc(layer, prv_update_proc);
  return layer;
}

void health_weight_summary_card_select_click_handler(Layer *layer) {
  WeightSummaryData *data = layer_get_data(layer);
  Window *window = health_weight_detail_card_create(data->health_data);
  window_set_window_handlers(window, &(WindowHandlers){.unload = prv_detail_unload});
  app_window_stack_push(window, true);
}

void health_weight_summary_card_destroy(Layer *layer) {
  i18n_free_all(layer);
  layer_destroy(layer);
}

GColor health_weight_summary_card_get_bg_color(Layer *layer) {
  return CARD_BACKGROUND_COLOR;
}

bool health_weight_summary_show_select_indicator(Layer *layer) {
  return true;
}
