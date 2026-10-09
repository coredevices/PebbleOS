/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

#include <pbl/cron/cron.h>
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/services/notifications/alerts_preferences.h>
#include <pbl/services/notifications/alerts_preferences_private.h>
#include <pbl/services/notifications/do_not_disturb.h>
#include <pbl/services/notifications/do_not_disturb_toggle.h>
#include <pbl/services/system_task.h>
#include <pbl/services/timeline/calendar.h>
#include <pbl/util/math.h>
#include <pbl/util/time.h>
#include <pbl/util/units.h>

#include <applib/ui/action_toggle.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/dialogs/actionable_dialog.h>
#include <applib/ui/dialogs/dialog.h>
#include <applib/ui/dialogs/expandable_dialog.h>
#include <applib/ui/window_manager.h>
#include <kernel/events.h>
#include <kernel/ui/modals/modal_manager.h>
#include <process_state/app_state/app_state.h>
#include <resource/resource_ids.auto.h>
#include <syscall/syscall_internal.h>
#include <system/passert.h>

PBL_LOG_MODULE_DECLARE(service_notifications, CONFIG_SERVICE_NOTIFICATIONS_LOG_LEVEL);

typedef struct DoNotDisturbData {
  bool is_in_schedule_period;
  bool manually_override_dnd;
  bool was_active;
} DoNotDisturbData;

static DoNotDisturbData s_data;

static QuietTimeScheduleConfig s_qt_schedule_cache[MAX_QUIET_TIME_SCHEDULES];

//! Cron jobs for each schedule slot's window start and end. The job
//! structures are owned here and stay valid while scheduled.
static struct pbl_cron_job s_qt_jobs[MAX_QUIET_TIME_SCHEDULES][2];

static bool prv_is_smart_dnd_active(void);
static bool prv_is_schedule_active(void);
static void prv_update_schedule_mode(void);
static void prv_reload_qt_schedule_cache(void);

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

//! Re-evaluate active DND state and post PEBBLE_DO_NOT_DISTURB_EVENT if it changed.
void do_not_disturb_refresh_active_state(void) {
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

void quiet_time_get_scheduled_days(const QuietTimeScheduleConfig *config,
                                   bool out_days[PBL_DAY_PER_WEEK]) {
  switch (config->kind) {
    case QT_KIND_EVERYDAY:
      for (int i = 0; i < PBL_DAY_PER_WEEK; i++) {
        out_days[i] = true;
      }
      break;
    case QT_KIND_WEEKDAYS:
      out_days[PBL_SUNDAY] = false;
      out_days[PBL_MONDAY] = true;
      out_days[PBL_TUESDAY] = true;
      out_days[PBL_WEDNESDAY] = true;
      out_days[PBL_THURSDAY] = true;
      out_days[PBL_FRIDAY] = true;
      out_days[PBL_SATURDAY] = false;
      break;
    case QT_KIND_WEEKENDS:
      out_days[PBL_SUNDAY] = true;
      out_days[PBL_MONDAY] = false;
      out_days[PBL_TUESDAY] = false;
      out_days[PBL_WEDNESDAY] = false;
      out_days[PBL_THURSDAY] = false;
      out_days[PBL_FRIDAY] = false;
      out_days[PBL_SATURDAY] = true;
      break;
    case QT_KIND_CUSTOM:
      memcpy(out_days, config->scheduled_days, PBL_DAY_PER_WEEK);
      break;
    default:
      memset(out_days, 0, PBL_DAY_PER_WEEK);
      break;
  }
}

//! Effective end of a schedule's quiet window, in minutes-from-midnight. A
//! from==to schedule means a one-minute window, never a no-op.
static int prv_schedule_end_minutes(const QuietTimeScheduleConfig *schedule) {
  int from_minutes = schedule->from_hour * 60 + schedule->from_minute;
  int to_minutes = schedule->to_hour * 60 + schedule->to_minute;
  if (from_minutes == to_minutes) {
    to_minutes = (to_minutes + 1) % (24 * 60);
    if (to_minutes == 0) {
      to_minutes = 1;
    }
  }
  return to_minutes;
}

//! Whether a schedule is active now. A wrapping window belongs to the day it
//! started on: after midnight it is still yesterday's window.
static bool prv_schedule_is_active(const struct tm *now, const QuietTimeScheduleConfig *s) {
  bool days[PBL_DAY_PER_WEEK];
  quiet_time_get_scheduled_days(s, days);
  int now_m = now->tm_hour * 60 + now->tm_min;
  int from_m = s->from_hour * 60 + s->from_minute;
  int to_m = prv_schedule_end_minutes(s);
  if (from_m <= to_m) {
    return days[now->tm_wday] && now_m >= from_m && now_m < to_m;
  }
  int yesterday = (now->tm_wday + PBL_DAY_PER_WEEK - 1) % PBL_DAY_PER_WEEK;
  return (days[now->tm_wday] && now_m >= from_m) || (days[yesterday] && now_m < to_m);
}

static bool prv_is_any_qt_schedule_active_now(void) {
  struct tm time;
  rtc_get_time_tm(&time);
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    if (!s_qt_schedule_cache[i].is_used || !s_qt_schedule_cache[i].enabled)
      continue;
    if (prv_schedule_is_active(&time, &s_qt_schedule_cache[i]))
      return true;
  }
  return false;
}

