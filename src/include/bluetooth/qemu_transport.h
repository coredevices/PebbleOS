/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Called by the QEMU serial driver whenever Pebble Protocol data is received.
void qemu_transport_handle_received_data(const uint8_t *data, uint32_t length);

//! Called by qemu version of comm_init() to tell ISPP that it is connected
void qemu_transport_set_connected(bool is_connected);
void qemu_transport_close_session();

bool qemu_transport_is_connected(void);
