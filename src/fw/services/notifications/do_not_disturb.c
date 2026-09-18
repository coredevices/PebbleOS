/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/do_not_disturb.h"
#include "pbl/services/notifications/do_not_disturb_toggle.h"

#include "applib/ui/action_toggle.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/ui/window_manager.h"
#include <pbl/drivers/rtc.h>
#include "kernel/events.h"
#include "kernel/ui/modals/modal_manager.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/system_task.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "pbl/services/notifications/alerts_preferences_private.h"
#include "pbl/services/timeline/calendar.h"
#include "syscall/syscall_internal.h"
#include <pbl/logging/logging.h>
#include "system/passert.h"
#include "pbl/util/math.h"
#include "util/time/time.h"

#include <pbl/cron/cron.h>
#include <stdbool.h>

PBL_LOG_MODULE_DECLARE(service_notifications, CONFIG_SERVICE_NOTIFICATIONS_LOG_LEVEL);

typedef struct DoNotDisturbData {
  ActivitySleepState sleep_state;
  bool is_in_schedule_period;
  bool manually_override_dnd;
  bool sleep_dnd_override;
  bool was_active;
} DoNotDisturbData;

static DoNotDisturbData s_data;

//! Cron jobs for the schedule boundaries, and for midnight of the days on
//! which the weekday/weekend schedule takes over from the other.
static struct pbl_cron_job s_weekday_from_job;
static struct pbl_cron_job s_weekday_to_job;
static struct pbl_cron_job s_weekend_from_job;
static struct pbl_cron_job s_weekend_to_job;
static struct pbl_cron_job s_schedule_switch_job;

static bool prv_is_smart_dnd_active(void);
static bool prv_is_schedule_active(void);
static bool prv_is_sleep_dnd_active(void);
static bool prv_is_until_wake_active(void);
static void prv_update_schedule_mode(void);

static void prv_update_active_time(bool is_active) {
  if (is_active) {
  } else {
  }
}

static void prv_put_dnd_event(bool is_active) {
  PebbleEvent e = (PebbleEvent){
    .type = PEBBLE_DO_NOT_DISTURB_EVENT,
    .do_not_disturb = {
      .is_active = is_active,
    }
  };

  event_put(&e);
}

static char *prv_bool_to_string(bool active) {
  return active ? "Active" : "Inactive";
}

static void prv_do_update(void) {
  const bool is_active = do_not_disturb_is_active();
  if (is_active == s_data.was_active) {
    // No change
    return;
  }
  s_data.was_active = is_active;
  PBL_LOG_DBG("Quiet Time: %s", prv_bool_to_string(is_active));

  prv_update_active_time(is_active);
  prv_put_dnd_event(is_active);
}

static void prv_toggle_smart_dnd(void *e_dialog) {
  alerts_preferences_dnd_set_smart_enabled(!alerts_preferences_dnd_is_smart_enabled());
  s_data.manually_override_dnd = false;
  prv_do_update();
}

static void prv_toggle_manual_dnd_from_action_menu(void *e_dialog) {
  do_not_disturb_toggle_push(ActionTogglePrompt_NoPrompt, false /* set_exit_reason */);
}

static void prv_toggle_manual_dnd_from_settings_menu(void *e_dialog) {
  do_not_disturb_set_manually_enabled(!do_not_disturb_is_manually_enabled());
}

static void prv_push_first_use_dialog(const char *msg, DialogCallback dialog_close_cb) {
  DialogCallbacks callbacks = {.unload = dialog_close_cb};
  ExpandableDialog *first_use_dialog = expandable_dialog_create_with_params(
      "DNDFirstUse", RESOURCE_ID_QUIET_TIME, msg, GColorBlack, GColorMediumAquamarine, &callbacks,
      RESOURCE_ID_ACTION_BAR_ICON_CHECK, expandable_dialog_close_cb);
  i18n_free(msg, &s_data);
  expandable_dialog_push(first_use_dialog,
                         window_manager_get_window_stack(ModalPriorityNotification));
}

static void prv_push_smart_dnd_first_use_dialog(void) {
  const char *msg = i18n_get(
      "Calendar Aware enables Quiet Time automatically during "
      "calendar events.",
      &s_data);
  prv_push_first_use_dialog(msg, prv_toggle_smart_dnd);
}

