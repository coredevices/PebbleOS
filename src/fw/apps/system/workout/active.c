/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "active.h"
#include "dialog.h"
#include "summary.h"
#include "style.h"
#include "hr_history.h"
#include "workout.h"

#include "applib/app.h"
#include "applib/ui/action_menu_window.h"
#include "applib/ui/ui.h"
#include "applib/ui/window_manager.h"
#include "apps/system/timeline/text_node.h"
#include "kernel/pbl_malloc.h"
#ifdef CONFIG_TOUCH
#include "applib/touch_service.h"
#endif
#include "resource/resource_ids.auto.h"
#include "pbl/services/clock.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/health_util.h"
#include "pbl/services/activity/hr_util.h"
#include "pbl/services/activity/workout_service.h"
#include <pbl/logging/logging.h>
#include "pbl/util/size.h"

#include <stdio.h>

#define TEXT_COLOR (GColorBlack)
#define TEXT_ALIGNMENT (PBL_IF_RECT_ELSE(GTextAlignmentLeft, GTextAlignmentRight))
#define BACKGROUND_COLOR PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite)

typedef enum WorkoutLayout {
  WorkoutLayout_Dashboard,
  WorkoutLayout_SingleMetric,
  WorkoutLayout_StaticAndScrollable,
  WorkoutLayout_TwoStaticAndScrollable,
} WorkoutLayout;

typedef struct WorkoutActiveWindow {
  Window window;
#ifdef CONFIG_TOUCH
  GPoint touch_start;
  bool touch_active;
#endif
  ActionBarLayer action_bar;
  StatusBarLayer status_layer;
  Layer base_layer;
  Layer top_metric_layer;
  Layer middle_metric_layer;
  Layer scrollable_metric_layer;
  WorkoutDialog end_workout_dialog;

  ButtonId pause_button;

  WorkoutController *workout_controller;
  void *workout_data;

  WorkoutLayout layout;
  ActivitySessionType activity_type;
  bool has_hrm;
  bool finished;
  bool confirming_finish;
  WorkoutHrHistory *hr_history;
  Animation *hr_plot_animation;
  AnimationProgress hr_plot_progress;
  AnimationProgress hr_plot_start;
  bool hr_plot_visible;

  WorkoutMetricType top_metric;
  WorkoutMetricType middle_metric;

  int num_scrollable_metrics;
  int current_scrollable_metric;
  WorkoutMetricType scrollable_metrics[WorkoutMetricTypeCount];

  GBitmap *heart_icon;
  GBitmap *activity_icon;
  GBitmap *hr_measuring_icon;

  GBitmap *action_bar_start;
  GBitmap *action_bar_pause;
  GBitmap *action_bar_stop;
  GBitmap *action_bar_more;
  GBitmap *action_bar_next;

  AppTimer *update_timer;
  AppTimer *hr_measuring_timer;

  int cur_hr_measuring_width_idx;
} WorkoutActiveWindow;

static void prv_click_config_provider(void *context);

static const int s_hr_measuring_widths[] = {36, 0, 24, 28, 32};

static void prv_draw_heart_node_callback(GContext *ctx, const GRect *box,
                                         const GTextNodeDrawConfig *config, bool render,
                                         GSize *size_out, void *user_data);

static void prv_draw_hr_measuring_node_callback(GContext *ctx, const GRect *box,
                                                const GTextNodeDrawConfig *config, bool render,
                                                GSize *size_out, void *user_data);

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Helpers

static void prv_add_scrollable_metrics(WorkoutActiveWindow *active_window,
                                       int num_scrollable_metrics, WorkoutMetricType *metrics) {
  for (int i = 0; i < num_scrollable_metrics; i++) {
    active_window->scrollable_metrics[active_window->num_scrollable_metrics++] = metrics[i];
  }
}

static const char *prv_get_label_for_hr_metric(int bpm) {
  switch (hr_util_get_hr_zone(bpm)) {
    case HRZone_Zone1:
      /// Zone 1 HR Label
      return i18n_noop("FAT BURN");
    case HRZone_Zone2:
      /// Zone 2 HR Label
      return i18n_noop("ENDURANCE");
    case HRZone_Zone3:
      /// Zone 3 HR Label
      return i18n_noop("PERFORMANCE");
    default:
      /// Default/Zone 0 HR Label
      return i18n_noop("HEART RATE");
  }
}

static const char *prv_get_label_for_metric(WorkoutMetricType metric_type,
                                            WorkoutActiveWindow *active_window) {
  switch (metric_type) {
    case WorkoutMetricType_Hr: {
      int bpm = active_window->workout_controller->get_metric_value(WorkoutMetricType_Hr,
                                                                    active_window->workout_data);
      return prv_get_label_for_hr_metric(bpm);
    }
    case WorkoutMetricType_Custom:
      /// Custom Label from Sports App
      return active_window->workout_controller->get_custom_metric_label_string();
    case WorkoutMetricType_Duration:
      /// Duration Label
      return i18n_noop("DURATION");
    case WorkoutMetricType_AvgPace:
#if PBL_RECT
      /// Average Pace Label
      return i18n_noop("AVG PACE");
#else
      /// Average Pace Label with units
      return active_window->workout_controller->get_distance_string(i18n_noop("AVG PACE (/MI)"),
                                                                    i18n_noop("AVG PACE (/KM)"));
#endif
    case WorkoutMetricType_Pace:
#if PBL_RECT
      /// Pace Label
      return i18n_noop("PACE");
#else
      /// Pace Label with units
      return active_window->workout_controller->get_distance_string(i18n_noop("PACE (/MI)"),
                                                                    i18n_noop("PACE (/KM)"));
#endif
    case WorkoutMetricType_Speed:
#if PBL_RECT
      /// Speed Label
      return i18n_noop("SPEED");
#else
      /// Speed Label with units
      return active_window->workout_controller->get_distance_string(i18n_noop("SPEED (MPH)"),
                                                                    i18n_noop("SPEED (KM/H)"));
#endif
    case WorkoutMetricType_Distance:
#if PBL_RECT
      /// Distance Label
      return i18n_noop("DISTANCE");
#else
      /// Distance Label with units
      return active_window->workout_controller->get_distance_string(i18n_noop("DISTANCE (MI)"),
                                                                    i18n_noop("DISTANCE (KM)"));
#endif
    case WorkoutMetricType_Steps:
      /// Steps Label
      return i18n_noop("STEPS");
    default:
      return "";
  }
}

static GColor prv_get_bg_color_for_metric(WorkoutMetricType metric_type,
                                          WorkoutActiveWindow *active_window, bool is_scrollable) {
#if PBL_BW
  return GColorWhite;
#else
  if (metric_type == WorkoutMetricType_Hr) {
    switch (hr_util_get_hr_zone(active_window->workout_controller->get_metric_value(
        metric_type, active_window->workout_data))) {
      case HRZone_Zone0:
        return GColorWhite;
      case HRZone_Zone1:
        return GColorMelon;
      case HRZone_Zone2:
        return GColorChromeYellow;
      case HRZone_Zone3:
        return GColorOrange;
      default:
        return BACKGROUND_COLOR;
    }
  } else {
    return is_scrollable ? GColorPastelYellow : BACKGROUND_COLOR;
  }
#endif
}

static GFont prv_get_number_font(bool prefer_larger_font) {
#if PBL_DISPLAY_HEIGHT >= 200
  return prefer_larger_font ? fonts_get_system_font(FONT_KEY_LECO_60_NUMBERS_AM_PM)
                            : fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
#else
  return prefer_larger_font ? fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS)
                            : fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
#endif
}

//! Font for durations that include hours ("HH:MM:SS"). On a narrow rectangular
//! display the regular reduced number font (LECO_42) is too wide for 8 chars and
//! the seconds get clipped, so use a narrower font there.
static GFont prv_get_duration_font(void) {
#if PBL_DISPLAY_HEIGHT >= 200 && PBL_RECT
  return fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
#else
  return prv_get_number_font(false);
#endif
}

