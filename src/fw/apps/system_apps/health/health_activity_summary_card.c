/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "health_activity_summary_card.h"
#include "health_activity_summary_card_segments.h"
#include "health_activity_detail_card.h"
#include "health_progress.h"
#include "health_ui.h"
#include "services/normal/activity/health_util.h"

#include "applib/pbl_std/pbl_std.h"
#include "applib/ui/kino/kino_reel.h"
#include "applib/ui/text_layer.h"
#include "board/display.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "system/logging.h"
#include "util/size.h"
#include "util/string.h"

// Compile-time display offset calculations
#define HEALTH_X_OFFSET ((DISP_COLS - LEGACY_2X_DISP_COLS) / 2)
#define HEALTH_Y_OFFSET ((DISP_ROWS - LEGACY_2X_DISP_ROWS) / 2)

// Use larger font for taller displays
#if DISP_ROWS > LEGACY_2X_DISP_ROWS
#define HEALTH_STEPS_FONT FONT_KEY_LECO_32_BOLD_NUMBERS
#else
#define HEALTH_STEPS_FONT FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM
#endif

typedef struct HealthActivitySummaryCardData {
  HealthData *health_data;
  HealthProgressBar progress_bar;
  KinoReel *icon;
  int32_t current_steps;
  int32_t typical_steps;
  int32_t daily_average_steps;
} HealthActivitySummaryCardData;


#define PROGRESS_CURRENT_COLOR (PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorDarkGray))
#define PROGRESS_TYPICAL_COLOR (PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack))
#define PROGRESS_BACKGROUND_COLOR (PBL_IF_COLOR_ELSE(GColorDarkGray, GColorClear))
#define PROGRESS_OUTLINE_COLOR (PBL_IF_COLOR_ELSE(GColorClear, GColorBlack))

#define CURRENT_TEXT_COLOR PROGRESS_CURRENT_COLOR
#define CARD_BACKGROUND_COLOR (PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite))


static void prv_render_progress_bar(GContext *ctx, Layer *base_layer) {
  HealthActivitySummaryCardData *data = layer_get_data(base_layer);

  health_progress_bar_fill(ctx, &data->progress_bar, PROGRESS_BACKGROUND_COLOR,
                           0, HEALTH_PROGRESS_BAR_MAX_VALUE);

  const int32_t progress_max = MAX(data->current_steps, data->daily_average_steps);
  if (!progress_max) {
    health_progress_bar_outline(ctx, &data->progress_bar, PROGRESS_OUTLINE_COLOR);
    return;
  }

  const int current_fill = data->current_steps * HEALTH_PROGRESS_BAR_MAX_VALUE / progress_max;
  const int typical_fill = data->typical_steps * HEALTH_PROGRESS_BAR_MAX_VALUE / progress_max;

#if PBL_COLOR
  const bool behind_typical = (data->current_steps < data->typical_steps);
  if (behind_typical) {
    health_progress_bar_fill(ctx, &data->progress_bar, PROGRESS_TYPICAL_COLOR, 0, typical_fill);
  }
#endif

  if (data->current_steps) {
    health_progress_bar_fill(ctx, &data->progress_bar, PROGRESS_CURRENT_COLOR, 0, current_fill);
  }

#if PBL_COLOR
  if (!behind_typical) {
    health_progress_bar_mark(ctx, &data->progress_bar, PROGRESS_TYPICAL_COLOR, typical_fill);
  }
#else
  health_progress_bar_mark(ctx, &data->progress_bar, PROGRESS_TYPICAL_COLOR, typical_fill);
#endif

  // This needs to be done after drawing the progress bars or else the progress fill
  // overlaps the outline and things look weird
  health_progress_bar_outline(ctx, &data->progress_bar, PROGRESS_OUTLINE_COLOR);
}

