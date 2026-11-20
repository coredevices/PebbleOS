/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/timeline/item.h>

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
 * @return 0, also when there are no reminders, or a negative errno.
 */
int reminders_update_timer(void);

/**
 * @brief Insert a reminder into the Reminders database.
 *
 * @param reminder Reminder to insert.
 * @return 0 or a negative errno.
 */
int reminders_insert(Reminder *reminder);

/**
 * @brief Initialize reminders and arm the timer.
 *
 * @return 0 or a negative errno.
 */
int reminders_init(void);

/**
 * @brief Delete a reminder, emitting a BlobDB event.
 *
 * @param reminder_id Id of the reminder.
 * @return 0 or a negative errno.
 */
int reminders_delete(ReminderId *reminder_id);

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
 * @retval 0 Snoozed.
 * @retval -ENOTSUP The reminder cannot be snoozed.
 * @return Another negative errno if reinserting failed.
 */
int reminders_snooze(Reminder *reminder);

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
