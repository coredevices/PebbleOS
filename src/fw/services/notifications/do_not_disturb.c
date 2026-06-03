/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/do_not_disturb.h"
#include "pbl/services/notifications/do_not_disturb_toggle.h"

#include "applib/ui/action_toggle.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window_manager.h"
#include "applib/ui/window_stack.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "kernel/ui/modals/modal_manager.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "pbl/services/analytics/analytics.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/system_task.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "pbl/services/notifications/alerts_preferences_private.h"
#include "pbl/services/timeline/calendar.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/list.h"
#include "util/math.h"
#include "util/time/time.h"

#include <stdbool.h>
#include <string.h>

typedef struct DoNotDisturbData {
  TimerID update_timer_id;
  bool is_in_schedule_period;
  bool manually_override_dnd;
  bool was_active;
} DoNotDisturbData;

static DoNotDisturbData s_data;

static QuietTimeScheduleConfig s_qt_schedule_cache[MAX_QUIET_TIME_SCHEDULES];

static bool prv_is_smart_dnd_active(void);
static bool prv_is_schedule_active(void);
static void prv_set_schedule_mode_timer();
static void prv_reload_qt_schedule_cache(void);

static void prv_update_active_time(bool is_active) {
  if (is_active) {
  } else {
  }
}

static void prv_put_dnd_event(bool is_active) {
  PebbleEvent e = (PebbleEvent) {
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
    return;
  }
  s_data.was_active = is_active;
  PBL_LOG_INFO("Quiet Time: %s", prv_bool_to_string(is_active));

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

static void prv_push_first_use_dialog(const char* msg,
                                      DialogCallback dialog_close_cb) {
  DialogCallbacks callbacks = { .unload = dialog_close_cb };
  ExpandableDialog *first_use_dialog = expandable_dialog_create_with_params(
      "DNDFirstUse", RESOURCE_ID_QUIET_TIME, msg, GColorBlack, GColorMediumAquamarine,
      &callbacks, RESOURCE_ID_ACTION_BAR_ICON_CHECK, expandable_dialog_close_cb);
  i18n_free(msg, &s_data);
  expandable_dialog_push(first_use_dialog,
                         window_manager_get_window_stack(ModalPriorityNotification));
}

static void prv_push_smart_dnd_first_use_dialog(void) {
  const char *msg = i18n_get("Calendar Aware enables Quiet Time automatically during " \
      "calendar events.", &s_data);
  prv_push_first_use_dialog(msg, prv_toggle_smart_dnd);
}

static void prv_push_manual_dnd_first_use_dialog(ManualDNDFirstUseSource source) {
  const char *msg = i18n_get("Press and hold the Back button from a notification to turn " \
      "Quiet Time on or off.", &s_data);
  if (source == ManualDNDFirstUseSourceActionMenu) {
    prv_push_first_use_dialog(msg, prv_toggle_manual_dnd_from_action_menu);
  } else {
    prv_push_first_use_dialog(msg, prv_toggle_manual_dnd_from_settings_menu);
  }
}

static void prv_reload_qt_schedule_cache(void) {
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    alerts_preferences_qt_get_schedule(i, &s_qt_schedule_cache[i]);
  }
}

static bool prv_is_any_qt_schedule_enabled(void) {
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    if (s_qt_schedule_cache[i].is_used && s_qt_schedule_cache[i].enabled) {
      return true;
    }
  }
  return false;
}

void quiet_time_get_scheduled_days(const QuietTimeScheduleConfig *config, bool out_days[DAYS_PER_WEEK]) {
  switch (config->kind) {
    case QT_KIND_EVERYDAY:
      for (int i = 0; i < DAYS_PER_WEEK; i++) { out_days[i] = true; }
      break;
    case QT_KIND_WEEKDAYS:
      out_days[Sunday] = false;
      out_days[Monday] = true;
      out_days[Tuesday] = true;
      out_days[Wednesday] = true;
      out_days[Thursday] = true;
      out_days[Friday] = true;
      out_days[Saturday] = false;
      break;
    case QT_KIND_WEEKENDS:
      out_days[Sunday] = true;
      out_days[Monday] = false;
      out_days[Tuesday] = false;
      out_days[Wednesday] = false;
      out_days[Thursday] = false;
      out_days[Friday] = false;
      out_days[Saturday] = true;
      break;
    case QT_KIND_CUSTOM:
      memcpy(out_days, config->scheduled_days, DAYS_PER_WEEK);
      break;
    default:
      memset(out_days, 0, DAYS_PER_WEEK);
      break;
  }
}