static void prv_render_icon(GContext *ctx, Layer *base_layer) {
  HealthActivitySummaryCardData *data = layer_get_data(base_layer);

  const int y = PBL_IF_RECT_ELSE(PBL_IF_BW_ELSE(43, 38), 43) + HEALTH_Y_OFFSET;
  const int x_center_offset = PBL_IF_BW_ELSE(19, 18);
  kino_reel_draw(data->icon, ctx, GPoint(base_layer->bounds.size.w / 2 - x_center_offset, y));
}

static void prv_render_current_steps(GContext *ctx, Layer *base_layer) {
  HealthActivitySummaryCardData *data = layer_get_data(base_layer);

  char buffer[8];
  GFont font;
  if (data->current_steps) {
    font = fonts_get_system_font(HEALTH_STEPS_FONT);
    snprintf(buffer, sizeof(buffer), "%"PRIu32"", data->current_steps);
  } else {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    snprintf(buffer, sizeof(buffer), EM_DASH);
  }

  const int y = PBL_IF_RECT_ELSE(PBL_IF_BW_ELSE(85, 83), 88) + HEALTH_Y_OFFSET;
  graphics_context_set_text_color(ctx, CURRENT_TEXT_COLOR);
  graphics_draw_text(ctx, buffer, font,
                     GRect(0, y, base_layer->bounds.size.w, 40),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void prv_render_typical_steps(GContext *ctx, Layer *base_layer) {
  HealthActivitySummaryCardData *data = layer_get_data(base_layer);

  char steps_buffer[12];
  if (data->typical_steps) {
    snprintf(steps_buffer, sizeof(steps_buffer), "%"PRId32, data->typical_steps);
  } else {
    snprintf(steps_buffer, sizeof(steps_buffer), EM_DASH);
  }

  health_ui_render_typical_text_box(ctx, base_layer, steps_buffer);
}

static void prv_base_layer_update_proc(Layer *base_layer, GContext *ctx) {
  HealthActivitySummaryCardData *data = layer_get_data(base_layer);

  data->current_steps = health_data_current_steps_get(data->health_data);
  data->typical_steps = health_data_steps_get_current_average(data->health_data);
  data->daily_average_steps = health_data_steps_get_cur_wday_average(data->health_data);

  prv_render_icon(ctx, base_layer);

  prv_render_progress_bar(ctx, base_layer);

  prv_render_current_steps(ctx, base_layer);

  prv_render_typical_steps(ctx, base_layer);
}

static void prv_activity_detail_card_unload_callback(Window *window) {
  health_activity_detail_card_destroy(window);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// API Functions
//

Layer *health_activity_summary_card_create(HealthData *health_data) {
  // create base layer
  Layer *base_layer = layer_create_with_data(GRectZero, sizeof(HealthActivitySummaryCardData));
  HealthActivitySummaryCardData *health_activity_summary_card_data = layer_get_data(base_layer);
  layer_set_update_proc(base_layer, prv_base_layer_update_proc);
  // set health data
  *health_activity_summary_card_data = (HealthActivitySummaryCardData) {
    .health_data = health_data,
    .icon = kino_reel_create_with_resource(RESOURCE_ID_HEALTH_APP_ACTIVITY),
    .progress_bar = {
      .num_segments = ARRAY_LENGTH(s_activity_summary_progress_segments),
      .segments = s_activity_summary_progress_segments,
    },
  };

  return base_layer;
}

void health_activity_summary_card_select_click_handler(Layer *layer) {
  HealthActivitySummaryCardData *health_activity_summary_card_data = layer_get_data(layer);
  HealthData *health_data = health_activity_summary_card_data->health_data;
  Window *window = health_activity_detail_card_create(health_data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .unload = prv_activity_detail_card_unload_callback,
  });
  app_window_stack_push(window, true);
}

void health_activity_summary_card_destroy(Layer *base_layer) {
  HealthActivitySummaryCardData *data = layer_get_data(base_layer);
  i18n_free_all(base_layer);
  kino_reel_destroy(data->icon);
  layer_destroy(base_layer);
}

GColor health_activity_summary_card_get_bg_color(Layer *layer) {
  return CARD_BACKGROUND_COLOR;
}

bool health_activity_summary_show_select_indicator(Layer *layer) {
  return true;
}