static GTextNode *prv_create_text_node(WorkoutActiveWindow *active_window,
                                       WorkoutMetricType metric_type, bool prefer_larger_font,
                                       void *i18n_owner) {
  GTextNodeHorizontal *horiz_container = graphics_text_node_create_horizontal(MAX_TEXT_NODES);
  GTextNodeContainer *container = &horiz_container->container;
  horiz_container->horizontal_alignment = TEXT_ALIGNMENT;

  const GFont number_font = prv_get_number_font(prefer_larger_font);
  const GFont units_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  const int units_offset_y = fonts_get_font_height(number_font) - fonts_get_font_height(units_font);

  switch (metric_type) {
    case WorkoutMetricType_Hr: {
      GPoint heart_node_offset = GPoint(2, prefer_larger_font ? 5 : 0);
      GTextNodeCustom *heart_node;
      if (active_window->workout_controller->get_metric_value(metric_type,
                                                              active_window->workout_data) > 0) {
        const size_t buffer_size = sizeof("000");
        GTextNodeText *number_text_node =
            health_util_create_text_node(buffer_size, number_font, TEXT_COLOR, container);
        active_window->workout_controller->metric_to_string(
            metric_type, (char *)number_text_node->text, buffer_size, i18n_owner,
            active_window->workout_data);
        heart_node_offset.y += fonts_get_font_cap_offset(number_font);
        heart_node = graphics_text_node_create_custom(prv_draw_heart_node_callback, active_window);
      } else {
        // if metric value is 0, we draw another icon that needs different offset
        heart_node_offset.x += 2;
        heart_node_offset.y += 7;
        heart_node =
            graphics_text_node_create_custom(prv_draw_hr_measuring_node_callback, active_window);
      }
      heart_node->node.offset = heart_node_offset;
      graphics_text_node_container_add_child(container, &heart_node->node);
      break;
    }
    case WorkoutMetricType_AvgHr:
    case WorkoutMetricType_AvgCadence:
    case WorkoutMetricType_ActiveCalories:
    case WorkoutMetricType_Steps: {
      const size_t buffer_size = sizeof("000000");
      GTextNodeText *number_text_node =
          health_util_create_text_node(buffer_size, number_font, TEXT_COLOR, container);
      active_window->workout_controller->metric_to_string(
          metric_type, (char *)number_text_node->text, buffer_size, i18n_owner,
          active_window->workout_data);
      break;
    }
    case WorkoutMetricType_AvgSpeed:
    case WorkoutMetricType_Distance: {
      GTextNodeText *number_text_node = health_util_create_text_node(
          HEALTH_WHOLE_AND_DECIMAL_LENGTH, number_font, TEXT_COLOR, container);
      active_window->workout_controller->metric_to_string(
          metric_type, (char *)number_text_node->text, HEALTH_WHOLE_AND_DECIMAL_LENGTH, i18n_owner,
          active_window->workout_data);

#if PBL_RECT
      /// MI/KM units string
      const char *units_string =
          active_window->workout_controller->get_distance_string(i18n_noop("MI"), i18n_noop("KM"));
      GTextNodeText *units_text_node = health_util_create_text_node_with_text(
          i18n_get(units_string, i18n_owner), units_font, TEXT_COLOR, container);
      units_text_node->node.offset.y = units_offset_y;
#endif
      break;
    }
    case WorkoutMetricType_Custom: {
      const size_t buffer_size = 20;
      GTextNodeText *number_text_node =
          health_util_create_text_node(buffer_size, number_font, TEXT_COLOR, container);
      number_text_node->overflow = GTextOverflowModeTrailingEllipsis;
      active_window->workout_controller->metric_to_string(
          metric_type, (char *)number_text_node->text, buffer_size, i18n_owner,
          active_window->workout_data);
      if (strlen(number_text_node->text) > 5) {
        number_text_node->font = prv_get_number_font(false);
      }
      break;
    }
    case WorkoutMetricType_Duration: {
      const size_t buffer_size = sizeof("00:00:00");
      GTextNodeText *number_text_node =
          health_util_create_text_node(buffer_size, number_font, TEXT_COLOR, container);
      active_window->workout_controller->metric_to_string(
          metric_type, (char *)number_text_node->text, buffer_size, i18n_owner,
          active_window->workout_data);

      if (strlen(number_text_node->text) > 5) {
        // text is long (includes hours) so use a font that fits the seconds
        number_text_node->font = prv_get_duration_font();
      }
      break;
    }
    case WorkoutMetricType_Pace:
    case WorkoutMetricType_AvgPace: {
      if (active_window->workout_controller->get_metric_value(
              metric_type, active_window->workout_data) >= SECONDS_PER_HOUR) {
        GTextNodeText *text_node =
            health_util_create_text_node_with_text(EM_DASH, units_font, TEXT_COLOR, container);
        text_node->node.offset.x += 1;
        text_node->node.offset.y = units_offset_y;
      } else {
        const size_t buffer_size = sizeof("00:00:00");
        GTextNodeText *number_text_node =
            health_util_create_text_node(buffer_size, number_font, TEXT_COLOR, container);
        active_window->workout_controller->metric_to_string(
            metric_type, (char *)number_text_node->text, buffer_size, i18n_owner,
            active_window->workout_data);

#if PBL_RECT
        GTextNodeText *divider_text_node =
            health_util_create_text_node_with_text("/", units_font, TEXT_COLOR, container);
        divider_text_node->node.offset.y = units_offset_y;

        /// MI/KM units string
        const char *units_string = active_window->workout_controller->get_distance_string(
            i18n_noop("MI"), i18n_noop("KM"));
        GTextNodeText *units_text_node = health_util_create_text_node_with_text(
            i18n_get(units_string, i18n_owner), units_font, TEXT_COLOR, container);
        units_text_node->node.offset.y = units_offset_y;
#endif
      }
      break;
    }
    case WorkoutMetricType_Speed: {
      const size_t buffer_size = sizeof("00:00:00");
      GTextNodeText *number_text_node =
          health_util_create_text_node(buffer_size, number_font, TEXT_COLOR, container);
      active_window->workout_controller->metric_to_string(
          metric_type, (char *)number_text_node->text, buffer_size, i18n_owner,
          active_window->workout_data);

#if PBL_RECT
      /// MI/KM units string
      const char *units_string = active_window->workout_controller->get_distance_string(
          i18n_noop("MPH"), i18n_noop("KM/H"));
      GTextNodeText *units_text_node = health_util_create_text_node_with_text(
          i18n_get(units_string, i18n_owner), units_font, TEXT_COLOR, container);
      units_text_node->node.offset.y = units_offset_y;
#endif
      break;
    }
    // don't have default here so when we have a new type, we don't forget to add it here
    case WorkoutMetricType_None:
    case WorkoutMetricTypeCount:
      break;
  }

  return &container->node;
}

static void prv_set_action_bar_icons(WorkoutActiveWindow *active_window) {
  if (active_window->layout == WorkoutLayout_Dashboard) {
    ActionBarLayer *bar = &active_window->action_bar;
    const bool show = !active_window->finished && active_window->workout_controller->is_paused();
    if (show) {
      action_bar_layer_set_icon(bar, BUTTON_ID_SELECT,
                                active_window->confirming_finish ? active_window->action_bar_more
                                                                 : active_window->action_bar_start);
      action_bar_layer_set_icon(bar, BUTTON_ID_DOWN,
                                active_window->confirming_finish ? active_window->action_bar_next
                                                                 : active_window->action_bar_stop);
      if (!bar->window) {
        action_bar_layer_add_to_window(bar, &active_window->window);
      }
    } else if (bar->window) {
      action_bar_layer_remove_from_window(bar);
      window_set_click_config_provider_with_context(&active_window->window,
                                                    prv_click_config_provider, active_window);
    }
    return;
  }
  ActionBarLayer *action_bar = &active_window->action_bar;
  bool is_paused = false;
  bool can_stop = false;
  if (active_window->workout_controller) {
    is_paused = active_window->workout_controller->is_paused();
    can_stop = active_window->workout_controller->stop != NULL;
  }

  if (active_window->finished) {
    action_bar_layer_clear_icon(action_bar, BUTTON_ID_UP);
    action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, active_window->action_bar_more);
    if (active_window->num_scrollable_metrics > 1) {
      action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, active_window->action_bar_next);
    } else {
      action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);
    }
    return;
  }

  if (is_paused) {
    action_bar_layer_set_icon(action_bar, active_window->pause_button,
                              active_window->action_bar_start);
    if (can_stop) {
      action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, active_window->action_bar_stop);
    }
  } else {
    action_bar_layer_clear_icon(action_bar, BUTTON_ID_SELECT);
    action_bar_layer_set_icon(action_bar, active_window->pause_button,
                              active_window->action_bar_pause);
  }

  if (active_window->num_scrollable_metrics > 1) {
    action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, active_window->action_bar_next);
  }
}

static void prv_update_ui(WorkoutActiveWindow *active_window) {
  if (window_manager_is_window_visible(&active_window->window)) {
    layer_mark_dirty(&active_window->base_layer);

    // Update the action bar in case another client updated the workout's status
    prv_set_action_bar_icons(active_window);
  }
}

static bool prv_is_heart_page(WorkoutActiveWindow *window) {
  return window->layout == WorkoutLayout_Dashboard && window->has_hrm &&
         window->current_scrollable_metric == window->num_scrollable_metrics - 1;
}

T_STATIC void prv_record_hr_sample(WorkoutActiveWindow *window) {
  if (!window->hr_history || window->finished || window->workout_controller->is_paused()) {
    return;
  }
  const int elapsed = window->workout_controller->get_metric_value(WorkoutMetricType_Duration,
                                                                   window->workout_data);
  const int bpm =
      window->workout_controller->get_metric_value(WorkoutMetricType_Hr, window->workout_data);
  workout_hr_history_add(window->hr_history, elapsed, bpm);
}

static void prv_cancel_hr_plot_animation(WorkoutActiveWindow *window) {
  if (window->hr_plot_animation) {
    Animation *animation = window->hr_plot_animation;
    window->hr_plot_animation = NULL;
    animation_unschedule(animation);
    animation_destroy(animation);
  }
}

static void prv_hr_plot_animation_update(Animation *animation, AnimationProgress progress) {
  WorkoutActiveWindow *window = animation_get_context(animation);
  const int target = window->hr_plot_visible ? ANIMATION_NORMALIZED_MAX : 0;
  window->hr_plot_progress = window->hr_plot_start + (int64_t)(target - window->hr_plot_start) *
                                                         progress / ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(&window->base_layer);
}

static void prv_hr_plot_animation_stopped(Animation *animation, bool finished, void *context) {
  if (!finished) {
    return;
  }
  WorkoutActiveWindow *window = context;
  window->hr_plot_animation = NULL;
  window->hr_plot_progress = window->hr_plot_visible ? ANIMATION_NORMALIZED_MAX : 0;
  animation_destroy(animation);
  layer_mark_dirty(&window->base_layer);
}

