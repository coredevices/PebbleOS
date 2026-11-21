/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// Pebble protocol over the emulator's serial link, emulating the phone connection: a transport of
// the firmware, built on its emulator serial protocol.

/** @brief Open the session if the emulator is configured to start connected. */
void qemu_pp_start(void);

/** @brief Tell host tools the system is up, once the first app has had time to start. */
void qemu_pp_notify_ready(void);

/** @brief Drop the emulated connection and close the session. */
void qemu_pp_stop(void);

/**
 * @brief Feed Pebble protocol bytes received from the emulator.
 *
 * @param data Received bytes.
 * @param length Length of @p data in bytes.
 */
void qemu_pp_receive(const uint8_t *data, uint32_t length);

/**
 * @brief Apply a phone connection change reported by the emulator.
 *
 * @param is_connected Whether the phone is connected.
 */
void qemu_pp_set_connected(bool is_connected);

/**
 * @brief Check whether the emulated phone is connected.
 *
 * @return true if a session is open and the emulated phone is connected.
 */
bool qemu_pp_is_connected(void);
