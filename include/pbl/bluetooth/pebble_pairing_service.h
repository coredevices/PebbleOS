/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include <pbl/bluetooth/pebble_bt.h>
#include <pbl/bluetooth/responsiveness.h>

#define PBL_BT_PPS_CONNECTION_STATUS_UUID PBL_BT_PEBBLE_UUID_EXPAND(1)
#define PBL_BT_PPS_TRIGGER_PAIRING_UUID   PBL_BT_PEBBLE_UUID_EXPAND(2)
// Note: UUID 4 was used by the 3.14-rc Android App for V0 of the Connection Param characteristic
// but never shipped externally
#define PBL_BT_PPS_CONNECTION_PARAMETERS_UUID PBL_BT_PEBBLE_UUID_EXPAND(5)

enum pbl_bt_pps_gatt_error {
  PBL_BT_PPS_GATT_ERROR_UNKNOWN_COMMAND_ID = PBL_BT_GATT_ERROR_APPLICATION_SPECIFIC_ERROR_START,
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_INVALID_REMOTE_DESIRED_STATE,
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MIN_SLOTS_TOO_SMALL,
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MIN_SLOTS_TOO_LARGE,
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_MAX_SLOTS_TOO_LARGE,
  PBL_BT_PPS_GATT_ERROR_CONN_PARAMS_SUPERVISION_TIMEOUT_TOO_SMALL,
  PBL_BT_PPS_GATT_ERROR_DEVICE_DOES_NOT_SUPPORT_PLE,
};

//! The connectivity status, with respect to the device reading it.
struct PBL_PACKED pbl_bt_pps_connectivity_status {
  union {
    struct {
      //! true if the device that is reading the status is connected (always true)
      bool ble_is_connected : 1;
      //! true if the device that is reading the status is bonded, false if not
      bool ble_is_bonded : 1;
      //! true if the current LE link is encrypted, false if not
      bool ble_is_encrypted : 1;
      //! true if the watch has a bonding to a gateway (LE-based).
      bool has_bonded_gateway : 1;
      //! true if the watch supports writing the "Don't send slave security request" bit.
      //! See https://pebbletechnology.atlassian.net/wiki/display/DEV/Pebble+GATT+Services
      bool supports_pinning_without_security_request : 1;
      //! true if the reversed ppogatt was enabled at the time of bonding
      bool is_reversed_ppogatt_enabled : 1;

      //! Reserved, leave zero for future use.
      uint32_t rsvd : 18;

      //! The error of the last pairing process or all zeroes, if no pairing process has completed
      //! or when there were no errors. Also see BT Spec 4.2, Vol 3, Part H, 3.5.5 Pairing Failed.
      uint8_t last_pairing_result;
    };
    uint8_t bytes[4];
  };
};

_Static_assert(sizeof(struct pbl_bt_pps_connectivity_status) == 4, "");

struct PBL_PACKED pbl_bt_pps_trigger_request {
  bool should_pin_address : 1;

  //! @note Not available in Bluetopia/cc2564x implementation
  //! This flag and should_force_slave_security_request are mutually exclusive!
  bool no_slave_security_request : 1;

  //! @note Not available in Bluetopia/cc2564x implementation
  //! This flag and no_slave_security_request are mutually exclusive!
  bool should_force_slave_security_request : 1;

  //! @note Not available in Bluetopia/cc2564x implementation
  //! Flag to indicate that when re-pairing this device, the re-pairing should be accepted
  //! automatically for this remote device (matching IRK or matching identity address).
  //! @note This is a work-around for an Android 4.4.x bug. This opens up a security hole :( where
  //! a phone could pretend to be the "trusted" phone and pair w/o the user even knowing about it.
  //! @see https://pebbletechnology.atlassian.net/browse/PBL-39369
  bool should_auto_accept_re_pairing : 1;

