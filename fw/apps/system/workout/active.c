/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "active.h"

#include "hr_history.h"
#include "style.h"
#include "summary.h"
#include "workout.h"

#include <applib/app.h>
#include <applib/ui/action_menu_window.h>
#include <applib/ui/animation_interpolate.h>
#include <applib/ui/ui.h>
#include <applib/ui/window_manager.h>
#include <apps/system/timeline/text_node.h>
#include <kernel/pbl_malloc.h>
#ifdef CONFIG_TOUCH
#include <applib/touch_service.h>
#endif
#include <stdio.h>

#include <pbl/logging/logging.h>
#include <pbl/services/activity/activity_private.h>
#include <pbl/services/activity/health_util.h>
#include <pbl/services/activity/hr_util.h>
#include <pbl/services/activity/workout_service.h>
#include <pbl/services/clock.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/testing.h>
#include <pbl/util/trig.h>
#include <pbl/util/units.h>

#include <resource/resource_ids.auto.h>

#define TEXT_COLOR       (GColorBlack)
#define TEXT_ALIGNMENT   (PBL_IF_RECT_ELSE(GTextAlignmentLeft, GTextAlignmentRight))
#define BACKGROUND_COLOR PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite)

typedef enum WorkoutLayout {
  WorkoutLayout_Dashboard,
  WorkoutLayout_TwoStaticAndScrollable,
} WorkoutLayout;

typedef struct WorkoutActiveWindow {
  Window window;
#ifdef CONFIG_TOUCH
  GPoint touch_start;
  bool touch_active;
  AppTimer *long_press_timer;
#endif
  ActionBarLayer action_bar;
  StatusBarLayer status_layer;
  Layer base_layer;
  Layer top_metric_layer;
  Layer middle_metric_layer;
  Layer scrollable_metric_layer;

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
  bool has_spo2;

  AppTimer *beat_timer;
  bool beat;
  int last_hr_samples;
  Animation *bars_animation;
  AnimationProgress bars_progress;
  Animation *confetti_animation;
  AnimationProgress confetti_progress;

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
  GBitmap *action_bar_next;
  GBitmap *action_bar_confirm;
  GBitmap *action_bar_cancel;

  AppTimer *update_timer;
  AppTimer *hr_measuring_timer;

  int cur_hr_measuring_width_idx;
} WorkoutActiveWindow;

static void prv_click_config_provider(void *context);
PBL_T_STATIC void prv_finish_workout(WorkoutActiveWindow *active_window);

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

static GFont prv_get_number_font(void) {
#if PBL_DISPLAY_HEIGHT >= 200
  return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
#else
  return fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
#endif
}

//! Font for durations that include hours ("HH:MM:SS"). On a narrow rectangular
//! display the regular reduced number font (LECO_42) is too wide for 8 chars and
//! the seconds get clipped, so use a narrower font there.
static GFont prv_get_duration_font(void) {
#if PBL_DISPLAY_HEIGHT >= 200 && PBL_RECT
  return fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
#else
  return prv_get_number_font();
#endif
}

static GTextNode *prv_create_text_node(WorkoutActiveWindow *active_window,
                                       WorkoutMetricType metric_type, void *i18n_owner) {
  GTextNodeHorizontal *horiz_container = graphics_text_node_create_horizontal(MAX_TEXT_NODES);
  GTextNodeContainer *container = &horiz_container->container;
  horiz_container->horizontal_alignment = TEXT_ALIGNMENT;

  const GFont number_font = prv_get_number_font();
  const GFont units_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  const int units_offset_y = fonts_get_font_height(number_font) - fonts_get_font_height(units_font);

  switch (metric_type) {
    case WorkoutMetricType_Hr: {
      GPoint heart_node_offset = GPoint(2, 0);
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
    case WorkoutMetricType_Steps: {
      const size_t buffer_size = sizeof("000000");
      GTextNodeText *number_text_node =
          health_util_create_text_node(buffer_size, number_font, TEXT_COLOR, container);
      active_window->workout_controller->metric_to_string(
          metric_type, (char *)number_text_node->text, buffer_size, i18n_owner,
          active_window->workout_data);
      break;
    }
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
        number_text_node->font = prv_get_number_font();
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
              metric_type, active_window->workout_data) >= PBL_SEC_PER_HOUR) {
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
    // Dashboard only
    case WorkoutMetricType_AvgSpeed:
    case WorkoutMetricType_AvgCadence:
    case WorkoutMetricType_ActiveCalories:
    case WorkoutMetricType_AvgHr:
    case WorkoutMetricType_SpO2:
    case WorkoutMetricType_SpO2Min:
    case WorkoutMetricType_HrZone0Time:
    case WorkoutMetricType_HrZone1Time:
    case WorkoutMetricType_HrZone2Time:
    case WorkoutMetricType_HrZone3Time:
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
                                active_window->confirming_finish ? active_window->action_bar_confirm
                                                                 : active_window->action_bar_start);
      action_bar_layer_set_icon(bar, BUTTON_ID_DOWN,
                                active_window->confirming_finish ? active_window->action_bar_cancel
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
  action_bar_layer_set_icon(action_bar, active_window->pause_button,
                            active_window->workout_controller->is_paused()
                                ? active_window->action_bar_start
                                : active_window->action_bar_pause);
  if (active_window->num_scrollable_metrics > 1) {
    action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, active_window->action_bar_next);
  }
}

