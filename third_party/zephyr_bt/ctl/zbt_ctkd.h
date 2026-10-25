/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// BR/EDR link key from an LE Secure Connections LTK (cross-transport key
// derivation, Core Vol 3 Part H 2.4.2.4). All values are little-endian, as
// HCI carries them.

#include <stdbool.h>
#include <stdint.h>

//! h6(W, keyID) = AES-CMAC_W(keyID) (Part H 2.2.10).
int zbt_ctkd_h6(const uint8_t w[16], const uint8_t key_id[4], uint8_t out[16]);

//! h7(SALT, W) = AES-CMAC_SALT(W) (Part H 2.2.11).
int zbt_ctkd_h7(const uint8_t salt[16], const uint8_t w[16], uint8_t out[16]);

//! The link key for @p ltk; @p ct2 when both sides set CT2 in their AuthReq.
int zbt_ctkd_link_key(const uint8_t ltk[16], bool ct2, uint8_t link_key[16]);
