/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "alarm_popup.h"

#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window_stack.h"
#include "kernel/event_loop.h"
#include "kernel/low_power.h"
#include "kernel/pbl_malloc.h"
#include "kernel/ui/modals/modal_manager.h"
#include "resource/resource_ids.auto.h"
#include "services/common/clock.h"
#include "services/common/i18n/i18n.h"
#include "services/common/light.h"
#include "services/common/new_timer/new_timer.h"
#include "services/normal/alarms/alarm.h"
#include "util/time/time.h"

#include <stdio.h>
#include <string.h>

#if !PLATFORM_TINTIN
#include "services/normal/vibes/vibe_client.h"
#include "services/normal/vibes/vibe_score.h"
#endif

#if !TINTIN_FORCE_FIT
#define DIALOG_TIMEOUT_SNOOZE 2000
#define DIALOG_TIMEOUT_DISMISS DIALOG_TIMEOUT_SNOOZE

#define ALARM_PRIORITY (ModalPriorityAlarm)

static WindowStack *prv_get_window_stack(void) {
  return modal_manager_get_window_stack(ALARM_PRIORITY);
}

// ----------------------------------------------------------------------------------------------
//! Snooze confirm dialog

static void prv_show_snooze_confirm_dialog(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("AlarmSnooze");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  const char *snooze_text = i18n_noop("Snooze for %d minutes");
  char snooze_buf[32];
  snprintf(snooze_buf, sizeof(snooze_buf), i18n_get(snooze_text, dialog), alarm_get_snooze_delay());
  i18n_free(snooze_text, dialog);
  dialog_set_text(dialog, snooze_buf);
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_CONFIRMATION_LARGE);
  dialog_set_background_color(dialog, GColorJaegerGreen);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_SNOOZE);
  simple_dialog_push(simple_dialog, prv_get_window_stack());
}

// ----------------------------------------------------------------------------------------------
//! Dismiss confirm dialog

static void prv_show_dismiss_confirm_dialog(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("AlarmSnooze");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  const char *dismiss_text = i18n_noop("Alarm dismissed");
  dialog_set_text(dialog, i18n_get(dismiss_text, dialog));
  i18n_free(dismiss_text, dialog);
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_CONFIRMATION_LARGE);
  dialog_set_background_color(dialog, GColorJaegerGreen);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_DISMISS);
  simple_dialog_push(simple_dialog, prv_get_window_stack());
}

// ----------------------------------------------------------------------------------------------
//! Main Window

typedef struct {
  ActionableDialog *alarm_popup;
  GBitmap *bitmap;
  GBitmap *action_bar_dismiss;
  GBitmap *action_bar_snooze;
  ActionBarLayer action_bar;

  TimerID vibe_timer;
  int max_vibes;
  int vibe_count;
#if CAPABILITY_HAS_VIBE_SCORES
  VibeScore *vibe_score;
#endif
} AlarmPopupData;

AlarmPopupData *s_alarm_popup_data = NULL;


static void prv_stop_animation_kernel_main_cb(void *callback_context) {
  if (s_alarm_popup_data) {
    dialog_set_icon((Dialog *) s_alarm_popup_data->alarm_popup,
                    RESOURCE_ID_ALARM_CLOCK_LARGE_STATIC);
  }
}

static void prv_stop_vibes(void) {
  if (s_alarm_popup_data->vibe_timer != TIMER_INVALID_ID) {
    new_timer_stop(s_alarm_popup_data->vibe_timer);
    new_timer_delete(s_alarm_popup_data->vibe_timer);
    s_alarm_popup_data->vibe_timer = TIMER_INVALID_ID;
#if CAPABILITY_HAS_VIBE_SCORES
    if (s_alarm_popup_data->vibe_score) {
      vibe_score_destroy(s_alarm_popup_data->vibe_score);
      s_alarm_popup_data->vibe_score = NULL;
    }
#endif
  }
  vibes_cancel();
}

// ----------------------------------------------------------------------------------------------
//! Vibe Timer
#define TINTIN_VIBE_REPEAT_INTERVAL_MS (1000)
#define TINTIN_MAX_VIBES (10 * 60) // 10 minutes at 1 vibe a second
#define TINTIN_LPM_VIBES_PER_MINUTE (10)
#define VIBE_DURATION (10 * SECONDS_PER_MINUTE * MS_PER_SECOND)
static void prv_vibe_kernel_main_cb(void *callback_context) {
  if (s_alarm_popup_data) {
    if (s_alarm_popup_data->vibe_count < s_alarm_popup_data->max_vibes) {
      s_alarm_popup_data->vibe_count++;
#if CAPABILITY_HAS_VIBE_SCORES
      vibe_score_do_vibe(s_alarm_popup_data->vibe_score);
#else
      if (low_power_is_active()) {
        // Only vibe 10 seconds every minute in low_power_mode
        _Static_assert(TINTIN_VIBE_REPEAT_INTERVAL_MS == MS_PER_SECOND,
                       "LPM Vibes timing incorrect");
        if (s_alarm_popup_data->vibe_count % SECONDS_PER_MINUTE < TINTIN_LPM_VIBES_PER_MINUTE) {
          vibes_long_pulse();
        }
      } else {
        vibes_long_pulse();
      }
#endif
    }
    else {
      prv_stop_vibes();
      launcher_task_add_callback(prv_stop_animation_kernel_main_cb, NULL);
      // Auto-dismiss the alarm after the vibration period ends
      alarm_dismiss_alarm();
      if (s_alarm_popup_data && s_alarm_popup_data->alarm_popup) {
        actionable_dialog_pop(s_alarm_popup_data->alarm_popup);
      }
    }
  }
}