static void prv_update_ui(WorkoutActiveWindow *active_window) {
  if (!active_window->workout_controller->is_paused()) {
    // Resuming, by button or by another client, leaves the end-workout confirmation.
    active_window->confirming_finish = false;
  }
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

PBL_T_STATIC void prv_record_hr_sample(WorkoutActiveWindow *window) {
  if (!window->hr_history || window->finished || window->workout_controller->is_paused()) {
    return;
  }
  const int elapsed = window->workout_controller->get_metric_value(WorkoutMetricType_Duration,
                                                                   window->workout_data);
  const int bpm =
      window->workout_controller->get_metric_value(WorkoutMetricType_Hr, window->workout_data);
  workout_hr_history_add(window->hr_history, elapsed, bpm);
}

static void prv_hr_plot_animation_update(Animation *animation, AnimationProgress progress) {
  WorkoutActiveWindow *window = animation_get_context(animation);
  const int target = window->hr_plot_visible ? ANIMATION_NORMALIZED_MAX : 0;
  window->hr_plot_progress = interpolate_int64(progress, window->hr_plot_start, target);
  layer_mark_dirty(&window->base_layer);
}

static void prv_hr_plot_animation_stopped(Animation *animation, bool finished, void *context) {
  if (!finished) {
    return;
  }
  WorkoutActiveWindow *window = context;
  window->hr_plot_animation = nullptr;
  window->hr_plot_progress = window->hr_plot_visible ? ANIMATION_NORMALIZED_MAX : 0;
  layer_mark_dirty(&window->base_layer);
}

PBL_T_STATIC void prv_set_hr_plot(WorkoutActiveWindow *window, bool visible, bool animated) {
  if (!window->hr_history) {
    return;
  }
  workout_cancel_animation(&window->hr_plot_animation);
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

static void prv_bars_animation_update(Animation *animation, AnimationProgress progress) {
  WorkoutActiveWindow *window = animation_get_context(animation);
  window->bars_progress = progress;
  layer_mark_dirty(&window->base_layer);
}

static void prv_bars_animation_stopped(Animation *animation, bool finished, void *context) {
  WorkoutActiveWindow *window = context;
  if (window->bars_animation == animation) {
    window->bars_animation = nullptr;
    window->bars_progress = ANIMATION_NORMALIZED_MAX;
  }
}

static void prv_grow_bars(WorkoutActiveWindow *window) {
  workout_cancel_animation(&window->bars_animation);
  window->bars_progress = ANIMATION_NORMALIZED_MAX;
  Animation *animation = animation_create();
  if (!animation) {
    return;
  }
  static const AnimationImplementation implementation = {.update = prv_bars_animation_update};
  animation_set_implementation(animation, &implementation);
  animation_set_duration(animation, 480);
  animation_set_curve(animation, AnimationCurveEaseOut);
  animation_set_handlers(animation, (AnimationHandlers){.stopped = prv_bars_animation_stopped},
                         window);
  window->bars_animation = animation;
  window->bars_progress = 0;
  animation_schedule(animation);
}

// Bars rise one after another.
static int prv_bar_progress(WorkoutActiveWindow *window, int index) {
  const int delay = index * ANIMATION_NORMALIZED_MAX / 8;
  const int span = ANIMATION_NORMALIZED_MAX - 3 * ANIMATION_NORMALIZED_MAX / 8;
  return CLIP((int64_t)(window->bars_progress - delay) * ANIMATION_NORMALIZED_MAX / span, 0,
              ANIMATION_NORMALIZED_MAX);
}

#if PBL_COLOR
static void prv_confetti_animation_update(Animation *animation, AnimationProgress progress) {
  WorkoutActiveWindow *window = animation_get_context(animation);
  window->confetti_progress = progress;
  layer_mark_dirty(&window->base_layer);
}

static void prv_confetti_animation_stopped(Animation *animation, bool finished, void *context) {
  WorkoutActiveWindow *window = context;
  if (window->confetti_animation == animation) {
    window->confetti_animation = nullptr;
    layer_mark_dirty(&window->base_layer);
  }
}

#endif

static void prv_throw_confetti(WorkoutActiveWindow *window) {
#if PBL_COLOR
  workout_cancel_animation(&window->confetti_animation);
  Animation *animation = animation_create();
  if (!animation) {
    return;
  }
  static const AnimationImplementation implementation = {.update = prv_confetti_animation_update};
  animation_set_implementation(animation, &implementation);
  animation_set_duration(animation, 1600);
  animation_set_curve(animation, AnimationCurveLinear);
  animation_set_handlers(animation, (AnimationHandlers){.stopped = prv_confetti_animation_stopped},
                         window);
  window->confetti_animation = animation;
  window->confetti_progress = 0;
  animation_schedule(animation);
#endif
}

static void prv_beat_timer_callback(void *data) {
  WorkoutActiveWindow *window = data;
  window->beat_timer = nullptr;
  window->beat = false;
  layer_mark_dirty(&window->base_layer);
}

typedef enum DashboardPage {
  DashboardPage_Main,
  DashboardPage_Grid,
  DashboardPage_Zones,
  DashboardPage_Gauge,
} DashboardPage;

static DashboardPage prv_dashboard_page(WorkoutActiveWindow *window) {
  const bool open = window->activity_type == ActivitySessionType_Open;
  int page = window->current_scrollable_metric;
  if (page == 0) {
    return DashboardPage_Main;
  }
  if (!open && page == 1) {
    return DashboardPage_Grid;
  }
  page -= open ? 1 : 2;
  return page == 0 ? DashboardPage_Zones : DashboardPage_Gauge;
}

// The heart is drawn on the main page's band and on the gauge.
static bool prv_heart_visible(WorkoutActiveWindow *window) {
  if (!window->has_hrm) {
    return false;
  }
  const DashboardPage page = prv_dashboard_page(window);
  return page == DashboardPage_Main || (page == DashboardPage_Gauge && !window->hr_plot_visible);
}

// One beat of the heart for each new heart rate reading. Called from the one second update, so
// it never beats more than once a second. Each beat is a full redraw, so only where it shows.
PBL_T_STATIC bool prv_beat(WorkoutActiveWindow *window) {
  if (!window->workout_controller->get_hr_samples) {
    return false;
  }
  const int samples = window->workout_controller->get_hr_samples(window->workout_data);
  const bool new_reading = samples != window->last_hr_samples;
  window->last_hr_samples = samples;
  const int bpm =
      window->workout_controller->get_metric_value(WorkoutMetricType_Hr, window->workout_data);
  if (!new_reading || bpm <= 0 || window->beat_timer || window->finished ||
      window->workout_controller->is_paused() || !prv_heart_visible(window)) {
    return false;
  }
  window->beat = true;
  window->beat_timer = app_timer_register(140, prv_beat_timer_callback, window);
  return true;
}

static void prv_page_changed(WorkoutActiveWindow *window) {
  prv_grow_bars(window);
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
    active_window->hr_measuring_timer = nullptr;
  }
}

PBL_T_STATIC void prv_update_timer_callback(void *data) {
  WorkoutActiveWindow *active_window = data;
  if (active_window->finished) {
    return;
  }

  if (active_window->workout_controller->update_data &&
      !active_window->workout_controller->update_data(active_window->workout_data)) {
    // Ended by the service, e.g. from another client: on to the recap.
    prv_finish_workout(active_window);
    prv_update_ui(active_window);
    return;
  }

  prv_record_hr_sample(active_window);
  prv_update_ui(active_window);
  active_window->update_timer = app_timer_register(1000, prv_update_timer_callback, active_window);
  if (active_window->layout == WorkoutLayout_Dashboard) {
    prv_beat(active_window);
  }

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
                     TEXT_ALIGNMENT, nullptr);
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
                              GColor bg_color, bool draw_hr_zones) {
  WorkoutActiveWindow *active_window = window_get_user_data(layer_get_window(layer));

  prv_render_bg_color(ctx, &layer->bounds, bg_color);

  const int16_t rl_margin = PBL_IF_RECT_ELSE(5, 23);

  GRect rect = grect_inset(layer->bounds, GEdgeInsets(0, rl_margin));
  rect.origin.y = (&active_window->scrollable_metric_layer == layer) ? PBL_IF_RECT_ELSE(-2, 0)
                                                                     : PBL_IF_RECT_ELSE(-4, -2);

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
  rect.origin.y += 15;

  if (draw_hr_zones) {
    prv_render_hr_zones(ctx, &rect, active_window);
    // update rect y for the zones height
    rect.origin.y += PBL_IF_RECT_ELSE(18, 15);
  }

  // adjust rect for drawing the text node
  rect.origin.x -= PBL_IF_RECT_ELSE(1, 46);
  rect.size.w += (rl_margin * 2);

  GTextNode *text_node = prv_create_text_node(active_window, metric_type, layer);
  graphics_text_node_draw(text_node, ctx, &rect, nullptr, nullptr);
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

  prv_render_metric(ctx, metric_type, layer, bg_color, draw_zones);

  if (layer == &active_window->top_metric_layer) {
    status_bar_layer_set_colors(&active_window->status_layer, bg_color, GColorBlack);
  }

  if (layer == &active_window->middle_metric_layer) {
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
  prv_render_metric(ctx, metric_type, layer, bg_color, draw_hr_zones);
}

static void prv_dashboard_text(GContext *ctx, const char *text, const char *font_key, GRect box,
                               GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key), box,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, nullptr);
}

