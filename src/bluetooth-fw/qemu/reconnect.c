/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdbool.h>

void bt_driver_reconnect_pause(void) {
}

void bt_driver_reconnect_resume(void) {
}

void bt_driver_reconnect_try_now(bool ignore_paused) {
}

void bt_driver_reconnect_reset_interval(void) {
}

void bt_driver_reconnect_notify_platform_bitfield(uint32_t platform_bitfield) {
  // Don't care
}
