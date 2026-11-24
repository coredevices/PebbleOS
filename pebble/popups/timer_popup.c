/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "timer_popup.h"

#include <pbl/kernel/malloc.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/light.h>
#include <pbl/services/timers/timer.h>
#include <pbl/services/vibes/vibe_client.h>
#include <pbl/services/vibes/vibe_score.h>
#include <pbl/util/units.h>

#include <applib/fonts/fonts.h>
#include <applib/platform.h>
#include <applib/ui/dialogs/actionable_dialog.h>
#include <applib/ui/dialogs/dialog.h>
#include <applib/ui/vibes.h>
#include <kernel/event_loop.h>
#include <kernel/low_power.h>
#include <kernel/ui/modals/modal_manager.h>
#include <resource/resource_ids.auto.h>

#define TIMER_PRIORITY (ModalPriorityAlarm)

// How long the popup rings before it dismisses itself.
#define TIMER_VIBE_DURATION_MS        (60 * PBL_MSEC_PER_SEC)
#define TIMER_VIBE_REPEAT_INTERVAL_MS (1000)
#define TIMER_LPM_VIBES_PER_MINUTE    (10)

typedef struct {
  ActionableDialog *timer_popup;
  GBitmap *action_bar_dismiss;
  ActionBarLayer action_bar;

  int max_vibes;
  int vibe_count;
  VibeScore *vibe_score;
} TimerPopupData;

static TimerPopupData *s_timer_popup_data = nullptr;

static void prv_vibe(struct pbl_work *work);

// Outside the popup data: stopping must not wait for prv_vibe, which can stall on the event queue
// that KernelMain drains. Cleared before canceling, so a running prv_vibe does not re-arm.
static bool s_vibe_repeating;
static pbl_tick_t s_vibe_period_ticks;
static PBL_DELAYED_WORK_DEFINE(s_vibe_timer, prv_vibe);

static WindowStack *prv_get_window_stack(void) {
  return modal_manager_get_window_stack(TIMER_PRIORITY);
}

static void prv_stop_vibes(void) {
  if (s_vibe_repeating) {
    s_vibe_repeating = false;
    pbl_delayed_work_cancel(&s_vibe_timer);
    if (s_timer_popup_data->vibe_score) {
      vibe_score_destroy(s_timer_popup_data->vibe_score);
      s_timer_popup_data->vibe_score = nullptr;
    }
  }
  vibes_cancel();
}

static void prv_vibe_kernel_main_cb(void *callback_context) {
  if (!s_timer_popup_data) {
    return;
  }
  if (s_timer_popup_data->vibe_count < s_timer_popup_data->max_vibes) {
    s_timer_popup_data->vibe_count++;
    vibes_cancel();
    if (s_timer_popup_data->vibe_score) {
      vibe_score_do_vibe(s_timer_popup_data->vibe_score);
    } else {
      vibes_long_pulse();
    }
  } else {
    prv_stop_vibes();
    actionable_dialog_pop(s_timer_popup_data->timer_popup);
  }
}

static void prv_vibe(struct pbl_work *work) {
  launcher_task_add_callback(prv_vibe_kernel_main_cb, nullptr);
  if (s_vibe_repeating) {
    pbl_delayed_work_schedule_next(&s_vibe_timer, s_vibe_period_ticks);
  }
}