static GRect prv_dashboard_value(GContext *ctx, WorkoutActiveWindow *active_window,
                                 WorkoutMetricType metric, GRect box, const char *font_key,
                                 GColor color) {
  char text[24] = {};
  const int preferred_height = fonts_get_font_height(fonts_get_system_font(font_key));
  active_window->workout_controller->metric_to_string(metric, text, sizeof(text), active_window,
                                                      active_window->workout_data);
  GSize text_size = graphics_text_layout_get_max_used_size(
      ctx, text, fonts_get_system_font(font_key), GRect(0, 0, 1000, 100), GTextOverflowModeFill,
      GTextAlignmentLeft, nullptr);
  const char *fallbacks[] = {
    FONT_KEY_LECO_42_NUMBERS, FONT_KEY_LECO_32_BOLD_NUMBERS, FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
    FONT_KEY_LECO_20_BOLD_NUMBERS, FONT_KEY_GOTHIC_18_BOLD
  };
  for (size_t i = 0; text_size.w > box.size.w && i < ARRAY_LENGTH(fallbacks); i++) {
    if (fonts_get_font_height(fonts_get_system_font(fallbacks[i])) >= preferred_height) {
      continue;
    }
    font_key = fallbacks[i];
    text_size = graphics_text_layout_get_max_used_size(
        ctx, text, fonts_get_system_font(font_key), GRect(0, 0, 1000, 100), GTextOverflowModeFill,
        GTextAlignmentLeft, nullptr);
  }

  // Keep LECO digits visually centered when fitting a smaller font.
  box.origin.y +=
      (preferred_height - fonts_get_font_height(fonts_get_system_font(font_key))) * 2 / 3;
  prv_dashboard_text(ctx, text, font_key, box, color);
  box.origin.x += (box.size.w - text_size.w) / 2;
  box.size.w = text_size.w;
  return box;
}

#define INK_COLOR  GColorWhite
#define DIM_COLOR  PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define RULE_COLOR PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite)

static GColor prv_zone_color(HRZone zone, bool dim) {
#if PBL_COLOR
  switch (zone) {
    case HRZone_Zone1:
      return dim ? GColorDarkGreen : GColorGreen;
    case HRZone_Zone2:
      return dim ? GColorWindsorTan : GColorOrange;
    case HRZone_Zone3:
      return dim ? GColorDarkCandyAppleRed : GColorRed;
    default:
      return dim ? GColorCobaltBlue : GColorPictonBlue;
  }
#else
  return GColorWhite;
#endif
}

static GColor prv_metric_color(WorkoutMetricType metric) {
#if PBL_COLOR
  switch (metric) {
    case WorkoutMetricType_Duration:
      return GColorYellow;
    case WorkoutMetricType_Distance:
      return GColorCyan;
    case WorkoutMetricType_AvgPace:
    case WorkoutMetricType_AvgSpeed:
      return GColorGreen;
    case WorkoutMetricType_AvgCadence:
    case WorkoutMetricType_Steps:
      return GColorShockingPink;
    case WorkoutMetricType_ActiveCalories:
      return GColorOrange;
    case WorkoutMetricType_Hr:
    case WorkoutMetricType_AvgHr:
      return GColorRed;
    case WorkoutMetricType_SpO2:
    case WorkoutMetricType_SpO2Min:
      return GColorVividCerulean;
    default:
      break;
  }
#endif
  return DIM_COLOR;
}

// Row of the given height across the available width, narrowed to what the round display
// shows at its middle.
static GRect prv_row_box(GSize size, int avail, int inset, int y, int h) {
  int w = avail - inset;
#if PBL_ROUND
  const int r = size.w / 2;
  const int dy = y + h / 2 - size.h / 2;
  w = MIN(w, 2 * integer_sqrt(MAX(0, r * r - dy * dy)) - 16);
#endif
  return GRect((avail - w) / 2, y, w, h);
}

static void prv_draw_rule(GContext *ctx, GPoint from, GPoint to) {
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, RULE_COLOR);
  graphics_draw_line(ctx, from, to);
}

// Labelled data field, as on sports watch data screens.
static void prv_draw_field(GContext *ctx, WorkoutActiveWindow *window, GRect box,
                           WorkoutMetricType metric, const char *label, const char *font_key,
                           GColor color) {
  const bool large = window->window.layer.bounds.size.h >= 200;
  prv_dashboard_text(ctx, i18n_get(label, window), FONT_KEY_GOTHIC_14_BOLD,
                     GRect(box.origin.x, box.origin.y, box.size.w, 18), prv_metric_color(metric));
  const int font_h = fonts_get_font_height(fonts_get_system_font(font_key));
  const int label_h = large ? 15 : 12;
  const int value_y = box.origin.y + label_h + (box.size.h - label_h - font_h) / 2 - font_h / 6;
  prv_dashboard_value(ctx, window, metric, GRect(box.origin.x, value_y, box.size.w, font_h + 8),
                      font_key, color);
}

typedef struct DashboardField {
  WorkoutMetricType metric;
  const char *label;
} DashboardField;

static void prv_draw_field_row(GContext *ctx, WorkoutActiveWindow *window, GRect row,
                               const DashboardField *fields, int count, const char *font_key) {
  const int field_w = row.size.w / count;
  for (int i = 0; i < count; i++) {
    prv_draw_field(ctx, window,
                   GRect(row.origin.x + i * field_w, row.origin.y, field_w, row.size.h),
                   fields[i].metric, fields[i].label, font_key, INK_COLOR);
    if (i > 0) {
      const int x = row.origin.x + i * field_w;
      prv_draw_rule(ctx, GPoint(x, row.origin.y + 4), GPoint(x, row.origin.y + row.size.h - 4));
    }
  }
}

static int prv_dashboard_bpm(WorkoutActiveWindow *window) {
  return window->workout_controller->get_metric_value(
      window->finished ? WorkoutMetricType_AvgHr : WorkoutMetricType_Hr, window->workout_data);
}

static void prv_dashboard_bpm_string(WorkoutActiveWindow *window, char *buffer, size_t size) {
  window->workout_controller->metric_to_string(
      window->finished ? WorkoutMetricType_AvgHr : WorkoutMetricType_Hr, buffer, size, window,
      window->workout_data);
}

// Heart rate position across the four zones, in thousandths of the scale.
static int prv_zone_position(int bpm) {
  const int bounds[] = {
    activity_prefs_heart_get_resting_hr(),      activity_prefs_heart_get_zone1_threshold(),
    activity_prefs_heart_get_zone2_threshold(), activity_prefs_heart_get_zone3_threshold(),
    activity_prefs_heart_get_max_hr(),
  };
  if (bpm <= bounds[0]) {
    return 0;
  }
  for (int i = 0; i < 4; i++) {
    if (bpm < bounds[i + 1]) {
      return (i * 1000 + 1000 * (bpm - bounds[i]) / MAX(1, bounds[i + 1] - bounds[i])) / 4;
    }
  }
  return 1000;
}

static void prv_dashboard_heart_icon(GContext *ctx, WorkoutActiveWindow *window, GRect box,
                                     GColor color) {
  if (window->beat) {
    box = grect_inset(box, GEdgeInsets(-MAX(2, box.size.w / 8)));
  }
  workout_fill_heart(ctx, box, color);
}

