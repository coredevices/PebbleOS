/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weight_entry_window.h"

#include "weight_entry_ui.h"

#include "applib/fonts/fonts.h"
#include "applib/ui/action_bar_layer.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#ifdef CONFIG_TOUCH
#include "applib/touch_service.h"
#endif
#include "kernel/pbl_malloc.h"
#include "kernel/ui/kernel_ui.h"
#include "kernel/ui/system_icons.h"
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/health_util.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/util/math.h"

#define ENTRY_BACKGROUND_COLOR PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite)

#ifdef CONFIG_TOUCH
#define TOUCH_PIXELS_PER_STEP (18)
#define TOUCH_TAP_SLOP        (12)
#define TOUCH_BACK_THRESHOLD  (40)
#endif

typedef struct {
  Window window;
  ActionBarLayer action_bar;
  int32_t value_tenths;
  int32_t min_tenths;
  int32_t max_tenths;
  const char *title;
  const char *unit;
  WeightEntrySavedCallback saved_callback;
  void *context;
#ifdef CONFIG_TOUCH
  int32_t touch_start_value;
  int16_t touch_start_x;
  int16_t touch_start_y;
  bool touch_active;
  bool touch_on_action_bar;
#endif
} WeightEntryWindow;

static void prv_mark_dirty(WeightEntryWindow *entry_window) {
  layer_mark_dirty(&entry_window->window.layer);
}

static void prv_adjust_value(WeightEntryWindow *entry_window, int32_t delta) {
  const int32_t value =
      CLIP(entry_window->value_tenths + delta, entry_window->min_tenths,
           entry_window->max_tenths);
  if (value != entry_window->value_tenths) {
    entry_window->value_tenths = value;
    prv_mark_dirty(entry_window);
  }
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_adjust_value(context, 1);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_adjust_value(context, -1);
}

static void prv_save(WeightEntryWindow *entry_window) {
  const uint16_t weight_dag =
      health_util_weight_tenths_to_dag(entry_window->value_tenths);
  activity_prefs_set_weight_dag(weight_dag);
  activity_weight_history_add(rtc_get_time(), weight_dag);
  if (entry_window->saved_callback) {
    entry_window->saved_callback(weight_dag, entry_window->context);
  }
  app_window_stack_remove(&entry_window->window, true);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_save(context);
}

static void prv_click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 50, prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 50, prv_down_click_handler);
  window_multi_click_subscribe(BUTTON_ID_SELECT, 1, 2, 25, true, prv_select_click_handler);
}

#ifdef CONFIG_TOUCH
static int16_t prv_abs(int16_t value) {
  return value < 0 ? -value : value;
}

static int32_t prv_touch_steps(int16_t delta_y) {
  const int16_t half_step = TOUCH_PIXELS_PER_STEP / 2;
  return delta_y >= 0 ? (delta_y + half_step) / TOUCH_PIXELS_PER_STEP
                      : (delta_y - half_step) / TOUCH_PIXELS_PER_STEP;
}

static void prv_touch_set_value(WeightEntryWindow *entry_window, int32_t value) {
  value = CLIP(value, entry_window->min_tenths, entry_window->max_tenths);
  if (value != entry_window->value_tenths) {
    entry_window->value_tenths = value;
    prv_mark_dirty(entry_window);
  }
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  WeightEntryWindow *entry_window = context;
  if (event->type == TouchEvent_Touchdown) {
    if (event->non_navigational) {
      return;
    }
    entry_window->touch_active = true;
    entry_window->touch_start_x = event->x;
    entry_window->touch_start_y = event->y;
    entry_window->touch_start_value = entry_window->value_tenths;
    entry_window->touch_on_action_bar =
        event->x >= entry_window->window.layer.bounds.size.w - ACTION_BAR_WIDTH;
    return;
  }

  if (!entry_window->touch_active) {
    return;
  }

  const int16_t delta_x = event->x - entry_window->touch_start_x;
  const int16_t delta_y = event->y - entry_window->touch_start_y;
  if (event->type == TouchEvent_PositionUpdate && !entry_window->touch_on_action_bar) {
    prv_touch_set_value(entry_window,
                        entry_window->touch_start_value + prv_touch_steps(-delta_y));
    return;
  }

  if (event->type != TouchEvent_Liftoff) {
    return;
  }

  entry_window->touch_active = false;
  const bool is_tap =
      prv_abs(delta_x) <= TOUCH_TAP_SLOP && prv_abs(delta_y) <= TOUCH_TAP_SLOP;
  if (entry_window->touch_on_action_bar && is_tap) {
    const int16_t zone = event->y * 3 / entry_window->window.layer.bounds.size.h;
    if (zone == 0) {
      prv_adjust_value(entry_window, 1);
    } else if (zone == 1) {
      prv_save(entry_window);
    } else {
      prv_adjust_value(entry_window, -1);
    }
  } else if (!entry_window->touch_on_action_bar &&
             delta_x > TOUCH_BACK_THRESHOLD && prv_abs(delta_x) > prv_abs(delta_y)) {
    app_window_stack_remove(&entry_window->window, true);
  } else if (!entry_window->touch_on_action_bar) {
    prv_touch_set_value(entry_window,
                        entry_window->touch_start_value + prv_touch_steps(-delta_y));
  }
}