static bool prv_is_time_in_range(const struct tm *now, const QuietTimeScheduleConfig *schedule) {
  int now_minutes = now->tm_hour * 60 + now->tm_min;
  int from_minutes = schedule->from_hour * 60 + schedule->from_minute;
  int to_minutes = schedule->to_hour * 60 + schedule->to_minute;
  if (from_minutes == to_minutes) {
    to_minutes = (to_minutes + 1) % (24 * 60);
    if (to_minutes == 0) {
      to_minutes = 1;
    }
  }
  if (from_minutes <= to_minutes) {
    return (now_minutes >= from_minutes && now_minutes < to_minutes);
  } else {
    return (now_minutes >= from_minutes || now_minutes < to_minutes);
  }
}

static bool prv_is_any_qt_schedule_active_now(void) {
  struct tm time;
  rtc_get_time_tm(&time);
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    if (!s_qt_schedule_cache[i].is_used || !s_qt_schedule_cache[i].enabled) continue;
    bool days[DAYS_PER_WEEK];
    quiet_time_get_scheduled_days(&s_qt_schedule_cache[i], days);
    if (!days[time.tm_wday]) continue;
    if (prv_is_time_in_range(&time, &s_qt_schedule_cache[i])) return true;
  }
  return false;
}

static void prv_try_update_schedule_mode(void *data) {
  const bool clear_override = (bool) (uintptr_t) data;
  if (clear_override) {
    s_data.manually_override_dnd = false;
  }

  prv_reload_qt_schedule_cache();

  if (prv_is_any_qt_schedule_enabled()) {
    prv_set_schedule_mode_timer();
  } else {
    new_timer_stop(s_data.update_timer_id);
    s_data.is_in_schedule_period = false;
  }
  prv_do_update();
}

static void prv_try_update_schedule_mode_callback(bool clear_manual_override) {
  system_task_add_callback(prv_try_update_schedule_mode, (void*)(uintptr_t) clear_manual_override);
}

static void prv_update_schedule_mode_timer_callback(void* not_used) {
  prv_try_update_schedule_mode_callback(true);
}

static void prv_set_schedule_mode_timer() {
  struct tm time;
  rtc_get_time_tm(&time);
  time_t earliest_transition = SECONDS_PER_DAY * 7;
  bool currently_active = prv_is_any_qt_schedule_active_now();

  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    if (!s_qt_schedule_cache[i].is_used || !s_qt_schedule_cache[i].enabled) continue;
    bool days[DAYS_PER_WEEK];
    quiet_time_get_scheduled_days(&s_qt_schedule_cache[i], days);

    if (days[time.tm_wday]) {
      time_t s = time_util_get_seconds_until_daily_time(&time,
                   s_qt_schedule_cache[i].from_hour, s_qt_schedule_cache[i].from_minute);
      time_t e = time_util_get_seconds_until_daily_time(&time,
                   s_qt_schedule_cache[i].to_hour, s_qt_schedule_cache[i].to_minute);
      earliest_transition = MIN(earliest_transition, MIN(s, e));
    }

    int tomorrow = (time.tm_wday + 1) % DAYS_PER_WEEK;
    if (days[tomorrow]) {
      time_t midnight = time_util_get_seconds_until_daily_time(&time, 0, 0);
      earliest_transition = MIN(earliest_transition, midnight);
    }
  }

  if (currently_active != s_data.is_in_schedule_period) {
    if (currently_active && do_not_disturb_is_manually_enabled()) {
      do_not_disturb_set_manually_enabled(false);
    }
    if (!currently_active && s_data.manually_override_dnd) {
      s_data.manually_override_dnd = false;
    }
    s_data.is_in_schedule_period = currently_active;
  }

  PBL_ASSERTN(earliest_transition > 0);

  PBL_LOG_DBG("%s scheduled period. %u seconds until update",
      s_data.is_in_schedule_period ? "In" : "Out of", (unsigned int) earliest_transition);

  bool success = new_timer_start(s_data.update_timer_id, earliest_transition * 1000,
                                 prv_update_schedule_mode_timer_callback, NULL, 0 /*flags*/);
  PBL_ASSERTN(success);
}

static bool prv_is_schedule_active(void) {
  return (s_data.is_in_schedule_period && !s_data.manually_override_dnd);
}