static void prv_draw_status(GContext *ctx, WorkoutActiveWindow *window, GSize size, int w) {
  const bool paused = !window->finished && window->workout_controller->is_paused();
  const int status_h = workout_status_height(size);
  const int center_y = PBL_IF_ROUND_ELSE(status_h - 15, (status_h - 2) / 2);
  const int text_y = center_y - 12;
  const char *banner = nullptr;
  GColor banner_color = GColorWhite;
  GColor banner_ink = GColorBlack;
  if (window->finished) {
    banner = i18n_noop("COMPLETE");
    banner_color = workout_activity_color(window->activity_type);
  } else if (paused && window->confirming_finish) {
    banner = i18n_noop("END WORKOUT?");
    banner_color = PBL_IF_COLOR_ELSE(GColorRed, GColorWhite);
    banner_ink = PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack);
  } else if (paused) {
    banner = i18n_noop("PAUSED");
    banner_color = PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite);
  }
  if (banner) {
    graphics_context_set_fill_color(ctx, banner_color);
    graphics_fill_rect(ctx, &GRect(0, 0, w, status_h));
    prv_dashboard_text(ctx, i18n_get(banner, window), FONT_KEY_GOTHIC_18_BOLD,
                       GRect(0, text_y, w, 24), banner_ink);
    return;
  }

  char clock[16];
  clock_copy_time_string(clock, sizeof(clock));
  const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const GSize clock_size = graphics_text_layout_get_max_used_size(
      ctx, clock, font, GRect(0, 0, w, 24), GTextOverflowModeFill, GTextAlignmentLeft, nullptr);
  const int clock_x = (w - clock_size.w) / 2;
  prv_dashboard_text(ctx, clock, FONT_KEY_GOTHIC_18_BOLD, GRect(0, text_y, w, 24), INK_COLOR);

  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  graphics_fill_circle(ctx, GPoint(clock_x - 9, center_y), 3);

  if (size.h >= 200) {
    const GSize icon_size = gbitmap_get_bounds(window->activity_icon).size;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(
        ctx, window->activity_icon,
        &GRect(clock_x + clock_size.w + 6, center_y - icon_size.h / 2, icon_size.w, icon_size.h));
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  }

  const GRect rule = workout_heading_rule(GSize(w, size.h));
  graphics_context_set_fill_color(ctx, workout_activity_color(window->activity_type));
  graphics_fill_rect(ctx, &rule);
}

// Heart rate field, filled with the color of the current zone.
static void prv_draw_hr_band(GContext *ctx, WorkoutActiveWindow *window, GSize size, GRect band) {
  const bool large = size.h >= 200;
  const int bpm = prv_dashboard_bpm(window);
  const HRZone zone = hr_util_get_hr_zone(bpm);
  graphics_context_set_fill_color(
      ctx, bpm > 0 ? prv_zone_color(zone, false) : PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, &band);

  const GRect row = prv_row_box(size, band.size.w, 8, band.origin.y, large ? 44 : 34);
  const char *font = large ? FONT_KEY_LECO_38_BOLD_NUMBERS : FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM;
  const int heart_h = large ? 20 : 14;
  const int value_y = band.origin.y + (large ? 4 : 3);
  char value[12];
  prv_dashboard_bpm_string(window, value, sizeof(value));
  const GSize value_size = graphics_text_layout_get_max_used_size(
      ctx, value, fonts_get_system_font(font), GRect(0, 0, row.size.w, 50), GTextOverflowModeFill,
      GTextAlignmentLeft, nullptr);
  const char *label = window->finished ? i18n_noop("AVG") : i18n_noop("BPM");
  char badge[12] = {};
  if (bpm > 0) {
    snprintf(badge, sizeof(badge), i18n_get("Z%d", window), (int)zone + 1);
  }
  const int badge_w = large ? 34 : 26;
  const int group_w = heart_h + 6 + value_size.w + 8 + badge_w;
  const int x = row.origin.x + (row.size.w - group_w) / 2;
  const int center_y = value_y + (large ? 22 : 15);
  prv_dashboard_heart_icon(ctx, window, GRect(x, center_y - heart_h / 2, heart_h, heart_h),
                           GColorBlack);
  prv_dashboard_text(ctx, value, font, GRect(x + heart_h + 6, value_y, value_size.w + 2, 50),
                     GColorBlack);
  const int badge_x = x + heart_h + 6 + value_size.w + 8;
  prv_dashboard_text(ctx, badge, large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
                     GRect(badge_x, center_y - (large ? 17 : 13), badge_w, 28), GColorBlack);
  prv_dashboard_text(ctx, i18n_get(label, window), FONT_KEY_GOTHIC_14_BOLD,
                     GRect(badge_x - 6, center_y + (large ? 4 : 2), badge_w + 12, 18), GColorBlack);

  // Zone scale with a marker at the exact heart rate.
  const GRect scale = prv_row_box(size, band.size.w, large ? 40 : 24,
                                  band.origin.y + (large ? 52 : 40), large ? 6 : 5);
  const int segment_w = (scale.size.w - 9) / 4;
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int i = 0; i < 4; i++) {
    const int grown = segment_w * prv_bar_progress(window, i) / ANIMATION_NORMALIZED_MAX;
    GRect segment =
        GRect(scale.origin.x + i * (segment_w + 3), scale.origin.y, MAX(1, grown), scale.size.h);
    if (bpm > 0 && i == (int)zone) {
      graphics_fill_rect(ctx, &segment);
    } else {
      graphics_draw_rect(ctx, &segment);
    }
  }
  if (bpm > 0) {
    const int marker_x = scale.origin.x + (scale.size.w - 1) * prv_zone_position(bpm) / 1000;
    const int marker_y = scale.origin.y - 2;
    const int r = window->beat ? 5 : 4;
    GPoint points[] = {
      {marker_x - r, marker_y - r},
      {marker_x + r, marker_y - r},
      {marker_x, marker_y}
    };
    GPath path = {.num_points = ARRAY_LENGTH(points), .points = points};
    gpath_draw_filled(ctx, &path);
  }
}

static void prv_draw_main(GContext *ctx, WorkoutActiveWindow *window, GSize size, int w, int top) {
  const bool large = size.h >= 200;
  const bool open = window->activity_type == ActivitySessionType_Open;
  const int area = size.h - top;
  const int timer_h = area * 35 / 100;
  const int pair_h = area * 29 / 100;

  const bool paused = !window->finished && window->workout_controller->is_paused();
  GRect row = prv_row_box(size, w, 0, top, timer_h);
  prv_draw_field(ctx, window, GRect(0, row.origin.y + 2, w, row.size.h), WorkoutMetricType_Duration,
                 window->finished ? i18n_noop("TOTAL TIME") : i18n_noop("TIMER"),
                 large ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_32_BOLD_NUMBERS,
                 paused ? PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite) : INK_COLOR);

  const int pair_y = top + timer_h;
  row = prv_row_box(size, w, 0, pair_y, pair_h);
  prv_draw_rule(ctx, GPoint(row.origin.x, pair_y), GPoint(row.origin.x + row.size.w, pair_y));
  const char *distance =
      window->workout_controller->get_distance_string(i18n_noop("DIST MI"), i18n_noop("DIST KM"));
  const char *pace =
      window->workout_controller->get_distance_string(i18n_noop("PACE /MI"), i18n_noop("PACE /KM"));
  const DashboardField pair[] = {
    open ? (DashboardField){WorkoutMetricType_Steps, i18n_noop("STEPS")}
         : (DashboardField){WorkoutMetricType_Distance, distance},
    open ? (DashboardField){WorkoutMetricType_ActiveCalories, i18n_noop("KCAL")}
         : (DashboardField){WorkoutMetricType_AvgPace, pace},
  };
  const char *pair_font =
      large ? FONT_KEY_LECO_32_BOLD_NUMBERS : FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM;
  prv_draw_field_row(ctx, window, row, pair, 2, pair_font);

  const int band_y = pair_y + pair_h;
  if (window->has_hrm) {
    prv_draw_hr_band(ctx, window, size, GRect(0, band_y + 1, w, size.h - band_y - 1));
    return;
  }
  row = prv_row_box(size, w, 0, band_y, size.h - band_y - PBL_IF_ROUND_ELSE(20, 4));
  prv_draw_rule(ctx, GPoint(row.origin.x, band_y), GPoint(row.origin.x + row.size.w, band_y));
  const DashboardField last[] = {
    open ? (DashboardField){WorkoutMetricType_AvgCadence, i18n_noop("CADENCE")}
         : (DashboardField){WorkoutMetricType_Steps, i18n_noop("STEPS")},
    {WorkoutMetricType_ActiveCalories, i18n_noop("KCAL")},
  };
  prv_draw_field_row(ctx, window, row, last, open ? 1 : 2, pair_font);
}

