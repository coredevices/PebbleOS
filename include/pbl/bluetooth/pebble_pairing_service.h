/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/pebble_bt.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_pebble_pairing_service Pebble Pairing Service
 * @ingroup bluetooth
 * @brief GATT service through which the phone app checks the connection and triggers pairing.
 *
 * The service (PBL_BT_PPS_UUID_16BIT) has a Connectivity Status characteristic (read, notify)
 * and a Trigger Pairing characteristic (read, write). A subscribed central is notified of the
 * status when the link gets encrypted. The wire formats below must fit the minimum ATT MTU.
 * @{
 */

/** @brief UUID of the Connectivity Status characteristic, as an initializer list. */
#define PBL_BT_PPS_CONNECTION_STATUS_UUID PBL_BT_PEBBLE_UUID_EXPAND(1)
/** @brief UUID of the Trigger Pairing characteristic, as an initializer list. */
#define PBL_BT_PPS_TRIGGER_PAIRING_UUID PBL_BT_PEBBLE_UUID_EXPAND(2)

/** @brief Connectivity Status value, with respect to the device reading it. */
struct PBL_PACKED pbl_bt_pps_connectivity_status {
  union {
    struct {
      /** True if the reading device is connected (always true). */
      bool ble_is_connected : 1;
      /** True if the reading device is bonded. */
      bool ble_is_bonded : 1;
      /** True if the current LE link is encrypted. */
      bool ble_is_encrypted : 1;
      /** True if the watch has a bonding to an LE gateway. */
      bool has_bonded_gateway : 1;
      /**
       * True if the watch supports the @ref pbl_bt_pps_trigger_request::no_slave_security_request
       * bit.
       */
      bool supports_pinning_without_security_request : 1;
      /** True if reversed PPoGATT was enabled at the time of bonding. */
      bool is_reversed_ppogatt_enabled : 1;

      /** Reserved, zero. */
      uint32_t rsvd : 18;

      /**
       * Error of the last pairing, or zero if no pairing completed or it succeeded.
       *
       * See Bluetooth Core Specification v4.2, Vol 3, Part H, 3.5.5 Pairing Failed.
       */
      uint8_t last_pairing_result;
    };
    /** Raw value. */
    uint8_t bytes[4];
  };
};

static_assert(sizeof(struct pbl_bt_pps_connectivity_status) == 4, "");

/** @brief Value written to the Trigger Pairing characteristic. */
struct PBL_PACKED pbl_bt_pps_trigger_request {
  /** Pin the local address for this device. */
  bool should_pin_address : 1;

  /**
   * Don't send a security request. Mutually exclusive with
   * @ref should_force_slave_security_request.
   */
  bool no_slave_security_request : 1;

  /**
   * Send a security request even if the link is already encrypted. Mutually exclusive with
   * @ref no_slave_security_request.
   */
  bool should_force_slave_security_request : 1;

  /**
   * Accept re-pairing with this device automatically (matching IRK or identity address).
   *
   * @note A work-around for an Android 4.4.x bug. It opens a security hole: a phone could
   * impersonate the trusted phone and pair without the user knowing.
   */
  bool should_auto_accept_re_pairing : 1;

  /**
   * Reverse the PPoGATT server and client roles for this phone.
   *
   * For older Android phones with a broken GATT server API: the watch then hosts a "reversed"
   * PPoGATT service the phone app connects to as a client. It only works if this bit is set
   * before pairing, which keeps unpaired devices and non-Pebble apps on a phone that supports
   * normal PPoGATT away from the reversed service.
   */
  bool is_reversed_ppogatt_enabled : 1;
};

/** @} */
