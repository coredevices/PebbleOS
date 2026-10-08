/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/pebble_bt.h>
#include <pbl/bluetooth/responsiveness.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_pebble_pairing_service Pebble Pairing Service
 * @ingroup bluetooth
 * @brief GATT service through which the phone app checks the connection and triggers pairing.
 *
 * The service (PBL_BT_PPS_UUID_16BIT) has a Connectivity Status characteristic (read, notify)
 * and a Trigger Pairing characteristic (read, write). The NimBLE backend does not implement the
 * Connection Parameters characteristic. The wire formats below must fit the minimum ATT MTU.
 * @{
 */

/** @brief UUID of the Connectivity Status characteristic, as an initializer list. */
#define PBL_BT_PPS_CONNECTION_STATUS_UUID PBL_BT_PEBBLE_UUID_EXPAND(1)
/** @brief UUID of the Trigger Pairing characteristic, as an initializer list. */
#define PBL_BT_PPS_TRIGGER_PAIRING_UUID PBL_BT_PEBBLE_UUID_EXPAND(2)
/**
 * @brief UUID of the Connection Parameters characteristic, as an initializer list.
 *
 * UUID 4 was used by a pre-release Android app for an earlier version of it and must not be
 * reused.
 */
#define PBL_BT_PPS_CONNECTION_PARAMETERS_UUID PBL_BT_PEBBLE_UUID_EXPAND(5)

/** @brief Application specific ATT errors returned by the service. */
enum pbl_bt_pps_gatt_error {
  /** Unknown command. */
  PBL_BT_PPS_GATT_ERROR_UNKNOWN_COMMAND_ID = PBL_BT_GATT_ERROR_APPLICATION_SPECIFIC_ERROR_START,
  /** The requested remote desired state is invalid. */
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_INVALID_REMOTE_DESIRED_STATE,
  /** The minimum connection interval is too small. */
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MIN_SLOTS_TOO_SMALL,
  /** The minimum connection interval is too large. */
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MIN_SLOTS_TOO_LARGE,
  /** The maximum connection interval is too large. */
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MAX_SLOTS_TOO_LARGE,
  /** The supervision timeout is too small. */
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_SUPERVISION_TIMEOUT_TOO_SMALL,
  /** The device does not support Packet Length Extension. */
  PBL_BT_PPS_GATT_ERROR_DEVICE_DOES_NOT_SUPPORT_PLE,
};

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

_Static_assert(sizeof(struct pbl_bt_pps_connectivity_status) == 4, "");

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

/** @brief A connection parameter set, in the Connection Parameters characteristic format. */
struct PBL_PACKED pbl_bt_pps_conn_param_set {
  /** Minimum connection interval in 1.25 ms units, 7.5 ms to 4 s. */
  uint16_t interval_min_1_25ms;

  /**
   * Maximum minus minimum connection interval in 1.25 ms units.
   *
   * A one-byte delta, not the spec's uint16_t, to fit the minimum MTU.
   */
  uint8_t interval_max_delta_1_25ms;

  /**
   * Peripheral latency in connection events.
   *
   * One byte, not the spec's uint16_t, to fit the minimum MTU.
   */
  uint8_t slave_latency_events;

  /**
   * Supervision timeout in 30 ms units (not the spec's 10 ms, to fit one byte), 100 ms to 32 s.
   */
  uint8_t supervision_timeout_30ms;
};

/** @brief Connection Parameters read or notified value, for the reading device's connection. */
struct PBL_PACKED pbl_bt_pps_conn_params_read_notif {
  /** True if Packet Length Extension is supported. */
  uint8_t packet_length_extension_supported : 1;
  /** Reserved. */
  uint8_t rsvd : 7;

  /** Current connection interval in 1.25 ms units, 7.5 ms to 4 s. */
  uint16_t current_interval_1_25ms;

  /** Current peripheral latency in connection events, at most 0x01F3. */
  uint16_t current_slave_latency_events;

  /** Current supervision timeout in 10 ms units, 100 ms to 32 s. */
  uint16_t current_supervision_timeout_10ms;
};

/** @brief Commands written to the Connection Parameters characteristic. */
enum pbl_bt_pps_conn_params_write_cmd {
  /** Change the connection parameter sets and take over parameter management. */
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_PARAM_MGMT_SETTINGS = 0x00,
  /** Request a connection parameter change if the watch is not in the desired state. */
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_DESIRED_STATE = 0x01,
  /** Control the LE Packet Length Extension feature. */
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_ENABLE_PACKET_LENGTH_EXTENSION = 0x02,
  /** Disable the controller sleep mode, a safeguard for a Dialog controller issue. */
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_INHIBIT_BLE_SLEEP = 0x03,
  /** Number of commands. */
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_NUM,
};

/** @brief Payload of PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_PARAM_MGMT_SETTINGS. */
struct PBL_PACKED pbl_bt_pps_remote_param_mgmt_settings {
  /**
   * True if the remote device manages the connection parameters.
   *
   * The watch then never requests a connection parameter change.
   */
  bool is_remote_device_managing_connection_parameters : 1;
  /** Reserved. */
  uint8_t rsvd : 7;
  /** Optional parameter sets for the watch's connection parameter manager. */
  struct pbl_bt_pps_conn_param_set connection_parameter_sets[];
};

/** @brief Payload of PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_DESIRED_STATE. */
struct PBL_PACKED pbl_bt_pps_remote_desired_state {
  /**
   * Response time desired by the remote device, an enum pbl_bt_response_time_state.
   *
   * The remote can ask for PBL_BT_RESPONSE_TIME_MIN before a bulk transfer the watch cannot
   * anticipate, and is responsible for setting PBL_BT_RESPONSE_TIME_MAX when done. The watch
   * falls back to PBL_BT_RESPONSE_TIME_MAX after 5 minutes; write again before then to keep the
   * state.
   */
  uint8_t state : 2;

  /** Reserved. */
  uint8_t rsvd : 6;
};

/** @brief Payload of PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_ENABLE_PACKET_LENGTH_EXTENSION. */
struct PBL_PACKED pbl_bt_pps_packet_length_extension {
  /** Trigger an LL length request. */
  uint8_t trigger_ll_length_req : 1;
  /** Reserved. */
  uint8_t rsvd : 7;
};

/** @brief Payload of PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_INHIBIT_BLE_SLEEP. */
struct PBL_PACKED pbl_bt_pps_inhibit_ble_sleep {
  /** Reserved. */
  uint8_t rsvd;
};

/** @brief Value written to the Connection Parameters characteristic. */
struct PBL_PACKED pbl_bt_pps_conn_params_write {
  /** The command, selects the payload. */
  enum pbl_bt_pps_conn_params_write_cmd cmd : 8;
  /** Command payload, selected by @c cmd. */
  union PBL_PACKED {
    /** Valid iff @c cmd is PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_PARAM_MGMT_SETTINGS. */
    struct pbl_bt_pps_remote_param_mgmt_settings remote_param_mgmt_settings;

    /** Valid iff @c cmd is PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_DESIRED_STATE. */
    struct pbl_bt_pps_remote_desired_state remote_desired_state;

    /** Valid iff @c cmd is PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_ENABLE_PACKET_LENGTH_EXTENSION. */
    struct pbl_bt_pps_packet_length_extension ple_req;

    /** Valid iff @c cmd is PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_INHIBIT_BLE_SLEEP. */
    struct pbl_bt_pps_inhibit_ble_sleep ble_sleep;
  };
};

/** @brief Size of struct pbl_bt_pps_remote_param_mgmt_settings with all parameter sets. */
#define PBL_BT_PPS_REMOTE_PARAM_MGMT_SETTINGS_SIZE_WITH_PARAM_SETS \
  (sizeof(struct pbl_bt_pps_remote_param_mgmt_settings) +          \
   (sizeof(struct pbl_bt_pps_conn_param_set) * PBL_BT_RESPONSE_TIME_NUM))

/** @brief Size of a parameter management settings write with all parameter sets. */
#define PBL_BT_PPS_CONN_PARAMS_WRITE_SIZE_WITH_PARAM_SETS                      \
  (offsetof(struct pbl_bt_pps_conn_params_write, remote_param_mgmt_settings) + \
   PBL_BT_PPS_REMOTE_PARAM_MGMT_SETTINGS_SIZE_WITH_PARAM_SETS)

_Static_assert(PBL_BT_RESPONSE_TIME_NUM == 3, "");
_Static_assert(sizeof(struct pbl_bt_pps_conn_params_read_notif) <= 20, "Larger than minimum MTU!");
_Static_assert(PBL_BT_PPS_CONN_PARAMS_WRITE_SIZE_WITH_PARAM_SETS <= 20, "Larger than minimum MTU!");
_Static_assert(sizeof(struct pbl_bt_pps_conn_params_write) <= 20, "Larger than minimum MTU!");
_Static_assert(sizeof(struct pbl_bt_pps_connectivity_status) <= 20, "Larger than minimum MTU!");

/** @brief LE connection state kept by the firmware (see @c comm/ble/gap_le_connection.h). */
typedef struct GAPLEConnection GAPLEConnection;

/**
 * @brief Signal a change of the connection status (pairing, encryption, ...).
 *
 * Notifies the Connectivity Status to the subscribed device.
 *
 * @param connection The connection whose status changed.
 */
void pbl_bt_pps_handle_status_change(const GAPLEConnection *connection);

/**
 * @brief Called when the Connectivity Status characteristic is unsubscribed from.
 *
 * Used to detect that the Pebble iOS app was terminated. Not invoked by the NimBLE backend.
 */
extern void pbl_bt_cb_pps_handle_ios_app_termination_detected(void);

/**
 * @brief Called when the Connection Parameters characteristic was written.
 *
 * Not invoked by the NimBLE backend.
 *
 * @param device The device that wrote the characteristic.
 * @param conn_params The value written, validated by the stack.
 * @param conn_params_length Length of @p conn_params in bytes.
 */
extern void pbl_bt_cb_pps_handle_connection_parameter_write(
    const struct pbl_bt_device_internal *device,
    const struct pbl_bt_pps_conn_params_write *conn_params, size_t conn_params_length);

/** @} */
