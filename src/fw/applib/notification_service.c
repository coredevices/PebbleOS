/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notification_service.h"

#include "syscall/syscall.h"

uint8_t notification_service_get_unread_count(void) {
  return sys_notification_get_unread_count();
}
