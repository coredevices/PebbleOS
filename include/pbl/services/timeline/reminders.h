/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/status_codes.h"
#include "pbl/services/timeline/item.h"
#include "pbl/services/new_timer/new_timer.h"

/**
 * @defgroup services_timeline_reminders Reminders
 * @ingroup services_timeline
 * @brief Pops up reminders at their time.
 *
 * Reminders are timeline items of type TimelineItemTypeReminder stored in the Reminders BlobDB;
 * their parent is the pin they remind of. A once-a-second check against the RTC triggers the next
 * reminder, marks it reminded and posts a @c PEBBLE_REMINDER_EVENT.
 * @{
 */

/** @brief A reminder. */
typedef TimelineItem Reminder;
/** @brief Id of a reminder. */
typedef TimelineItemId ReminderId;

/**
 * @brief Arm the reminder timer for the next stored reminder.
 *
 * @return S_SUCCESS, also when there are no reminders, or an error.
 */
status_t reminders_update_timer(void);

/**
 * @brief Insert a reminder into the Reminders database.
 *
 * @param reminder Reminder to insert.
 * @return S_SUCCESS or an error.
 */
status_t reminders_insert(Reminder *reminder);

/**
 * @brief Initialize reminders and arm the timer.
 *
 * @return S_SUCCESS or an error.
 */
status_t reminders_init(void);

/**
 * @brief Delete a reminder, emitting a BlobDB event.
 *
 * @param reminder_id Id of the reminder.
 * @return S_SUCCESS or an error.
 */
status_t reminders_delete(ReminderId *reminder_id);

/**
 * @brief Check whether a reminder can be snoozed.
 *
 * @param reminder Reminder.
 * @return true if it can snooze for a non-zero amount of time.
 */
bool reminders_can_snooze(Reminder *reminder);

/**
 * @brief Snooze a reminder.
 *
 * Long before the event of the parent pin, snoozes for half the time left; close to it, for a
 * constant delay; too long after it, not at all. The reminder is reinserted with the new time and
 * its reminded status cleared.
 *
 * @param reminder Reminder to snooze.
 * @retval S_SUCCESS Snoozed.
 * @retval E_INVALID_OPERATION The reminder cannot be snoozed.
 * @return Another error if reinserting failed.
 */
status_t reminders_snooze(Reminder *reminder);

/**
 * @brief Post an event telling that a reminder was removed.
 *
 * @param reminder_id Id of the removed reminder.
 */
void reminders_handle_reminder_removed(const Uuid *reminder_id);

/**
 * @brief Post an event telling that a triggered reminder changed.
 *
 * @param reminder_id Id of the updated reminder.
 */
void reminders_handle_reminder_updated(const Uuid *reminder_id);

/** @} */