static bool prv_is_smart_dnd_active(void) {
  return (calendar_event_is_ongoing() &&
          do_not_disturb_is_smart_dnd_enabled() &&
          !s_data.manually_override_dnd);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Public Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_SYSCALL(bool, sys_do_not_disturb_is_active, void) {
  return do_not_disturb_is_active();
}

bool do_not_disturb_is_active(void) {
  if (do_not_disturb_is_manually_enabled() ||
      prv_is_schedule_active() ||
      prv_is_smart_dnd_active()) {
    return true;
  }
  return false;
}

bool do_not_disturb_is_manually_enabled(void) {
  return alerts_preferences_dnd_is_manually_enabled();
}

void do_not_disturb_set_manually_enabled(bool enable) {
  const bool is_auto_dnd = prv_is_any_qt_schedule_enabled() ||
                           do_not_disturb_is_smart_dnd_enabled();
  const bool was_active = do_not_disturb_is_active();

  alerts_preferences_dnd_set_manually_enabled(enable);
  if (!enable && was_active && is_auto_dnd) {
    s_data.manually_override_dnd = true;
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

void do_not_disturb_get_schedule(DoNotDisturbScheduleType type,
                                 DoNotDisturbSchedule *schedule_out) {
  alerts_preferences_dnd_get_schedule(type, schedule_out);
}

void do_not_disturb_set_schedule(DoNotDisturbScheduleType type, DoNotDisturbSchedule *schedule) {
  alerts_preferences_dnd_set_schedule(type, schedule);
  QuietTimeScheduleConfig qt_config;
  int qt_index = (type == WeekdaySchedule) ? 0 : 1;
  QuietTimeKind qt_kind = (type == WeekdaySchedule) ? QT_KIND_WEEKDAYS : QT_KIND_WEEKENDS;
  bool enabled = alerts_preferences_dnd_is_schedule_enabled(type);
  qt_config = (QuietTimeScheduleConfig){
    .is_used = true,
    .kind = qt_kind,
    .from_hour = schedule->from_hour,
    .from_minute = schedule->from_minute,
    .to_hour = schedule->to_hour,
    .to_minute = schedule->to_minute,
    .enabled = enabled,
  };
  memset(qt_config.scheduled_days, 0, sizeof(qt_config.scheduled_days));
  alerts_preferences_qt_set_schedule(qt_index, &qt_config);
  prv_try_update_schedule_mode_callback(true);
}

bool do_not_disturb_is_schedule_enabled(DoNotDisturbScheduleType type) {
  return alerts_preferences_dnd_is_schedule_enabled(type);
}

void do_not_disturb_set_schedule_enabled(DoNotDisturbScheduleType type, bool scheduled) {
  alerts_preferences_dnd_set_schedule_enabled(type, scheduled);
  int qt_index = (type == WeekdaySchedule) ? 0 : 1;
  QuietTimeScheduleConfig qt_config;
  alerts_preferences_qt_get_schedule(qt_index, &qt_config);
  qt_config.is_used = true;
  qt_config.enabled = scheduled;
  alerts_preferences_qt_set_schedule(qt_index, &qt_config);
  prv_try_update_schedule_mode_callback(true);
}

void do_not_disturb_toggle_scheduled(DoNotDisturbScheduleType type) {
  alerts_preferences_dnd_set_schedule_enabled(type,
                                              !alerts_preferences_dnd_is_schedule_enabled(type));
  int qt_index = (type == WeekdaySchedule) ? 0 : 1;
  QuietTimeScheduleConfig qt_config;
  alerts_preferences_qt_get_schedule(qt_index, &qt_config);
  qt_config.is_used = true;
  qt_config.enabled = !qt_config.enabled;
  alerts_preferences_qt_set_schedule(qt_index, &qt_config);
  prv_try_update_schedule_mode_callback(true);
}

//! Quiet Time schedule API

void quiet_time_for_each_schedule(QuietTimeScheduleCallback cb, void *context) {
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    QuietTimeScheduleConfig config;
    alerts_preferences_qt_get_schedule(i, &config);
    cb(i, &config, context);
  }
}

void quiet_time_get_schedule(int index, QuietTimeScheduleConfig *out) {
  alerts_preferences_qt_get_schedule(index, out);
}

void quiet_time_set_schedule(int index, const QuietTimeScheduleConfig *config) {
  if (index < 0 || index >= MAX_QUIET_TIME_SCHEDULES) return;
  QuietTimeScheduleConfig stored = *config;
  stored.is_used = true;
  alerts_preferences_qt_set_schedule(index, &stored);
  prv_try_update_schedule_mode_callback(true);
}

int quiet_time_create_schedule(const QuietTimeScheduleConfig *config) {
  if (config->kind == QT_KIND_CUSTOM) {
    bool any_day = false;
    for (int i = 0; i < DAYS_PER_WEEK; i++) {
      any_day |= config->scheduled_days[i];
    }
    if (!any_day) return -1;
  }
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    QuietTimeScheduleConfig existing;
    alerts_preferences_qt_get_schedule(i, &existing);
    if (!existing.is_used) {
      QuietTimeScheduleConfig new_config = *config;
      new_config.is_used = true;
      alerts_preferences_qt_set_schedule(i, &new_config);
      prv_try_update_schedule_mode_callback(true);
      return i;
    }
  }
  return -1;
}

void quiet_time_delete_schedule(int index) {
  if (index < 0 || index >= MAX_QUIET_TIME_SCHEDULES) return;
  QuietTimeScheduleConfig empty = {0};
  alerts_preferences_qt_set_schedule(index, &empty);
  prv_try_update_schedule_mode_callback(true);
}

void quiet_time_set_schedule_enabled(int index, bool enabled) {
  if (index < 0 || index >= MAX_QUIET_TIME_SCHEDULES) return;
  QuietTimeScheduleConfig config;
  alerts_preferences_qt_get_schedule(index, &config);
  config.enabled = enabled;
  alerts_preferences_qt_set_schedule(index, &config);
  prv_try_update_schedule_mode_callback(true);
}

int quiet_time_get_num_active(void) {
  return alerts_preferences_qt_get_num_active();
}

const char *quiet_time_get_string_for_kind(QuietTimeKind kind) {
  switch (kind) {
    case QT_KIND_EVERYDAY: return i18n_noop("Every Day");
    case QT_KIND_WEEKDAYS: return i18n_noop("Weekdays");
    case QT_KIND_WEEKENDS: return i18n_noop("Weekends");
    case QT_KIND_CUSTOM: return i18n_noop("Custom");
    default: return "";
  }
}

void quiet_time_get_string_for_custom(const bool *scheduled_days, char *buffer, size_t buf_len) {
  static const char * const day_strings[] = {
    i18n_noop("Sun"), i18n_noop("Mon"), i18n_noop("Tue"), i18n_noop("Wed"),
    i18n_noop("Thu"), i18n_noop("Fri"), i18n_noop("Sat"),
  };
  static const char * const full_day_strings[] = {
    i18n_noop("Sundays"), i18n_noop("Mondays"), i18n_noop("Tuesdays"), i18n_noop("Wednesdays"),
    i18n_noop("Thursdays"), i18n_noop("Fridays"), i18n_noop("Saturdays"),
  };

  PBL_ASSERTN(buffer != NULL);
  PBL_ASSERTN(buf_len > 0);
  buffer[0] = '\0';

  int num_days = 0;
  int last_day_idx = 0;
  for (int i = 0; i < DAYS_PER_WEEK; i++) {
    if (scheduled_days[i]) {
      num_days++;
      last_day_idx = i;
    }
  }

  if (num_days == 0) {
    return;
  }

  if (num_days == 1) {
    i18n_get_with_buffer(full_day_strings[last_day_idx], buffer, buf_len);
    return;
  }

  // Monday-first ordering: skip Sunday (index 0) and iterate Mon..Sat, then Sun.
  size_t pos = 0;
  for (int idx = 1; idx <= DAYS_PER_WEEK; idx++) {
    int i = idx % DAYS_PER_WEEK;
    if (!scheduled_days[i]) {
      continue;
    }
    if (pos > 0) {
      if (pos + 1 >= buf_len) {
        break;
      }
      buffer[pos++] = ',';
    }
    char day_buf[12];
    i18n_get_with_buffer(day_strings[i], day_buf, sizeof(day_buf));
    size_t day_len = strlen(day_buf);
    if (pos + day_len >= buf_len) {
      break;
    }
    memcpy(buffer + pos, day_buf, day_len);
    pos += day_len;
  }
  buffer[pos] = '\0';
}

void do_not_disturb_init(void) {
  s_data = (DoNotDisturbData) {
    .update_timer_id = new_timer_create(),
    .was_active = false,
  };
  prv_try_update_schedule_mode((void*) true);
}

void do_not_disturb_handle_clock_change(void) {
  prv_try_update_schedule_mode_callback(false);
}

void do_not_disturb_handle_calendar_event(PebbleCalendarEvent *e) {
  prv_do_update();
}

void do_not_disturb_manual_toggle_with_dialog(void) {
  do_not_disturb_toggle_push(ActionTogglePrompt_Auto, false /* set_exit_reason */);
}

#ifdef UNITTEST
TimerID get_dnd_timer_id(void) {
  return s_data.update_timer_id;
}

void set_dnd_timer_id(TimerID id) {
  s_data.update_timer_id = id;
}
#endif
