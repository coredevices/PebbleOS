/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/preferred_content_size.h"
#include "kernel/events.h"
#include "pbl/services/notifications/alerts_private.h"
#include "pbl/services/notifications/do_not_disturb.h"
#include "pbl/services/vibes/vibe_intensity.h"
#include "pbl/util/units.h"
#include "pbl/services/vibes/vibe_client.h"
#include "pbl/services/vibes/vibe_score_info.h"

/**
 * @defgroup services_notifications_alerts_preferences_private Alert preferences (internal)
 * @ingroup services_notifications
 * @brief Preferences backing the alerts, Do Not Disturb and notification UI services.
 *
 * Setters update the cached value and persist it in the @c notifpref settings file. Values
 * written by the phone through BlobDB are reloaded by alerts_preferences_handle_blob_db_event().
 * @{
 */

/** @brief Notification window timeout meaning "never time out". */
#define NOTIF_WINDOW_TIMEOUT_INFINITE ((uint32_t)~0)
/** @brief Default notification window timeout, in milliseconds. */
#define NOTIF_WINDOW_TIMEOUT_DEFAULT (3 * PBL_MSEC_PER_MIN)
/** @brief Shortest notification window timeout, in milliseconds. */
#define NOTIF_WINDOW_TIMEOUT_MIN (15 * PBL_MSEC_PER_SEC)

/** @brief Load all preferences from the settings file, migrating legacy keys. */
void alerts_preferences_init(void);

/**
 * @brief Get the alert mask.
 *
 * A legacy "all on" mask is migrated to AlertMaskAllOn.
 *
 * @return Alert types that may alert the user. Defaults to AlertMaskAllOn.
 */
AlertMask alerts_preferences_get_alert_mask(void);

/**
 * @brief Set the alert mask.
 *
 * @param mask Alert types that may alert the user.
 */
void alerts_preferences_set_alert_mask(AlertMask mask);

/**
 * @brief Get the Do Not Disturb mask.
 *
 * @return Alert types that may still vibrate or light the backlight while DND is active. Defaults
 *         to AlertMaskAllOff.
 */
AlertMask alerts_preferences_dnd_get_mask(void);

/**
 * @brief Set the Do Not Disturb mask.
 *
 * @param mask Alert types that may still vibrate or light the backlight while DND is active.
 */
void alerts_preferences_dnd_set_mask(AlertMask mask);

/**
 * @brief Get the notification window timeout.
 *
 * @return Timeout in milliseconds, never less than @ref NOTIF_WINDOW_TIMEOUT_MIN.
 */
uint32_t alerts_preferences_get_notification_window_timeout_ms(void);

/**
 * @brief Set the notification window timeout.
 *
 * @param timeout_ms Timeout in milliseconds, or @ref NOTIF_WINDOW_TIMEOUT_INFINITE.
 */
void alerts_preferences_set_notification_window_timeout_ms(uint32_t timeout_ms);

/**
 * @brief Get whether notifications use the alternative design.
 *
 * @return true for the alternative (black banner) design, false for the standard one (default).
 */
bool alerts_preferences_get_notification_alternative_design(void);

/**
 * @brief Set whether notifications use the alternative design.
 *
 * @param alternative true for the alternative (black banner) design.
 */
void alerts_preferences_set_notification_alternative_design(bool alternative);

/**
 * @brief Get whether the notification vibration is delayed.
 *
 * @return true to vibrate at the end of the notification animation (default), false to vibrate
 *         immediately.
 */
bool alerts_preferences_get_notification_vibe_delay(void);

/**
 * @brief Set whether the notification vibration is delayed.
 *
 * @param delay true to vibrate at the end of the notification animation.
 */
void alerts_preferences_set_notification_vibe_delay(bool delay);

/**
 * @brief Get whether notifications turn on the backlight.
 *
 * @return true if enabled (default).
 */
bool alerts_preferences_get_notification_backlight(void);

/**
 * @brief Set whether notifications turn on the backlight.
 *
 * @param enable true to enable.
 */
void alerts_preferences_set_notification_backlight(bool enable);

/** @brief Age range within which notifications from the same sender are grouped. */
typedef enum {
  /** Grouping disabled (default). */
  NotificationGroupingRange_Never = 0,
  /** Group notifications received within the last day. */
  NotificationGroupingRange_OneDay,
  /** Group notifications received within the last week. */
  NotificationGroupingRange_OneWeek,
  /** Group all notifications. */
  NotificationGroupingRange_All,
  /** Number of ranges. */
  NotificationGroupingRangeCount,
} NotificationGroupingRange;

/**
 * @brief Get the notification grouping range.
 *
 * @return Grouping range; NotificationGroupingRange_Never if the stored value is invalid.
 */
NotificationGroupingRange alerts_preferences_get_notification_grouping_range(void);

/**
 * @brief Set the notification grouping range.
 *
 * @param range Grouping range.
 */
void alerts_preferences_set_notification_grouping_range(NotificationGroupingRange range);