static void prv_try_update_schedule_mode(void *data) {
  const bool clear_override = (bool)(uintptr_t)data;
  if (clear_override) {
    s_data.manually_override_dnd = false;
  }
  prv_reload_qt_schedule_cache();
  prv_update_schedule_mode();
  prv_do_update();
}

static void prv_try_update_schedule_mode_callback(bool clear_manual_override) {
  system_task_add_callback(prv_try_update_schedule_mode, (void *)(uintptr_t)clear_manual_override);
}

static void prv_qt_cron_callback(struct pbl_cron_job *job, void *data) {
  const int slot = (int)(uintptr_t)data / 2;
  const bool is_end = (bool)((int)(uintptr_t)data % 2);
  PBL_LOG_DBG("Quiet Time slot %d %s boundary fired", slot, is_end ? "end" : "start");
  prv_try_update_schedule_mode_callback(true);
}

//! Day mask for a cron job from a scheduled-days array. tm_wday numbering
//! matches the PBL_CRON_WDAY_ bits (Sunday is bit 0).
static uint8_t prv_qt_wday_mask(const bool days[PBL_DAY_PER_WEEK]) {
  uint8_t mask = 0;
  for (int i = 0; i < PBL_DAY_PER_WEEK; i++) {
    if (days[i]) {
      mask |= (1 << i);
    }
  }
  return mask;
}

static void prv_schedule_boundary_job(struct pbl_cron_job *job, int slot, bool is_end, int hour,
                                      int minute, uint8_t wday) {
  *job = (struct pbl_cron_job){
    .cb = prv_qt_cron_callback,
    .cb_data = (void *)(uintptr_t)(slot * 2 + (is_end ? 1 : 0)),
    .minute = minute,
    .hour = hour,
    .mday = PBL_CRON_MDAY_ANY,
    .month = PBL_CRON_MONTH_ANY,
    .wday = wday,
  };
  pbl_cron_job_schedule(job);
}

static void prv_schedule_qt_slot_jobs(int slot, const QuietTimeScheduleConfig *s) {
  bool days[PBL_DAY_PER_WEEK];
  quiet_time_get_scheduled_days(s, days);
  const uint8_t start_mask = prv_qt_wday_mask(days);
  const int from_min = s->from_hour * 60 + s->from_minute;
  const int to_min = prv_schedule_end_minutes(s);
  uint8_t end_mask = start_mask;
  if (from_min > to_min) {
    // Wrapping window: the end belongs to the day after each start day.
    end_mask = 0;
    for (int d = 0; d < PBL_DAY_PER_WEEK; d++) {
      if (days[d]) {
        end_mask |= (1 << ((d + 1) % PBL_DAY_PER_WEEK));
      }
    }
  }
  if (start_mask == 0 && end_mask == 0) {
    // No days selected: a zero mask means PBL_CRON_WDAY_ANY, so skip both jobs.
    return;
  }
  prv_schedule_boundary_job(&s_qt_jobs[slot][0], slot, false, s->from_hour, s->from_minute,
                            start_mask);
  prv_schedule_boundary_job(&s_qt_jobs[slot][1], slot, true, to_min / 60, to_min % 60, end_mask);
}

static void prv_update_schedule_mode(void) {
  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    pbl_cron_job_unschedule(&s_qt_jobs[i][0]);
    pbl_cron_job_unschedule(&s_qt_jobs[i][1]);
  }

  for (int i = 0; i < MAX_QUIET_TIME_SCHEDULES; i++) {
    if (!s_qt_schedule_cache[i].is_used || !s_qt_schedule_cache[i].enabled) {
      continue;
    }
    prv_schedule_qt_slot_jobs(i, &s_qt_schedule_cache[i]);
  }

  const bool currently_active = prv_is_any_qt_schedule_active_now();
  if (currently_active != s_data.is_in_schedule_period) {
    if (currently_active && do_not_disturb_is_manually_enabled()) {
      alerts_preferences_dnd_set_manually_enabled(false);
      s_data.manually_override_dnd = false;
    } else if (!currently_active && s_data.is_in_schedule_period &&
               do_not_disturb_is_manually_enabled()) {
      // Coming out of scheduled DND with manual DND on, turning it off.
      do_not_disturb_set_manually_enabled(false);
    } else if (!currently_active && s_data.manually_override_dnd) {
      s_data.manually_override_dnd = false;
    }
    s_data.is_in_schedule_period = currently_active;
  }

  PBL_LOG_DBG("%s scheduled period", s_data.is_in_schedule_period ? "In" : "Out of");
}

