/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

/**
 * @defgroup drivers_accel_qemu QEMU accelerometer
 * @ingroup drivers_accel
 * @brief Accelerometer fed with samples from the QEMU host.
 * @{
 */

/**
 * @brief Handle a `QemuProtocol_Accel` message from the host.
 *
 * Called by the QEMU host channel.
 *
 * @param data Message payload, a `QemuProtocolAccelHeader` followed by the samples.
 * @param len Length of @p data in bytes.
 */
void qemu_accel_msg_callback(const uint8_t *data, uint32_t len);

/** @} */
