/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weight_detail_card.h"

#include "weight_entry_window.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/confirmation_dialog.h"
#include "applib/ui/scroll_layer.h"
#include "kernel/pbl_malloc.h"
#include "pbl/drivers/rtc.h"
#include "pbl/services/activity/health_util.h"
#include "pbl/services/clock.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/util/math.h"
#include "resource/resource_ids.auto.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define SUMMARY_ROW_HEIGHT PBL_IF_ROUND_ELSE(200, 166)
#define SAMPLE_ROW_HEIGHT 40
#define WEIGHT_DAY_SEC (24 * 60 * 60)
#define WEIGHT_GRAPH_SHORT_DAYS 3
#define WEIGHT_GRAPH_WEEK_DAYS 7
#define WEIGHT_TREND_CURRENT_WEIGHT 2
#define WEIGHT_TREND_PREVIOUS_WEIGHT 3

typedef struct {
  Window window;
  ScrollLayer scroll_layer;
  Layer *content_layer;
  HealthData *health_data;
  char delete_text[64];
} WeightDetailCard;

typedef struct {
  time_t utc_sec;
  int32_t value;
  int32_t trend;
} WeightGraphPoint;

typedef struct {
  time_t period_start;
  uint16_t oldest_weight_dag;
  size_t actual_count;
  uint8_t period_days;
  bool has_older_baseline;
} WeightGraphRange;

static void prv_format_weight_with_unit(char *buffer, size_t buffer_size, uint16_t weight_dag,
                                        void *i18n_owner) {
  char value[16];
  health_util_format_weight(value, sizeof(value), weight_dag);
  snprintf(buffer, buffer_size, "%s %s", value,
           i18n_get(health_util_get_weight_unit(), i18n_owner));
}

static void prv_format_delta(char *buffer, size_t buffer_size, uint16_t current_weight_dag,
                             uint16_t baseline_weight_dag, void *i18n_owner) {
  const int32_t current = health_util_weight_dag_to_tenths(current_weight_dag);
  const int32_t baseline = health_util_weight_dag_to_tenths(baseline_weight_dag);
  const int32_t tenths = current - baseline;
  const int32_t magnitude = ABS(tenths);
  snprintf(buffer, buffer_size, "%s%" PRId32 ".%" PRId32 " %s", tenths < 0 ? "-" : "+",
           magnitude / 10, magnitude % 10,
           i18n_get(health_util_get_weight_unit(), i18n_owner));
}

static WeightGraphRange prv_get_graph_range(WeightDetailCard *card, time_t now) {
  const ActivityWeightSample *samples;
  const size_t sample_count = health_data_weight_get_samples(card->health_data, &samples);
  const time_t month_start = now - ACTIVITY_WEIGHT_HISTORY_DAYS * WEIGHT_DAY_SEC;
  time_t oldest_recent = now;
  WeightGraphRange range = {};

  for (size_t i = 0; i < sample_count; i++) {
    const time_t sample_time = samples[i].utc_sec;
    if (sample_time >= month_start && sample_time <= now) {
      oldest_recent = sample_time;
      range.oldest_weight_dag = samples[i].weight_dag;
      range.actual_count++;
    }
  }

  if (range.actual_count == 0) {
    return range;
  }

  const time_t short_start = now - WEIGHT_GRAPH_SHORT_DAYS * WEIGHT_DAY_SEC;
  const time_t week_start = now - WEIGHT_GRAPH_WEEK_DAYS * WEIGHT_DAY_SEC;
  if (oldest_recent >= short_start) {
    range.period_days = WEIGHT_GRAPH_SHORT_DAYS;
  } else if (oldest_recent >= week_start) {
    range.period_days = WEIGHT_GRAPH_WEEK_DAYS;
  } else {
    range.period_days = ACTIVITY_WEIGHT_HISTORY_DAYS;
  }
  range.period_start = now - range.period_days * WEIGHT_DAY_SEC;

  for (size_t i = 0; i < sample_count; i++) {
    if ((time_t)samples[i].utc_sec < range.period_start) {
      range.has_older_baseline = true;
      break;
    }
  }
  return range;
}

