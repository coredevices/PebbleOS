/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Opens the session if the emulator is configured to start connected.
void qemu_transport_start(void);

//! Tells host tools the system is up, once the first app has had time to start.
void qemu_transport_notify_ready(void);

//! Drops the connection and tears the session down.
void qemu_transport_stop(void);

//! Called by the QEMU serial driver whenever Pebble Protocol data is received.
void qemu_transport_handle_received_data(const uint8_t *data, uint32_t length);

//! Called by the QEMU serial driver when the emulator reports a connection change.
void qemu_transport_set_connected(bool is_connected);

bool qemu_transport_is_connected(void);