static void prv_push_manual_dnd_first_use_dialog(ManualDNDFirstUseSource source) {
  const char *msg = i18n_get(
      "Press and hold the Back button from a notification to turn "
      "Quiet Time on or off.",
      &s_data);
  if (source == ManualDNDFirstUseSourceActionMenu) {
    prv_push_first_use_dialog(msg, prv_toggle_manual_dnd_from_action_menu);
  } else {
    prv_push_first_use_dialog(msg, prv_toggle_manual_dnd_from_settings_menu);
  }
}

static void prv_try_update_schedule_mode(void *data) {
  const bool clear_override = (bool)(uintptr_t)data;
  if (clear_override) {
    s_data.manually_override_dnd = false;
  }
  prv_update_schedule_mode();
  prv_do_update();
}

static void prv_try_update_schedule_mode_callback(bool clear_manual_override) {
  system_task_add_callback(prv_try_update_schedule_mode, (void *)(uintptr_t)clear_manual_override);
}

static void prv_schedule_cron_callback(struct pbl_cron_job *job, void *data) {
  prv_try_update_schedule_mode_callback(true);
}

static DoNotDisturbScheduleType prv_current_schedule_type(void) {
  struct tm time;
  rtc_get_time_tm(&time);
  return ((time.tm_wday == Saturday || time.tm_wday == Sunday) ? WeekendSchedule : WeekdaySchedule);
}

static bool prv_is_in_schedule_period(void) {
  const DoNotDisturbScheduleType type = prv_current_schedule_type();
  if (!do_not_disturb_is_schedule_enabled(type)) {
    return false;
  }

  DoNotDisturbSchedule schedule;
  do_not_disturb_get_schedule(type, &schedule);
  const int from = schedule.from_hour * MINUTES_PER_HOUR + schedule.from_minute;
  const int to = schedule.to_hour * MINUTES_PER_HOUR + schedule.to_minute;

  struct tm time;
  rtc_get_time_tm(&time);
  const int now = time.tm_hour * MINUTES_PER_HOUR + time.tm_min;

  if (from < to) {
    return now >= from && now < to;
  }
  return from != to && (now >= from || now < to);
}

static void prv_schedule_job(struct pbl_cron_job *job, int hour, int minute, uint8_t wday) {
  *job = (struct pbl_cron_job){
    .cb = prv_schedule_cron_callback,
    .minute = minute,
    .hour = hour,
    .mday = PBL_CRON_MDAY_ANY,
    .month = PBL_CRON_MONTH_ANY,
    .wday = wday,
  };
  pbl_cron_job_schedule(job);
}

static void prv_schedule_jobs(DoNotDisturbScheduleType type, struct pbl_cron_job *from_job,
                              struct pbl_cron_job *to_job, uint8_t wday) {
  DoNotDisturbSchedule schedule;
  do_not_disturb_get_schedule(type, &schedule);
  prv_schedule_job(from_job, schedule.from_hour, schedule.from_minute, wday);
  prv_schedule_job(to_job, schedule.to_hour, schedule.to_minute, wday);
}

static void prv_update_schedule_mode(void) {
  pbl_cron_job_unschedule(&s_weekday_from_job);
  pbl_cron_job_unschedule(&s_weekday_to_job);
  pbl_cron_job_unschedule(&s_weekend_from_job);
  pbl_cron_job_unschedule(&s_weekend_to_job);
  pbl_cron_job_unschedule(&s_schedule_switch_job);

  const bool weekday_enabled = do_not_disturb_is_schedule_enabled(WeekdaySchedule);
  const bool weekend_enabled = do_not_disturb_is_schedule_enabled(WeekendSchedule);
  if (weekday_enabled) {
    prv_schedule_jobs(WeekdaySchedule, &s_weekday_from_job, &s_weekday_to_job,
                      PBL_CRON_WDAY_WEEKDAYS);
  }
  if (weekend_enabled) {
    prv_schedule_jobs(WeekendSchedule, &s_weekend_from_job, &s_weekend_to_job,
                      PBL_CRON_WDAY_WEEKENDS);
  }
  if (weekday_enabled || weekend_enabled) {
    prv_schedule_job(&s_schedule_switch_job, 0, 0, PBL_CRON_WDAY_MONDAY | PBL_CRON_WDAY_SATURDAY);
  }

  const bool in_period = prv_is_in_schedule_period();
  // Coming out of scheduled DND with manual DND on, turning it off
  if (s_data.is_in_schedule_period && !in_period && do_not_disturb_is_manually_enabled()) {
    do_not_disturb_set_manually_enabled(false);
  }
  s_data.is_in_schedule_period = in_period;
  PBL_LOG_DBG("%s scheduled period", s_data.is_in_schedule_period ? "In" : "Out of");
}