/** @brief Style of the clock in the notification window status bar. */
typedef enum {
  /** Regular clock (default). */
  NotificationStatusBarStyle_Default = 0,
  /** Bold clock. */
  NotificationStatusBarStyle_Bold = 1,
  /** Large bold clock. */
  NotificationStatusBarStyle_LargeBold = 2,
  /** Number of styles. */
  NotificationStatusBarStyleCount,
} NotificationStatusBarStyle;

/**
 * @brief Get the notification status bar style.
 *
 * @return Status bar style.
 */
NotificationStatusBarStyle alerts_preferences_get_notification_status_bar_style(void);

/**
 * @brief Set the notification status bar style.
 *
 * @param style Status bar style.
 */
void alerts_preferences_set_notification_status_bar_style(NotificationStatusBarStyle style);

/** @brief Notification content size value meaning "follow the system content size". */
#define NotificationContentSizeSystem ((PreferredContentSize)NumPreferredContentSizes)

/**
 * @brief Get the notification content size.
 *
 * @return Content size, or @ref NotificationContentSizeSystem when notifications follow the system
 *         content size.
 */
PreferredContentSize alerts_preferences_get_notification_content_size(void);

/**
 * @brief Set the notification content size.
 *
 * @param size Content size or @ref NotificationContentSizeSystem; larger values are ignored.
 */
void alerts_preferences_set_notification_content_size(PreferredContentSize size);

/**
 * @brief Get whether notifications vibrate.
 *
 * @return true if enabled (default).
 */
bool alerts_preferences_get_vibrate(void);

/**
 * @brief Set whether notifications vibrate.
 *
 * @param enable true to vibrate.
 */
void alerts_preferences_set_vibrate(bool enable);

/**
 * @brief Get the legacy vibration intensity.
 *
 * @return Vibration intensity.
 */
VibeIntensity alerts_preferences_get_vibe_intensity(void);

/**
 * @brief Set the legacy vibration intensity.
 *
 * @param intensity Vibration intensity.
 */
void alerts_preferences_set_vibe_intensity(VibeIntensity intensity);

/**
 * @brief Get the vibration pattern used for a vibe client.
 *
 * @param client Vibe client.
 * @return Vibe score of @p client.
 */
VibeScoreId alerts_preferences_get_vibe_score_for_client(VibeClient client);

/**
 * @brief Set the vibration pattern used for a vibe client.
 *
 * @param client Vibe client.
 * @param id Vibe score to use.
 */
void alerts_preferences_set_vibe_score_for_client(VibeClient client, VibeScoreId id);

/**
 * @brief Get the stored manual DND state.
 *
 * @return true if manual DND is on.
 */
bool alerts_preferences_dnd_is_manually_enabled(void);

/**
 * @brief Store the manual DND state.
 *
 * Use do_not_disturb_set_manually_enabled() to also update the DND state.
 *
 * @param enable true to turn manual DND on.
 */
void alerts_preferences_dnd_set_manually_enabled(bool enable);

/**
 * @brief Get a DND schedule.
 *
 * @param type Schedule to get.
 * @param[out] schedule_out Schedule.
 */
void alerts_preferences_dnd_get_schedule(DoNotDisturbScheduleType type,
                                         DoNotDisturbSchedule *schedule_out);

/**
 * @brief Store a DND schedule.
 *
 * @param type Schedule to set.
 * @param schedule New schedule.
 */
void alerts_preferences_dnd_set_schedule(DoNotDisturbScheduleType type,
                                         const DoNotDisturbSchedule *schedule);

/**
 * @brief Get whether a DND schedule is enabled.
 *
 * @param type Schedule to check.
 * @return true if enabled.
 */
bool alerts_preferences_dnd_is_schedule_enabled(DoNotDisturbScheduleType type);

/**
 * @brief Store whether a DND schedule is enabled.
 *
 * @param type Schedule to change.
 * @param enable true to enable.
 */
void alerts_preferences_dnd_set_schedule_enabled(DoNotDisturbScheduleType type, bool enable);

/**
 * @brief Get whether calendar-aware (smart) DND is enabled.
 *
 * @return true if enabled.
 */
bool alerts_preferences_dnd_is_smart_enabled(void);

/**
 * @brief Store whether calendar-aware (smart) DND is enabled.
 *
 * @param enable true to enable.
 */
void alerts_preferences_dnd_set_smart_enabled(bool enable);

/**
 * @brief Lock the alerts preferences mutex.
 *
 * Must be paired with alerts_preferences_unlock().
 */
void alerts_preferences_lock(void);

/**
 * @brief Unlock the alerts preferences mutex.
 *
 * Must be paired with alerts_preferences_lock().
 */
void alerts_preferences_unlock(void);

/**
 * @brief Process a BlobDB event for notification preferences.
 *
 * For BlobDBEventTypeInsert events, reloads the cached copy of the preference whose key was
 * written. A change to a DND state preference also re-evaluates the DND state.
 *
 * @param event BlobDB event.
 */
void alerts_preferences_handle_blob_db_event(PebbleBlobDBEvent *event);

/** @} */