T_STATIC void prv_set_hr_plot(WorkoutActiveWindow *window, bool visible, bool animated) {
  if (!window->hr_history) {
    return;
  }
  prv_cancel_hr_plot_animation(window);
  window->hr_plot_visible = visible;
  if (animated) {
    window->hr_plot_start = window->hr_plot_progress;
    window->hr_plot_animation = animation_create();
  }
  if (!window->hr_plot_animation) {
    window->hr_plot_progress = visible ? ANIMATION_NORMALIZED_MAX : 0;
  } else {
    static const AnimationImplementation implementation = {.update = prv_hr_plot_animation_update};
    animation_set_implementation(window->hr_plot_animation, &implementation);
    animation_set_duration(window->hr_plot_animation, 240);
    animation_set_curve(window->hr_plot_animation, AnimationCurveEaseInOut);
    animation_set_handlers(window->hr_plot_animation,
                           (AnimationHandlers){.stopped = prv_hr_plot_animation_stopped}, window);
    animation_schedule(window->hr_plot_animation);
  }
  prv_update_ui(window);
}

static void prv_hr_measuring_timer_callback(void *data) {
  WorkoutActiveWindow *active_window = data;

  active_window->cur_hr_measuring_width_idx =
      (active_window->cur_hr_measuring_width_idx + 1) % ARRAY_LENGTH(s_hr_measuring_widths);

  prv_update_ui(active_window);

  if (active_window->workout_controller->get_metric_value(WorkoutMetricType_Hr,
                                                          active_window->workout_data) == 0) {
    int timeout_ms = (active_window->cur_hr_measuring_width_idx == 0) ? 800 : 200;
    active_window->hr_measuring_timer =
        app_timer_register(timeout_ms, prv_hr_measuring_timer_callback, active_window);
  } else {
    active_window->hr_measuring_timer = NULL;
  }
}

T_STATIC void prv_update_timer_callback(void *data) {
  WorkoutActiveWindow *active_window = data;
  if (active_window->finished) {
    return;
  }

  if (active_window->workout_controller->update_data) {
    active_window->workout_controller->update_data(active_window->workout_data);
  }

  prv_record_hr_sample(active_window);
  prv_update_ui(active_window);
  active_window->update_timer = app_timer_register(1000, prv_update_timer_callback, active_window);

  const int bpm = active_window->workout_controller->get_metric_value(WorkoutMetricType_Hr,
                                                                      active_window->workout_data);
  if (active_window->layout != WorkoutLayout_Dashboard && bpm == 0 &&
      !active_window->hr_measuring_timer) {
    active_window->cur_hr_measuring_width_idx = 0;
    prv_hr_measuring_timer_callback(active_window);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Drawing

static void prv_draw_heart_icon(GContext *ctx, GBitmap *icon, const GRect *rect, bool render,
                                GSize *size_out) {
  if (render) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, icon, rect);
  }
  if (size_out) {
    *size_out = rect->size;
  }
}

static void prv_draw_heart_node_callback(GContext *ctx, const GRect *box,
                                         const GTextNodeDrawConfig *config, bool render,
                                         GSize *size_out, void *user_data) {
  WorkoutActiveWindow *active_window = user_data;
  GRect bounds = gbitmap_get_bounds(active_window->heart_icon);
  bounds.origin = box->origin;
  prv_draw_heart_icon(ctx, active_window->heart_icon, &bounds, render, size_out);
}

static void prv_draw_hr_measuring_node_callback(GContext *ctx, const GRect *box,
                                                const GTextNodeDrawConfig *config, bool render,
                                                GSize *size_out, void *user_data) {
  WorkoutActiveWindow *active_window = user_data;
  GRect bounds = gbitmap_get_bounds(active_window->hr_measuring_icon);
  bounds.origin = box->origin;
  bounds.size.w = s_hr_measuring_widths[active_window->cur_hr_measuring_width_idx];
  prv_draw_heart_icon(ctx, active_window->hr_measuring_icon, &bounds, render, size_out);
}

static void prv_render_separator(GContext *ctx, Layer *layer) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_horizontal_line_dotted(ctx, GPoint(0, layer->bounds.size.h - 1),
                                       layer->bounds.size.w);
}

static void prv_render_bg_color(GContext *ctx, GRect *bounds, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, bounds);
}

static void prv_render_metric_label(GContext *ctx, GRect *box, WorkoutMetricType metric_type,
                                    WorkoutActiveWindow *active_window, void *i18n_owner) {
  GRect label_box = *box;
  GTextOverflowMode overflow_mode = GTextOverflowModeWordWrap;
  if (metric_type == WorkoutMetricType_Custom) {
    // I seriously have no idea why the height is hardcoded to 40 and overflow is set to word
    // wrap when there's a note that says the height is being set to 40 to avoid wrapping. Also,
    // with a font size of 18, I don't know how it wouldn't wrap. This fixes the inconsistent
    // magic number problem for the WorkoutMetricType_Custom only
    label_box.size.h = 20;
    overflow_mode = GTextOverflowModeTrailingEllipsis;
  }

  graphics_context_set_text_color(ctx, TEXT_COLOR);
  graphics_draw_text(ctx,
                     i18n_get(prv_get_label_for_metric(metric_type, active_window), i18n_owner),
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), label_box, overflow_mode,
                     TEXT_ALIGNMENT, NULL);
}

static void prv_render_hr_zones(GContext *ctx, GRect *box, WorkoutActiveWindow *active_window) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);

  GRect zone_rect = *box;
  zone_rect.origin.x += PBL_IF_RECT_ELSE(1, 69);
  // add some padding after the label
  zone_rect.origin.y += 10;
  // size of a zone rect
  zone_rect.size = GSize(20, 8);

  const int zone_padding = 2;

  for (HRZone i = HRZone_Zone1; i < HRZoneCount; i++) {
    if (i <= hr_util_get_hr_zone(active_window->workout_controller->get_metric_value(
                 WorkoutMetricType_Hr, active_window->workout_data))) {
      graphics_fill_rect(ctx, &zone_rect);
    } else {
      // drawing it twice to draw a 2px border
      GRect inner_rect = grect_inset(zone_rect, GEdgeInsets(1));
      graphics_draw_rect(ctx, &zone_rect);
      graphics_draw_rect(ctx, &inner_rect);
    }
    // increment x to draw more zones
    zone_rect.origin.x += zone_rect.size.w + zone_padding;
  }
}

static void prv_render_metric(GContext *ctx, WorkoutMetricType metric_type, Layer *layer,
                              GColor bg_color, bool draw_hr_zones, bool prefer_larger_font) {
  WorkoutActiveWindow *active_window = window_get_user_data(layer_get_window(layer));

  prv_render_bg_color(ctx, &layer->bounds, bg_color);

  const int16_t rl_margin = PBL_IF_RECT_ELSE(5, 23);

  GRect rect = grect_inset(layer->bounds, GEdgeInsets(0, rl_margin));

  // set rect y depending on layout, primary metric and display shape
  if (active_window->layout == WorkoutLayout_SingleMetric) {
    rect.origin.y = PBL_IF_RECT_ELSE(35, 41);
  } else if (active_window->layout == WorkoutLayout_StaticAndScrollable) {
    rect.origin.y = prefer_larger_font ? PBL_IF_RECT_ELSE(2, 13) : PBL_IF_RECT_ELSE(5, 1);
  } else if (active_window->layout == WorkoutLayout_TwoStaticAndScrollable) {
    rect.origin.y = (&active_window->scrollable_metric_layer == layer) ? PBL_IF_RECT_ELSE(-2, 0)
                                                                       : PBL_IF_RECT_ELSE(-4, -2);
  }

  // set the rect height so we don't wrap text to the next line
  rect.size.h = 40;

#if PBL_ROUND
  if (draw_hr_zones) {
    // padding between text and zones is less on round
    rect.origin.y -= 10;
  }
  rect.origin.x -= 24;
#endif

  prv_render_metric_label(ctx, &rect, metric_type, active_window, layer);

  // update rect y for the label height
  if (active_window->layout == WorkoutLayout_TwoStaticAndScrollable) {
    rect.origin.y += prefer_larger_font ? 11 : 15;
  } else {
    rect.origin.y += prefer_larger_font ? 12 : 15;
  }

  if (draw_hr_zones) {
    prv_render_hr_zones(ctx, &rect, active_window);
    // update rect y for the zones height
    rect.origin.y += PBL_IF_RECT_ELSE(18, 15);
  }

  // adjust rect for drawing the text node
  rect.origin.x -= PBL_IF_RECT_ELSE(1, 46);
  rect.size.w += (rl_margin * 2);

  GTextNode *text_node =
      prv_create_text_node(active_window, metric_type, prefer_larger_font, layer);
  graphics_text_node_draw(text_node, ctx, &rect, NULL, NULL);
  graphics_text_node_destroy(text_node);
}