static bool prv_is_current_schedule_enabled() {
  return (do_not_disturb_is_schedule_enabled(prv_current_schedule_type()));
}

static bool prv_is_schedule_active(void) {
  return (prv_is_current_schedule_enabled() && s_data.is_in_schedule_period &&
          !s_data.manually_override_dnd);
}

static bool prv_is_smart_dnd_active(void) {
  return (calendar_event_is_ongoing() && do_not_disturb_is_smart_dnd_enabled() &&
          !s_data.manually_override_dnd);
}

static bool prv_sleep_state_is_asleep(ActivitySleepState state) {
  return state == ActivitySleepStateLightSleep || state == ActivitySleepStateRestfulSleep;
}

static bool prv_is_sleep_dnd_active(void) {
  return do_not_disturb_is_sleep_dnd_enabled() &&
         prv_sleep_state_is_asleep(s_data.sleep_state) &&
         !s_data.sleep_dnd_override;
}

static ActivitySleepState prv_get_sleep_state(void) {
  int32_t sleep_state;
  if (!activity_tracking_on() ||
      !activity_get_metric(ActivityMetricSleepState, 1, &sleep_state)) {
    return ActivitySleepStateUnknown;
  }

  switch (sleep_state) {
    case ActivitySleepStateAwake:
    case ActivitySleepStateRestfulSleep:
    case ActivitySleepStateLightSleep:
    case ActivitySleepStateUnknown:
      return sleep_state;
  }
  return ActivitySleepStateUnknown;
}

static bool prv_is_until_wake_active(void) {
  const DndUntilWakeState state = alerts_preferences_dnd_get_until_wake_state();
  return state == DndUntilWakeStateWaitingForSleep ||
         state == DndUntilWakeStateWaitingForWake;
}

static void prv_update_until_wake_state(void) {
  const DndUntilWakeState state = alerts_preferences_dnd_get_until_wake_state();
  if (state == DndUntilWakeStateWaitingForSleep &&
      prv_sleep_state_is_asleep(s_data.sleep_state)) {
    alerts_preferences_dnd_set_until_wake_state(DndUntilWakeStateWaitingForWake);
  } else if (state == DndUntilWakeStateWaitingForWake &&
             s_data.sleep_state == ActivitySleepStateAwake) {
    alerts_preferences_dnd_set_until_wake_state(DndUntilWakeStateDisabled);
  } else if (state != DndUntilWakeStateDisabled &&
             state != DndUntilWakeStateWaitingForSleep &&
             state != DndUntilWakeStateWaitingForWake) {
    alerts_preferences_dnd_set_until_wake_state(DndUntilWakeStateDisabled);
  }
}

static void prv_set_sleep_state(ActivitySleepState sleep_state) {
  s_data.sleep_state = sleep_state;

  // Only clear the manual override when we've definitively observed waking up.
  // This preserves the override across temporary tracking interruptions.
  if (sleep_state == ActivitySleepStateAwake) {
    s_data.sleep_dnd_override = false;
  }

  prv_update_until_wake_state();
  prv_do_update();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Public Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_SYSCALL(bool, sys_do_not_disturb_is_active, void) {
  return do_not_disturb_is_active();
}

bool do_not_disturb_is_active(void) {
  if (do_not_disturb_is_manually_enabled() || prv_is_schedule_active() ||
      prv_is_smart_dnd_active() || prv_is_sleep_dnd_active() || prv_is_until_wake_active()) {
    return true;
  }
  return false;
}

bool do_not_disturb_is_manually_enabled(void) {
  return alerts_preferences_dnd_is_manually_enabled();
}

void do_not_disturb_set_manually_enabled(bool enable) {
  const bool was_manually_enabled = do_not_disturb_is_manually_enabled();
  const bool is_schedule_or_smart_dnd_active =
      prv_is_schedule_active() || prv_is_smart_dnd_active();
  const bool is_schedule_or_smart_dnd_enabled =
      prv_is_current_schedule_enabled() || do_not_disturb_is_smart_dnd_enabled();
  const bool is_sleep_dnd_active = prv_is_sleep_dnd_active();
  const bool was_active = do_not_disturb_is_active();

  alerts_preferences_dnd_set_manually_enabled(enable);
  // Turning manual DND off overrides any automatic modes that are currently in effect.
  if (!enable && was_active) {
    if (is_schedule_or_smart_dnd_active ||
        (was_manually_enabled && is_schedule_or_smart_dnd_enabled)) {
      s_data.manually_override_dnd = true;
    }
    if (is_sleep_dnd_active) {
      s_data.sleep_dnd_override = true;
    }
  }
  prv_do_update();
}

void do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSource source) {
  FirstUseSource first_use_source = (FirstUseSource)source;
  if (!alerts_preferences_check_and_set_first_use_complete(first_use_source)) {
    prv_push_manual_dnd_first_use_dialog(source);
  } else {
    if (source == ManualDNDFirstUseSourceSettingsMenu) {
      prv_toggle_manual_dnd_from_settings_menu(NULL);
    } else {
      prv_toggle_manual_dnd_from_action_menu(NULL);
    }
  }
}

