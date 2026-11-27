/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <time.h>

#include <pbl/util/units.h>

/**
 * @defgroup services_alarms Alarms
 * @ingroup services
 * @brief User alarms, persisted across resets.
 *
 * When an enabled alarm goes off, a @c PEBBLE_ALARM_CLOCK_EVENT is put and a pin is added to the
 * timeline. Each scheduled alarm also keeps a pin for its next occurrence in the timeline. A
 * smart alarm starts watching for light sleep or movement @ref SMART_ALARM_RANGE_S before its
 * time and fires as soon as the user is awake or moving, or at its time at the latest. Up to 10
 * alarms can be configured.
 *
 * @code{.c}
 * const AlarmInfo info = {
 *   .hour = 7,
 *   .minute = 30,
 *   .kind = ALARM_KIND_WEEKDAYS,
 *   .vibrate_enabled = true,
 * };
 * AlarmId id = alarm_create(&info);
 * @endcode
 * @{
 */

/** @brief How long before its time a smart alarm may go off, in seconds. */
#define SMART_ALARM_RANGE_S (30 * PBL_SEC_PER_MIN)
/** @brief Interval at which a smart alarm re-checks the sleep state, in seconds. */
#define SMART_ALARM_SNOOZE_DELAY_S (1 * PBL_SEC_PER_MIN)
/** @brief Light sleep threshold for smart alarms, in seconds. Currently only used by tests. */
#define SMART_ALARM_MAX_LIGHT_SLEEP_S (30 * PBL_SEC_PER_MIN)
/** @brief Number of sleep state checks a smart alarm makes before firing unconditionally. */
#define SMART_ALARM_MAX_SMART_SNOOZE (SMART_ALARM_RANGE_S / SMART_ALARM_SNOOZE_DELAY_S)

/** @brief Highlight color of the alarms app. */
#define ALARMS_APP_HIGHLIGHT_COLOR PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack)

/** @brief Slot ID of a configured alarm. IDs may be reused after an alarm is deleted. */
typedef int AlarmId;

/** @brief Invalid alarm ID, returned on failure. */
#define ALARM_INVALID_ID (-1)

/** @brief Recurrence of an alarm. */
typedef enum AlarmKind {
  /** Every day. */
  ALARM_KIND_EVERYDAY = 0,
  /** Saturday and Sunday. */
  ALARM_KIND_WEEKENDS,
  /** Monday to Friday. */
  ALARM_KIND_WEEKDAYS,
  /** The next time the specified time occurs; the alarm is disabled once it fires. */
  ALARM_KIND_JUST_ONCE,
  /** The specified days of the week. */
  ALARM_KIND_CUSTOM,
} AlarmKind;

/** @brief Alarm type, as shown on its timeline pin. */
typedef enum AlarmType {
  /** Regular alarm. */
  AlarmType_Basic,
  /** Smart alarm. */
  AlarmType_Smart,
  /** Number of alarm types. */
  AlarmTypeCount,
} AlarmType;

/** @brief Built-in alarm tones, played on speaker hardware when sound is enabled. */
typedef enum AlarmTone {
  /** Reveille. */
  AlarmTone_Reveille = 0,
  /** Beacon. */
  AlarmTone_Beacon,
  /** Bell. */
  AlarmTone_Bell,
  /** Chime. */
  AlarmTone_Chime,
} AlarmTone;

/** @brief Alarm configuration. */
typedef struct AlarmInfo {
  /** Hour, 0-23, where 0 is 12am. */
  int hour;
  /** Minute, 0-59. */
  int minute;
  /** Recurrence of the alarm. */
  AlarmKind kind;
  /**
   * Days the alarm runs on, one flag per weekday (Sunday = index 0), for
   * @ref ALARM_KIND_CUSTOM. May be NULL.
   */
  bool (*scheduled_days)[PBL_DAY_PER_WEEK];
  /** Whether the alarm goes off at its time. */
  bool enabled;
  /** Whether the alarm is a smart alarm. */
  bool is_smart;
  /** Whether the alarm plays a tone on speaker hardware. */
  bool sound_enabled;
  /** Whether the alarm vibrates. */
  bool vibrate_enabled;
  /** Tone played when @ref sound_enabled is set. */
  AlarmTone tone;
} AlarmInfo;

