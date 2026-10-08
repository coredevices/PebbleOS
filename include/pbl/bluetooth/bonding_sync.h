/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/sm_types.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_bonding_sync Bonding synchronization
 * @ingroup bluetooth
 * @brief Keep the stack's bondings and CCCD states in sync with persistent storage.
 *
 * The firmware owns persistent storage. It pushes the stored bondings and Client Characteristic
 * Configuration Descriptor states into the stack with the @c pbl_bt_handle_host_* functions, and
 * the stack reports new bondings through pbl_bt_cb_handle_create_bonding().
 * @{
 */

/** @brief A bonding with a remote device. Packed because it is serialized. */
struct PBL_PACKED pbl_bt_bonding {
  /** Keys and identity exchanged during pairing. */
  struct pbl_bt_sm_pairing_info pairing_info;
  /** True if the remote device is capable of talking PPoGATT. */
  bool is_gateway : 1;

  /** True if the local device address should be pinned. */
  bool should_pin_address : 1;

  /**
   * Backend specific flags.
   *
   * The NimBLE backend sets bit 0 for Secure Connections and bit 1 for an authenticated
   * (MITM protected) pairing. Persistent storage keeps only 5 bits.
   */
  uint8_t flags : 5;

  /** Reserved. */
  uint8_t rsvd : 1;

  /** Address to pin. Valid iff @ref should_pin_address is true. */
  struct pbl_bt_addr pinned_address;
};

/** @brief Stored Client Characteristic Configuration Descriptor state of a peer. */
struct PBL_PACKED pbl_bt_cccd {
  /** The peer device. */
  struct pbl_bt_device_internal peer;
  /** Value handle of the characteristic the CCCD belongs to. */
  uint16_t chr_val_handle;
  /** CCCD value: bit 0 enables notifications, bit 1 indications. */
  uint16_t flags;
  /** True if the value changed while the peer was disconnected. */
  bool value_changed : 1;
};

/**
 * @brief Register an existing bonding with the stack.
 *
 * Called by the firmware for each stored bonding, before starting the stack so they can be
 * restored. There are no matching removal calls when the stack stops.
 *
 * @param bonding The bonding.
 */
void pbl_bt_handle_host_added_bonding(const struct pbl_bt_bonding *bonding);

/**
 * @brief Unregister a bonding, for example after the user forgot it in Settings.
 *
 * @param bonding The bonding.
 */
void pbl_bt_handle_host_removed_bonding(const struct pbl_bt_bonding *bonding);

/**
 * @brief Register a stored CCCD state with the stack.
 *
 * @param cccd The CCCD state.
 */
void pbl_bt_handle_host_added_cccd(const struct pbl_bt_cccd *cccd);

/**
 * @brief Unregister a stored CCCD state.
 *
 * @param cccd The CCCD state.
 */
void pbl_bt_handle_host_removed_cccd(const struct pbl_bt_cccd *cccd);

/**
 * @brief Called after a new device was paired, to persist the bonding.
 *
 * Implemented by the firmware. The NimBLE backend calls it on KernelMain.
 *
 * @param bonding The new bonding.
 * @param addr Address of the connection, used to associate the bonding with its
 *             GAPLEConnection.
 */
extern void pbl_bt_cb_handle_create_bonding(const struct pbl_bt_bonding *bonding,
                                            const struct pbl_bt_addr *addr);

/** @} */