bool do_not_disturb_is_smart_dnd_enabled(void) {
  return alerts_preferences_dnd_is_smart_enabled();
}

void do_not_disturb_toggle_smart_dnd(void) {
  if (!alerts_preferences_check_and_set_first_use_complete(FirstUseSourceSmartDND)) {
    prv_push_smart_dnd_first_use_dialog();
  } else {
    prv_toggle_smart_dnd(NULL);
  }
}

bool do_not_disturb_is_sleep_dnd_enabled(void) {
  return alerts_preferences_dnd_is_sleep_enabled();
}

void do_not_disturb_toggle_sleep_dnd(void) {
  alerts_preferences_dnd_set_sleep_enabled(!alerts_preferences_dnd_is_sleep_enabled());
  s_data.sleep_dnd_override = false;
  prv_do_update();
}

bool do_not_disturb_is_until_wake_enabled(void) {
  return prv_is_until_wake_active();
}

void do_not_disturb_set_until_wake_enabled(bool enable) {
  DndUntilWakeState state = DndUntilWakeStateDisabled;
  if (enable) {
    state = prv_sleep_state_is_asleep(s_data.sleep_state) ?
        DndUntilWakeStateWaitingForWake : DndUntilWakeStateWaitingForSleep;
  }
  alerts_preferences_dnd_set_until_wake_state(state);
  prv_do_update();
}

void do_not_disturb_get_schedule(DoNotDisturbScheduleType type,
                                 DoNotDisturbSchedule *schedule_out) {
  alerts_preferences_dnd_get_schedule(type, schedule_out);
}

void do_not_disturb_set_schedule(DoNotDisturbScheduleType type, DoNotDisturbSchedule *schedule) {
  alerts_preferences_dnd_set_schedule(type, schedule);
  prv_try_update_schedule_mode_callback(true);
}

bool do_not_disturb_is_schedule_enabled(DoNotDisturbScheduleType type) {
  return alerts_preferences_dnd_is_schedule_enabled(type);
}

void do_not_disturb_set_schedule_enabled(DoNotDisturbScheduleType type, bool scheduled) {
  alerts_preferences_dnd_set_schedule_enabled(type, scheduled);
  prv_try_update_schedule_mode_callback(true);
}

void do_not_disturb_toggle_scheduled(DoNotDisturbScheduleType type) {
  alerts_preferences_dnd_set_schedule_enabled(type,
                                              !alerts_preferences_dnd_is_schedule_enabled(type));
  prv_try_update_schedule_mode_callback(true);
}

void do_not_disturb_init(void) {
  s_data = (DoNotDisturbData){
    .sleep_state = prv_get_sleep_state(),
    .was_active = false,
  };
  prv_update_until_wake_state();
  prv_try_update_schedule_mode((void *)true);
}

void do_not_disturb_handle_clock_change(void) {
  prv_try_update_schedule_mode_callback(false);
}

void do_not_disturb_handle_pref_synced(void) {
  prv_try_update_schedule_mode_callback(false);
}

void do_not_disturb_handle_calendar_event(PebbleCalendarEvent *e) {
  prv_do_update();
}

void do_not_disturb_handle_activity_event(PebbleActivityEvent *e) {
  switch (e->type) {
    case PebbleActivityEvent_TrackingStarted:
      prv_set_sleep_state(prv_get_sleep_state());
      break;
    case PebbleActivityEvent_TrackingStopped:
      prv_set_sleep_state(ActivitySleepStateUnknown);
      break;
    case PebbleActivityEvent_SleepStateChanged:
      prv_set_sleep_state(e->sleep_state);
      break;
    case PebbleActivityEventNum:
      break;
  }
}

void do_not_disturb_manual_toggle_with_dialog(void) {
  do_not_disturb_toggle_push(ActionTogglePrompt_Auto, false /* set_exit_reason */);
}
