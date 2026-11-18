/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>

/**
 * @defgroup bluetooth_bond Bonds
 * @ingroup bluetooth
 * @brief Peers the stack is bonded with, identified by their identity address.
 *
 * The stack keeps its bonds, with their keys and Client Characteristic Configuration Descriptor
 * states, in the NimBLE store. They can be read and deleted with the stack stopped as well.
 *
 * Changes are reported on KernelMain through pbl_bt_cb_bond_added(), pbl_bt_cb_bond_changed()
 * and pbl_bt_cb_bond_deleted(), after the store has them.
 * @{
 */

/** @brief Bond flag: paired with Secure Connections. */
#define PBL_BT_BOND_FLAG_SECURE_CONNECTIONS 0x01U
/** @brief Bond flag: the pairing was authenticated (MITM protected). */
#define PBL_BT_BOND_FLAG_AUTHENTICATED 0x02U

/** @brief A bonded peer. */
struct pbl_bt_bond_info {
  /** Identity address of the peer. */
  struct pbl_bt_device_internal addr;
  /** Identity Resolving Key of the peer, valid if @ref irk_valid. */
  struct pbl_bt_sm_key irk;
  /** True if the peer distributed its Identity Resolving Key. */
  bool irk_valid;
};

/** @brief The keys of a bond, to copy it to or from other storage. */
struct pbl_bt_bond_keys {
  /** Keys and identity exchanged during pairing. */
  struct pbl_bt_sm_pairing_info info;
  /** @c PBL_BT_BOND_FLAG_* values. */
  uint8_t flags;
};

/**
 * @brief Callback for pbl_bt_foreach_bond().
 *
 * @param info The bond.
 * @param user_data As passed to pbl_bt_foreach_bond().
 */
typedef void (*pbl_bt_bond_cb_t)(const struct pbl_bt_bond_info *info, void *user_data);

/**
 * @brief Iterate over the bonds.
 *
 * @param func Called for each bond. It must not add or delete bonds.
 * @param user_data Passed to @p func.
 */
void pbl_bt_foreach_bond(pbl_bt_bond_cb_t func, void *user_data);

/**
 * @brief Count the bonds.
 *
 * @return Number of bonds.
 */
size_t pbl_bt_bond_count(void);

/**
 * @brief Get a bond.
 *
 * @param addr Identity address of the peer.
 * @param[out] info The bond. May be NULL.
 * @retval 0 Success.
 * @retval -ENOENT Not bonded with @p addr.
 */
int pbl_bt_bond_get(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_info *info);

/**
 * @brief Get the keys of a bond.
 *
 * @param addr Identity address of the peer.
 * @param[out] keys The keys.
 * @retval 0 Success.
 * @retval -ENOENT Not bonded with @p addr.
 */
int pbl_bt_bond_get_keys(const struct pbl_bt_device_internal *addr, struct pbl_bt_bond_keys *keys);

/**
 * @brief Add a bond from its keys, or replace the keys of the one with the same peer.
 *
 * For bonds made elsewhere, before the stack starts. Nothing is written if the store has the same
 * keys, and no callback is called.
 *
 * @param keys The keys.
 * @retval 0 Success.
 * @retval -EIO The store could not take them.
 */
int pbl_bt_bond_set_keys(const struct pbl_bt_bond_keys *keys);

/**
 * @brief Delete a bond, with its Client Characteristic Configuration Descriptor states.
 *
 * A connection to the peer is left as it is.
 *
 * @param addr Identity address of the peer, or NULL for all bonds.
 * @retval 0 Success.
 * @retval -ENOENT Not bonded with @p addr.
 * @retval -EIO Store error.
 */
int pbl_bt_unpair(const struct pbl_bt_device_internal *addr);

/**
 * @brief Called after bonding with a new peer.
 *
 * Implemented by the firmware. Runs on KernelMain.
 *
 * @param info The new bond.
 */
extern void pbl_bt_cb_bond_added(const struct pbl_bt_bond_info *info);

/**
 * @brief Called after the keys of a bond changed, for example when its peer paired again.
 *
 * Implemented by the firmware. Runs on KernelMain.
 *
 * @param info The bond.
 */
extern void pbl_bt_cb_bond_changed(const struct pbl_bt_bond_info *info);

/**
 * @brief Called after a bond was deleted.
 *
 * Implemented by the firmware. Runs on KernelMain.
 *
 * @param addr Identity address of the peer.
 */
extern void pbl_bt_cb_bond_deleted(const struct pbl_bt_device_internal *addr);

/** @} */
