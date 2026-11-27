/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <pbl/util/time.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup AlarmService
//! \brief Access to the wearer's configured alarms.
//!
//! The AlarmService lets an app inspect the wearer's alarms, create a new
//! alarm, update it, enable or disable it, and delete it. The system supports
//! up to 10 configured alarms; use alarm_service_get_ids() to list them rather
//! than assuming how many slots exist.
//!
//! Reading alarms, including peeking at the next one, requires the
//! `AlarmsRead` permission. Creating, updating, enabling, disabling, or
//! deleting an alarm requires the `AlarmsWrite` permission. Declare
//! `manifestVersion: 2` and the permissions your app uses in the `pebble`
//! block of `package.json`:
//!
//! ```json
//! "usesPermissions": ["AlarmsRead", "AlarmsWrite"]
//! ```
//!
//! The phone app shows each permission at install time and syncs the user's
//! decisions to the watch. Apps without a permission get
//! @ref AlarmServiceResultErrPermissionDenied or
//! @ref AlarmServiceCreateErrPermissionDenied. An alarm created this way
//! becomes a normal wearer alarm: it appears in the Alarms app and survives
//! app uninstall. An app with `AlarmsWrite` can update or delete any alarm,
//! including one the wearer created.
//!   @{

//! @brief Maximum number of configured alarms.
#define ALARM_SERVICE_MAX_ALARMS 10

//! @brief Invalid alarm ID.
#define ALARM_SERVICE_INVALID_ID (-1)

//! @brief Recurrence of an alarm.
typedef enum AlarmServiceKind {
  //! Every day.
  AlarmServiceKindEveryday = 0,
  //! Saturday and Sunday.
  AlarmServiceKindWeekends,
  //! Monday to Friday.
  AlarmServiceKindWeekdays,
  //! The next time the specified time occurs; the alarm is disabled once it fires.
  AlarmServiceKindJustOnce,
  //! The days selected in @ref AlarmServiceInfo::custom_days.
  AlarmServiceKindCustom,
} AlarmServiceKind;

//! @brief Built-in alarm tones, played when sound is enabled.
typedef enum AlarmServiceTone {
  //! Reveille.
  AlarmServiceToneReveille = 0,
  //! Beacon.
  AlarmServiceToneBeacon,
  //! Bell.
  AlarmServiceToneBell,
  //! Chime.
  AlarmServiceToneChime,
} AlarmServiceTone;

//! @note `AlarmServiceInfo` and `AlarmServiceAlarm` are part of the app ABI and
//! are frozen. Add new functions for new data rather than growing these
//! structs.
//! @brief Alarm configuration used by alarm_service_create() and
//! alarm_service_update().
typedef struct AlarmServiceInfo {
  //! Hour, 0-23, where 0 is 12am.
  uint8_t hour;
  //! Minute, 0-59.
  uint8_t minute;
  //! Recurrence of the alarm.
  AlarmServiceKind kind;
  //! Days the alarm runs on for @ref AlarmServiceKindCustom, as a bitmask with
  //! bit 0 for Sunday through bit 6 for Saturday. Ignored for other kinds.
  uint8_t custom_days;
  //! Whether the alarm uses the smart alarm window while activity tracking
  //! is available.
  bool smart;
  //! Whether the alarm plays a tone.
  bool sound;
  //! Whether the alarm vibrates.
  bool vibrate;
  //! Tone played when @ref sound is set.
  AlarmServiceTone tone;
} AlarmServiceInfo;

//! @brief Slot ID of a configured alarm.
//!
//! IDs are only valid for the current set of configured alarms. They may be
//! reused after an alarm is deleted, so do not persist one for a later
//! destructive call; list the alarms again with alarm_service_get_ids() first.
typedef int32_t AlarmServiceId;

//! @brief A configured alarm and its current state.
typedef struct AlarmServiceAlarm {
  //! Slot ID of the alarm.
  AlarmServiceId id;
  //! Configuration of the alarm.
  AlarmServiceInfo info;
  //! Whether the alarm is enabled.
  bool enabled;
} AlarmServiceAlarm;

//! @brief Result of an alarm operation.
typedef enum AlarmServiceResult {
  //! The operation succeeded.
  AlarmServiceResultOk = 0,
  //! The app does not have the required alarm permission.
  AlarmServiceResultErrPermissionDenied,
  //! The configuration or output argument is invalid.
  AlarmServiceResultErrInvalidArgument,
  //! No alarm with that ID is configured.
  AlarmServiceResultErrDoesNotExist,
  //! The platform does not support managing alarms.
  AlarmServiceResultErrUnavailable,
  //! The alarm store could not be read or written.
  AlarmServiceResultErrInternal,
} AlarmServiceResult;

//! @brief Result of alarm_service_create().
typedef enum AlarmServiceCreateResult {
  //! The alarm was created and enabled.
  AlarmServiceCreateOk = 0,
  //! The app does not have the required alarm permission.
  AlarmServiceCreateErrPermissionDenied,
  //! The configuration is invalid.
  AlarmServiceCreateErrInvalidArgument,
  //! All alarm slots are in use.
  AlarmServiceCreateErrNoSpace,
  //! The platform does not support creating alarms.
  AlarmServiceCreateErrUnavailable,
  //! The alarm could not be stored.
  AlarmServiceCreateErrInternal,
} AlarmServiceCreateResult;

//! Peek at the next scheduled, enabled alarm. Requires the `AlarmsRead`
//! permission.
//! @param timestamp_out Must point to a valid `pbl_time32_t`, or NULL. If NULL,
//!        the function returns false. If an enabled alarm exists, it is set to
//!        its configured UTC time. Smart alarms report their configured time,
//!        not the start of the early window.
//! @return true if at least one enabled alarm is scheduled and the app has
//!         the `AlarmsRead` permission, false otherwise.
bool alarm_service_peek_next(pbl_time32_t *timestamp_out);