/**
 * @brief Callback for alarm_for_each().
 *
 * @param id Alarm ID.
 * @param info Alarm configuration, valid during the call only.
 * @param context Context passed to alarm_for_each().
 */
typedef void (*AlarmForEach)(AlarmId id, const AlarmInfo *info, void *context);

/**
 * @brief Create and schedule an alarm.
 *
 * The alarm is created enabled, whatever @ref AlarmInfo::enabled says.
 *
 * @param info Alarm configuration. @c scheduled_days is used for @ref ALARM_KIND_CUSTOM.
 * @return ID of the new alarm, or @ref ALARM_INVALID_ID on failure.
 */
AlarmId alarm_create(const AlarmInfo *info);

/** @brief Result of alarm_create_checked(). */
typedef enum AlarmCreateResult {
  /** The alarm was created and enabled. */
  AlarmCreateResultOk = 0,
  /** All alarm slots are in use. */
  AlarmCreateResultNoSpace,
  /** The configuration is invalid. */
  AlarmCreateResultInvalidArgument,
  /** The alarm could not be stored. */
  AlarmCreateResultStorageError,
} AlarmCreateResult;

/**
 * @brief Create and schedule an alarm, reporting why it failed.
 *
 * The slot is allocated and the configuration persisted under the alarm service lock, so
 * concurrent callers cannot both take the last free slot.
 *
 * @param info Alarm configuration. @c scheduled_days is used for @ref ALARM_KIND_CUSTOM.
 * @param[out] id_out Set to the new alarm ID on success, or @ref ALARM_INVALID_ID on failure.
 * @return The result of the operation.
 */
AlarmCreateResult alarm_create_checked(const AlarmInfo *info, AlarmId *id_out);

/** @brief Result of alarm_update_checked(). */
typedef enum AlarmUpdateResult {
  /** The alarm was updated. */
  AlarmUpdateResultOk = 0,
  /** No alarm with that ID is configured. */
  AlarmUpdateResultDoesNotExist,
  /** The configuration is invalid. */
  AlarmUpdateResultInvalidArgument,
  /** The alarm could not be stored. */
  AlarmUpdateResultStorageError,
} AlarmUpdateResult;

/**
 * @brief Replace an alarm's configuration, preserving its enabled state.
 *
 * The time, recurrence, smart, sound, vibration, and tone settings are
 * replaced. Use alarm_set_enabled() to change whether the alarm is enabled.
 *
 * @param id Alarm to update.
 * @param info New configuration. @c scheduled_days is used for @ref ALARM_KIND_CUSTOM.
 * @return The result of the operation.
 */
AlarmUpdateResult alarm_update_checked(AlarmId id, const AlarmInfo *info);

/**
 * @brief Set the time of an alarm.
 *
 * @param id Alarm to update.
 * @param hour Hour, 0-23, where 0 is 12am.
 * @param minute Minute, 0-59.
 */
void alarm_set_time(AlarmId id, int hour, int minute);

/**
 * @brief Set the recurrence of an alarm.
 *
 * @param id Alarm to update.
 * @param kind New recurrence. @ref ALARM_KIND_CUSTOM is ignored, use alarm_set_custom().
 */
void alarm_set_kind(AlarmId id, AlarmKind kind);

/**
 * @brief Make an alarm run on specific days of the week.
 *
 * Sets the kind to @ref ALARM_KIND_CUSTOM.
 *
 * @param id Alarm to update.
 * @param scheduled_days One flag per weekday (Sunday = index 0); the alarm runs on each day set.
 */
void alarm_set_custom(AlarmId id, const bool scheduled_days[PBL_DAY_PER_WEEK]);

/**
 * @brief Set whether an alarm is a smart alarm.
 *
 * @param id Alarm to update.
 * @param smart Whether the alarm is a smart alarm.
 */
void alarm_set_smart(AlarmId id, bool smart);

/**
 * @brief Set whether an alarm plays a tone.
 *
 * @param id Alarm to update.
 * @param enabled Whether the alarm plays a tone on speaker hardware.
 */
void alarm_set_sound_enabled(AlarmId id, bool enabled);

/**
 * @brief Set whether an alarm vibrates.
 *
 * @param id Alarm to update.
 * @param enabled Whether the alarm vibrates.
 */
void alarm_set_vibrate_enabled(AlarmId id, bool enabled);

/**
 * @brief Set the tone of an alarm.
 *
 * @param id Alarm to update.
 * @param tone Tone to play when sound is enabled.
 */
