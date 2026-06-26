/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "alarm_service.h"

#include "syscall/syscall.h"

bool alarm_service_peek_next(time_t *timestamp_out) {
  return sys_alarm_get_next_enabled(timestamp_out);
}
