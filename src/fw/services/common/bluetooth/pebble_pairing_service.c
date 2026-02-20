/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/pebble_pairing_service.h>

#include "comm/ble/gap_le_connect_params.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/kernel_le_client/app_launch/app_launch.h"
#include "comm/ble/kernel_le_client/ppogatt/ppogatt.h"
#include "comm/bt_conn_mgr.h"
#include "comm/bt_lock.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"

extern void gap_le_connect_params_re_evaluate(GAPLEConnection *connection);

static void prv_convert_pps_request_params(const PebblePairingServiceConnParamSet *pps_params_in,
                                           GAPLEConnectRequestParams *params_out) {
  const uint16_t min_1_25ms = pps_params_in->interval_min_1_25ms;
  params_out->connection_interval_min_1_25ms = min_1_25ms;
  params_out->connection_interval_max_1_25ms =
      min_1_25ms + pps_params_in->interval_max_delta_1_25ms;
#if RECOVERY_FW
  if (pps_params_in->slave_latency_events != 0) {
    PBL_LOG_DBG("Overriding requested slave latency with 0 because PRF");
  }
  params_out->slave_latency_events = 0;
#else
  params_out->slave_latency_events = pps_params_in->slave_latency_events;
#endif
  params_out->supervision_timeout_10ms = pps_params_in->supervision_timeout_30ms * 3;
}

static void prv_handle_set_remote_param_mgmt_settings(GAPLEConnection *connection,
    const PebblePairingServiceRemoteParamMgmtSettings *settings, size_t settings_length) {
  bool is_remote_device_managing_connection_parameters =
      settings->is_remote_device_managing_connection_parameters;
  connection->is_remote_device_managing_connection_parameters =
      is_remote_device_managing_connection_parameters;
  PBL_LOG_INFO("PPS: is_remote_mgmt=%u",
          is_remote_device_managing_connection_parameters);

  if (settings_length >= PEBBLE_PAIRING_SERVICE_REMOTE_PARAM_MGTM_SETTINGS_SIZE_WITH_PARAM_SETS) {
    if (!connection->connection_parameter_sets) {
      const size_t size = sizeof(GAPLEConnectRequestParams) * NumResponseTimeState;
      connection->connection_parameter_sets =
          (GAPLEConnectRequestParams *) kernel_zalloc_check(size);
    }
    for (ResponseTimeState s = ResponseTimeMax; s < NumResponseTimeState; ++s) {
      const PebblePairingServiceConnParamSet *pps_params =
          &settings->connection_parameter_sets[s];
      GAPLEConnectRequestParams *params = &connection->connection_parameter_sets[s];
      prv_convert_pps_request_params(pps_params, params);
      PBL_LOG_INFO("PPS: Updated param set %u: %u-%u, slave lat: %u, supervision timeout: %u",
              s, params->connection_interval_min_1_25ms, params->connection_interval_max_1_25ms,
              params->slave_latency_events, params->supervision_timeout_10ms);
    }
  }

  // Always just re-evaluate, should be idempotent:
  gap_le_connect_params_re_evaluate(connection);
}

static void prv_handle_set_remote_desired_state(GAPLEConnection *connection,
    const PebblePairingServiceRemoteDesiredState *desired_state) {
  const ResponseTimeState remote_desired_state = (ResponseTimeState)desired_state->state;
  PBL_LOG_INFO("PPS: desired_state=%u", remote_desired_state);

  // "As a safety measure, the watch will reset it back to ResponseTimeMax after 5 minutes."
  const uint16_t max_period_secs = 5 * 60;
  conn_mgr_set_ble_conn_response_time(connection, BtConsumerPebblePairingServiceRemoteDevice,
                                      remote_desired_state, max_period_secs);
}

void bt_driver_cb_pebble_pairing_service_handle_connection_parameter_write(
    const BTDeviceInternal *device,
    const PebblePairingServiceConnParamsWrite *conn_params,
    size_t conn_params_length) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_by_device(device);
    if (!connection) {
      goto unlock;
    }
    const size_t length = (conn_params_length - offsetof(PebblePairingServiceConnParamsWrite,
                                                         remote_desired_state));
    switch (conn_params->cmd) {
      case PebblePairingServiceConnParamsWriteCmd_SetRemoteParamMgmtSettings:
        prv_handle_set_remote_param_mgmt_settings(connection,
                                                  &conn_params->remote_param_mgmt_settings, length);
        break;

      case PebblePairingServiceConnParamsWriteCmd_SetRemoteDesiredState:
        prv_handle_set_remote_desired_state(connection, &conn_params->remote_desired_state);
        break;
      case PebblePairingServiceConnParamsWriteCmd_EnablePacketLengthExtension:
        PBL_LOG_INFO("Enabling BLE Packet Length Extension");
        break;
      case PebblePairingServiceConnParamsWriteCmd_InhibitBLESleep:
        PBL_LOG_INFO("BLE Sleep Mode inhibited!");
        break;
      default:
        PBL_LOG_ERR("Unknown write_cmd %d", conn_params->cmd);
        break;
    }
  }
unlock:
  bt_unlock();
}

void bt_driver_cb_pebble_pairing_service_handle_ios_app_termination_detected(void) {
  app_launch_trigger();
}

void bt_driver_cb_pebble_pairing_service_handle_ppog_reinit(void) {
  ppogatt_reset_all();
}
