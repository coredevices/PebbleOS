/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "kernel/events.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "util/time/time.h"

#include <inttypes.h>
#include <stdbool.h>

typedef enum DoNotDisturbScheduleType {
  WeekdaySchedule,
  WeekendSchedule,
  NumDNDSchedules,
} DoNotDisturbScheduleType;

typedef struct PACKED DoNotDisturbSchedule {
  uint8_t from_hour;
  uint8_t from_minute;
  uint8_t to_hour;
  uint8_t to_minute;
} DoNotDisturbSchedule;

#define MAX_QUIET_TIME_SCHEDULES (5)

typedef enum {
  QT_KIND_EVERYDAY = 0,
  QT_KIND_WEEKDAYS,
  QT_KIND_WEEKENDS,
  QT_KIND_CUSTOM,
} QuietTimeKind;

//! QuietTimeScheduleConfig stores a single quiet time schedule slot.
//! The scheduled_days array is always present but only meaningful when kind == QT_KIND_CUSTOM.
//! For other kinds, the day mask is derived from the kind at runtime.
//! is_used indicates whether the slot contains a valid schedule (unused slots are zeroed out).
typedef struct PACKED QuietTimeScheduleConfig {
  bool is_used;
  QuietTimeKind kind;
  bool scheduled_days[DAYS_PER_WEEK];
  uint8_t from_hour;
  uint8_t from_minute;
  uint8_t to_hour;
  uint8_t to_minute;
  bool enabled;
} QuietTimeScheduleConfig;

typedef enum ManualDNDFirstUseSource {
  ManualDNDFirstUseSourceActionMenu = 0,
  ManualDNDFirstUseSourceSettingsMenu
} ManualDNDFirstUseSource;

//! The Do Not Disturb service is meant for internal use only. Clients should use the Alerts
//! Service to determine how/when the user can be notified.

//! DND (Quiet Time) Activation Modes
//! Manual - Allows the user to quickly put the watch into an active DND mode. It
//! overrides other DND activation modes if toggled off. Once the watch comes out of scheduled DND,
//! manual DND automatically turns off.
//! Smart DND (Calendar Aware) - Leverages the calendar service to determine if an event is ongoing
//! and automatically puts the watch into an Active DND Mode
//! Scheduled DND - Allows the user to specify a daily schedule for when the DND should be in active
//! mode. Once coming out of a schedule, if the Manual DND is enabled, it disables that setting.

//! @return True if DND is in effect, false if not.
bool do_not_disturb_is_active(void);

//! Manual DND is a simple on / off switch for DND,
//! which works along side automatic modes (scheduled and calendar aware ('smart')

//! @return True if DND has been manually enabled, false if not.
bool do_not_disturb_is_manually_enabled(void);

//! Set the current manual DND state
void do_not_disturb_set_manually_enabled(bool enable);

//! Toggle the current manual DND state. Provide the source from which it was toggled.
//! Toggling from the settings menu simply toggles the Manual DND setting
//! Toggling from a notification action menu sets the Manual DND setting to opposite of the current
//! DND active state.
void do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSource source);

bool do_not_disturb_is_smart_dnd_enabled(void);

void do_not_disturb_toggle_smart_dnd(void);

void do_not_disturb_get_schedule(DoNotDisturbScheduleType type, DoNotDisturbSchedule *schedule_out);

void do_not_disturb_set_schedule(DoNotDisturbScheduleType type, DoNotDisturbSchedule *schedule);

bool do_not_disturb_is_schedule_enabled(DoNotDisturbScheduleType type);

void do_not_disturb_set_schedule_enabled(DoNotDisturbScheduleType type, bool scheduled);

void do_not_disturb_toggle_scheduled(DoNotDisturbScheduleType type);

//! Iterate over all quiet time schedules. Callback is called for each slot (0..MAX_QUIET_TIME_SCHEDULES-1)
//! regardless of whether the slot is active.
typedef void (*QuietTimeScheduleCallback)(int index, const QuietTimeScheduleConfig *config, void *context);
void quiet_time_for_each_schedule(QuietTimeScheduleCallback cb, void *context);

//! Get/set an individual quiet time schedule slot
void quiet_time_get_schedule(int index, QuietTimeScheduleConfig *out);
void quiet_time_set_schedule(int index, const QuietTimeScheduleConfig *config);

//! Create a new quiet time schedule. Returns the slot index, or -1 if all slots are full.
//! Rejects QT_KIND_CUSTOM with no days selected (returns -1).
int quiet_time_create_schedule(const QuietTimeScheduleConfig *config);

//! Delete a quiet time schedule slot
void quiet_time_delete_schedule(int index);

//! Enable/disable a quiet time schedule slot
void quiet_time_set_schedule_enabled(int index, bool enabled);

//! Derive the day mask for a schedule kind. For QT_KIND_CUSTOM, copies from config->scheduled_days.
void quiet_time_get_scheduled_days(const QuietTimeScheduleConfig *config, bool out_days[DAYS_PER_WEEK]);

//! Display string for a QuietTimeKind
const char *quiet_time_get_string_for_kind(QuietTimeKind kind);

//! Display string for custom scheduled days. Buffer must be at least 28 bytes.
void quiet_time_get_string_for_custom(const bool *scheduled_days, char *buffer, size_t buf_len);

//! Get the number of active (enabled and non-empty) schedule slots
int quiet_time_get_num_active(void);

void do_not_disturb_init(void);

void do_not_disturb_handle_clock_change(void);

void do_not_disturb_handle_calendar_event(PebbleCalendarEvent *e);

void do_not_disturb_manual_toggle_with_dialog(void);

#if UNITTEST
#include "pbl/services/new_timer/new_timer.h"
TimerID get_dnd_timer_id(void);
void set_dnd_timer_id(TimerID id);
#endif