static void prv_appear(Window *window) {
  touch_service_subscribe(prv_touch_handler, window_get_user_data(window));
}

static void prv_disappear(Window *window) {
  touch_service_unsubscribe();
}
#endif

static void prv_draw(Layer *layer, GContext *ctx) {
  WeightEntryWindow *entry_window = window_get_user_data(layer_get_window(layer));
  health_weight_entry_ui_draw(ctx, &layer->bounds, entry_window->value_tenths,
                              entry_window->min_tenths, entry_window->max_tenths,
                              entry_window->title, entry_window->unit,
                              FONT_KEY_BITHAM_34_MEDIUM_NUMBERS);
}

static void prv_load(Window *window) {
  WeightEntryWindow *entry_window = window_get_user_data(window);
  ActionBarLayer *action_bar = &entry_window->action_bar;
  action_bar_layer_set_context(action_bar, entry_window);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, &s_bar_icon_up_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, &s_bar_icon_down_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, &s_bar_icon_check_bitmap);
  action_bar_layer_add_to_window(action_bar, window);
  action_bar_layer_set_click_config_provider(action_bar, prv_click_config_provider);
}

static void prv_unload(Window *window) {
  WeightEntryWindow *entry_window = window_get_user_data(window);
  action_bar_layer_deinit(&entry_window->action_bar);
  window_deinit(window);
  i18n_free_all(entry_window);
  app_free(entry_window);
}

void health_weight_entry_window_push(uint16_t initial_weight_dag,
                                     WeightEntrySavedCallback saved_callback, void *context) {
  WeightEntryWindow *entry_window = app_zalloc_check(sizeof(*entry_window));
  entry_window->value_tenths = health_util_weight_dag_to_tenths(initial_weight_dag);
  entry_window->min_tenths = health_util_weight_dag_to_tenths(ACTIVITY_WEIGHT_MIN_DAG);
  entry_window->max_tenths = health_util_weight_dag_to_tenths(ACTIVITY_WEIGHT_MAX_DAG);
  entry_window->value_tenths =
      CLIP(entry_window->value_tenths, entry_window->min_tenths, entry_window->max_tenths);
  entry_window->title = i18n_get("ADD WEIGHT", entry_window);
  entry_window->unit = i18n_get(health_util_get_weight_unit(), entry_window);
  entry_window->saved_callback = saved_callback;
  entry_window->context = context;

  window_init(&entry_window->window, WINDOW_NAME("Weight Entry"));
  window_set_user_data(&entry_window->window, entry_window);
  window_set_background_color(&entry_window->window, ENTRY_BACKGROUND_COLOR);
  window_set_window_handlers(
      &entry_window->window,
      &(WindowHandlers){
        .load = prv_load,
#ifdef CONFIG_TOUCH
        .appear = prv_appear,
        .disappear = prv_disappear,
#endif
        .unload = prv_unload,
      });
#ifdef CONFIG_TOUCH
  window_set_touch_bridge_disabled(&entry_window->window, true);
#endif
  layer_set_update_proc(&entry_window->window.layer, prv_draw);
  action_bar_layer_init(&entry_window->action_bar);
  app_window_stack_push(&entry_window->window, true);
}
