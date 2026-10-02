/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

/**
 * @defgroup drivers_qemu_qemu_accel Accelerometer
 * @ingroup drivers_qemu
 * @brief Accelerometer fed with samples from the QEMU host.
 * @{
 */

/**
 * @brief Handle a @ref QemuProtocol_Accel message from the host.
 *
 * Called by the QEMU serial driver.
 *
 * @param data Message payload, a @ref QemuProtocolAccelHeader followed by the samples.
 * @param len Length of @p data in bytes.
 */
void qemu_accel_msg_callback(const uint8_t *data, uint32_t len);

/** @} */
