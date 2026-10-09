/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/notifications/alerts_preferences.h>

#include <kernel/events.h>

/**
 * @defgroup services_notifications_do_not_disturb Do Not Disturb
 * @ingroup services_notifications
 * @brief Quiet Time state, combining manual, scheduled and calendar-aware activation.
 *
 * Meant for internal use; clients should ask @ref services_notifications_alerts whether and how
 * the user can be notified. DND is active when any of these modes is:
 *
 * - Manual: an on/off switch. Turning it off while an automatic mode is active overrides that mode
 *   until the next schedule boundary or settings change. It turns itself off when a scheduled
 *   period ends.
 * - Scheduled: a daily period, with separate schedules for weekdays and weekends.
 * - Smart (calendar aware): active while a calendar event is ongoing.
 *
 * Every change of the active state posts a @c PEBBLE_DO_NOT_DISTURB_EVENT.
 * @{
 */

/** @brief Schedule selector. */
typedef enum DoNotDisturbScheduleType {
  /** Schedule used Monday to Friday. */
  WeekdaySchedule,
  /** Schedule used on Saturday and Sunday. */
  WeekendSchedule,
  /** Number of schedules. */
  NumDNDSchedules,
} DoNotDisturbScheduleType;

/**
 * @brief Daily DND period, in local time.
 *
 * A period whose end is earlier than its start spans midnight; equal start and end never match.
 */
typedef struct PBL_PACKED DoNotDisturbSchedule {
  /** Start hour, 0 to 23. */
  uint8_t from_hour;
  /** Start minute, 0 to 59. */
  uint8_t from_minute;
  /** End hour (exclusive), 0 to 23. */
  uint8_t to_hour;
  /** End minute (exclusive), 0 to 59. */
  uint8_t to_minute;
} DoNotDisturbSchedule;

/** @brief Where manual DND was toggled from, matching the FirstUseSource values. */
typedef enum ManualDNDFirstUseSource {
  /** Notification action menu. */
  ManualDNDFirstUseSourceActionMenu = 0,
  /** Settings menu. */
  ManualDNDFirstUseSourceSettingsMenu
} ManualDNDFirstUseSource;

/**
 * @brief Check whether DND is in effect.
 *
 * @return true if manual, scheduled or smart DND is active.
 */
bool do_not_disturb_is_active(void);

/**
 * @brief Check whether manual DND is on.
 *
 * @return true if DND has been manually enabled.
 */
bool do_not_disturb_is_manually_enabled(void);

/**
 * @brief Set the manual DND state.
 *
 * Turning it off while scheduled or smart DND is active overrides the automatic mode.
 *
 * @param enable true to turn manual DND on.
 */
void do_not_disturb_set_manually_enabled(bool enable);

/**
 * @brief Toggle manual DND, showing the first use dialog the first time.
 *
 * From the settings menu this flips the manual setting. From a notification action menu it sets
 * manual DND to the opposite of the current DND active state, showing a confirmation.
 *
 * @param source Where the toggle came from.
 */
void do_not_disturb_toggle_manually_enabled(ManualDNDFirstUseSource source);

/**
 * @brief Check whether calendar-aware (smart) DND is enabled.
 *
 * @return true if enabled.
 */
bool do_not_disturb_is_smart_dnd_enabled(void);

/** @brief Toggle smart DND, showing the first use dialog the first time. */
void do_not_disturb_toggle_smart_dnd(void);

/**
 * @brief Get a DND schedule.
 *
 * @param type Schedule to get.
 * @param[out] schedule_out Schedule.
 */
void do_not_disturb_get_schedule(DoNotDisturbScheduleType type, DoNotDisturbSchedule *schedule_out);

/**
 * @brief Set a DND schedule and re-evaluate the DND state.
 *
 * @param type Schedule to set.
 * @param schedule New schedule.
 */
void do_not_disturb_set_schedule(DoNotDisturbScheduleType type, DoNotDisturbSchedule *schedule);

/**
 * @brief Check whether a DND schedule is enabled.
 *
 * @param type Schedule to check.
 * @return true if enabled.
 */
bool do_not_disturb_is_schedule_enabled(DoNotDisturbScheduleType type);

/**
 * @brief Enable or disable a DND schedule and re-evaluate the DND state.
 *
 * @param type Schedule to change.
 * @param scheduled true to enable.
 */
void do_not_disturb_set_schedule_enabled(DoNotDisturbScheduleType type, bool scheduled);

/**
 * @brief Toggle a DND schedule and re-evaluate the DND state.
 *
 * @param type Schedule to toggle.
 */
void do_not_disturb_toggle_scheduled(DoNotDisturbScheduleType type);

/** @brief Initialize the DND service and arm the schedule timers. */
void do_not_disturb_init(void);

/** @brief Re-evaluate the schedule after the wall clock or timezone changed. */
void do_not_disturb_handle_clock_change(void);

/**
 * @brief Handle a DND state preference (manual, smart or schedule) written by phone sync.
 *
 * Re-evaluates the DND state so the change fires the usual event and updates the schedule timers.
 */
void do_not_disturb_handle_pref_synced(void);

/**
 * @brief Re-evaluate smart DND after a calendar event.
 *
 * @param e Calendar event.
 */
void do_not_disturb_handle_calendar_event(PebbleCalendarEvent *e);

/** @brief Push the manual DND toggle prompt, which sets manual DND to the opposite state. */
void do_not_disturb_manual_toggle_with_dialog(void);

/** @} */
