/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/notifications/alerts.h"

typedef enum AlertMask {
  AlertMaskAllOff       = 0,
  AlertMaskPhoneCalls   = NotificationPhoneCall,
  AlertMaskOther        = NotificationOther,
  AlertMaskAllOnLegacy  =
    NotificationMobile | NotificationPhoneCall | NotificationOther,
  AlertMaskAllOn        =
    NotificationMobile | NotificationPhoneCall | NotificationOther | NotificationReminder
} AlertMask;

bool alerts_get_vibrate(void);

AlertMask alerts_get_mask(void);

AlertMask alerts_get_dnd_mask(void);

uint32_t alerts_get_notification_window_timeout_ms(void);

void alerts_set_vibrate(bool enable);

void alerts_set_mask(AlertMask mask);

void alerts_set_dnd_mask(AlertMask mask);

void alerts_set_notification_window_timeout_ms(uint32_t timeout_ms);

void alerts_init(void);