static void prv_static_layer_update_proc(struct Layer *layer, GContext *ctx) {
  WorkoutActiveWindow *active_window = window_get_user_data(layer_get_window(layer));

  WorkoutMetricType metric_type = WorkoutMetricType_None;
  if (layer == &active_window->top_metric_layer) {
    metric_type = active_window->top_metric;
  } else if (layer == &active_window->middle_metric_layer) {
    metric_type = active_window->middle_metric;
  }

  GColor bg_color = prv_get_bg_color_for_metric(metric_type, active_window, false);

  HRZone hr_zone = hr_util_get_hr_zone(active_window->workout_controller->get_metric_value(
      metric_type, active_window->workout_data));
  const bool draw_zones = (metric_type == WorkoutMetricType_Hr) && hr_zone > HRZone_Zone0;
  const bool prefer_larger_font = active_window->layout == WorkoutLayout_SingleMetric ||
                                  active_window->layout == WorkoutLayout_StaticAndScrollable;

  prv_render_metric(ctx, metric_type, layer, bg_color, draw_zones, prefer_larger_font);

  if (layer == &active_window->top_metric_layer) {
    status_bar_layer_set_colors(&active_window->status_layer, bg_color, GColorBlack);
  }

  if (active_window->layout == WorkoutLayout_StaticAndScrollable ||
      (active_window->layout == WorkoutLayout_TwoStaticAndScrollable &&
       layer == &active_window->middle_metric_layer)) {
    prv_render_separator(ctx, layer);
  }
}

static void prv_scrollable_layer_update_proc(struct Layer *layer, GContext *ctx) {
  WorkoutActiveWindow *active_window = window_get_user_data(layer_get_window(layer));

  if (!active_window->num_scrollable_metrics) {
    return;
  }

  WorkoutMetricType metric_type =
      active_window->scrollable_metrics[active_window->current_scrollable_metric];

  GColor bg_color = prv_get_bg_color_for_metric(metric_type, active_window, true);

  const bool draw_hr_zones = false;
  const bool prefer_larger_font = false;
  prv_render_metric(ctx, metric_type, layer, bg_color, draw_hr_zones, prefer_larger_font);
}

static void prv_dashboard_text(GContext *ctx, const char *text, const char *font_key, GRect box,
                               GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key), box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static GRect prv_dashboard_value(GContext *ctx, WorkoutActiveWindow *active_window,
                                 WorkoutMetricType metric, GRect box, const char *font_key,
                                 GColor color) {
  char text[24] = {0};
  const int preferred_height = fonts_get_font_height(fonts_get_system_font(font_key));
  WorkoutController *controller = active_window->workout_controller;
  const int value = controller->get_metric_value(metric, active_window->workout_data);
  if (((metric == WorkoutMetricType_Hr || metric == WorkoutMetricType_AvgHr) && value <= 0) ||
      ((metric == WorkoutMetricType_AvgSpeed || metric == WorkoutMetricType_AvgCadence) &&
       value < 0) ||
      (metric == WorkoutMetricType_AvgPace && (value <= 0 || value >= SECONDS_PER_HOUR))) {
    strcpy(text, "--");
  } else {
    controller->metric_to_string(metric, text, sizeof(text), active_window,
                                 active_window->workout_data);
  }
  if (metric == WorkoutMetricType_Duration && strlen(text) > 5) {
    font_key =
        box.size.w >= 145 ? FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM : FONT_KEY_LECO_20_BOLD_NUMBERS;
  }
  GSize text_size = graphics_text_layout_get_max_used_size(
      ctx, text, fonts_get_system_font(font_key), GRect(0, 0, 1000, 100), GTextOverflowModeFill,
      GTextAlignmentLeft, NULL);
  const char *fallbacks[] = {FONT_KEY_LECO_42_NUMBERS, FONT_KEY_LECO_32_BOLD_NUMBERS,
                             FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM, FONT_KEY_LECO_20_BOLD_NUMBERS,
                             FONT_KEY_GOTHIC_18_BOLD};
  for (size_t i = 0; text_size.w > box.size.w && i < ARRAY_LENGTH(fallbacks); i++) {
    font_key = fallbacks[i];
    text_size = graphics_text_layout_get_max_used_size(
        ctx, text, fonts_get_system_font(font_key), GRect(0, 0, 1000, 100), GTextOverflowModeFill,
        GTextAlignmentLeft, NULL);
  }

  // Keep LECO digits visually centered when fitting a smaller font.
  box.origin.y +=
      (preferred_height - fonts_get_font_height(fonts_get_system_font(font_key))) * 2 / 3;
  prv_dashboard_text(ctx, text, font_key, box, color);
  box.origin.x += (box.size.w - text_size.w) / 2;
  box.size.w = text_size.w;
  return box;
}

static GColor prv_zone_color(HRZone zone, bool text) {
  switch (zone) {
    case HRZone_Zone1:
      return PBL_IF_COLOR_ELSE(text ? GColorArmyGreen : GColorChromeYellow, GColorBlack);
    case HRZone_Zone2:
      return PBL_IF_COLOR_ELSE(text ? GColorWindsorTan : GColorOrange, GColorBlack);
    case HRZone_Zone3:
      return PBL_IF_COLOR_ELSE(text ? GColorDarkCandyAppleRed : GColorRed, GColorBlack);
    default:
      return PBL_IF_COLOR_ELSE(text ? GColorDukeBlue : GColorPictonBlue, GColorBlack);
  }
}

static void prv_dashboard_pair(GContext *ctx, WorkoutActiveWindow *window, GRect box,
                               WorkoutMetricType left, const char *left_label,
                               WorkoutMetricType right, const char *right_label) {
  const int half = box.size.w / 2;
  const WorkoutMetricType metrics[] = {left, right};
  const char *labels[] = {left_label, right_label};
  for (int i = 0; i < 2; i++) {
    const int x = box.origin.x + i * half;
    prv_dashboard_text(ctx, i18n_get(labels[i], window), FONT_KEY_GOTHIC_14,
                       GRect(x, box.origin.y, half, 18),
                       PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    GColor color = metrics[i] == WorkoutMetricType_ActiveCalories
                       ? PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorBlack)
                       : GColorBlack;
    prv_dashboard_value(
        ctx, window, metrics[i], GRect(x, box.origin.y + 16, half, 36),
        half >= 74 ? FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM : FONT_KEY_LECO_20_BOLD_NUMBERS, color);
  }
}

static void prv_dashboard_heart_icon(GContext *ctx, GRect box) {
  GPoint points[] = {{0, 5}, {5, 0}, {10, 5}, {15, 0}, {20, 5}, {20, 11}, {10, 21}, {0, 11}};
  for (size_t i = 0; i < ARRAY_LENGTH(points); i++) {
    points[i].x = box.origin.x + 1 + points[i].x * (box.size.w - 3) / 20;
    points[i].y = box.origin.y + 1 + points[i].y * (box.size.h - 3) / 21;
  }
  GPath path = {.num_points = ARRAY_LENGTH(points), .points = points};
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  gpath_draw_filled(ctx, &path);
  gpath_draw_outline(ctx, &path);
  graphics_context_set_stroke_width(ctx, 1);
}

