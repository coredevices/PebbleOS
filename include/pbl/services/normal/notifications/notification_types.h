/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"

//! This list is shared by notifications and reminders.
typedef enum {
  NotificationInvalid   = 0,
  NotificationMobile    = (1 << 0),
  NotificationPhoneCall = (1 << 1),
  NotificationOther     = (1 << 2),
  NotificationReminder  = (1 << 3)
} NotificationType;

//! Type and Id for the notification or reminder.
typedef struct {
  NotificationType type;
  Uuid id;
} NotificationInfo;