  //! @note Not available in Bluetopia/cc2564x implementation
  //! Flag to indicate that the PPoGATT server/client roles should be reversed to support the
  //! connected phone. Some older Android phones' GATT service API is completely busted. For those
  //! poor phones, this bit is set before pairing. The Pebble includes a "reversed" PPoGATT service
  //! that the phone app can connect to as GATT client, but this service only works if this bit
  //! gets set *before pairing*. This is a security measure: 1. to prevent non-paired devices from
  //! talking to the "reversed" PPoGATT service. 2. to prevent non-Pebble apps on paired phone that
  //! does support normal PPoGATT from connecting to the "reversed" PPoGATT service.
  //! @see ppogatt_emulated_server_wa.c
  //! @see https://pebbletechnology.atlassian.net/browse/PBL-39634
  bool is_reversed_ppogatt_enabled : 1;
};

struct PBL_PACKED pbl_bt_pps_conn_param_set {
  //! interval_min_ms / 1.25 msec – valid range: 7.5 msec to 4 seconds
  uint16_t interval_min_1_25ms;

  //! (interval_max_ms - interval_min_ms) / 1.25 msec
  //! @note To fit the parent struct in the minimum GATT MTU, this field is a delta and only one
  //! byte instead of the uint16_t that the BT spec uses.
  uint8_t interval_max_delta_1_25ms;

  //! Slave latency (in number of connection events)
  //! @note To fit the parent struct in the minimum GATT MTU, this field is only one byte instead
  //! of the uint16_t that the BT spec uses.
  uint8_t slave_latency_events;

  //! Supervision Timeout / 30 msec – valid range: 100 msec to 32 seconds. To fit this into one
  //! byte and to fit the parent struct in the minimum GATT MTU, the increments is not the standard
  //! 10msec!
  uint8_t supervision_timeout_30ms;
};

//! The connection parameters settings, with respect to connection to the device reading them.
struct PBL_PACKED pbl_bt_pps_conn_params_read_notif {
  //! Capability bits. Reserved for future use.
  uint8_t packet_length_extension_supported : 1;
  uint8_t rsvd : 7;

  //! Current interval / 1.25 msec – valid range: 7.5 msec to 4 seconds
  uint16_t current_interval_1_25ms;

  //! Current Slave latency (in number of connection events) – actual max is 0x01F3, but in
  //! practice values are much lower.
  uint16_t current_slave_latency_events;

  //! Current Supervision Timeout / 10 msec – valid range: 100 msec to 32 seconds.
  uint16_t current_supervision_timeout_10ms;
};

enum pbl_bt_pps_conn_params_write_cmd {
  //! Allows phone to change connection parameter set and take over control of parameter management
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_PARAM_MGMT_SETTINGS = 0x00,
  //! Issues a connection parameter change request if the watch is not in the desired state
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_DESIRED_STATE = 0x01,
  //! Controls settings for BLE 4.2 Packet Length Extension feature
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_ENABLE_PACKET_LENGTH_EXTENSION = 0x02,
  //! If written to disables Dialog BLE sleep mode (safeguard against PBL-39777 in case it affects
  //! more watches in the future)
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_INHIBIT_BLE_SLEEP = 0x03,
  PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_NUM,
};

struct PBL_PACKED pbl_bt_pps_remote_param_mgmt_settings {
  //! If false/zero, Pebble should manage the connection parameters. If true/one, Pebble should
  //! NOT manage the connection parameters. In this mode, Pebble will never request a
  //! connection parameter change.
  bool is_remote_device_managing_connection_parameters : 1;
  uint8_t rsvd : 7;
  //! Optional. Current parameters sets used by Pebble's Connection Parameter manager.
  struct pbl_bt_pps_conn_param_set connection_parameter_sets[];
};

struct PBL_PACKED pbl_bt_pps_remote_desired_state {
  //! The desired ResponseTime as desired by the remote device.  The remote end can set this
  //! value to a faster mode when it's about to transfer/receive a lot of data. For example,
  //! when a lot of BlobDB operations are queued up, the watch doesn't know how much data is
  //! queued up on the remote end. In this case, the remote could write "PBL_BT_RESPONSE_TIME_MIN"
  //! so increase the speed temporarily. It's the remote end's responsibility to reset this to
  //! PBL_BT_RESPONSE_TIME_MAX when the bulk transfer is done.  As a safety measure, the watch is
  //! will reset it back to PBL_BT_RESPONSE_TIME_MAX after 5 minutes.  In case the phone app still
  //! wants to keep a particular desired ResponseTime, the phone app is responsible for making sure
  //! to write the value again before the 5 minute timer expires.
  uint8_t state : 2;