static void prv_dashboard_heart(GContext *ctx, WorkoutActiveWindow *window, GRect box) {
  const int bpm =
      window->workout_controller->get_metric_value(WorkoutMetricType_Hr, window->workout_data);
  const bool large = window->window.layer.bounds.size.h >= 200;
  if (!window->has_hrm) {
    GRect label = GRect(box.origin.x, box.origin.y + (box.size.h - 24) / 2, box.size.w, 24);
    prv_dashboard_text(ctx, i18n_get("No heart rate", window), FONT_KEY_GOTHIC_18, label,
                       PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    return;
  }
  const HRZone zone = hr_util_get_hr_zone(bpm);
  const int heart_h = large ? 19 : 13;
  const int icon_gap = large ? 5 : 4;
  const char *font = large ? FONT_KEY_LECO_38_BOLD_NUMBERS : FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM;
  char text[12];
  if (bpm > 0) {
    snprintf(text, sizeof(text), "%d", bpm);
  } else {
    strcpy(text, "--");
  }
  const GSize text_size = graphics_text_layout_get_max_used_size(
      ctx, text, fonts_get_system_font(font), GRect(0, 0, box.size.w, 44), GTextOverflowModeFill,
      GTextAlignmentLeft, NULL);
  const int bar_w = large ? 12 : 7;
  const int bar_left = box.origin.x + box.size.w - (4 * bar_w + 9);
  const int value_x = box.origin.x + heart_h + icon_gap;
  const int value_w = MIN(text_size.w, bar_left - 6 - value_x);
  const int bar_h = large ? 27 : 19;
  const int number_y = box.origin.y + (box.size.h - bar_h) / 2 - (large ? 11 : 7);
  const int value_center_y = box.origin.y + box.size.h / 2;
  prv_dashboard_heart_icon(ctx,
                           GRect(box.origin.x, value_center_y - heart_h / 2, heart_h, heart_h));
  prv_dashboard_value(ctx, window, WorkoutMetricType_Hr, GRect(value_x, number_y, value_w, 44),
                      font, prv_zone_color(zone, true));
  const int bar_bottom = number_y + (large ? 38 : 26);
  for (int i = 0; i < 4; i++) {
    const int height = bpm > 0 && i == zone ? bar_h : bar_h / 2;
    GRect bar = GRect(bar_left + i * (bar_w + 3), bar_bottom - height, bar_w, height);
    graphics_context_set_fill_color(ctx, prv_zone_color(i, false));
    graphics_fill_rect(ctx, &bar);
  }
  if (bpm <= 0) {
    prv_dashboard_text(ctx, i18n_get("MEASURING", window), FONT_KEY_GOTHIC_14,
                       GRect(box.origin.x, box.origin.y + box.size.h - 16, box.size.w, 18),
                       PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
  }
}

static void prv_draw_hr_chart(GContext *ctx, WorkoutActiveWindow *window, GRect box,
                              AnimationProgress progress) {
  const int left = box.origin.x + 24;
  const int right = box.origin.x + box.size.w - 25;
  const int bottom = box.origin.y + box.size.h - 16;
  const int height = MAX(1, box.size.h - 20);
  const int top = bottom - height * progress / ANIMATION_NORMALIZED_MAX;
  const int elapsed = window->workout_controller->get_metric_value(WorkoutMetricType_Duration,
                                                                   window->workout_data);
  int minimum;
  int maximum;
  const bool has_reading =
      workout_hr_history_range(window->hr_history, elapsed, &minimum, &maximum);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  graphics_draw_line(ctx, GPoint(left, top), GPoint(right, top));
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(left, top), GPoint(left, bottom));
  graphics_draw_line(ctx, GPoint(left, bottom), GPoint(right, bottom));

  bool previous_valid = false;
  GPoint previous = GPointZero;
  int previous_time = 0;
  for (unsigned i = 0; window->hr_history && i < window->hr_history->count; i++) {
    const WorkoutHrSample *sample = workout_hr_history_get(window->hr_history, i);
    const int age = elapsed - sample->elapsed_s;
    if (!sample->bpm || age < 0 || age >= WORKOUT_HR_HISTORY_SECONDS) {
      previous_valid = false;
      continue;
    }
    const GPoint point =
        GPoint(right - age * (right - left) / WORKOUT_HR_HISTORY_SECONDS,
               bottom - (sample->bpm - minimum) * (bottom - top) / (maximum - minimum));
    graphics_context_set_stroke_color(ctx, prv_zone_color(hr_util_get_hr_zone(sample->bpm), true));
    graphics_context_set_stroke_width(ctx, 2);
    if (previous_valid && sample->elapsed_s == previous_time + 1) {
      graphics_draw_line(ctx, previous, point);
    } else {
      graphics_draw_pixel(ctx, point);
    }
    previous = point;
    previous_time = sample->elapsed_s;
    previous_valid = true;
  }
  graphics_context_set_stroke_width(ctx, 1);
  if (progress == ANIMATION_NORMALIZED_MAX) {
    char label[12];
    if (has_reading) {
      snprintf(label, sizeof(label), "%d", maximum);
    } else {
      strcpy(label, "--");
    }
    prv_dashboard_text(ctx, label, FONT_KEY_GOTHIC_14, GRect(box.origin.x, top - 8, 22, 18),
                       GColorBlack);
    if (has_reading) {
      snprintf(label, sizeof(label), "%d", minimum);
    }
    prv_dashboard_text(ctx, label, FONT_KEY_GOTHIC_14, GRect(box.origin.x, bottom - 8, 22, 18),
                       GColorBlack);
    prv_dashboard_text(ctx, i18n_get("Last minute", window), FONT_KEY_GOTHIC_14,
                       GRect(left, bottom, right - left, 18), GColorBlack);
    if (!has_reading) {
      GRect label_box = GRect(left + 2, (top + bottom - 18) / 2, right - left - 4, 18);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, &label_box);
      prv_dashboard_text(ctx, i18n_get("WAITING FOR HR", window), FONT_KEY_GOTHIC_14, label_box,
                         GColorBlack);
    }
  }
}

