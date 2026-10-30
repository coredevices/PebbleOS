/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <pbl/bluetooth/sm_types.h>
#include <pbl/bluetooth/types.h>

/**
 * @defgroup bluetooth_bond Bonds
 * @ingroup bluetooth
 * @brief Peers the stack is bonded with, identified by their identity address.
 *
 * The stack stores its bonds itself: in normal firmware in a settings file, kept in sync with the
 * single bond of shared PRF storage, and in recovery firmware in shared PRF storage directly.
 * Only one bond is kept: bonding with a new peer deletes the previous one.
 *
 * Bond changes are reported on KernelMain through pbl_bt_cb_bond_added() and
 * pbl_bt_cb_bond_deleted().
 * @{
 */

/** @brief A bonded peer. */
struct pbl_bt_bond_info {
  /** Identity address of the peer. */
  struct pbl_bt_device_internal addr;
  /** Identity Resolving Key of the peer, valid iff @ref irk_valid. */
  struct pbl_bt_sm_key irk;
  /** True if the peer distributed its Identity Resolving Key. */
  bool irk_valid;
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
 * @param func Called for each bond.
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
 * @brief Delete a bond.
 *
 * Works with the stack stopped as well.
 *
 * @param addr Identity address of the peer, or NULL for all bonds.
 * @retval 0 Success.
 * @retval -ENOENT Not bonded with @p addr.
 * @retval -EIO Stack or storage error.
 */
int pbl_bt_unpair(const struct pbl_bt_device_internal *addr);

/**
 * @brief Called when bonding with a new peer.
 *
 * Runs on KernelMain.
 *
 * @param info The new bond.
 */
extern void pbl_bt_cb_bond_added(const struct pbl_bt_bond_info *info);

/**
 * @brief Called when a bond is deleted.
 *
 * Runs on KernelMain.
 *
 * @param addr Identity address of the peer.
 */
extern void pbl_bt_cb_bond_deleted(const struct pbl_bt_device_internal *addr);

/** @} */
