/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <host/ble_gap.h>
#include <stdint.h>

void nimble_pebble_pairing_service_handle_disconnect(uint16_t conn_handle);

bool nimble_pebble_pairing_service_peer_is_gateway(const ble_addr_t *peer_addr);
