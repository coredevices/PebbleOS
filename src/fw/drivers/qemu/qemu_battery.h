/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

//! Handler called by qemu_serial driver when we receive a QemuProtocol_Battery message
//!  over the qemu serial connection.
void qemu_battery_msg_callack(const uint8_t *data, uint32_t len);