static void prv_vibe(void *unused) {
  launcher_task_add_callback(prv_vibe_kernel_main_cb, NULL);
}

static void prv_start_vibes(void) {
  s_alarm_popup_data->vibe_count = 0;
  unsigned int vibe_repeat_interval_ms = TINTIN_VIBE_REPEAT_INTERVAL_MS;
#if CAPABILITY_HAS_VIBE_SCORES
  if (low_power_is_active()) {
    s_alarm_popup_data->vibe_score = vibe_client_get_score(VibeClient_AlarmsLPM);
  } else {
    s_alarm_popup_data->vibe_score = vibe_client_get_score(VibeClient_Alarms);
  }
  if (!s_alarm_popup_data->vibe_score) {
    return;
  }
  vibe_repeat_interval_ms = vibe_score_get_duration_ms(s_alarm_popup_data->vibe_score) +
      vibe_score_get_repeat_delay_ms(s_alarm_popup_data->vibe_score);
  s_alarm_popup_data->max_vibes = DIVIDE_CEIL(VIBE_DURATION, vibe_repeat_interval_ms);
#else
  s_alarm_popup_data->max_vibes = TINTIN_MAX_VIBES;
#endif
  s_alarm_popup_data->vibe_timer = new_timer_create();
  prv_vibe(NULL);
  new_timer_start(s_alarm_popup_data->vibe_timer, vibe_repeat_interval_ms, prv_vibe,
                  NULL, TIMER_START_FLAG_REPEATING);
}

// ----------------------------------------------------------------------------------------------
//! Click Handler
static void prv_dismiss_click_handler(ClickRecognizerRef recognizer, void *data) {
  alarm_dismiss_alarm();
  prv_show_dismiss_confirm_dialog();
  actionable_dialog_pop(s_alarm_popup_data->alarm_popup);
}

static void prv_snooze_click_handler(ClickRecognizerRef recognizer, void *data) {
  alarm_set_snooze_alarm();
  prv_show_snooze_confirm_dialog();
  actionable_dialog_pop(s_alarm_popup_data->alarm_popup);
}

static void prv_click_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_dismiss_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_snooze_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_snooze_click_handler);
}

// ----------------------------------------------------------------------------------------------
//! Main Window Setup
static void prv_setup_action_bar(void) {
  ActionBarLayer *action_bar = &s_alarm_popup_data->action_bar;
  action_bar_layer_init(action_bar);
  action_bar_layer_set_background_color(action_bar, GColorBlack);

  s_alarm_popup_data->action_bar_snooze =
      gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_SNOOZE);
  s_alarm_popup_data->action_bar_dismiss =
      gbitmap_create_with_resource(RESOURCE_ID_ACTION_BAR_ICON_X);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, s_alarm_popup_data->action_bar_snooze);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, s_alarm_popup_data->action_bar_dismiss);
  action_bar_layer_set_click_config_provider(action_bar, prv_click_provider);
}

static void prv_cleanup_alarm_popup(void *callback_context) {
  if (s_alarm_popup_data) {
    prv_stop_vibes();
    gbitmap_destroy(s_alarm_popup_data->bitmap);
    gbitmap_destroy(s_alarm_popup_data->action_bar_snooze);
    gbitmap_destroy(s_alarm_popup_data->action_bar_dismiss);
    task_free(s_alarm_popup_data);
    s_alarm_popup_data = NULL;
  }
}

// ----------------------------------------------------------------------------------------------
//! API
#endif
void alarm_popup_push_window(PebbleAlarmClockEvent *event) {
#if !TINTIN_FORCE_FIT
  if (s_alarm_popup_data) {
    // The window is already visible, don't show another one
    return;
  }

  s_alarm_popup_data = task_malloc_check(sizeof(AlarmPopupData));
  *s_alarm_popup_data = (AlarmPopupData){};
  s_alarm_popup_data->vibe_timer = TIMER_INVALID_ID;

  prv_setup_action_bar();

  s_alarm_popup_data->alarm_popup = actionable_dialog_create("Alarm Popup");
  actionable_dialog_set_action_bar_type(s_alarm_popup_data->alarm_popup, DialogActionBarCustom,
      &s_alarm_popup_data->action_bar);

  Dialog *dialog = actionable_dialog_get_dialog(s_alarm_popup_data->alarm_popup);
  char display_time[16];
  struct tm alarm_tm;
  localtime_r(&event->alarm_time, &alarm_tm);
  if (clock_is_24h_style()) {
    strftime(display_time, 16, "%H:%M", &alarm_tm);
  } else {
    strftime(display_time, 16, "%I:%M %p", &alarm_tm);
  }
  dialog_set_text(dialog, display_time);
  dialog_set_icon(dialog, RESOURCE_ID_ALARM_CLOCK_LARGE);
  dialog_set_background_color(dialog, GColorJaegerGreen);
  DialogCallbacks callback = {
    .unload = prv_cleanup_alarm_popup,
  };
  dialog_set_callbacks(dialog, &callback, NULL);
  actionable_dialog_push(s_alarm_popup_data->alarm_popup, prv_get_window_stack());

  prv_start_vibes();

  light_enable_interaction();
#else
  return;
#endif
}
