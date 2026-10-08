/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <inttypes.h>

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>

#include <pbl/kernel/compiler.h>
#include <pbl/bluetooth/hci_types.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_gap_le_connect LE connections
 * @ingroup bluetooth
 * @brief LE connection events and disconnection.
 *
 * The event structures carry the data of the HCI events of the Bluetooth Core Specification
 * v4.2, Vol 2, Part E, 7.7, noted on each. The firmware implements the
 * @c pbl_bt_handle_le_* callbacks; unless noted otherwise they are invoked on the NimBLE host
 * task.
 * @{
 */

/** @brief LE address type. One byte in size. */
enum pbl_bt_addr_type {
  /** Public device address. */
  PBL_BT_ADDR_TYPE_PUBLIC,
  /** Random device address. */
  PBL_BT_ADDR_TYPE_RANDOM
};

#ifndef __clang__
_Static_assert(sizeof(enum pbl_bt_addr_type) == 1, "enum pbl_bt_addr_type is not 1 byte in size");
#endif

/** @brief Parameters of an established connection. */
struct PBL_PACKED pbl_bt_conn_params {
  /** Connection interval in 1.25 ms units. */
  uint16_t conn_interval_1_25ms;
  /** Peripheral latency, in connection events. */
  uint16_t slave_latency_events;
  /** Supervision timeout in 10 ms units. */
  uint16_t supervision_timeout_10ms;
};

/** @brief Link layer version of a peer, as in LL_VERSION_IND (v4.2, Vol 6, Part B, 2.4.2.13). */
struct PBL_PACKED pbl_bt_remote_version_info {
  /** Bluetooth Core Specification version. */
  uint8_t version_number;
  /** Company identifier of the controller manufacturer. */
  uint16_t company_identifier;
  /** Implementation specific subversion. */
  uint16_t subversion_number;
};

/** @brief Remote version information of a peer was received. */
struct PBL_PACKED pbl_bt_remote_version_info_received_event {
  /** The peer. */
  struct pbl_bt_device_internal peer_address;
  /** The peer's version information. */
  struct pbl_bt_remote_version_info remote_version_info;
};

/** @brief A connection was established (LE Connection Complete Event, 7.7.65.1). */
struct PBL_PACKED pbl_bt_conn_complete_event {
  /** Connection parameters. */
  struct pbl_bt_conn_params conn_params;
  /** The peer, by its identity address when it is known. */
  struct pbl_bt_device_internal peer_address;
  /** Status. The NimBLE backend only reports successful connections. */
  enum pbl_bt_hci_status status;
  /** True if the local device is the central. */
  bool is_master;
  /** True if the peer's address was resolved with a stored IRK. */
  bool is_resolved;
  /** The peer's IRK. Valid iff @ref is_resolved is true. */
  struct pbl_bt_sm_key irk;
  /** Connection handle. */
  uint16_t handle;
  /** Current ATT MTU. */
  uint16_t mtu;
};

/** @brief A connection was terminated (Disconnection Complete Event, 7.7.5). */
struct PBL_PACKED pbl_bt_disconn_complete_event {
  /** The peer. */
  struct pbl_bt_device_internal peer_address;
  /** Status of the disconnection. */
  enum pbl_bt_hci_status status;
  /** Reason of the disconnection. The NimBLE backend reports its own error codes. */
  enum pbl_bt_hci_status reason;
  /** Connection handle. */
  uint16_t handle;
};

/** @brief Connection parameters were updated (LE Connection Update Complete Event, 7.7.65.3). */
struct PBL_PACKED pbl_bt_conn_update_complete_event {
  /** New connection parameters. */
  struct pbl_bt_conn_params conn_params;
  /** Address of the peer. The address type is not available. */
  struct pbl_bt_addr dev_address;
  /** Status. The NimBLE backend only reports successful updates. */
  enum pbl_bt_hci_status status;
};

/** @brief Link encryption changed (Encryption Change Event, 7.7.8). */
struct PBL_PACKED pbl_bt_encryption_change {
  /** Address of the peer. The address type is not available. */
  struct pbl_bt_addr dev_address;
  /** Status. The NimBLE backend reports its own error codes. */
  enum pbl_bt_hci_status status;
  /** True if the link is now encrypted. */
  bool encryption_enabled;
};

/** @brief The address of a connected peer changed, typically once its identity was resolved. */
struct PBL_PACKED pbl_bt_addr_change {
  /** Current device address. */
  struct pbl_bt_device_internal device;
  /** New device address. */
  struct pbl_bt_device_internal new_device;
};

/** @brief The IRK of a peer changed. */
struct PBL_PACKED pbl_bt_irk_change {
  /** The peer. */
  struct pbl_bt_device_internal device;
  /** True if @ref irk is valid. */
  bool irk_valid;
  /** Identity Resolving Key. */
  struct pbl_bt_sm_key irk;
};

/**
 * @brief Terminate a connection.
 *
 * Completion is reported through pbl_bt_handle_le_disconnection_complete_event().
 *
 * @param peer_address The peer to disconnect from.
 * @retval 0 The disconnection was initiated.
 * @retval -1 There is no connection with @p peer_address.
 * @return A positive NimBLE error code if the termination could not be initiated.
 */
int pbl_bt_gap_le_disconnect(const struct pbl_bt_device_internal *peer_address);

/**
 * @brief Called when a connection is established.
 *
 * @param event The connection.
 */
extern void pbl_bt_handle_le_connection_complete_event(
    const struct pbl_bt_conn_complete_event *event);
/**
 * @brief Called when a connection is terminated.
 *
 * @param event The disconnection.
 */
extern void pbl_bt_handle_le_disconnection_complete_event(
    const struct pbl_bt_disconn_complete_event *event);
/**
 * @brief Called when the encryption of a connection changes.
 *
 * @param event The encryption change.
 */
extern void pbl_bt_handle_le_encryption_change_event(const struct pbl_bt_encryption_change *event);
/**
 * @brief Called when the parameters of a connection were updated.
 *
 * @param event The new parameters.
 */
extern void pbl_bt_handle_le_conn_params_update_event(
    const struct pbl_bt_conn_update_complete_event *event);
/**
 * @brief Called when the identity address of a connected peer was resolved.
 *
 * @param e The address change.
 */
extern void pbl_bt_handle_le_connection_handle_update_address(const struct pbl_bt_addr_change *e);
/**
 * @brief Called when a new IRK of a peer was stored.
 *
 * Invoked on KernelMain by the NimBLE backend.
 *
 * @param e The IRK change.
 */
extern void pbl_bt_handle_le_connection_handle_update_irk(const struct pbl_bt_irk_change *e);
/**
 * @brief Called when the link layer version of a peer was received.
 *
 * Not invoked by the NimBLE backend.
 *
 * @param e The version information.
 */
extern void pbl_bt_handle_peer_version_info_event(
    const struct pbl_bt_remote_version_info_received_event *e);

/** @} */