static size_t prv_collect_graph_points(WeightDetailCard *card, const WeightGraphRange *range,
                                       time_t now, WeightGraphPoint *points) {
  const ActivityWeightSample *samples;
  const size_t sample_count = health_data_weight_get_samples(card->health_data, &samples);
  size_t count = 0;

  if (range->has_older_baseline) {
    for (size_t i = 0; i < sample_count; i++) {
      if ((time_t)samples[i].utc_sec < range->period_start) {
        points[count++] = (WeightGraphPoint){
          .utc_sec = range->period_start,
          .value = health_util_weight_dag_to_tenths(samples[i].weight_dag),
        };
        break;
      }
    }
  }

  for (size_t i = sample_count; i > 0; i--) {
    const ActivityWeightSample *sample = &samples[i - 1];
    const time_t sample_time = sample->utc_sec;
    if (sample_time >= range->period_start && sample_time <= now) {
      points[count++] = (WeightGraphPoint){
        .utc_sec = sample_time,
        .value = health_util_weight_dag_to_tenths(sample->weight_dag),
      };
    }
  }
  return count;
}

static GPoint prv_graph_point(const GRect *frame, const WeightGraphPoint *point,
                              time_t period_start, int32_t min_value, int32_t max_value,
                              uint32_t period_sec, bool use_trend) {
  const int32_t value = use_trend ? point->trend : point->value;
  const int16_t x =
      frame->origin.x +
      ((int64_t)(point->utc_sec - period_start) * (frame->size.w - 1)) /
          period_sec;
  const int16_t y =
      frame->origin.y + frame->size.h - 1 -
      ((value - min_value) * (frame->size.h - 1)) / (max_value - min_value);
  return GPoint(x, y);
}

static void prv_draw_point_line(GContext *ctx, const GRect *frame, WeightGraphPoint *points,
                                size_t count, time_t period_start, int32_t min_value,
                                int32_t max_value, uint32_t period_sec, bool use_trend, GColor color,
                                uint8_t stroke_width, uint8_t point_radius) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_width(ctx, stroke_width);

  GPoint previous = GPointZero;
  for (size_t i = 0; i < count; i++) {
    const GPoint point =
        prv_graph_point(frame, &points[i], period_start, min_value, max_value, period_sec,
                        use_trend);
    if (i > 0) {
      graphics_draw_line(ctx, previous, point);
    }
    graphics_fill_circle(ctx, point, point_radius);
    previous = point;
  }
}

static void prv_draw_graph(GContext *ctx, const GRect *frame, WeightDetailCard *card, time_t now,
                           const WeightGraphRange *range) {
  WeightGraphPoint points[ACTIVITY_WEIGHT_RECENT_MAX];
  const size_t count = prv_collect_graph_points(card, range, now, points);
  if (count == 0) {
    return;
  }

  int32_t min_value = points[0].value;
  int32_t max_value = points[0].value;
  for (size_t i = 1; i < count; i++) {
    min_value = MIN(min_value, points[i].value);
    max_value = MAX(max_value, points[i].value);
  }
  const int32_t padding = MAX((max_value - min_value) / 5, 1);
  min_value -= padding;
  max_value += padding;

  const uint32_t period_sec = range->period_days * WEIGHT_DAY_SEC;
  const GColor trend_color = PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack);
  if (count < 3) {
    prv_draw_point_line(ctx, frame, points, count, range->period_start, min_value, max_value,
                        period_sec, false, trend_color, 2, 2);
    if (range->has_older_baseline) {
      const GPoint baseline = prv_graph_point(frame, &points[0], range->period_start, min_value,
                                              max_value, period_sec, false);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, baseline, 2);
      graphics_context_set_stroke_color(ctx, trend_color);
      graphics_draw_circle(ctx, baseline, 2);
    }
    return;
  }

  points[0].trend = points[0].value;
  for (size_t i = 1; i < count; i++) {
    points[i].trend =
        (WEIGHT_TREND_CURRENT_WEIGHT * points[i].value +
         WEIGHT_TREND_PREVIOUS_WEIGHT * points[i - 1].trend) /
        (WEIGHT_TREND_CURRENT_WEIGHT + WEIGHT_TREND_PREVIOUS_WEIGHT);
  }

  const GColor raw_color = PBL_IF_COLOR_ELSE(GColorTiffanyBlue, GColorBlack);
  prv_draw_point_line(ctx, frame, points, count, range->period_start, min_value, max_value,
                      period_sec, false, raw_color, 1, 1);
  prv_draw_point_line(ctx, frame, points, count, range->period_start, min_value, max_value,
                      period_sec, true, trend_color, 2, 1);
  const GPoint latest = prv_graph_point(frame, &points[count - 1], range->period_start, min_value,
                                        max_value, period_sec, true);
  graphics_context_set_fill_color(ctx, trend_color);
  graphics_fill_circle(ctx, latest, 2);

  if (range->has_older_baseline) {
    const GPoint baseline = prv_graph_point(frame, &points[0], range->period_start, min_value,
                                            max_value, period_sec, false);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, baseline, 2);
    graphics_context_set_stroke_color(ctx, raw_color);
    graphics_draw_circle(ctx, baseline, 2);
  }
}