static void prv_start_vibes(void) {
  s_timer_popup_data->vibe_count = 0;
  s_timer_popup_data->vibe_score =
      vibe_client_get_score(low_power_is_active() ? VibeClient_AlarmsLPM : VibeClient_Alarms);

  unsigned int vibe_repeat_interval_ms = 0;
  if (s_timer_popup_data->vibe_score) {
    vibe_repeat_interval_ms = vibe_score_get_duration_ms(s_timer_popup_data->vibe_score) +
                              vibe_score_get_repeat_delay_ms(s_timer_popup_data->vibe_score);
  }
  if (vibe_repeat_interval_ms == 0) {
    // Missing (e.g. resource load failure under memory pressure) or empty
    // score: a timer must never be silent, so pulse on a fixed cadence.
    PBL_LOG_WRN("No usable timer vibe score; falling back to fixed pulse");
    if (s_timer_popup_data->vibe_score) {
      vibe_score_destroy(s_timer_popup_data->vibe_score);
      s_timer_popup_data->vibe_score = nullptr;
    }
    vibe_repeat_interval_ms =
        low_power_is_active() ? (PBL_SEC_PER_MIN * PBL_MSEC_PER_SEC / TIMER_LPM_VIBES_PER_MINUTE)
                              : TIMER_VIBE_REPEAT_INTERVAL_MS;
  }
  s_timer_popup_data->max_vibes = PBL_DIV_ROUND_UP(TIMER_VIBE_DURATION_MS, vibe_repeat_interval_ms);
  launcher_task_add_callback(prv_vibe_kernel_main_cb, nullptr);
  s_vibe_period_ticks = pbl_ms_to_ticks(vibe_repeat_interval_ms);
  s_vibe_repeating = true;
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_vibe_timer, PBL_TICKS(s_vibe_period_ticks));
}

static void prv_dismiss_click_handler(ClickRecognizerRef recognizer, void *data) {
  if (s_timer_popup_data) {
    actionable_dialog_pop(s_timer_popup_data->timer_popup);
  }
}

static void prv_click_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_dismiss_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_dismiss_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_dismiss_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_dismiss_click_handler);
}

static void prv_setup_action_bar(void) {
  ActionBarLayer *action_bar = &s_timer_popup_data->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_set_background_color(action_bar, GColorBlack);

  s_timer_popup_data->action_bar_dismiss =
      gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_X);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, s_timer_popup_data->action_bar_dismiss);
  action_bar_layer_set_click_config_provider(action_bar, prv_click_provider);
}

static void prv_cleanup_timer_popup(void *callback_context) {
  if (s_timer_popup_data) {
    prv_stop_vibes();
    light_enable(false);
    // The action bar owns a redraw timer armed on button press; it must be
    // cancelled before the layer's memory goes away. actionable_dialog leaves
    // custom action bars to their owner.
    action_bar_layer_deinit(&s_timer_popup_data->action_bar);
    gbitmap_destroy(s_timer_popup_data->action_bar_dismiss);
    pbl_free(s_timer_popup_data);
    s_timer_popup_data = nullptr;
  }
}

// ----------------------------------------------------------------------------------------------
//! API
void timer_popup_push_window(const PebbleTimerEvent *event) {
  if (s_timer_popup_data) {
    // The window is already visible, don't show another one.
    return;
  }

  s_timer_popup_data = pbl_malloc_check(sizeof(TimerPopupData));
  *s_timer_popup_data = (TimerPopupData){};

  prv_setup_action_bar();

  s_timer_popup_data->timer_popup = actionable_dialog_create("Timer Popup");
  actionable_dialog_set_action_bar_type(s_timer_popup_data->timer_popup, DialogActionBarCustom,
                                        &s_timer_popup_data->action_bar);

  Dialog *dialog = actionable_dialog_get_dialog(s_timer_popup_data->timer_popup);
  char display_time[TIMER_DESCRIPTION_SIZE];
  timer_format_duration(display_time, sizeof(display_time), event->duration_s);
  dialog_set_text(dialog, display_time);
  if (PBL_DISPLAY_HEIGHT > 180) {
    actionable_dialog_set_text_font(s_timer_popup_data->timer_popup,
                                    fonts_get_system_font(FONT_KEY_GOTHIC_36_BOLD));
  }
  dialog_set_icon(dialog, RESOURCE_ID_ALARM_CLOCK_LARGE);
  dialog_set_background_color(dialog, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite));
  DialogCallbacks callback = {
    .unload = prv_cleanup_timer_popup,
  };
  dialog_set_callbacks(dialog, &callback, nullptr);
#ifdef CONFIG_TOUCH
  // A ringing timer must not be dismissed by a stray touch; buttons only.
  window_set_touch_bridge_disabled(&dialog->window, true);
#endif
  actionable_dialog_push(s_timer_popup_data->timer_popup, prv_get_window_stack());

  prv_start_vibes();
  light_enable(true);
}