void alarm_set_tone(AlarmId id, AlarmTone tone);

/** @brief Result of alarm_get_info_checked(). */
typedef enum AlarmGetResult {
  /** The alarm was read. */
  AlarmGetResultOk = 0,
  /** No alarm with that ID is configured. */
  AlarmGetResultDoesNotExist,
  /** The output argument is invalid. */
  AlarmGetResultInvalidArgument,
  /** The alarm could not be read. */
  AlarmGetResultStorageError,
} AlarmGetResult;

/**
 * @brief Read an alarm and its custom days under one lock.
 *
 * @param id Alarm to look up.
 * @param[out] info_out Configuration. Its @c scheduled_days is set to NULL.
 * @param[out] scheduled_days One flag per weekday (Sunday = index 0). May be NULL.
 * @return The result of the operation.
 */
AlarmGetResult alarm_get_info_checked(AlarmId id, AlarmInfo *info_out,
                                      bool scheduled_days[PBL_DAY_PER_WEEK]);

/**
 * @brief Get the configuration of an alarm.
 *
 * @param id Alarm to look up.
 * @param[out] info_out Configuration. Its @c scheduled_days is set to NULL; use
 *                      alarm_get_custom_days() for the per-weekday flags.
 * @return true if the alarm exists.
 */
bool alarm_get_info(AlarmId id, AlarmInfo *info_out);

/**
 * @brief Get the most recently fired alarm.
 *
 * Used by the alarm popup to look up the settings of the firing alarm.
 *
 * @return ID of the most recently fired alarm, or @ref ALARM_INVALID_ID if none fired since boot
 *         (or it was since disabled or deleted).
 */
AlarmId alarm_get_most_recent_id(void);

/**
 * @brief Get the days of the week an alarm runs on.
 *
 * @param id Alarm to look up.
 * @param[out] scheduled_days One flag per weekday (Sunday = index 0), set for each day the alarm
 *                            runs on.
 * @return true if the alarm exists.
 */
bool alarm_get_custom_days(AlarmId id, bool scheduled_days[PBL_DAY_PER_WEEK]);

/** @brief Result of alarm_set_enabled_checked(). */
typedef enum AlarmSetEnabledResult {
  /** The alarm was updated. */
  AlarmSetEnabledResultOk = 0,
  /** No alarm with that ID is configured. */
  AlarmSetEnabledResultDoesNotExist,
  /** The alarm could not be stored. */
  AlarmSetEnabledResultStorageError,
} AlarmSetEnabledResult;

/**
 * @brief Enable or disable an alarm, reporting why it failed.
 *
 * Disabling the most recently fired alarm cancels its snooze.
 *
 * @param id Alarm to update.
 * @param enable Whether to enable the alarm.
 * @return The result of the operation.
 */
AlarmSetEnabledResult alarm_set_enabled_checked(AlarmId id, bool enable);

/**
 * @brief Enable or disable an alarm.
 *
 * Disabling the most recently fired alarm cancels its snooze.
 *
 * @param id Alarm to update.
 * @param enable Whether to enable the alarm.
 */
void alarm_set_enabled(AlarmId id, bool enable);

/**
 * @brief Delete an alarm and its timeline pins.
 *
 * @param id Alarm to delete.
 */
void alarm_delete(AlarmId id);

/** @brief Result of alarm_delete_checked(). */
typedef enum AlarmDeleteResult {
  /** The alarm was deleted. */
  AlarmDeleteResultOk = 0,
  /** No alarm with that ID is configured. */
  AlarmDeleteResultDoesNotExist,
  /** The alarm could not be deleted. */
  AlarmDeleteResultStorageError,
} AlarmDeleteResult;

/**
 * @brief Delete an alarm and its timeline pins, reporting why it failed.
 *
 * @param id Alarm to delete.
 * @return The result of the operation.
 */
AlarmDeleteResult alarm_delete_checked(AlarmId id);

/**
 * @brief Check whether an alarm is enabled.
 *
 * @param id Alarm to query.
 * @return true if the alarm exists and is enabled.
 */
bool alarm_get_enabled(AlarmId id);

/**
 * @brief Get the time of an alarm.
 *
 * @param id Alarm to query.
 * @param[out] hour_out Hour of the alarm, may be NULL.
 * @param[out] minute_out Minute of the alarm, may be NULL.
 * @return true if the alarm exists.
 */