static void prv_draw_summary(GContext *ctx, Layer *layer, WeightDetailCard *card) {
  const ActivityWeightSample *samples;
  const size_t sample_count = health_data_weight_get_samples(card->health_data, &samples);
  const uint16_t current_weight =
      sample_count ? samples[0].weight_dag
                   : health_data_weight_get_profile_dag(card->health_data);
  graphics_context_set_text_color(ctx, GColorBlack);

  GRect frame = GRect(8, PBL_IF_ROUND_ELSE(25, 4), layer->bounds.size.w - 16, 24);
  graphics_draw_text(ctx, i18n_get("WEIGHT", card), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char current[24];
  prv_format_weight_with_unit(current, sizeof(current), current_weight, card);
  frame.origin.y += 21;
  frame.size.h = 38;
  graphics_draw_text(ctx, current, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  const time_t now = rtc_get_time();
  const WeightGraphRange graph_range = prv_get_graph_range(card, now);
  char status[64];
  if (sample_count == 0) {
    snprintf(status, sizeof(status), "%s\n%s", i18n_get("PROFILE WEIGHT", card),
             i18n_get("No weigh-ins yet", card));
  } else if (graph_range.actual_count == 0) {
    snprintf(status, sizeof(status), "%s", i18n_get("No recent entries", card));
  } else if (sample_count == 1) {
    snprintf(status, sizeof(status), "%s", i18n_get("No change yet", card));
  } else {
    char previous_delta[20];
    prv_format_delta(previous_delta, sizeof(previous_delta), samples[0].weight_dag,
                     samples[1].weight_dag, card);
    if (graph_range.actual_count >= 3) {
      char period_delta[20];
      prv_format_delta(period_delta, sizeof(period_delta), samples[0].weight_dag,
                       graph_range.oldest_weight_dag, card);
      snprintf(status, sizeof(status), i18n_get("%s last  %s / %ud", card), previous_delta,
               period_delta, (unsigned int)graph_range.period_days);
    } else {
      snprintf(status, sizeof(status), i18n_get("%s since last", card), previous_delta);
    }
  }
  frame.origin.y += 36;
  frame.size.h = 28;
  graphics_draw_text(ctx, status, fonts_get_system_font(FONT_KEY_GOTHIC_18), frame,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  if (graph_range.actual_count > 0) {
    const GRect graph_frame =
        GRect(PBL_IF_ROUND_ELSE(45, 25), frame.origin.y + 30,
              layer->bounds.size.w - PBL_IF_ROUND_ELSE(90, 50),
              PBL_IF_ROUND_ELSE(35, 36));
    prv_draw_graph(ctx, &graph_frame, card, now, &graph_range);

    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    char period[4];
    snprintf(period, sizeof(period), "%uD", (unsigned int)graph_range.period_days);
    const GRect period_frame =
        GRect(graph_frame.origin.x, grect_get_max_y(&graph_frame) - 1, 30, 16);
    graphics_draw_text(ctx, period,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14), period_frame,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    const GRect now_frame =
        GRect(grect_get_max_x(&graph_frame) - 30, period_frame.origin.y, 30, 16);
    graphics_draw_text(ctx, i18n_get("NOW", card),
                       fonts_get_system_font(FONT_KEY_GOTHIC_14), now_frame,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }

  const int16_t hint_y = SUMMARY_ROW_HEIGHT - PBL_IF_ROUND_ELSE(30, 24);
  GFont hint_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  if (sample_count > 1) {
    const int16_t inset = PBL_IF_ROUND_ELSE(24, 10);
    const int16_t half_width = (layer->bounds.size.w - 2 * inset) / 2;
    GRect select_frame = GRect(inset, hint_y, half_width, 22);
    graphics_draw_text(ctx, i18n_get("SELECT: LOG", card), hint_font, select_frame,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    GRect delete_frame = GRect(inset + half_width, hint_y, half_width, 22);
    graphics_draw_text(ctx, i18n_get("HOLD: DELETE", card), hint_font, delete_frame,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  } else {
    frame.origin.y = hint_y;
    frame.size.h = 22;
    graphics_draw_text(ctx, i18n_get("SELECT TO LOG", card), hint_font, frame,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void prv_draw_sample(GContext *ctx, Layer *layer, WeightDetailCard *card,
                            size_t sample_index, int16_t origin_y) {
  const ActivityWeightSample *samples;
  const size_t count = health_data_weight_get_samples(card->health_data, &samples);
  if (sample_index >= count) {
    return;
  }

  char weight[24];
  prv_format_weight_with_unit(weight, sizeof(weight), samples[sample_index].weight_dag, card);
  const time_t timestamp = samples[sample_index].utc_sec;
  char date[12];
  char time[12];
  clock_get_date(date, sizeof(date), timestamp);
  clock_copy_time_string_timestamp(time, sizeof(time), timestamp);
  char entry[56];
  snprintf(entry, sizeof(entry), "%s  %s, %s", weight, date, time);

  graphics_context_set_text_color(ctx, GColorBlack);
  const int16_t inset = PBL_IF_ROUND_ELSE(35, 10);
  const GTextAlignment alignment = PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft);
  const GRect entry_frame = GRect(inset, origin_y + 7, layer->bounds.size.w - 2 * inset, 26);
  graphics_draw_text(ctx, entry, fonts_get_system_font(FONT_KEY_GOTHIC_18), entry_frame,
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  const int16_t separator_inset = PBL_IF_ROUND_ELSE(35, 10);
  graphics_draw_line(ctx, GPoint(separator_inset, origin_y),
                     GPoint(layer->bounds.size.w - separator_inset, origin_y));
}

static void prv_content_update_proc(Layer *layer, GContext *ctx) {
  WeightDetailCard *card = *(WeightDetailCard **)layer_get_data(layer);
  prv_draw_summary(ctx, layer, card);

  const ActivityWeightSample *samples;
  const size_t count = health_data_weight_get_samples(card->health_data, &samples);
  for (size_t i = 0; i < count; i++) {
    prv_draw_sample(ctx, layer, card, i, SUMMARY_ROW_HEIGHT + i * SAMPLE_ROW_HEIGHT);
  }
}

static void prv_update_content_size(WeightDetailCard *card) {
  const ActivityWeightSample *samples;
  const size_t count = health_data_weight_get_samples(card->health_data, &samples);
  const int16_t history_height = SUMMARY_ROW_HEIGHT + (int16_t)count * SAMPLE_ROW_HEIGHT;
  const int16_t content_height =
      MAX(card->window.layer.bounds.size.h, history_height);
  const GSize content_size = GSize(card->window.layer.bounds.size.w, content_height);
  scroll_layer_set_content_size(&card->scroll_layer, content_size);
  scroll_layer_set_content_offset(&card->scroll_layer, GPointZero, false);
  layer_set_frame(card->content_layer, &(GRect){.size = content_size});
}

static void prv_weight_saved(uint16_t weight_dag, void *context) {
  WeightDetailCard *card = context;
  health_data_update_weight(card->health_data);
  prv_update_content_size(card);
  layer_mark_dirty(card->content_layer);
}

static void prv_delete_dialog_pop(ClickRecognizerRef recognizer, void *context) {
  confirmation_dialog_pop(context);
}

static void prv_delete_confirm(ClickRecognizerRef recognizer, void *context) {
  ConfirmationDialog *dialog = context;
  WeightDetailCard *card =
      actionable_dialog_get_user_data((ActionableDialog *)dialog);
  uint16_t new_weight_dag;
  if (activity_weight_history_remove_latest(rtc_get_time(), &new_weight_dag)) {
    activity_prefs_set_weight_dag(new_weight_dag);
    health_data_update_weight(card->health_data);
    prv_update_content_size(card);
    layer_mark_dirty(card->content_layer);
  }
  confirmation_dialog_pop(dialog);
}

static void prv_delete_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_delete_confirm);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_delete_dialog_pop);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_delete_dialog_pop);
}

static void prv_delete_latest(ClickRecognizerRef recognizer, void *context) {
  WeightDetailCard *card = context;
  const ActivityWeightSample *samples;
  const size_t count = health_data_weight_get_samples(card->health_data, &samples);
  if (count <= 1) {
    return;
  }

  char weight[24];
  char date[12];
  prv_format_weight_with_unit(weight, sizeof(weight), samples[0].weight_dag, card);
  clock_get_date(date, sizeof(date), samples[0].utc_sec);
  snprintf(card->delete_text, sizeof(card->delete_text), "%s\n%s  %s",
           i18n_get("Delete latest weight?", card), weight, date);

  ConfirmationDialog *dialog = confirmation_dialog_create("Delete Weight");
  Dialog *base_dialog = confirmation_dialog_get_dialog(dialog);
  dialog_set_text(base_dialog, card->delete_text);
  dialog_set_background_color(base_dialog, GColorRed);
  dialog_set_text_color(base_dialog, GColorWhite);
  dialog_set_icon(base_dialog, RESOURCE_ID_GENERIC_WARNING_SMALL);
  actionable_dialog_set_user_data((ActionableDialog *)dialog, card);
  confirmation_dialog_set_click_config_provider(dialog, prv_delete_click_config);
  app_confirmation_dialog_push(dialog);
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  WeightDetailCard *card = context;
  health_weight_entry_window_push(health_data_weight_get_profile_dag(card->health_data),
                                  prv_weight_saved, card);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 1000, prv_delete_latest, NULL);
}

Window *health_weight_detail_card_create(HealthData *health_data) {
  WeightDetailCard *card = app_zalloc_check(sizeof(*card));
  card->health_data = health_data;
  window_init(&card->window, WINDOW_NAME("Weight Detail"));
  window_set_user_data(&card->window, card);

  scroll_layer_init(&card->scroll_layer, &card->window.layer.bounds);
  scroll_layer_set_context(&card->scroll_layer, card);
  scroll_layer_set_callbacks(
      &card->scroll_layer,
      (ScrollLayerCallbacks){.click_config_provider = prv_click_config_provider});
  scroll_layer_set_click_config_onto_window(&card->scroll_layer, &card->window);
  scroll_layer_set_shadow_hidden(&card->scroll_layer, true);
  layer_add_child(&card->window.layer, scroll_layer_get_layer(&card->scroll_layer));

  card->content_layer =
      layer_create_with_data(GRectZero, sizeof(WeightDetailCard *));
  *(WeightDetailCard **)layer_get_data(card->content_layer) = card;
  layer_set_update_proc(card->content_layer, prv_content_update_proc);
  scroll_layer_add_child(&card->scroll_layer, card->content_layer);
  prv_update_content_size(card);
  return &card->window;
}

void health_weight_detail_card_destroy(Window *window) {
  WeightDetailCard *card = window_get_user_data(window);
  layer_destroy(card->content_layer);
  scroll_layer_deinit(&card->scroll_layer);
  window_deinit(window);
  i18n_free_all(card);
  app_free(card);
}
