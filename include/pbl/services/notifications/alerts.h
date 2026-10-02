/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include "pbl/services/notifications/notification_types.h"

/**
 * @defgroup services_notifications_alerts Alerts
 * @ingroup services_notifications
 * @brief Decides whether and how the user is alerted for a call or notification.
 *
 * Nothing alerts in low power mode or during a firmware update. Otherwise an alert type must be
 * enabled in the alert mask; while Do Not Disturb is active it must also be in the DND mask to
 * vibrate or turn on the backlight.
 * @{
 */

/** @brief Kind of alert, mirroring NotificationType. */
typedef enum AlertType {
  /** Not a valid type. */
  AlertInvalid = NotificationInvalid,
  /** Notification from a phone app. */
  AlertMobile = NotificationMobile,
  /** Phone call. */
  AlertPhoneCall = NotificationPhoneCall,
  /** Any other alert. */
  AlertOther = NotificationOther,
  /** Timeline reminder. */
  AlertReminder = NotificationReminder
} AlertType;

/**
 * @brief Record analytics for an incoming alert.
 *
 * Call before alerting the user for any notification or call.
 */
void alerts_incoming_alert_analytics();

/**
 * @brief Check whether the user should be notified at all.
 *
 * @param type Kind of alert.
 * @return false in low power mode, during a firmware update or if @p type is not in the alert
 *         mask, true otherwise.
 */
bool alerts_should_notify_for_type(AlertType type);

/**
 * @brief Check whether the backlight should turn on for an alert.
 *
 * @param type Kind of alert.
 * @return true if the notification backlight preference is on, @p type is allowed by the DND mask
 *         when DND is active, and alerts_should_notify_for_type() allows it.
 */
bool alerts_should_enable_backlight_for_type(AlertType type);

/**
 * @brief Check whether the watch should vibrate for an alert.
 *
 * Vibration is also suppressed while USB is connected and within 3 seconds of the last
 * alerts_set_notification_vibe_timestamp() call.
 *
 * @param type Kind of alert.
 * @return true if the watch should vibrate.
 */
bool alerts_should_vibrate_for_type(AlertType type);

/**
 * @brief Record that the watch vibrated for a notification.
 *
 * Starts the holdoff that prevents several vibrations within a short period.
 */
void alerts_set_notification_vibe_timestamp();

/** @} */
