/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include "services/normal/notifications/notification_types.h"

typedef enum AlertType {
  AlertInvalid   = NotificationInvalid,
  AlertMobile    = NotificationMobile,
  AlertPhoneCall = NotificationPhoneCall,
  AlertOther     = NotificationOther,
  AlertReminder  = NotificationReminder
} AlertType;

// Service to determine how and if the user gets alerted on a call/notification

//! Call this function before alerting the user in any notification/call for the alerts service
//! to handle analytics operations.
void alerts_incoming_alert_analytics();

bool alerts_should_notify_for_type(AlertType type);

bool alerts_should_enable_backlight_for_type(AlertType type);

bool alerts_should_vibrate_for_type(AlertType type);

//! When vibrating for an incoming notification, call this function to prevent multiple vibes
//! within a short period of time.
void alerts_set_notification_vibe_timestamp();
