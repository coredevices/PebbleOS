/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup bluetooth_pairability Pairability
 * @ingroup bluetooth
 * @brief Whether new LE pairings are accepted.
 * @{
 */

/**
 * @brief Allow or refuse new LE pairings.
 *
 * Does nothing in the NimBLE backend.
 *
 * @param enabled true to accept pairing requests.
 */
void pbl_bt_le_pairability_set_enabled(bool enabled);

/** @} */