static void prv_draw_grid(GContext *ctx, WorkoutActiveWindow *window, GSize size, int w, int top) {
  const bool large = size.h >= 200;
  const int bottom = size.h - PBL_IF_ROUND_ELSE(14, 2);
  const int row_h = (bottom - top) / 3;
  const char *speed = window->workout_controller->get_distance_string(i18n_noop("SPEED MPH"),
                                                                      i18n_noop("SPEED KM/H"));
  const char *pace =
      window->workout_controller->get_distance_string(i18n_noop("PACE /MI"), i18n_noop("PACE /KM"));
  DashboardField last = {
    WorkoutMetricType_Duration, window->finished ? i18n_noop("TOTAL TIME") : i18n_noop("TIMER")
  };
  if (window->finished && window->has_spo2) {
    last = (DashboardField){WorkoutMetricType_SpO2Min, i18n_noop("LOW SPO2 %")};
  } else if (window->has_hrm) {
    last = (DashboardField){WorkoutMetricType_AvgHr, i18n_noop("AVG BPM")};
  }
  const DashboardField fields[] = {
    {WorkoutMetricType_AvgPace, pace},
    {WorkoutMetricType_AvgSpeed, speed},
    {WorkoutMetricType_AvgCadence, i18n_noop("CADENCE")},
    {WorkoutMetricType_Steps, i18n_noop("STEPS")},
    {WorkoutMetricType_ActiveCalories, i18n_noop("KCAL")},
    last,
  };
  for (int i = 0; i < 3; i++) {
    const int y = top + i * row_h;
    const GRect row = prv_row_box(size, w, 0, y, row_h);
    if (i > 0) {
      prv_draw_rule(ctx, GPoint(row.origin.x, y), GPoint(row.origin.x + row.size.w, y));
    }
    prv_draw_field_row(ctx, window, row, &fields[2 * i], 2,
                       large ? FONT_KEY_LECO_32_BOLD_NUMBERS : FONT_KEY_LECO_20_BOLD_NUMBERS);
  }
}

// Time spent in each zone, hardest zone on top.
static void prv_draw_zones(GContext *ctx, WorkoutActiveWindow *window, GSize size, int w, int top) {
  const bool large = size.h >= 200;
  prv_dashboard_text(ctx, i18n_get(i18n_noop("TIME IN ZONE"), window), FONT_KEY_GOTHIC_14_BOLD,
                     GRect(0, top + 2, w, 18), DIM_COLOR);
  int times[HRZoneCount];
  int longest = 1;
  for (int i = 0; i < HRZoneCount; i++) {
    times[i] = window->workout_controller->get_metric_value(WorkoutMetricType_HrZone0Time + i,
                                                            window->workout_data);
    longest = MAX(longest, times[i]);
  }
  const int bpm = prv_dashboard_bpm(window);
  const int current = window->finished || bpm <= 0 ? -1 : (int)hr_util_get_hr_zone(bpm);
  const int rows_top = top + (large ? 24 : 18);
  const int row_h = (size.h - PBL_IF_ROUND_ELSE(22, 6) - rows_top) / HRZoneCount;
  const int bar_h = large ? 14 : 10;
  const char *time_font = large ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD;
  const int label_w = large ? 28 : 22;
  const int time_w = large ? 52 : 40;
  for (int i = 0; i < HRZoneCount; i++) {
    const int zone = HRZoneCount - 1 - i;
    const GRect row = prv_row_box(size, w, 28, rows_top + i * row_h, row_h);
    const int bar_y = row.origin.y + (row.size.h - bar_h) / 2;
    const GColor color = prv_zone_color(zone, false);
    char text[16];
    snprintf(text, sizeof(text), i18n_get("Z%d", window), zone + 1);
    GRect label = GRect(row.origin.x, bar_y + bar_h / 2 - (large ? 9 : 7), label_w, bar_h + 4);
    if (zone == current) {
      GRect badge = GRect(row.origin.x, bar_y - 2, label_w, bar_h + 4);
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_round_rect(ctx, &badge, 3, GCornersAll);
    }
    prv_dashboard_text(ctx, text, time_font, GRect(label.origin.x, label.origin.y - 3, label_w, 22),
                       zone == current ? GColorBlack : color);

    const int bar_x = row.origin.x + label_w + 4;
    const int bar_w = row.size.w - label_w - time_w - 8;
    GRect track = GRect(bar_x, bar_y, bar_w, bar_h);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack));
#if !PBL_COLOR
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_rect(ctx, &track);
#else
    graphics_fill_rect(ctx, &track);
#endif
    const int length = times[zone] ? MAX(2, bar_w * times[zone] / longest) : 0;
    const int grown = length * prv_bar_progress(window, i) / ANIMATION_NORMALIZED_MAX;
    if (grown > 0) {
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, &GRect(bar_x, bar_y, grown, bar_h));
    }
    window->workout_controller->metric_to_string(WorkoutMetricType_HrZone0Time + zone, text,
                                                 sizeof(text), window, window->workout_data);
    graphics_context_set_text_color(ctx, INK_COLOR);
    graphics_draw_text(ctx, text, fonts_get_system_font(time_font),
                       GRect(row.origin.x + row.size.w - time_w, label.origin.y - 3, time_w, 22),
                       GTextOverflowModeFill, GTextAlignmentRight, nullptr);
  }
}

#define GAUGE_SPAN_DEG 240

#define PAGE_INDICATOR_X(size) ((size).h >= 200 ? 7 : 4)

static int32_t prv_gauge_angle(int thousandths) {
  return DEG_TO_TRIGANGLE(-GAUGE_SPAN_DEG / 2) +
         DEG_TO_TRIGANGLE(GAUGE_SPAN_DEG) / 1000 * thousandths;
}

