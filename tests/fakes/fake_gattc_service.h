/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/bluetooth/clients/gatt.h>

//! Fake of the service clients of pbl/bluetooth/clients/gatt.h, for one service at a time:
//! the test plays the peer.

//! Takes a write to a characteristic, and may answer it right away.
//! @return What pbl_bt_gattc_service_write() returns.
typedef int (*FakeGattcServiceWriteHandler)(uint8_t chr, const uint8_t *data, uint16_t len);

void fake_gattc_service_init(void);

void fake_gattc_service_set_write_handler(FakeGattcServiceWriteHandler handler);

//! What pbl_bt_gattc_service_subscribe() returns from now on.
void fake_gattc_service_set_subscribe_rc(int rc);

//! What pbl_bt_gattc_service_read() returns from now on.
void fake_gattc_service_set_read_rc(int rc);

//! The reads asked for since fake_gattc_service_init().
int fake_gattc_service_num_reads(void);

//! Whether a service client is started.
bool fake_gattc_service_is_started(void);

// What the peer does: reported to the service client if it is started.

void fake_gattc_service_discovered(int status);

void fake_gattc_service_subscribed(uint8_t chr, bool enable, int status);

void fake_gattc_service_notified(uint8_t chr, const void *data, uint16_t len);

void fake_gattc_service_written(uint8_t chr, int status);

void fake_gattc_service_read(uint8_t chr, int status, const void *data, uint16_t len);
