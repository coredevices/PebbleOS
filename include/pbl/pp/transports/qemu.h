/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup pp_transport_qemu QEMU transport
 * @ingroup pp_transport
 * @brief Pebble protocol over the emulator's serial link, emulating the phone connection.
 * @{
 */

/** @brief Open the session if the emulator is configured to start connected. */
void pbl_pp_qemu_start(void);

/** @brief Tell host tools the system is up, once the first app has had time to start. */
void pbl_pp_qemu_notify_ready(void);

/** @brief Drop the emulated connection and close the session. */
void pbl_pp_qemu_stop(void);

/**
 * @brief Feed Pebble protocol bytes received from the emulator.
 *
 * @param data Received bytes.
 * @param length Length of @p data in bytes.
 */
void pbl_pp_qemu_receive(const uint8_t *data, uint32_t length);

/**
 * @brief Apply a phone connection change reported by the emulator.
 *
 * @param is_connected Whether the phone is connected.
 */
void pbl_pp_qemu_set_connected(bool is_connected);

/**
 * @brief Check whether the emulated phone is connected.
 *
 * @return true if a session is open and the emulated phone is connected.
 */
bool pbl_pp_qemu_is_connected(void);

/** @} */
