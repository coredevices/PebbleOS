/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_qemu_qemu_battery Battery
 * @ingroup drivers_qemu
 * @brief Battery state set from the QEMU host.
 * @{
 */

/**
 * @brief Handle a @ref QemuProtocol_Battery message from the host.
 *
 * Called by the QEMU serial driver.
 *
 * @param data Message payload, a @ref QemuProtocolBatteryHeader.
 * @param len Length of @p data in bytes.
 */
void qemu_battery_msg_callback(const uint8_t *data, uint32_t len);

/**
 * @brief Get the battery percentage last set by the host.
 *
 * Lets the battery service skip the lossy voltage curve round trip on QEMU.
 *
 * @return Charge percentage, 0 to 100.
 */
uint8_t qemu_battery_get_percent(void);

/** @} */
