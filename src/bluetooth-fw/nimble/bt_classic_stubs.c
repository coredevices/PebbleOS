/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <bluetooth/classic_connect.h>
#include <bluetooth/connectability.h>
#include <bluetooth/features.h>
#include <bluetooth/pairability.h>
#include <bluetooth/reconnect.h>

void bt_driver_classic_disconnect(const BTDeviceAddress* address) {}

bool bt_driver_classic_is_connected(void) { return false; }

bool bt_driver_classic_copy_connected_address(BTDeviceAddress* address) { return false; }

bool bt_driver_classic_copy_connected_device_name(char nm[BT_DEVICE_NAME_BUFFER_SIZE]) {
  return false;
}

void bt_driver_classic_update_connectability(void) {}

bool bt_driver_supports_bt_classic(void) { return false; }

void bt_driver_classic_pairability_set_enabled(bool enabled) {}

uint32_t sys_app_comm_get_sniff_interval(void) { return 0; }

void bt_driver_reconnect_pause(void) {}

void bt_driver_reconnect_resume(void) {}

void bt_driver_reconnect_try_now(bool ignore_paused) {}

void bt_driver_reconnect_reset_interval(void) {}

void bt_driver_reconnect_notify_platform_bitfield(uint32_t platform_bitfield) {}