static bool prv_is_schedule_active(void) {
  return (s_data.is_in_schedule_period && !s_data.manually_override_dnd);
}

static bool prv_is_smart_dnd_active(void) {
  return (calendar_event_is_ongoing() && do_not_disturb_is_smart_dnd_enabled() &&
          !s_data.manually_override_dnd);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Public Functions
///////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_SYSCALL(bool, sys_do_not_disturb_is_active, void) {
  return do_not_disturb_is_active();
}

bool do_not_disturb_is_active(void) {
  if (do_not_disturb_is_manually_enabled() || prv_is_schedule_active() ||
      prv_is_smart_dnd_active()) {
    return true;
  }
  return false;
}

bool do_not_disturb_is_manually_enabled(void) {
  return alerts_preferences_dnd_is_manually_enabled();
}

void do_not_disturb_set_manually_enabled(bool enable) {
  const bool is_auto_dnd =
      prv_is_any_qt_schedule_enabled() || do_not_disturb_is_smart_dnd_enabled();
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

bool do_not_disturb_is_schedule_enabled(DoNotDisturbScheduleType type) {
  return alerts_preferences_dnd_is_schedule_enabled(type);
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
  if (index < 0 || index >= MAX_QUIET_TIME_SCHEDULES)
    return;
  if (!quiet_time_schedule_is_valid(config))
    return;
  QuietTimeScheduleConfig stored = *config;
  stored.is_used = true;
  alerts_preferences_qt_set_schedule(index, &stored);
  prv_try_update_schedule_mode_callback(true);
}

int quiet_time_create_schedule(const QuietTimeScheduleConfig *config) {
  if (!quiet_time_schedule_is_valid(config))
    return -1;
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
  if (index < 0 || index >= MAX_QUIET_TIME_SCHEDULES)
    return;
  QuietTimeScheduleConfig empty = {0};
  alerts_preferences_qt_set_schedule(index, &empty);
  prv_try_update_schedule_mode_callback(true);
}

void quiet_time_set_schedule_enabled(int index, bool enabled) {
  if (index < 0 || index >= MAX_QUIET_TIME_SCHEDULES)
    return;
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
    case QT_KIND_EVERYDAY:
      return i18n_noop("Every Day");
    case QT_KIND_WEEKDAYS:
      return i18n_noop("Weekdays");
    case QT_KIND_WEEKENDS:
      return i18n_noop("Weekends");
    case QT_KIND_CUSTOM:
      return i18n_noop("Custom");
    default:
      return "";
  }
}

void quiet_time_get_string_for_custom(const uint8_t *scheduled_days, char *buffer, size_t buf_len) {
  static const char *const day_strings[] = {
    i18n_noop("Sun"), i18n_noop("Mon"), i18n_noop("Tue"), i18n_noop("Wed"),
    i18n_noop("Thu"), i18n_noop("Fri"), i18n_noop("Sat"),
  };
  static const char *const full_day_strings[] = {
    i18n_noop("Sundays"),   i18n_noop("Mondays"), i18n_noop("Tuesdays"),  i18n_noop("Wednesdays"),
    i18n_noop("Thursdays"), i18n_noop("Fridays"), i18n_noop("Saturdays"),
  };

  PBL_ASSERTN(buffer != NULL);
  PBL_ASSERTN(buf_len > 0);
  buffer[0] = '\0';

  int num_days = 0;
  int last_day_idx = 0;
  for (int i = 0; i < PBL_DAY_PER_WEEK; i++) {
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
  bool truncated = false;
  for (int idx = 1; idx <= PBL_DAY_PER_WEEK; idx++) {
    int i = idx % PBL_DAY_PER_WEEK;
    if (!scheduled_days[i]) {
      continue;
    }
    char day_buf[12];
    i18n_get_with_buffer(day_strings[i], day_buf, sizeof(day_buf));
    size_t day_len = strlen(day_buf);
    size_t needed = day_len + (pos > 0 ? 1 : 0);
    if (pos + needed >= buf_len) {
      truncated = true;
      break;
    }
    if (pos > 0) {
      buffer[pos++] = ',';
    }
    memcpy(buffer + pos, day_buf, day_len);
    pos += day_len;
  }
  // Some days did not fit: make the truncation visible instead of silently
  // hiding scheduled days. The marker only goes in when there is room for it
  // (the UTF-8 ellipsis is 3 bytes plus the terminator).
  if (truncated && buf_len >= pos + 4) {
    buffer[pos++] = 0xE2;
    buffer[pos++] = 0x80;
    buffer[pos++] = 0xA6;
  }
  buffer[pos] = '\0';
}

void do_not_disturb_init(void) {
  s_data = (DoNotDisturbData){
    .was_active = false,
  };
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

void do_not_disturb_manual_toggle_with_dialog(void) {
  do_not_disturb_toggle_push(ActionTogglePrompt_NoPrompt, false /* set_exit_reason */);
}
