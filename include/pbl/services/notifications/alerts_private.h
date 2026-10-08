/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/notifications/alerts.h>

/**
 * @defgroup services_notifications_alerts_private Alerts settings
 * @ingroup services_notifications
 * @brief Alert masks and settings accessors used by the settings UI.
 *
 * The getters and setters forward to @ref services_notifications_alerts_preferences; setters
 * persist the value.
 * @{
 */

/** @brief Set of alert types that may alert the user, a bitmask of NotificationType. */
typedef enum AlertMask {
  /** No alerts. */
  AlertMaskAllOff = 0,
  /** Phone calls only. */
  AlertMaskPhoneCalls = NotificationPhoneCall,
  /** Other alerts only. */
  AlertMaskOther = NotificationOther,
  /** All alerts as stored by older firmware; migrated to AlertMaskAllOn when read. */
  AlertMaskAllOnLegacy = NotificationMobile | NotificationPhoneCall | NotificationOther,
  /** All alerts, including reminders. */
  AlertMaskAllOn =
      NotificationMobile | NotificationPhoneCall | NotificationOther | NotificationReminder
} AlertMask;

/**
 * @brief Get whether notifications vibrate.
 *
 * @return true if vibration is enabled.
 */
bool alerts_get_vibrate(void);

/**
 * @brief Get the alert mask.
 *
 * @return Alert types that may alert the user.
 */
AlertMask alerts_get_mask(void);

/**
 * @brief Get the Do Not Disturb mask.
 *
 * @return Alert types that may still vibrate or light the backlight while DND is active.
 */
AlertMask alerts_get_dnd_mask(void);

/**
 * @brief Get the notification window timeout.
 *
 * @return Timeout in milliseconds, at least @ref NOTIF_WINDOW_TIMEOUT_MIN.
 */
uint32_t alerts_get_notification_window_timeout_ms(void);

/**
 * @brief Set whether notifications vibrate.
 *
 * @param enable true to vibrate.
 */
void alerts_set_vibrate(bool enable);

/**
 * @brief Set the alert mask.
 *
 * @param mask Alert types that may alert the user.
 */
void alerts_set_mask(AlertMask mask);

/**
 * @brief Set the Do Not Disturb mask.
 *
 * @param mask Alert types that may still vibrate or light the backlight while DND is active.
 */
void alerts_set_dnd_mask(AlertMask mask);

/**
 * @brief Set the notification window timeout.
 *
 * @param timeout_ms Timeout in milliseconds, or @ref NOTIF_WINDOW_TIMEOUT_INFINITE.
 */
void alerts_set_notification_window_timeout_ms(uint32_t timeout_ms);

/**
 * @brief Initialize the alerts service.
 *
 * Loads the alert preferences and initializes Do Not Disturb and the vibration intensity.
 */
void alerts_init(void);

/** @} */