bool alarm_get_hours_minutes(AlarmId id, int *hour_out, int *minute_out);

/**
 * @brief Get the recurrence of an alarm.
 *
 * @param id Alarm to query.
 * @param[out] kind_out Recurrence of the alarm, may be NULL.
 * @return true if the alarm exists.
 */
bool alarm_get_kind(AlarmId id, AlarmKind *kind_out);

/**
 * @brief Get the time of the next enabled alarm.
 *
 * @param[out] next_alarm_time_out Time of the next alarm, may be NULL.
 * @return true if at least one alarm is scheduled.
 */
bool alarm_get_next_enabled_alarm(time_t *next_alarm_time_out);

/**
 * @brief Check whether the next enabled alarm will use the smart alarm window.
 *
 * A smart alarm with activity tracking off is scheduled at its configured
 * time and reports false.
 *
 * @return true if an alarm is scheduled and its next occurrence uses the
 *         smart early window.
 */
bool alarm_is_next_enabled_alarm_smart(void);

/**
 * @brief Get the time until an alarm next goes off.
 *
 * The alarm may be disabled; the result is its next configured occurrence.
 *
 * @param id Alarm to query.
 * @param[out] time_out Seconds until the next occurrence of the alarm, may be NULL.
 * @return true if the alarm exists.
 */
bool alarm_get_time_until(AlarmId id, time_t *time_out);

/** @brief Snooze the most recently fired alarm for the current snooze delay. */
void alarm_set_snooze_alarm(void);

/**
 * @brief Get the snooze delay.
 *
 * @return Snooze delay in minutes.
 */
uint16_t alarm_get_snooze_delay(void);

/**
 * @brief Set and persist the snooze delay for all alarms.
 *
 * @param delay_m Snooze delay in minutes.
 */
void alarm_set_snooze_delay(uint16_t delay_m);

/** @brief Dismiss the most recently fired alarm, cancelling its snooze. */
void alarm_dismiss_alarm(void);

/**
 * @brief Call a function for each configured alarm.
 *
 * @param cb Callback, called with the alarm settings file locked.
 * @param context Passed to @p cb.
 */
void alarm_for_each(AlarmForEach cb, void *context);

/**
 * @brief Check whether another alarm can be created.
 *
 * @return true if the maximum number of alarms has not been reached.
 */
bool alarm_can_schedule(void);

/**
 * @brief Reschedule all alarms after the wall clock time changed.
 *
 * Required because alarm timers count seconds rather than absolute times. A smart alarm near its
 * deadline is fired; other snoozes are left running.
 */
void alarm_handle_clock_change(void);

/**
 * @brief Initialize the alarm service.
 *
 * Loads the alarms and the snooze delay, and detects an alarm missed while the watch was down.
 */
void alarm_init(void);

/**
 * @brief Enable or disable alarm events globally.
 *
 * While disabled, alarms are still scheduled but put no events. Enabling fires an alarm missed
 * while the watch was down, if it is at most a few minutes late.
 *
 * @param enable Whether alarms may go off.
 */
void alarm_service_enable_alarms(bool enable);

/**
 * @brief Get the display string of an alarm recurrence, e.g. "Weekends".
 *
 * @param kind Recurrence.
 * @param all_caps Whether to return the all-caps variant.
 * @return Untranslated string, to be passed through i18n.
 */
const char *alarm_get_string_for_kind(AlarmKind kind, bool all_caps);

/**
 * @brief Describe the days of a custom alarm.
 *
 * For example "Mondays" for one day, or "Mon,Sat,Sun" for several, translated and starting on
 * Monday.
 *
 * @param scheduled_days One flag per weekday (Sunday = index 0).
 * @param[in,out] alarm_day_text Buffer of at least 28 bytes holding an empty string; the text is
 *                               appended to it.
 */
void alarm_get_string_for_custom(bool scheduled_days[PBL_DAY_PER_WEEK], char *alarm_day_text);

/**
 * @brief Record the version of the alarms app that was last opened.
 *
 * @param version Alarms app version.
 */
void alarm_prefs_set_alarms_app_opened(uint8_t version);

/**
 * @brief Get the version of the alarms app that was last opened.
 *
 * @return Alarms app version.
 */
uint8_t alarm_prefs_get_alarms_app_opened(void);

/** @} */
