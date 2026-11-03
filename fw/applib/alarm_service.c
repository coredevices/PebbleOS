/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "alarm_service.h"

#include <applib/app_time32.h>
#include <syscall/syscall.h>

bool alarm_service_peek_next(time_t *timestamp_out) {
  return sys_alarm_get_next_enabled(timestamp_out);
}

bool alarm_service_peek_next_time32(pbl_time32_t *timestamp_out) {
  time_t t;
  if (!sys_alarm_get_next_enabled(&t)) {
    return false;
  }
  *timestamp_out = pbl_time_to_time32(t);
  return true;
}