static void prv_dashboard_update_proc(Layer *layer, GContext *ctx) {
  WorkoutActiveWindow *window = window_get_user_data(layer_get_window(layer));
  const bool paused = window->workout_controller->is_paused();
  const bool finished = window->finished;
  const bool show_sidebar = paused && !finished;
  const int w = layer->bounds.size.w - (show_sidebar ? ACTION_BAR_WIDTH : 0);
  const int h = layer->bounds.size.h;
  const bool large = h >= 200;
  const int margin = PBL_IF_ROUND_ELSE(w >= 240 ? 44 : 26, 8);
  const int content_w = w - 2 * margin;
  const int top = PBL_IF_ROUND_ELSE(22, 14);
  const int pair_y = h - (large ? PBL_IF_ROUND_ELSE(77, 65) : PBL_IF_ROUND_ELSE(65, 52));
  const bool open = window->activity_type == ActivitySessionType_Open;
  const int page = window->current_scrollable_metric;
  const bool heart_page = window->has_hrm && page == (open ? 1 : 3);
  const int header_h = workout_header_height();
  GRect accent = GRect(0, 0, w, header_h);
  graphics_context_set_fill_color(
      ctx, PBL_IF_COLOR_ELSE(workout_activity_color(window->activity_type), GColorBlack));
  graphics_fill_rect(ctx, &accent);

  GBitmap *heading_icon = window->activity_icon;
  const GSize icon_size = gbitmap_get_bounds(heading_icon).size;
  const GFont heading_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const char *activity_title = window->activity_type == ActivitySessionType_Run ? i18n_noop("RUN")
                               : window->activity_type == ActivitySessionType_Walk
                                   ? i18n_noop("WALK")
                                   : i18n_noop("WORKOUT");
  const char *heading_text = i18n_get(activity_title, window);
  GSize heading_size = graphics_text_layout_get_max_used_size(
      ctx, heading_text, heading_font, GRect(0, 0, content_w - icon_size.w - 4, 24),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  const int group_w = icon_size.w + 4 + heading_size.w;
  const int group_x = margin + (content_w - group_w) / 2;
  graphics_context_set_compositing_mode(ctx, PBL_IF_COLOR_ELSE(GCompOpSet, GCompOpAssignInverted));
  graphics_draw_bitmap_in_rect(
      ctx, heading_icon, &GRect(group_x, (header_h - icon_size.h) / 2, icon_size.w, icon_size.h));
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  graphics_draw_text(ctx, heading_text, heading_font,
                     GRect(group_x + icon_size.w + 4, (header_h - 24) / 2, heading_size.w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (paused && !finished) {
    prv_dashboard_text(ctx, i18n_get(window->confirming_finish ? "END WORKOUT?" : "PAUSED", window),
                       FONT_KEY_GOTHIC_18_BOLD, GRect(margin, top + 26, content_w, 24),
                       PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    prv_dashboard_value(ctx, window, WorkoutMetricType_Duration,
                        GRect(margin, top + 50, content_w, 58),
                        large ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
                        PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    if (large) {
      prv_dashboard_pair(ctx, window, GRect(margin, top + 110, content_w, 48),
                         WorkoutMetricType_Distance,
                         window->workout_controller->get_distance_string(i18n_noop("DIST MI"),
                                                                         i18n_noop("DIST KM")),
                         WorkoutMetricType_ActiveCalories, i18n_noop("EST KCAL"));
    }
    prv_dashboard_text(ctx, i18n_get("ELAPSED", window), FONT_KEY_GOTHIC_14,
                       GRect(margin, top + (large ? 94 : 78), content_w, 18),
                       PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
    i18n_free_all(window);
    return;
  }

  workout_draw_select_indicator(ctx, layer->bounds.size);
  WorkoutMetricType hero = WorkoutMetricType_Duration;
  const char *heading = i18n_noop("ELAPSED");
  GColor hero_color = paused ? PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack) : GColorBlack;
  if (heart_page) {
    hero = WorkoutMetricType_Hr;
    heading = i18n_noop("HEART RATE BPM");
    hero_color = prv_zone_color(hr_util_get_hr_zone(window->workout_controller->get_metric_value(
                                    hero, window->workout_data)),
                                true);
  } else if (!open && page == 1) {
    hero = WorkoutMetricType_AvgCadence;
    heading = i18n_noop("AVG CADENCE /MIN");
  } else if (!open && page == 2) {
    hero = WorkoutMetricType_AvgSpeed;
    heading = window->workout_controller->get_distance_string(i18n_noop("AVG SPEED MPH"),
                                                              i18n_noop("AVG SPEED KM/H"));
  }
  if (finished && page == 0) {
    heading = i18n_noop("TOTAL TIME");
  }
  const char *hero_font = large && !finished ? FONT_KEY_LECO_60_NUMBERS_AM_PM
                          : large            ? FONT_KEY_LECO_42_NUMBERS
                                             : FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM;
  GRect hero_box = GRect(margin, top + (large && !finished ? 16 : 22), content_w, large ? 70 : 40);
  if (heart_page) {
    const int bpm = window->workout_controller->get_metric_value(hero, window->workout_data);
    char text[12];
    if (bpm > 0) {
      snprintf(text, sizeof(text), "%d", bpm);
    } else {
      strcpy(text, "--");
    }
    const int heart_height = large ? (finished ? 30 : 42) : 19;
    const GSize heart_size = GSize(heart_height, heart_height);
    const GSize value_size = graphics_text_layout_get_max_used_size(
        ctx, text, fonts_get_system_font(hero_font), hero_box, GTextOverflowModeFill,
        GTextAlignmentCenter, NULL);
    const int gap = 8;
    const int value_w = MIN(value_size.w, content_w - heart_size.w - gap);
    const int group_x = margin + (content_w - heart_size.w - gap - value_w) / 2;
    const int value_center_y = hero_box.origin.y + (large ? (finished ? 27 : 39) : 17);
    prv_dashboard_heart_icon(
        ctx, GRect(group_x, value_center_y - heart_size.h / 2, heart_size.w, heart_size.h));
    hero_box.origin.x = group_x + heart_size.w + gap;
    hero_box.size.w = value_w;
  }
  const GRect hero_value = prv_dashboard_value(ctx, window, hero, hero_box, hero_font, hero_color);
  prv_dashboard_text(ctx, i18n_get(heading, window), FONT_KEY_GOTHIC_14,
                     GRect(margin, top + (large ? (finished ? 64 : 76) : 50), content_w, 18),
                     PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));

  if (finished && (!heart_page || (!window->hr_plot_visible && !window->hr_plot_progress))) {
    const int row_y = top + (large ? 84 : 66);
    const bool form = page == 1 || open;
    prv_dashboard_pair(ctx, window, GRect(margin, row_y, content_w, 48),
                       form ? WorkoutMetricType_Steps : WorkoutMetricType_Distance,
                       form ? i18n_noop("STEPS")
                            : window->workout_controller->get_distance_string(i18n_noop("DIST MI"),
                                                                              i18n_noop("DIST KM")),
                       form ? WorkoutMetricType_ActiveCalories : WorkoutMetricType_AvgPace,
                       form ? i18n_noop("EST KCAL")
                            : window->workout_controller->get_distance_string(
                                  i18n_noop("AVG /MI"), i18n_noop("AVG /KM")));
    if (large) {
      prv_dashboard_pair(ctx, window, GRect(margin, row_y + 48, content_w, 48),
                         WorkoutMetricType_AvgHr, i18n_noop("AVG BPM"),
                         WorkoutMetricType_ActiveCalories, i18n_noop("EST KCAL"));
    }
    prv_dashboard_text(ctx, i18n_get("COMPLETE", window), FONT_KEY_GOTHIC_14_BOLD,
                       GRect(margin, h - PBL_IF_ROUND_ELSE(34, 23), content_w, 18), GColorBlack);
    i18n_free_all(window);
    return;
  }

  if (heart_page) {
    const int half = ANIMATION_NORMALIZED_MAX / 2;
    const int chart_y = top + (large ? 96 : 66);
    const int chart_bottom = h - (finished ? 40 : PBL_IF_ROUND_ELSE(25, 17));
    if (window->hr_plot_progress > half) {
      prv_draw_hr_chart(ctx, window, GRect(margin, chart_y, content_w, chart_bottom - chart_y),
                        MIN(ANIMATION_NORMALIZED_MAX, 2 * (window->hr_plot_progress - half)));
    } else {
      const int bpm =
          window->workout_controller->get_metric_value(WorkoutMetricType_Hr, window->workout_data);
      const int bar_w = (content_w - 9) / 4;
      const int bar_top = top + (large ? 100 : 68);
      const int full_h = large ? 28 : 10;
      for (int i = 0; i < 4; i++) {
        const int bar_h = bpm > 0 && i == hr_util_get_hr_zone(bpm) ? full_h : full_h / 2;
        const int progress = window->hr_plot_progress;
        const int height = MAX(1, bar_h * (half - progress) / half);
        const int axis_left = margin + 24;
        const int axis_w = content_w - 49;
        const int from_x = margin + i * (bar_w + 3);
        const int target_x = axis_left + i * axis_w / 4;
        const int target_w = (i + 1) * axis_w / 4 - i * axis_w / 4;
        const int bottom =
            bar_top + full_h + (chart_bottom - 16 - bar_top - full_h) * progress / half;
        GRect bar = GRect(from_x + (target_x - from_x) * progress / half, bottom - height,
                          bar_w + (target_w - bar_w) * progress / half, height);
        graphics_context_set_fill_color(ctx, prv_zone_color(i, false));
        graphics_fill_rect(ctx, &bar);
      }
      if (!window->hr_plot_progress) {
        prv_dashboard_text(
            ctx, i18n_get(bpm > 0 ? prv_get_label_for_hr_metric(bpm) : "MEASURING", window),
            FONT_KEY_GOTHIC_14_BOLD, GRect(margin, bar_top + full_h + 2, content_w, 18),
            hero_color);
      }
    }
  } else {
    const int row_w = MAX(hero_value.size.w, MIN(content_w, large ? 160 : 108));
    const int row_y = top + (large ? 76 : 50) + 18;
    prv_dashboard_heart(ctx, window,
                        GRect(margin + (content_w - row_w) / 2, row_y, row_w, pair_y - row_y));
  }

  WorkoutMetricType left = WorkoutMetricType_Distance;
  WorkoutMetricType right = WorkoutMetricType_AvgPace;
  const char *left_label =
      window->workout_controller->get_distance_string(i18n_noop("DIST MI"), i18n_noop("DIST KM"));
  const char *right_label =
      window->workout_controller->get_distance_string(i18n_noop("AVG /MI"), i18n_noop("AVG /KM"));
  if (heart_page) {
    left = WorkoutMetricType_AvgHr;
    right = WorkoutMetricType_Duration;
    left_label = i18n_noop("AVG BPM");
    right_label = i18n_noop("ELAPSED");
  } else if (open || page == 1) {
    left = WorkoutMetricType_Steps;
    right = WorkoutMetricType_ActiveCalories;
    left_label = i18n_noop("STEPS");
    right_label = i18n_noop("EST KCAL");
  }
  if (!heart_page || !window->hr_plot_progress) {
    prv_dashboard_pair(ctx, window, GRect(margin, pair_y, content_w, 54), left, left_label, right,
                       right_label);
  }
  if (finished) {
    prv_dashboard_text(ctx, i18n_get("COMPLETE", window), FONT_KEY_GOTHIC_14_BOLD,
                       GRect(margin, h - PBL_IF_ROUND_ELSE(34, 23), content_w, 18), GColorBlack);
  } else if (window->num_scrollable_metrics > 1) {
    const int count = window->num_scrollable_metrics;
    for (int i = 0; i < count; i++) {
      GRect marker = GRect(w / 2 - count * 8 + i * 16, h - PBL_IF_ROUND_ELSE(18, 9), 11, 3);
      graphics_context_set_fill_color(ctx, i == page
                                               ? PBL_IF_COLOR_ELSE(GColorDukeBlue, GColorBlack)
                                               : PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
      graphics_fill_rect(ctx, &marker);
    }
  }
  i18n_free_all(window);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! End Workout

T_STATIC bool prv_finish_workout(WorkoutActiveWindow *active_window) {
  active_window->workout_controller->update_data(active_window->workout_data);
  if (!active_window->workout_controller->stop()) {
    return false;
  }
  prv_record_hr_sample(active_window);
  prv_set_hr_plot(active_window, false, false);
  active_window->finished = true;
  active_window->confirming_finish = false;
  active_window->current_scrollable_metric = 0;
  app_timer_cancel(active_window->update_timer);
  active_window->update_timer = NULL;
  return true;
}

static void prv_end_workout_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *active_window = context;

  if (active_window->layout == WorkoutLayout_Dashboard) {
    if (!prv_finish_workout(active_window)) {
      return;
    }
    workout_dialog_pop(&active_window->end_workout_dialog);
    prv_update_ui(active_window);
    return;
  }

  if (active_window->workout_controller) {
    active_window->workout_controller->stop();
  }
  workout_push_summary_window();
  workout_dialog_pop(&active_window->end_workout_dialog);
  app_window_stack_remove(&active_window->window, false);
}

static void prv_end_workout_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *active_window = context;

  workout_dialog_pop(&active_window->end_workout_dialog);
}

static void prv_end_workout_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_end_workout_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_end_workout_down_click_handler);
}

static void prv_end_workout(void *context) {
  WorkoutActiveWindow *active_window = context;
  if (active_window->layout == WorkoutLayout_Dashboard) {
    active_window->confirming_finish = true;
    prv_update_ui(active_window);
    return;
  }

  WorkoutDialog *workout_dialog = &active_window->end_workout_dialog;

  workout_dialog_init(workout_dialog, "Workout End");
  Dialog *dialog = workout_dialog_get_dialog(workout_dialog);

  dialog_show_status_bar_layer(dialog, true);
  dialog_set_fullscreen(dialog, true);
  dialog_set_text(dialog, i18n_get("End Workout?", workout_dialog));
  dialog_set_background_color(
      dialog, active_window->layout == WorkoutLayout_Dashboard ? GColorWhite : BACKGROUND_COLOR);
  dialog_set_text_color(dialog, TEXT_COLOR);
  dialog_set_icon(dialog, RESOURCE_ID_WORKOUT_APP_END);
  dialog_set_icon_animate_direction(dialog, DialogIconAnimateNone);
  dialog_set_destroy_on_pop(dialog, false);

  i18n_free_all(workout_dialog);

  workout_dialog_set_click_config_provider(workout_dialog, prv_end_workout_click_config_provider);
  workout_dialog_set_click_config_context(workout_dialog, context);

  app_workout_dialog_push(workout_dialog);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Handlers

static void prv_handle_pause_button(WorkoutActiveWindow *active_window) {
  bool is_paused = false;
  if (active_window->workout_controller) {
    is_paused = active_window->workout_controller->is_paused();
  }

  if (active_window->workout_controller) {
    active_window->workout_controller->pause(!is_paused);
  }

  prv_update_ui(active_window);
}

static void prv_handle_stop_button(WorkoutActiveWindow *active_window) {
  bool is_paused = false;
  bool can_stop = false;
  if (active_window->workout_controller) {
    is_paused = active_window->workout_controller->is_paused();
    can_stop = active_window->workout_controller->stop != NULL;
  }

  if (!is_paused || !can_stop) {
    return;
  }

  prv_end_workout(active_window);
}

static void prv_previous_page(WorkoutActiveWindow *window) {
  prv_set_hr_plot(window, false, false);
  if (window->num_scrollable_metrics > 0) {
    window->current_scrollable_metric =
        (window->current_scrollable_metric + window->num_scrollable_metrics - 1) %
        window->num_scrollable_metrics;
  }
  prv_update_ui(window);
}

T_STATIC void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *window = context;
  if (window->layout == WorkoutLayout_Dashboard) {
    if (window->finished || !window->workout_controller->is_paused()) {
      if (prv_is_heart_page(window) && window->hr_plot_visible) {
        prv_set_hr_plot(window, false, true);
      } else {
        prv_previous_page(window);
      }
    }
  } else if (window->pause_button == BUTTON_ID_UP) {
    prv_handle_pause_button(window);
  }
}

T_STATIC void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *window = context;
  if (window->finished) {
    workout_push_summary_window();
    app_window_stack_remove(&window->window, false);
    return;
  }
  if (window->layout == WorkoutLayout_Dashboard) {
    if (window->confirming_finish) {
      prv_finish_workout(window);
      prv_update_ui(window);
    } else {
      prv_handle_pause_button(window);
    }
  } else if (window->pause_button == BUTTON_ID_SELECT) {
    prv_handle_pause_button(window);
  } else {
    prv_handle_stop_button(window);
  }
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *window = context;
  if (window->confirming_finish) {
    window->confirming_finish = false;
    prv_update_ui(window);
  } else {
    prv_select_click_handler(recognizer, context);
  }
}

static void prv_set_pause_button(WorkoutActiveWindow *active_window) {
  bool can_stop = active_window->workout_controller->stop != NULL;
  if (can_stop || active_window->num_scrollable_metrics > 1) {
    active_window->pause_button = BUTTON_ID_UP;
  } else {
    active_window->pause_button = BUTTON_ID_SELECT;
  }
}

T_STATIC void prv_cycle_scrollable_metrics(WorkoutActiveWindow *active_window) {
  prv_set_hr_plot(active_window, false, false);
  if (active_window->num_scrollable_metrics == 0) {
    return;
  }
  active_window->current_scrollable_metric =
      (active_window->current_scrollable_metric + 1) % active_window->num_scrollable_metrics;
}

T_STATIC void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *active_window = context;
  if (active_window->layout == WorkoutLayout_Dashboard && !active_window->finished &&
      active_window->workout_controller->is_paused()) {
    if (active_window->confirming_finish) {
      active_window->confirming_finish = false;
      prv_update_ui(active_window);
    } else {
      prv_handle_stop_button(active_window);
    }
    return;
  }
  if (prv_is_heart_page(active_window) && !active_window->hr_plot_visible) {
    prv_set_hr_plot(active_window, true, true);
  } else {
    prv_cycle_scrollable_metrics(active_window);
    prv_update_ui(active_window);
  }
}