// Bezel gauge split into the four zones, with a pointer at the heart rate.
static void prv_draw_gauge_ring(GContext *ctx, WorkoutActiveWindow *window, GRect ring, int bpm) {
  const int thickness = window->window.layer.bounds.size.h >= 200 ? PBL_IF_ROUND_ELSE(12, 10) : 7;
  const HRZone zone = hr_util_get_hr_zone(bpm);
  const int32_t sweep_end =
      prv_gauge_angle(1000 * prv_bar_progress(window, 0) / ANIMATION_NORMALIZED_MAX);
  for (int i = 0; i < 4; i++) {
    const bool active = bpm > 0 && i == (int)zone;
    const int32_t start = prv_gauge_angle(i * 250) + DEG_TO_TRIGANGLE(2);
    const int32_t end = MIN(prv_gauge_angle((i + 1) * 250) - DEG_TO_TRIGANGLE(2), sweep_end);
    if (end <= start) {
      continue;
    }
    graphics_context_set_fill_color(ctx, prv_zone_color(i, !active));
#if PBL_COLOR
    const int inset = active ? 0 : 2;
    graphics_fill_radial(ctx, grect_inset(ring, GEdgeInsets(inset)), GOvalScaleModeFitCircle,
                         thickness - 2 * inset, start, end);
#else
    const int inset = active ? 0 : thickness / 2 - 1;
    graphics_fill_radial(ctx, grect_inset(ring, GEdgeInsets(inset)), GOvalScaleModeFitCircle,
                         active ? thickness : 2, start, end);
#endif
  }
  if (bpm <= 0 || window->bars_animation) {
    return;
  }
  const GPoint pointer =
      gpoint_from_polar(grect_inset(ring, GEdgeInsets(thickness / 2)), GOvalScaleModeFitCircle,
                        prv_gauge_angle(prv_zone_position(bpm)));
  const int r = thickness / 2 + 2 + (window->beat ? 2 : 0);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, pointer, r + 2);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, pointer, r);
}

static void prv_draw_hr_chart(GContext *ctx, WorkoutActiveWindow *window, GRect box,
                              AnimationProgress progress) {
  const int left = box.origin.x + 24;
  const int right = box.origin.x + box.size.w - 6;
  const int bottom = box.origin.y + box.size.h - 18;
  const int top = box.origin.y + 8;
  const int elapsed = window->workout_controller->get_metric_value(WorkoutMetricType_Duration,
                                                                   window->workout_data);
  int minimum;
  int maximum;
  const bool has_reading =
      workout_hr_history_range(window->hr_history, elapsed, &minimum, &maximum);
  prv_draw_rule(ctx, GPoint(left, top), GPoint(right, top));
  prv_draw_rule(ctx, GPoint(left, (top + bottom) / 2), GPoint(right, (top + bottom) / 2));
  prv_draw_rule(ctx, GPoint(left, bottom), GPoint(right, bottom));

  const int column_w = MAX(1, (right - left) / WORKOUT_HR_HISTORY_SECONDS);
  for (unsigned i = 0; window->hr_history && i < window->hr_history->count; i++) {
    const WorkoutHrSample *sample = workout_hr_history_get(window->hr_history, i);
    const int age = elapsed - sample->elapsed_s;
    if (!sample->bpm || age < 0 || age >= WORKOUT_HR_HISTORY_SECONDS) {
      continue;
    }
    const int x = right - (age + 1) * (right - left) / WORKOUT_HR_HISTORY_SECONDS;
    const int full = 4 + (sample->bpm - minimum) * (bottom - top - 4) / (maximum - minimum);
    const int height = MAX(1, full * progress / ANIMATION_NORMALIZED_MAX);
    graphics_context_set_fill_color(ctx, prv_zone_color(hr_util_get_hr_zone(sample->bpm), false));
    graphics_fill_rect(ctx, &GRect(x, bottom - height, column_w, height));
  }

  char label[12];
  if (has_reading) {
    snprintf(label, sizeof(label), "%d", maximum);
  } else {
    strcpy(label, "--");
  }
  prv_dashboard_text(ctx, label, FONT_KEY_GOTHIC_14_BOLD, GRect(box.origin.x, top - 9, 22, 18),
                     DIM_COLOR);
  if (has_reading) {
    snprintf(label, sizeof(label), "%d", minimum);
  }
  prv_dashboard_text(ctx, label, FONT_KEY_GOTHIC_14_BOLD, GRect(box.origin.x, bottom - 9, 22, 18),
                     DIM_COLOR);
  prv_dashboard_text(ctx, i18n_get(i18n_noop("LAST MINUTE"), window), FONT_KEY_GOTHIC_14_BOLD,
                     GRect(left, bottom + 1, right - left, 18), DIM_COLOR);
  if (!has_reading) {
    prv_dashboard_text(ctx, i18n_get(i18n_noop("WAITING FOR HR"), window), FONT_KEY_GOTHIC_14_BOLD,
                       GRect(left, (top + bottom - 18) / 2, right - left, 18), INK_COLOR);
  }
}

