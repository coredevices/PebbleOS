/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/section.h>

#include <host/ble_store.h>
#include <nimble/ble.h>

/**
 * @defgroup bluetooth_bond Bonds
 * @ingroup bluetooth
 * @brief Peers the stack is bonded with, identified by their identity address.
 *
 * The stack keeps its bonds, with their keys and Client Characteristic Configuration Descriptor
 * states, in the NimBLE store. They can be read and deleted with the stack stopped as well.
 *
 * Changes are reported to the listeners defined with PBL_BT_BOND_LISTENER_DEFINE(), on the system
 * work queue, after the store has them.
 * @{
 */

/** @brief A bonded peer. */
struct pbl_bt_bond_info {
  /** Identity address of the peer. */
  ble_addr_t addr;
  /** Identity Resolving Key of the peer, valid if @ref irk_valid. */
  uint8_t irk[16];
  /** True if the peer distributed its Identity Resolving Key. */
  bool irk_valid;
};

/** @brief The keys of a bond, as NimBLE stores them, to copy it to or from other storage. */
struct pbl_bt_bond_keys {
  /** The peer's keys. */
  struct ble_store_value_sec peer;
  /** The keys of the watch for that peer: without an LTK if it has none. */
  struct ble_store_value_sec our;
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
int pbl_bt_bond_get(const ble_addr_t *addr, struct pbl_bt_bond_info *info);

/**
 * @brief Get the keys of a bond.
 *
 * @param addr Identity address of the peer.
 * @param[out] keys The keys.
 * @retval 0 Success.
 * @retval -ENOENT Not bonded with @p addr.
 */
int pbl_bt_bond_get_keys(const ble_addr_t *addr, struct pbl_bt_bond_keys *keys);

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
int pbl_bt_unpair(const ble_addr_t *addr);

/** @brief What a bond listener is told. Each is optional. */
struct pbl_bt_bond_listener {
  /**
   * @brief Bonded with a new peer.
   *
   * @param info The new bond.
   */
  void (*added)(const struct pbl_bt_bond_info *info);
  /**
   * @brief The keys of a bond changed, for example when its peer paired again.
   *
   * @param info The bond.
   */
  void (*changed)(const struct pbl_bt_bond_info *info);
  /**
   * @brief A bond was deleted.
   *
   * @param addr Identity address of the peer.
   */
  void (*deleted)(const ble_addr_t *addr);
};

/**
 * @brief Define a listener of the changes to the bonds.
 *
 * @code{.c}
 * PBL_BT_BOND_LISTENER_DEFINE(my_listener, .added = prv_bond_added);
 * @endcode
 *
 * @param _name Name of the listener.
 * @param ... Initializers of its struct pbl_bt_bond_listener.
 */
#define PBL_BT_BOND_LISTENER_DEFINE(_name, ...)                                               \
  static const struct pbl_bt_bond_listener pbl_bt_bond_listener_##_name PBL_ITERABLE_SECTION( \
      const struct pbl_bt_bond_listener, bt_bond, #_name) = {__VA_ARGS__}

/** @} */
