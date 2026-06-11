/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <bluetooth/bluetooth_types.h>

//! Handles host-originated QEMU HRM bridge packets.
void bt_driver_qemu_hrm_bridge_handle_packet(const uint8_t *data, uint32_t length);

//! Disconnects the synthetic QEMU HRM central if the given device matches it.
bool bt_driver_qemu_hrm_bridge_disconnect(const BTDeviceInternal *peer_address);

//! Updates whether the host-side bridge should advertise the Heart Rate Service.
void bt_driver_qemu_hrm_bridge_set_advertising_enabled(bool enabled);