  uint8_t rsvd : 6;
};

struct PBL_PACKED pbl_bt_pps_packet_length_extension {
  uint8_t trigger_ll_length_req : 1;
  uint8_t rsvd : 7;
};

struct PBL_PACKED pbl_bt_pps_inhibit_ble_sleep {
  uint8_t rsvd; // for future use
};

//! The connection parameters settings, with respect to connection to the device writing them.
struct PBL_PACKED pbl_bt_pps_conn_params_write {
  enum pbl_bt_pps_conn_params_write_cmd cmd : 8;
  union PBL_PACKED {
    //! Valid iff cmd ==
    //! PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_PARAM_MGMT_SETTINGS
    struct pbl_bt_pps_remote_param_mgmt_settings remote_param_mgmt_settings;

    //! Valid iff cmd ==
    //! PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_DESIRED_STATE
    struct pbl_bt_pps_remote_desired_state remote_desired_state;

    //! Valid iff cmd ==
    //! PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_ENABLE_PACKET_LENGTH_EXTENSION
    struct pbl_bt_pps_packet_length_extension ple_req;

    //! Valid iff cmd == PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_INHIBIT_BLE_SLEEP
    struct pbl_bt_pps_inhibit_ble_sleep ble_sleep;
  };
};

#define PBL_BT_PPS_REMOTE_PARAM_MGMT_SETTINGS_SIZE_WITH_PARAM_SETS \
  (sizeof(struct pbl_bt_pps_remote_param_mgmt_settings) +          \
   (sizeof(struct pbl_bt_pps_conn_param_set) * PBL_BT_RESPONSE_TIME_NUM))

#define PBL_BT_PPS_CONN_PARAMS_WRITE_SIZE_WITH_PARAM_SETS                      \
  (offsetof(struct pbl_bt_pps_conn_params_write, remote_param_mgmt_settings) + \
   PBL_BT_PPS_REMOTE_PARAM_MGMT_SETTINGS_SIZE_WITH_PARAM_SETS)

_Static_assert(PBL_BT_RESPONSE_TIME_NUM == 3, "");
_Static_assert(sizeof(struct pbl_bt_pps_conn_params_read_notif) <= 20, "Larger than minimum MTU!");
_Static_assert(PBL_BT_PPS_CONN_PARAMS_WRITE_SIZE_WITH_PARAM_SETS <= 20, "Larger than minimum MTU!");
_Static_assert(sizeof(struct pbl_bt_pps_conn_params_write) <= 20, "Larger than minimum MTU!");
_Static_assert(sizeof(struct pbl_bt_pps_connectivity_status) <= 20, "Larger than minimum MTU!");

typedef struct GAPLEConnection GAPLEConnection;

//! Signals to the Pebble GATT service that status change has occurred (pairing, encryption, ...),
//! allowing it to notify any BLE devices that are subscribed to connectivity status updates of the
//! change.
//! @param connection The connection for which the status was changed.
void pbl_bt_pps_handle_status_change(const GAPLEConnection *connection);

//! Indicate to the FW that Connectivity Status characteristic has been unsubscribed from.
//! This is used to detect that the Pebble iOS app has been terminated.
extern void pbl_bt_cb_pps_handle_ios_app_termination_detected(void);

//! Indicate to the FW that the Connection Parameters characteristic has been written to with a new
//! values.
//! @param device The device that wrote to the characteristic.
//! @param conn_params The value as written to the Connection Parameters characteristic. The BT
//! driver lib is expected to validate any written values and only call this function with valid
//! values.
//! @param conn_params_length The length of conn_params in bytes.
extern void pbl_bt_cb_pps_handle_connection_parameter_write(
    const struct pbl_bt_device_internal *device,
    const struct pbl_bt_pps_conn_params_write *conn_params, size_t conn_params_length);