static void prv_draw_gauge(GContext *ctx, WorkoutActiveWindow *window, GSize size, int w, int top) {
  const bool large = size.h >= 200;
  if (window->hr_plot_progress > 0) {
    const GRect box = prv_row_box(size, w, PBL_IF_ROUND_ELSE(64, 28), top + 8,
                                  size.h - top - PBL_IF_ROUND_ELSE(40, 12));
    prv_draw_hr_chart(ctx, window, box, window->hr_plot_progress);
    return;
  }
  const int diameter =
      PBL_IF_ROUND_ELSE(size.w - 8, MIN(w - 2 * (PAGE_INDICATOR_X(size) + 8), size.h - top - 4));
  const GRect ring = GRect((w - diameter) / 2, PBL_IF_ROUND_ELSE(4, top + 4), diameter, diameter);
  const GPoint center = grect_center_point(&ring);
  const int bpm = prv_dashboard_bpm(window);
  const HRZone zone = hr_util_get_hr_zone(bpm);
  prv_draw_gauge_ring(ctx, window, ring, bpm);

  const bool spo2_measuring = !window->finished && window->has_spo2 &&
                              window->workout_controller->get_metric_value(
                                  WorkoutMetricType_SpO2, window->workout_data) < 0;
  char heading[24];
  GColor heading_color = bpm > 0 ? prv_zone_color(zone, false) : DIM_COLOR;
  if (spo2_measuring) {
    snprintf(heading, sizeof(heading), "%s", i18n_get(i18n_noop("READING SPO2"), window));
    heading_color = prv_metric_color(WorkoutMetricType_SpO2);
  } else if (bpm > 0) {
    snprintf(heading, sizeof(heading), i18n_get("ZONE %d", window), (int)zone + 1);
  } else {
    snprintf(heading, sizeof(heading), "%s",
             i18n_get(window->finished ? i18n_noop("AVG BPM") : i18n_noop("MEASURING"), window));
  }
  const int value_h = large ? 60 : 42;
  const int value_y = center.y - value_h / 2 - (large ? 10 : 8);
  prv_dashboard_text(ctx, heading, FONT_KEY_GOTHIC_18_BOLD,
                     GRect(center.x - 60, value_y - (large ? 22 : 16), 120, 24), heading_color);

  char value[12];
  prv_dashboard_bpm_string(window, value, sizeof(value));
  const char *font = large ? FONT_KEY_LECO_60_NUMBERS_AM_PM : FONT_KEY_LECO_42_NUMBERS;
  const GSize value_size = graphics_text_layout_get_max_used_size(
      ctx, value, fonts_get_system_font(font), GRect(0, 0, w, value_h + 10), GTextOverflowModeFill,
      GTextAlignmentLeft, nullptr);
  const int heart_h = large ? 22 : 14;
  const int group_x = center.x - (heart_h + 6 + value_size.w) / 2;
  prv_dashboard_heart_icon(
      ctx, window,
      GRect(group_x, value_y + value_h / 2 - heart_h / 2 + (large ? 4 : 3), heart_h, heart_h),
      PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
  prv_dashboard_text(ctx, value, font,
                     GRect(group_x + heart_h + 6, value_y, value_size.w + 2, value_h + 10),
                     INK_COLOR);

  if (bpm > 0 && !window->finished) {
    char text[48];
    char time[16];
    window->workout_controller->metric_to_string(WorkoutMetricType_HrZone0Time + zone, time,
                                                 sizeof(time), window, window->workout_data);
    snprintf(text, sizeof(text), "%s  %s", i18n_get(prv_get_label_for_hr_metric(bpm), window),
             time);
    prv_dashboard_text(ctx, text, FONT_KEY_GOTHIC_14_BOLD,
                       GRect(center.x - 70, value_y + value_h + (large ? 0 : 2), 140, 18),
                       heading_color);
  }

  const int pair_h = large ? 44 : 34;
  const int pair_y = PBL_IF_ROUND_ELSE(size.h - pair_h - 30, size.h - pair_h - 2);
  const DashboardField pair[] = {
    window->finished ? (DashboardField){WorkoutMetricType_Duration, i18n_noop("TOTAL TIME")}
                     : (DashboardField){WorkoutMetricType_AvgHr, i18n_noop("AVG BPM")},
    window->has_spo2
        ? (DashboardField){window->finished ? WorkoutMetricType_SpO2Min : WorkoutMetricType_SpO2, window->finished ? i18n_noop("LOW SPO2 %") : i18n_noop("SPO2 %")}
        : (DashboardField){WorkoutMetricType_ActiveCalories, i18n_noop("KCAL")},
  };
  const int pair_w = PBL_IF_ROUND_ELSE(150, 124);
  prv_draw_field_row(ctx, window, GRect((w - pair_w) / 2, pair_y, pair_w, pair_h), pair, 2,
                     large ? FONT_KEY_LECO_20_BOLD_NUMBERS : FONT_KEY_GOTHIC_18_BOLD);
}

static void prv_draw_confetti(GContext *ctx, WorkoutActiveWindow *window, GSize size) {
#if PBL_COLOR
  if (!window->confetti_animation) {
    return;
  }
  static const GColor colors[] = {GColorRed,           GColorChromeYellow, GColorPictonBlue,
                                  GColorScreaminGreen, GColorMagenta,      GColorOrange};
  const int t = window->confetti_progress;
  const int top = workout_status_height(size);
  for (int i = 0; i < 28; i++) {
    // Deterministic scatter: each piece gets a launch direction and speed from its index.
    const int seed = (i * 37 + 11) % 64;
    const int vx = (seed - 32) * size.w / 48;
    const int vy = -(size.h / 3) - (seed % 7) * size.h / 24;
    const int gravity = size.h * 2;
    const int x = size.w / 2 + vx * t / ANIMATION_NORMALIZED_MAX;
    const int y =
        top + (vy * t + gravity * t / ANIMATION_NORMALIZED_MAX * t / 2) / ANIMATION_NORMALIZED_MAX;
    const bool tall = ((i + t / 4096) & 1) != 0;
    graphics_context_set_fill_color(ctx, colors[i % ARRAY_LENGTH(colors)]);
    graphics_fill_rect(ctx, &GRect(x, y, tall ? 3 : 6, tall ? 6 : 3));
  }
#endif
}

static void prv_draw_page_indicator(GContext *ctx, WorkoutActiveWindow *window, GSize size,
                                    bool on_band) {
  const int count = window->num_scrollable_metrics;
  if (count < 2) {
    return;
  }
  for (int i = 0; i < count; i++) {
    const bool current = i == window->current_scrollable_metric;
    // Dots clear of the bezel, the current page larger.
    const int offset = i * 12 - (count - 1) * 6;
    const GPoint center = PBL_IF_ROUND_ELSE(GPoint(size.w / 2 + offset, size.h - 12),
                                            GPoint(PAGE_INDICATOR_X(size), size.h / 2 + offset));
    const GColor ink = PBL_IF_ROUND_ELSE(on_band, false) ? GColorBlack : INK_COLOR;
    graphics_context_set_fill_color(ctx, current ? ink : GColorLightGray);
    graphics_fill_circle(ctx, center, current ? 3 : 2);
  }
}

static void prv_dashboard_update_proc(Layer *layer, GContext *ctx) {
  WorkoutActiveWindow *window = window_get_user_data(layer_get_window(layer));
  const bool paused = !window->finished && window->workout_controller->is_paused();
  const GSize size = layer->bounds.size;
  const int w = size.w - (paused ? PBL_IF_RECT_ELSE(ACTION_BAR_WIDTH, 0) : 0);
  const int top = workout_status_height(size);
  const DashboardPage page = paused ? DashboardPage_Main : prv_dashboard_page(window);

  if (!(PBL_IF_ROUND_ELSE(page == DashboardPage_Gauge, false))) {
    prv_draw_status(ctx, window, size, w);
  }
  switch (page) {
    case DashboardPage_Main:
      prv_draw_main(ctx, window, size, w, top);
      break;
    case DashboardPage_Grid:
      prv_draw_grid(ctx, window, size, w, top);
      break;
    case DashboardPage_Zones:
      prv_draw_zones(ctx, window, size, w, top);
      break;
    case DashboardPage_Gauge:
      prv_draw_gauge(ctx, window, size, w, top);
      break;
  }
#if !PBL_COLOR
  if (paused) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, GPoint(w - 1, 0), GPoint(w - 1, size.h - 1));
  }
#endif
  if (!paused) {
    prv_draw_page_indicator(ctx, window, size, page == DashboardPage_Main && window->has_hrm);
    if (!window->finished) {
      workout_draw_select_indicator(ctx, size);
    }
  }
  prv_draw_confetti(ctx, window, size);
  i18n_free_all(window);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! End Workout

PBL_T_STATIC void prv_finish_workout(WorkoutActiveWindow *active_window) {
  active_window->workout_controller->update_data(active_window->workout_data);
  // Already ended by the service, e.g. abandoned, still gets its summary.
  active_window->workout_controller->stop();
  prv_record_hr_sample(active_window);
  prv_set_hr_plot(active_window, false, false);
  active_window->finished = true;
  active_window->confirming_finish = false;
  active_window->current_scrollable_metric = 0;
  prv_throw_confetti(active_window);
  app_timer_cancel(active_window->update_timer);
  active_window->update_timer = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Handlers

static void prv_handle_pause_button(WorkoutActiveWindow *active_window) {
  active_window->workout_controller->pause(!active_window->workout_controller->is_paused());
  prv_update_ui(active_window);
}

static void prv_previous_page(WorkoutActiveWindow *window) {
  prv_set_hr_plot(window, false, false);
  if (window->num_scrollable_metrics > 0) {
    window->current_scrollable_metric =
        (window->current_scrollable_metric + window->num_scrollable_metrics - 1) %
        window->num_scrollable_metrics;
    prv_page_changed(window);
  }
  prv_update_ui(window);
}

PBL_T_STATIC void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
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

PBL_T_STATIC void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
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
  }
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *window = context;
  if (window->confirming_finish) {
    window->confirming_finish = false;
    prv_update_ui(window);
  } else if (window->finished) {
    prv_select_click_handler(recognizer, context);
  } else {
    app_window_stack_pop(true);
  }
}

static void prv_set_pause_button(WorkoutActiveWindow *active_window) {
  if (active_window->num_scrollable_metrics > 1) {
    active_window->pause_button = BUTTON_ID_UP;
  } else {
    active_window->pause_button = BUTTON_ID_SELECT;
  }
}

