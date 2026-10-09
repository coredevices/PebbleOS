/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup bluetooth_pairability Pairability
 * @ingroup bluetooth
 * @brief Whether new LE pairings are accepted.
 * @{
 */

/**
 * @brief Allow or refuse new LE pairings.
 *
 * When disabled, pairing requests from peers that are not bonded are
 * rejected. Bonded peers can still re-encrypt and re-pair.
 *
 * @param enabled true to accept pairing requests.
 */
void pbl_bt_le_pairability_set_enabled(bool enabled);

/** @} */
