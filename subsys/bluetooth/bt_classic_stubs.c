/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <syscall/syscall.h>

SniffInterval sys_app_comm_get_sniff_interval(void) {
  return SNIFF_INTERVAL_NORMAL;
}
