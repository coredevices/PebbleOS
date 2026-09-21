/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/pebble_pairing_service.h>

#include "comm/ble/gap_le_connect_params.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/kernel_le_client/app_launch/app_launch.h"
#include "comm/bt_conn_mgr.h"
#include "comm/bt_lock.h"
#include "kernel/pbl_malloc.h"
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

extern void gap_le_connect_params_re_evaluate(GAPLEConnection *connection);

static void prv_convert_pps_request_params(const struct pbl_bt_pps_conn_param_set *pps_params_in,
                                           GAPLEConnectRequestParams *params_out) {
  const uint16_t min_1_25ms = pps_params_in->interval_min_1_25ms;
  params_out->connection_interval_min_1_25ms = min_1_25ms;
  params_out->connection_interval_max_1_25ms =
      min_1_25ms + pps_params_in->interval_max_delta_1_25ms;
#ifdef CONFIG_RECOVERY_FW
  if (pps_params_in->slave_latency_events != 0) {
    PBL_LOG_DBG("Overriding requested slave latency with 0 because PRF");
  }
  params_out->slave_latency_events = 0;
#else
  params_out->slave_latency_events = pps_params_in->slave_latency_events;
#endif
  params_out->supervision_timeout_10ms = pps_params_in->supervision_timeout_30ms * 3;
}

static void prv_handle_set_remote_param_mgmt_settings(
    GAPLEConnection *connection, const struct pbl_bt_pps_remote_param_mgmt_settings *settings,
    size_t settings_length) {
  bool is_remote_device_managing_connection_parameters =
      settings->is_remote_device_managing_connection_parameters;
  connection->is_remote_device_managing_connection_parameters =
      is_remote_device_managing_connection_parameters;

  if (settings_length >= PBL_BT_PPS_REMOTE_PARAM_MGMT_SETTINGS_SIZE_WITH_PARAM_SETS) {
    if (!connection->connection_parameter_sets) {
      const size_t size = sizeof(GAPLEConnectRequestParams) * PBL_BT_RESPONSE_TIME_NUM;
      connection->connection_parameter_sets =
          (GAPLEConnectRequestParams *)kernel_zalloc_check(size);
    }
    for (enum pbl_bt_response_time_state s = PBL_BT_RESPONSE_TIME_MAX; s < PBL_BT_RESPONSE_TIME_NUM;
         ++s) {
      const struct pbl_bt_pps_conn_param_set *pps_params = &settings->connection_parameter_sets[s];
      GAPLEConnectRequestParams *params = &connection->connection_parameter_sets[s];
      prv_convert_pps_request_params(pps_params, params);
    }
  }

  // Always just re-evaluate, should be idempotent:
  gap_le_connect_params_re_evaluate(connection);
}

static void prv_handle_set_remote_desired_state(
    GAPLEConnection *connection, const struct pbl_bt_pps_remote_desired_state *desired_state) {
  const enum pbl_bt_response_time_state remote_desired_state =
      (enum pbl_bt_response_time_state)desired_state->state;
  PBL_LOG_DBG("PPS: desired_state=%u", remote_desired_state);

  // "As a safety measure, the watch will reset it back to PBL_BT_RESPONSE_TIME_MAX after 5
  // minutes."
  const uint16_t max_period_secs = 5 * 60;
  conn_mgr_set_ble_conn_response_time(connection, PBL_BT_CONSUMER_PPS_REMOTE_DEVICE,
                                      remote_desired_state, max_period_secs);
}

void pbl_bt_cb_pps_handle_connection_parameter_write(
    const struct pbl_bt_device_internal *device,
    const struct pbl_bt_pps_conn_params_write *conn_params, size_t conn_params_length) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_device(device);
    if (!connection) {
      goto unlock;
    }
    const size_t length =
        (conn_params_length - offsetof(struct pbl_bt_pps_conn_params_write, remote_desired_state));
    switch (conn_params->cmd) {
      case PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_PARAM_MGMT_SETTINGS:
        prv_handle_set_remote_param_mgmt_settings(connection,
                                                  &conn_params->remote_param_mgmt_settings, length);
        break;

      case PBL_BT_PPS_CONN_PARAMS_WRITE_CMD_SET_REMOTE_DESIRED_STATE:
        prv_handle_set_remote_desired_state(connection, &conn_params->remote_desired_state);
        break;
      default:
        PBL_LOG_ERR("Unknown write_cmd %d", conn_params->cmd);
        break;
    }
  }
unlock:
  bt_unlock();
}

void pbl_bt_cb_pps_handle_ios_app_termination_detected(void) {
  app_launch_trigger();
}
