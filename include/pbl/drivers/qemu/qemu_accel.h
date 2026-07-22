/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

void qemu_accel_init(void);

//! Handler called by qemu_serial driver when we receive a QemuProtocol_Accel message
//!  over the qemu serial connection.
void qemu_accel_msg_callack(const uint8_t *data, uint32_t len);
