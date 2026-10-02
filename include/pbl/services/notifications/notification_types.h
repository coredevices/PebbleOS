/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/uuid.h"

/**
 * @defgroup services_notifications_notification_types Notification types
 * @ingroup services_notifications
 * @brief Kinds of alerting items shared by notifications and reminders.
 * @{
 */

/**
 * @brief Kind of a notification or reminder.
 *
 * Values are single bits so they can be combined into an AlertMask.
 */
typedef enum {
  /** Not a valid type. */
  NotificationInvalid = 0,
  /** Notification from a phone app. */
  NotificationMobile = (1 << 0),
  /** Phone call. */
  NotificationPhoneCall = (1 << 1),
  /** Any other alert. */
  NotificationOther = (1 << 2),
  /** Timeline reminder. */
  NotificationReminder = (1 << 3)
} NotificationType;

/** @brief Type and id of a notification or reminder. */
typedef struct {
  /** Kind of item. */
  NotificationType type;
  /** Id of the notification or reminder. */
  Uuid id;
} NotificationInfo;

/** @} */