static void prv_click_config_provider(void *context) {
  WorkoutActiveWindow *window = context;
  if (window->layout == WorkoutLayout_Dashboard) {
    window_set_click_context(BUTTON_ID_BACK, context);
    window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
  }
  window_set_click_context(BUTTON_ID_UP, context);
  window_set_click_context(BUTTON_ID_SELECT, context);
  window_set_click_context(BUTTON_ID_DOWN, context);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

#ifdef CONFIG_TOUCH
static void prv_touch_handler(const TouchEvent *event, void *context) {
  WorkoutActiveWindow *active_window = context;
  if (event->type == TouchEvent_Touchdown) {
    active_window->touch_start = GPoint(event->x, event->y);
    active_window->touch_active = !event->non_navigational;
    return;
  }
  if (event->type != TouchEvent_Liftoff || !active_window->touch_active) {
    return;
  }
  active_window->touch_active = false;
  const int dx = event->x - active_window->touch_start.x;
  const int dy = event->y - active_window->touch_start.y;
  const bool paused = !active_window->finished && active_window->workout_controller->is_paused();
  if (ABS(dy) > 20 && ABS(dy) > ABS(dx)) {
    if (!paused) {
      if (dy > 0) {
        prv_up_click_handler(NULL, active_window);
      } else {
        prv_down_click_handler(NULL, active_window);
      }
    }
  } else if (ABS(dx) > 20 && ABS(dx) > ABS(dy)) {
    if (!paused) {
      if (dx > 0) {
        prv_previous_page(active_window);
      } else {
        prv_cycle_scrollable_metrics(active_window);
        prv_update_ui(active_window);
      }
    }
  } else if (ABS(dx) <= 20 && ABS(dy) <= 20) {
    const int h = active_window->window.layer.bounds.size.h;
    if (paused) {
      const int w = active_window->window.layer.bounds.size.w;
      if (event->x >= w - ACTION_BAR_WIDTH) {
        if (event->y >= 2 * h / 3) {
          prv_down_click_handler(NULL, active_window);
        } else if (event->y >= h / 3) {
          prv_select_click_handler(NULL, active_window);
        }
      }
    } else {
      prv_select_click_handler(NULL, active_window);
    }
  }
}

static void prv_window_appear_handler(Window *window) {
  WorkoutActiveWindow *active_window = window_get_user_data(window);
  if (active_window->layout == WorkoutLayout_Dashboard) {
    active_window->touch_active = false;
    touch_service_subscribe(prv_touch_handler, active_window);
  }
}

static void prv_window_disappear_handler(Window *window) {
  WorkoutActiveWindow *active_window = window_get_user_data(window);
  if (active_window->layout == WorkoutLayout_Dashboard) {
    touch_service_unsubscribe();
  }
}
#endif

static void prv_window_unload_handler(Window *window) {
  WorkoutActiveWindow *active_window = window_get_user_data(window);
  if (active_window) {
    prv_cancel_hr_plot_animation(active_window);
    app_free(active_window->hr_history);
    app_timer_cancel(active_window->update_timer);
    app_timer_cancel(active_window->hr_measuring_timer);
    gbitmap_destroy(active_window->action_bar_start);
    gbitmap_destroy(active_window->action_bar_pause);
    gbitmap_destroy(active_window->action_bar_stop);
    gbitmap_destroy(active_window->action_bar_more);
    gbitmap_destroy(active_window->action_bar_next);
    gbitmap_destroy(active_window->heart_icon);
    gbitmap_destroy(active_window->activity_icon);
    gbitmap_destroy(active_window->hr_measuring_icon);
    action_bar_layer_deinit(&active_window->action_bar);
    status_bar_layer_deinit(&active_window->status_layer);
    layer_deinit(&active_window->top_metric_layer);
    layer_deinit(&active_window->middle_metric_layer);
    layer_deinit(&active_window->scrollable_metric_layer);
    layer_deinit(&active_window->base_layer);
    window_deinit(&active_window->window);
    i18n_free_all(active_window);
    app_free(active_window);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Common Setup
static void prv_create_window_common(WorkoutActiveWindow *active_window, void *workout_data,
                                     WorkoutController *workout_controller) {
  active_window->workout_data = workout_data;
  active_window->workout_controller = workout_controller;

  Window *window = &active_window->window;
  window_init(window, WINDOW_NAME("Workout Active Info"));
  window_set_user_data(window, active_window);
  window_set_background_color(
      window, active_window->layout == WorkoutLayout_Dashboard ? GColorWhite : BACKGROUND_COLOR);
  window_set_window_handlers(window, &(WindowHandlers){
                                         .unload = prv_window_unload_handler,
#ifdef CONFIG_TOUCH
                                         .appear = prv_window_appear_handler,
                                         .disappear = prv_window_disappear_handler,
#endif
                                     });

  GRect base_layer_bounds = window->layer.bounds;
#if PBL_RECT
  if (active_window->layout != WorkoutLayout_Dashboard) {
    base_layer_bounds.size.w -= ACTION_BAR_WIDTH;
  }
#endif

  base_layer_bounds.origin.y =
      active_window->layout == WorkoutLayout_Dashboard ? 0 : STATUS_BAR_LAYER_HEIGHT;
  layer_init(&active_window->base_layer, &base_layer_bounds);
  layer_add_child(&window->layer, &active_window->base_layer);
  base_layer_bounds.origin.y = 0;

  if (active_window->layout == WorkoutLayout_Dashboard) {
    layer_set_update_proc(&active_window->base_layer, prv_dashboard_update_proc);
  } else if (active_window->layout == WorkoutLayout_SingleMetric) {
    // Only 1 metric to show. It can have the whole screen
    GRect metric_bounds = base_layer_bounds;
    layer_init(&active_window->top_metric_layer, &metric_bounds);
    layer_set_update_proc(&active_window->top_metric_layer, prv_static_layer_update_proc);
    layer_add_child(&active_window->base_layer, &active_window->top_metric_layer);
  } else if (active_window->layout == WorkoutLayout_StaticAndScrollable) {
    // Two metrics. 1 big static metric above a smaller scrollable metric
    GRect top_metric_bounds = base_layer_bounds;
#if PBL_DISPLAY_HEIGHT >= 200
    top_metric_bounds.size.h = PBL_IF_RECT_ELSE(105, 115);
#else
    top_metric_bounds.size.h = PBL_IF_RECT_ELSE(90, 77);
#endif
    layer_init(&active_window->top_metric_layer, &top_metric_bounds);
    layer_set_update_proc(&active_window->top_metric_layer, prv_static_layer_update_proc);
    layer_add_child(&active_window->base_layer, &active_window->top_metric_layer);

    GRect scrollable_metric_bounds = top_metric_bounds;
    scrollable_metric_bounds.origin.y = scrollable_metric_bounds.size.h;
    scrollable_metric_bounds.size.h =
        window->layer.bounds.size.h - scrollable_metric_bounds.origin.y;
    layer_init(&active_window->scrollable_metric_layer, &scrollable_metric_bounds);
    layer_set_update_proc(&active_window->scrollable_metric_layer,
                          prv_scrollable_layer_update_proc);
    layer_add_child(&active_window->base_layer, &active_window->scrollable_metric_layer);
  } else if (active_window->layout == WorkoutLayout_TwoStaticAndScrollable) {
    // Three metrics. Two static metrics above a scrollable metric
#if PBL_DISPLAY_HEIGHT >= 200
    const int layer_height = 68;
#else
    const int layer_height = 51;
#endif
    GRect top_metric_bounds = base_layer_bounds;
    top_metric_bounds.size.h = layer_height;
    layer_init(&active_window->top_metric_layer, &top_metric_bounds);
    layer_set_update_proc(&active_window->top_metric_layer, prv_static_layer_update_proc);
    layer_add_child(&active_window->base_layer, &active_window->top_metric_layer);

    GRect middle_metric_bounds = top_metric_bounds;
    middle_metric_bounds.origin.y = top_metric_bounds.size.h;
    middle_metric_bounds.size.h = layer_height - 2;
    layer_init(&active_window->middle_metric_layer, &middle_metric_bounds);
    layer_set_update_proc(&active_window->middle_metric_layer, prv_static_layer_update_proc);
    layer_add_child(&active_window->base_layer, &active_window->middle_metric_layer);

    GRect scrollable_metric_bounds = middle_metric_bounds;
    scrollable_metric_bounds.origin.y = top_metric_bounds.size.h + middle_metric_bounds.size.h;
    scrollable_metric_bounds.size.h = layer_height + 10;
    layer_init(&active_window->scrollable_metric_layer, &scrollable_metric_bounds);
    layer_set_update_proc(&active_window->scrollable_metric_layer,
                          prv_scrollable_layer_update_proc);
    layer_add_child(&active_window->base_layer, &active_window->scrollable_metric_layer);
  }

  StatusBarLayer *status_layer = &active_window->status_layer;
  status_bar_layer_init(status_layer);
  status_bar_layer_set_colors(status_layer, GColorClear, GColorBlack);
  if (active_window->layout == WorkoutLayout_Dashboard) {
    status_bar_layer_set_colors(status_layer, workout_activity_color(active_window->activity_type),
                                GColorBlack);
  }
  if (active_window->layout != WorkoutLayout_Dashboard) {
    layer_add_child(&window->layer, status_bar_layer_get_layer(status_layer));
  }

#if PBL_RECT
  GRect status_layer_bounds = window->layer.bounds;
  status_layer_bounds.size.w -= ACTION_BAR_WIDTH;
  layer_set_frame(&status_layer->layer, &status_layer_bounds);
#endif

  ActionBarLayer *action_bar = &active_window->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_set_context(action_bar, active_window);
  action_bar_layer_set_click_config_provider(action_bar, prv_click_config_provider);
  if (active_window->layout == WorkoutLayout_Dashboard) {
    window_set_click_config_provider_with_context(window, prv_click_config_provider, active_window);
  } else {
    action_bar_layer_add_to_window(action_bar, window);
  }

  if (active_window->layout == WorkoutLayout_Dashboard) {
    active_window->heart_icon = gbitmap_create_with_resource(RESOURCE_ID_WORKOUT_APP_HEART);
    active_window->activity_icon = gbitmap_create_with_resource(
        active_window->activity_type == ActivitySessionType_Run ? RESOURCE_ID_WORKOUT_APP_RUN_TINY
        : active_window->activity_type == ActivitySessionType_Walk
            ? RESOURCE_ID_WORKOUT_APP_WALK_TINY
            : RESOURCE_ID_WORKOUT_APP_HEART);
    active_window->action_bar_start =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_START);
    active_window->action_bar_stop = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_STOP);
    active_window->action_bar_more =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_CHECK);
    active_window->action_bar_next = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_X);
  } else {
    active_window->heart_icon = gbitmap_create_with_resource(RESOURCE_ID_WORKOUT_APP_HEART),
    active_window->hr_measuring_icon =
        gbitmap_create_with_resource(RESOURCE_ID_WORKOUT_APP_MEASURING_HR),

    active_window->action_bar_start =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_START);
    active_window->action_bar_pause =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_PAUSE);
    active_window->action_bar_stop = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_STOP);
    active_window->action_bar_more = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_MORE);
    active_window->action_bar_next =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_TOGGLE);
  }

  prv_set_pause_button(active_window);
  prv_set_action_bar_icons(active_window);

  active_window->update_timer = app_timer_register(1000, prv_update_timer_callback, active_window);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Public API

