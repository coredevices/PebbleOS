/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <host/ble_store.h>

#define PBL_BT__STORE_KEY_SIZE 16

#define PBL_BT__STORE_FLAG_SC   0x01
#define PBL_BT__STORE_FLAG_AUTH 0x02

/** Install the store callbacks. Called once at boot. */
void pbl_bt__store_init(void);

/** Called before the host starts. */
void pbl_bt__store_start(void);

/** Called once the host has stopped. */
void pbl_bt__store_stop(void);

/**
 * Report a written security record. Called by the backends without their lock held.
 *
 * @param is_new_bond True if @p value_sec is the first peer record of the peer.
 */
void pbl_bt__store_sec_written(int obj_type, const struct ble_store_value_sec *value_sec,
                               bool is_new_bond);

/** Report a deleted bond. Called by the backends without their lock held. */
void pbl_bt__store_bond_deleted(const ble_addr_t *peer_addr);

/** Read the shared PRF storage bond. Same contract as the store read callback. */
int pbl_bt__sprf_read_sec(int obj_type, const struct ble_store_key_sec *key_sec,
                          struct ble_store_value_sec *value_sec);

/**
 * Write a security record to the shared PRF storage bond, replacing the bond of another peer.
 *
 * @param[out] replaced Peer whose bond got replaced, BLE_ADDR_ANY if none. May be NULL.
 */
int pbl_bt__sprf_write_sec(int obj_type, const struct ble_store_value_sec *value_sec,
                           ble_addr_t *replaced);

/** Delete a security record from the shared PRF storage bond. */
int pbl_bt__sprf_delete_sec(int obj_type, const ble_addr_t *peer_addr);

/** Get the IRK from shared PRF storage. */
int pbl_bt__sprf_irk_get(uint8_t irk[PBL_BT__STORE_KEY_SIZE]);

/** Store the IRK in shared PRF storage. */
void pbl_bt__sprf_irk_set(const uint8_t irk[PBL_BT__STORE_KEY_SIZE]);