PBL_T_STATIC void prv_cycle_scrollable_metrics(WorkoutActiveWindow *active_window) {
  prv_set_hr_plot(active_window, false, false);
  if (active_window->num_scrollable_metrics == 0) {
    return;
  }
  active_window->current_scrollable_metric =
      (active_window->current_scrollable_metric + 1) % active_window->num_scrollable_metrics;
  prv_page_changed(active_window);
}

PBL_T_STATIC void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutActiveWindow *active_window = context;
  if (active_window->layout == WorkoutLayout_Dashboard && !active_window->finished &&
      active_window->workout_controller->is_paused()) {
    active_window->confirming_finish = !active_window->confirming_finish;
    prv_update_ui(active_window);
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
#define LONG_PRESS_MS 700

static void prv_cancel_long_press(WorkoutActiveWindow *active_window) {
  app_timer_cancel(active_window->long_press_timer);
  active_window->long_press_timer = nullptr;
}

// Holding a finger still pauses or resumes, confirmed with a vibe.
static void prv_long_press_timer_callback(void *data) {
  WorkoutActiveWindow *active_window = data;
  active_window->long_press_timer = nullptr;
  active_window->touch_active = false;
  vibes_short_pulse();
  prv_handle_pause_button(active_window);
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  WorkoutActiveWindow *active_window = context;
  if (event->type == TouchEvent_Touchdown) {
    active_window->touch_start = GPoint(event->x, event->y);
    active_window->touch_active = !event->non_navigational;
    prv_cancel_long_press(active_window);
    if (active_window->touch_active && !active_window->finished &&
        !active_window->confirming_finish) {
      active_window->long_press_timer =
          app_timer_register(LONG_PRESS_MS, prv_long_press_timer_callback, active_window);
    }
    return;
  }
  if (event->type == TouchEvent_PositionUpdate) {
    if (ABS(event->x - active_window->touch_start.x) > 20 ||
        ABS(event->y - active_window->touch_start.y) > 20) {
      prv_cancel_long_press(active_window);
    }
    return;
  }
  prv_cancel_long_press(active_window);
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
        prv_up_click_handler(nullptr, active_window);
      } else {
        prv_down_click_handler(nullptr, active_window);
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
  } else if (ABS(dx) <= 20 && ABS(dy) <= 20 && paused) {
    // Pausing is button only; taps just reach the sidebar actions.
    const int h = active_window->window.layer.bounds.size.h;
    const int w = active_window->window.layer.bounds.size.w;
    if (event->x >= w - ACTION_BAR_WIDTH) {
      if (event->y >= 2 * h / 3) {
        prv_down_click_handler(nullptr, active_window);
      } else if (event->y >= h / 3) {
        prv_select_click_handler(nullptr, active_window);
      }
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
    prv_cancel_long_press(active_window);
  }
}
#endif

static void prv_window_unload_handler(Window *window) {
  WorkoutActiveWindow *active_window = window_get_user_data(window);
  if (active_window) {
    workout_cancel_animation(&active_window->hr_plot_animation);
    workout_cancel_animation(&active_window->bars_animation);
    workout_cancel_animation(&active_window->confetti_animation);
    app_timer_cancel(active_window->beat_timer);
#ifdef CONFIG_TOUCH
    app_timer_cancel(active_window->long_press_timer);
#endif
    app_free(active_window->hr_history);
    app_timer_cancel(active_window->update_timer);
    app_timer_cancel(active_window->hr_measuring_timer);
    gbitmap_destroy(active_window->action_bar_start);
    gbitmap_destroy(active_window->action_bar_pause);
    gbitmap_destroy(active_window->action_bar_stop);
    gbitmap_destroy(active_window->action_bar_next);
    gbitmap_destroy(active_window->action_bar_confirm);
    gbitmap_destroy(active_window->action_bar_cancel);
    gbitmap_destroy(active_window->heart_icon);
    gbitmap_destroy(active_window->activity_icon);
    gbitmap_destroy(active_window->hr_measuring_icon);
    action_bar_layer_deinit(&active_window->action_bar);
    if (active_window->layout != WorkoutLayout_Dashboard) {
      status_bar_layer_deinit(&active_window->status_layer);
    }
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
      window, active_window->layout == WorkoutLayout_Dashboard ? GColorBlack : BACKGROUND_COLOR);
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
  } else {
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

  ActionBarLayer *action_bar = &active_window->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_set_context(action_bar, active_window);
  action_bar_layer_set_click_config_provider(action_bar, prv_click_config_provider);
  if (active_window->layout == WorkoutLayout_Dashboard) {
    action_bar_layer_set_background_color(action_bar,
                                          workout_action_bar_color(active_window->activity_type));
    window_set_click_config_provider_with_context(window, prv_click_config_provider, active_window);
  } else {
    StatusBarLayer *status_layer = &active_window->status_layer;
    status_bar_layer_init(status_layer);
    status_bar_layer_set_colors(status_layer, GColorClear, GColorBlack);
    layer_add_child(&window->layer, status_bar_layer_get_layer(status_layer));
#if PBL_RECT
    GRect status_layer_bounds = window->layer.bounds;
    status_layer_bounds.size.w -= ACTION_BAR_WIDTH;
    layer_set_frame(&status_layer->layer, &status_layer_bounds);
#endif
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
    active_window->action_bar_confirm =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_CHECK);
    active_window->action_bar_cancel = gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_X);
  } else {
    active_window->heart_icon = gbitmap_create_with_resource(RESOURCE_ID_WORKOUT_APP_HEART);
    active_window->hr_measuring_icon =
        gbitmap_create_with_resource(RESOURCE_ID_WORKOUT_APP_MEASURING_HR);
    active_window->action_bar_start =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_START);
    active_window->action_bar_pause =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_PAUSE);
    active_window->action_bar_next =
        gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_TOGGLE);
  }

  prv_set_pause_button(active_window);
  prv_set_action_bar_icons(active_window);

  active_window->update_timer = app_timer_register(1000, prv_update_timer_callback, active_window);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Public API

WorkoutActiveWindow *workout_active_create_triple_layout(WorkoutMetricType top_metric,
                                                         WorkoutMetricType middle_metric,
                                                         int num_scrollable_metrics,
                                                         WorkoutMetricType *scrollable_metrics,
                                                         void *workout_data,
                                                         WorkoutController *workout_controller) {
  if (top_metric == WorkoutMetricType_None || middle_metric == WorkoutMetricType_None ||
      (num_scrollable_metrics != 0 && !scrollable_metrics)) {
    PBL_LOG_ERR("Invalid argument(s)");
    return nullptr;
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
    return nullptr;
  }
  WorkoutActiveWindow *active_window = app_zalloc_check(sizeof(WorkoutActiveWindow));
  active_window->layout = WorkoutLayout_Dashboard;
  active_window->activity_type = type;
  active_window->has_hrm = activity_is_hrm_present() && activity_prefs_heart_rate_is_enabled();
  active_window->num_scrollable_metrics =
      (type == ActivitySessionType_Open ? 1 : 2) + (active_window->has_hrm ? 2 : 0);
#ifdef CONFIG_SERVICE_HRM
  active_window->has_spo2 =
      active_window->has_hrm && activity_prefs_spo2_activity_tracking_is_enabled();
#endif
  active_window->bars_progress = ANIMATION_NORMALIZED_MAX;
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