//! Check whether an alarm slot is free.
//!
//! This only answers the capacity question. It does not check either
//! permission, and the result is advisory: alarm_service_create() must still
//! handle @ref AlarmServiceCreateErrNoSpace.
//!
//! @return true if at least one alarm slot is available.
bool alarm_service_can_create(void);

//! Create and enable an alarm. Requires the `AlarmsWrite` permission; declare
//! it in `package.json` with `"usesPermissions": ["AlarmsWrite"]`.
//!
//! The alarm is created enabled. For
//! @ref AlarmServiceKindCustom, @ref AlarmServiceInfo::custom_days must select
//! at least one day.
//!
//! @param info Alarm configuration.
//! @param[out] id_out Set to the ID of the new alarm on success. Set to
//!             @ref ALARM_SERVICE_INVALID_ID on failure.
//! @return The result of the operation.
AlarmServiceCreateResult alarm_service_create(const AlarmServiceInfo *info, AlarmServiceId *id_out);

//! Get the IDs of all configured alarms. Requires the `AlarmsRead` permission.
//!
//! Call with @p ids NULL to get the count without retrieving any IDs, then
//! call again with a buffer large enough for the result. The order of the IDs
//! is unspecified. Use these IDs with alarm_service_get(),
//! alarm_service_set_enabled(), alarm_service_update(), and
//! alarm_service_delete().
//!
//! @param[out] ids Buffer that receives the IDs, or NULL to only count them.
//! @param max Number of IDs that fit in @p ids.
//! @return Total number of configured alarms, which may exceed @p max. Returns
//!         0 without the `AlarmsRead` permission, when no alarms are
//!         configured, and when the alarm store cannot be opened; those cases
//!         are indistinguishable.
size_t alarm_service_get_ids(AlarmServiceId *ids, size_t max);

//! Read a configured alarm. Requires the `AlarmsRead` permission.
//!
//! Get the IDs to query from alarm_service_get_ids(). An ID that is not
//! configured returns @ref AlarmServiceResultErrDoesNotExist, and an ID
//! outside the implementation's range returns
//! @ref AlarmServiceResultErrInvalidArgument. The output is only valid when
//! the call returns @ref AlarmServiceResultOk.
//!
//! @param id Alarm to read.
//! @param[out] alarm_out Configuration and enabled state of the alarm.
//! @return The result of the operation.
AlarmServiceResult alarm_service_get(AlarmServiceId id, AlarmServiceAlarm *alarm_out);

//! Replace an existing alarm's configuration. Requires the `AlarmsWrite`
//! permission.
//!
//! This is a full replacement: fields not set in @p info are cleared. An app
//! that needs to preserve the current smart, sound, vibration, or tone
//! settings must read the alarm first, which requires `AlarmsRead`.
//!
//! The time, recurrence, smart, sound, vibration, and tone settings are
//! replaced. The enabled state is preserved; use alarm_service_set_enabled()
//! to change it. For @ref AlarmServiceKindCustom,
//! @ref AlarmServiceInfo::custom_days must select at least one day.
//!
//! A change to the time, recurrence, smart setting, or custom days also cancels
//! a pending snooze and a missed-alarm catch-up. Changing only the sound,
//! vibration, or tone leaves a firing or missed occurrence alone.
//!
//! Get the ID to update from alarm_service_get_ids().
//!
//! @param id Alarm to update.
//! @param info New configuration.
//! @return @ref AlarmServiceResultErrPermissionDenied without `AlarmsWrite`,
//!         @ref AlarmServiceResultErrInvalidArgument for an invalid ID or
//!         configuration, @ref AlarmServiceResultErrDoesNotExist for an unused
//!         ID, or @ref AlarmServiceResultErrInternal if the alarm store cannot
//!         be read or written. Returns @ref AlarmServiceResultOk on success.
AlarmServiceResult alarm_service_update(AlarmServiceId id, const AlarmServiceInfo *info);

//! Enable or disable a configured alarm. Requires the `AlarmsWrite`
//! permission. Disabling an alarm also clears any pending snooze and
//! missed-alarm catch-up for it.
//!
//! Get the IDs to update from alarm_service_get_ids(). An ID that is not
//! configured returns @ref AlarmServiceResultErrDoesNotExist, and an ID
//! outside the implementation's range returns
//! @ref AlarmServiceResultErrInvalidArgument.
//!
//! @param id Alarm to update.
//! @param enable Whether the alarm should be enabled.
//! @return The result of the operation.
AlarmServiceResult alarm_service_set_enabled(AlarmServiceId id, bool enable);

//! Delete a configured alarm and its timeline pins. Requires the
//! `AlarmsWrite` permission.
//!
//! Deletion cannot be undone. Any alarm can be deleted, including one the
//! wearer created.
//!
//! @param id Alarm to delete.
//! @return @ref AlarmServiceResultErrPermissionDenied without `AlarmsWrite`,
//!         @ref AlarmServiceResultErrInvalidArgument for an ID outside the
//!         implementation's range, @ref AlarmServiceResultErrDoesNotExist for
//!         an unused ID, or @ref AlarmServiceResultErrInternal if the alarm
//!         store cannot be read or written. Returns @ref AlarmServiceResultOk
//!         on success.
AlarmServiceResult alarm_service_delete(AlarmServiceId id);

//!     @} // group AlarmService
//!   @} // group EventService
//! @} // group Foundation