WorkoutActiveWindow *workout_active_create_single_layout(WorkoutMetricType metric,
                                                         void *workout_data,
                                                         WorkoutController *workout_controller) {
  if (metric == WorkoutMetricType_None) {
    PBL_LOG_ERR("Invalid argument");
    return NULL;
  }

  WorkoutActiveWindow *active_window = app_zalloc_check(sizeof(WorkoutActiveWindow));
  active_window->layout = WorkoutLayout_SingleMetric;

  active_window->top_metric = metric;

  prv_create_window_common(active_window, workout_data, workout_controller);

  return active_window;
}

WorkoutActiveWindow *workout_active_create_double_layout(WorkoutMetricType top_metric,
                                                         int num_scrollable_metrics,
                                                         WorkoutMetricType *scrollable_metrics,
                                                         void *workout_data,
                                                         WorkoutController *workout_controller) {
  if (top_metric == WorkoutMetricType_None || num_scrollable_metrics == 0 || !scrollable_metrics) {
    PBL_LOG_ERR("Invalid argument(s)");
    return NULL;
  }

  WorkoutActiveWindow *active_window = app_zalloc_check(sizeof(WorkoutActiveWindow));
  active_window->layout = WorkoutLayout_StaticAndScrollable;

  active_window->top_metric = top_metric;
  prv_add_scrollable_metrics(active_window, num_scrollable_metrics, scrollable_metrics);

  prv_create_window_common(active_window, workout_data, workout_controller);

  return active_window;
}

WorkoutActiveWindow *workout_active_create_triple_layout(WorkoutMetricType top_metric,
                                                         WorkoutMetricType middle_metric,
                                                         int num_scrollable_metrics,
                                                         WorkoutMetricType *scrollable_metrics,
                                                         void *workout_data,
                                                         WorkoutController *workout_controller) {
  if (top_metric == WorkoutMetricType_None || middle_metric == WorkoutMetricType_None ||
      (num_scrollable_metrics != 0 && !scrollable_metrics)) {
    PBL_LOG_ERR("Invalid argument(s)");
    return NULL;
  }

  WorkoutActiveWindow *active_window = app_zalloc_check(sizeof(WorkoutActiveWindow));
  active_window->layout = WorkoutLayout_TwoStaticAndScrollable;

  active_window->top_metric = top_metric;
  active_window->middle_metric = middle_metric;
  prv_add_scrollable_metrics(active_window, num_scrollable_metrics, scrollable_metrics);

  prv_create_window_common(active_window, workout_data, workout_controller);

  return active_window;
}

WorkoutActiveWindow *workout_active_create_for_activity_type(
    ActivitySessionType type, void *workout_data, WorkoutController *workout_controller) {
  if (type != ActivitySessionType_Open && type != ActivitySessionType_Walk &&
      type != ActivitySessionType_Run) {
    return NULL;
  }
  WorkoutActiveWindow *active_window = app_zalloc_check(sizeof(WorkoutActiveWindow));
  active_window->layout = WorkoutLayout_Dashboard;
  active_window->activity_type = type;
  active_window->has_hrm = activity_is_hrm_present() && activity_prefs_heart_rate_is_enabled();
  active_window->num_scrollable_metrics =
      (type == ActivitySessionType_Open ? 1 : 3) + (active_window->has_hrm ? 1 : 0);
  if (active_window->has_hrm) {
    active_window->hr_history = app_zalloc_check(sizeof(WorkoutHrHistory));
  }
  prv_create_window_common(active_window, workout_data, workout_controller);
  prv_record_hr_sample(active_window);
  return active_window;
}

void workout_active_window_push(WorkoutActiveWindow *active_window) {
  app_window_stack_push(&active_window->window, active_window->layout != WorkoutLayout_Dashboard);
}

void workout_active_update_scrollable_metrics(WorkoutActiveWindow *active_window,
                                              int num_scrollable_metrics,
                                              WorkoutMetricType *scrollable_metrics) {
  active_window->num_scrollable_metrics = 0;
  prv_add_scrollable_metrics(active_window, num_scrollable_metrics, scrollable_metrics);

  prv_set_pause_button(active_window);

  if (active_window->current_scrollable_metric >= active_window->num_scrollable_metrics) {
    active_window->current_scrollable_metric = 0;
  }

  prv_update_ui(active_window);
}
