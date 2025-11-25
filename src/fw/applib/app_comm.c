/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_comm.h"

#include "syscall/syscall.h"

void app_comm_set_sniff_interval(const SniffInterval interval) {
  sys_app_comm_set_responsiveness(interval);
}

SniffInterval app_comm_get_sniff_interval(void) {
  return sys_app_comm_get_sniff_interval();
}

