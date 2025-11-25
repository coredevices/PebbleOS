/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth_analytics.h"

#include "comm/ble/gap_le_connection.h"
#include "comm/bt_lock.h"
#include "drivers/rtc.h"
#include "services/common/analytics/analytics.h"
#include "services/common/bluetooth/bluetooth_ctl.h"
#include "services/common/comm_session/session.h"
#include "system/logging.h"
#include "util/bitset.h"
#include "util/math.h"

#include <bluetooth/analytics.h>
#include <bluetooth/gap_le_connect.h>

typedef struct {
  uint32_t slave_latency_events;
  uint32_t supervision_to_ms;
  int num_samps;
} LeConnectionParams;

static LeConnectionParams s_le_conn_params = { 0 };

void bluetooth_analytics_get_param_averages(uint16_t *params) {
  int num_samps = s_le_conn_params.num_samps;
  if (num_samps != 0) {
    params[0] = s_le_conn_params.slave_latency_events / num_samps;
    params[1] = s_le_conn_params.supervision_to_ms / num_samps;
  }

  s_le_conn_params = (LeConnectionParams){};
}

static void prv_update_conn_params(uint16_t slave_latency_events,
                                   uint16_t supervision_to_10ms) {
  bt_lock();
  s_le_conn_params.slave_latency_events += slave_latency_events;
  s_le_conn_params.supervision_to_ms += (supervision_to_10ms * 10);
  s_le_conn_params.num_samps++;
  bt_unlock();
}

static void prv_update_conn_event_timer(uint32_t interval_1_25ms, bool stop) {
  bt_lock();
  static bool s_analytic_conn_timer_running = false;
  if (stop || s_analytic_conn_timer_running) {
    analytics_stopwatch_stop(ANALYTICS_DEVICE_METRIC_BLE_CONN_EVENT_COUNT);
    s_analytic_conn_timer_running = false;
  }

  if (!stop) {
    // track (# connection attempts * 10^3) / sec
    uint32_t conn_attempts_per_sec = ((1000 * 1000 * 5) / (interval_1_25ms)) / 4;
    analytics_stopwatch_start_at_rate(
                                      ANALYTICS_DEVICE_METRIC_BLE_CONN_EVENT_COUNT,
                                      conn_attempts_per_sec, AnalyticsClient_System);
    s_analytic_conn_timer_running = true;
  }
  bt_unlock();
}

void bluetooth_analytics_handle_param_update_failed(void) {
  analytics_inc(ANALYTICS_DEVICE_METRIC_BLE_CONN_PARAM_UPDATE_FAILED_COUNT,
                AnalyticsClient_System);
}

//! only called when we are connected as a slave
void bluetooth_analytics_handle_connection_params_update(const BleConnectionParams *params) {
  // When connected as a slave device, the 'Slave Latency' connection parameter allows
  // the controller to skip the connection sync for that number of connection events.
  uint32_t effective_interval = params->conn_interval_1_25ms * (1 + params->slave_latency_events);

  prv_update_conn_event_timer(effective_interval, false);
  prv_update_conn_params(params->slave_latency_events, params->supervision_timeout_10ms);
}

void bluetooth_analytics_handle_connection_disconnection_event(
    AnalyticsEvent type, uint8_t reason, const BleRemoteVersionInfo *vers_info) {
  static uint32_t last_reset_counter_ticks = 0;
  static uint8_t num_events_logged = 0;

  const uint32_t ticks_per_hour = RTC_TICKS_HZ * 60 * 60;

  if ((rtc_get_ticks() - last_reset_counter_ticks) > ticks_per_hour) {
    num_events_logged = 0;
    last_reset_counter_ticks = rtc_get_ticks();
  }

  if (num_events_logged > 100) { // don't log a ridiculous amount of tightly looped disconnects
    return;
  }

  // It's okay to log to analytics directly from the BT02 callback thread
  // because flash writes are dispatched to KernelBG if the datalogging session
  // is buffered
  if (type != AnalyticsEvent_BtLeDisconnect) {
    analytics_event_bt_connection_or_disconnection(type, reason);
  } else {
    if (!vers_info) { // We expect version info
      PBL_LOG(LOG_LEVEL_WARNING, "Le Disconnect but no version info?");
    } else {
      analytics_event_bt_le_disconnection(reason, vers_info->version_number,
                                          vers_info->company_identifier,
                                          vers_info->subversion_number);
    }
  }

  num_events_logged++;
}

void bluetooth_analytics_handle_connect(
    const BTDeviceInternal *peer_addr, const BleConnectionParams *conn_params) {

  analytics_inc(ANALYTICS_DEVICE_METRIC_BLE_CONNECT_COUNT, AnalyticsClient_System);
  analytics_stopwatch_start(ANALYTICS_DEVICE_METRIC_BLE_CONNECT_TIME, AnalyticsClient_System);

  bluetooth_analytics_handle_connection_params_update(conn_params);

  uint8_t link_quality = 0;
  int8_t rssi = 0;
  bool success = bt_driver_analytics_get_connection_quality(peer_addr, &link_quality, &rssi);

  if (success) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Link quality: %x, RSSI: %d", link_quality, rssi);
    analytics_add(ANALYTICS_DEVICE_METRIC_BLE_LINK_QUALITY_SUM,
                  link_quality, AnalyticsClient_System);
    analytics_add(ANALYTICS_DEVICE_METRIC_BLE_RSSI_SUM,
                  ABS(rssi), AnalyticsClient_System);
  }
}

void bluetooth_analytics_handle_disconnect(bool local_is_master) {
  if (!local_is_master) {
    analytics_stopwatch_stop(ANALYTICS_DEVICE_METRIC_BLE_CONNECT_TIME);
    analytics_stopwatch_stop(ANALYTICS_DEVICE_METRIC_BLE_CONNECT_ENCRYPTED_TIME);

    prv_update_conn_event_timer(0, true);
  }
}

void bluetooth_analytics_handle_encryption_change(void) {
  analytics_stopwatch_start(ANALYTICS_DEVICE_METRIC_BLE_CONNECT_ENCRYPTED_TIME,
                            AnalyticsClient_System);
}

void bluetooth_analytics_handle_no_intent_for_connection(void) {
  analytics_inc(ANALYTICS_DEVICE_METRIC_BLE_CONNECT_NO_INTENT_COUNT, AnalyticsClient_System);
}

void bluetooth_analytics_handle_ble_pairing_request(void) {
  analytics_inc(ANALYTICS_DEVICE_METRIC_BLE_PAIRING_COUNT, AnalyticsClient_System);
}

void bluetooth_analytics_handle_bt_classic_pairing_request(void) {
  analytics_inc(ANALYTICS_DEVICE_METRIC_BT_PAIRING_COUNT, AnalyticsClient_System);
}

void bluetooth_analytics_handle_ble_pairing_error(uint32_t error) {
  analytics_event_bt_error(AnalyticsEvent_BtLePairingError, error);
}

void bluetooth_analytics_handle_bt_classic_pairing_error(uint32_t error) {
  analytics_event_bt_error(AnalyticsEvent_BtClassicPairingError, error);
}

void bluetooth_analytics_ble_mic_error(uint32_t num_sequential_mic_errors) {
  PBL_LOG(LOG_LEVEL_INFO, "MIC Error detected ... %"PRIu32" packets", num_sequential_mic_errors);
  analytics_event_bt_error(AnalyticsEvent_BtLeMicError, num_sequential_mic_errors);
}

static uint32_t prv_calc_other_errors(const SlaveConnEventStats *stats) {
  return (stats->num_type_errors + stats->num_len_errors + stats->num_crc_errors +
          stats->num_mic_errors);
}

static bool prv_calc_stats_and_print(const SlaveConnEventStats *orig_stats,
                                           SlaveConnEventStats *stats_buf, bool is_putbytes) {
  if (bt_driver_analytics_get_conn_event_stats(stats_buf)) {
    stats_buf->num_conn_events =
        serial_distance32(orig_stats->num_conn_events, stats_buf->num_conn_events);
    stats_buf->num_sync_errors =
        serial_distance32(orig_stats->num_sync_errors, stats_buf->num_sync_errors);
    stats_buf->num_conn_events_skipped =
        serial_distance32(orig_stats->num_conn_events_skipped, stats_buf->num_conn_events_skipped);
    stats_buf->num_type_errors =
        serial_distance32(orig_stats->num_type_errors, stats_buf->num_type_errors);
    stats_buf->num_len_errors =
        serial_distance32(orig_stats->num_len_errors, stats_buf->num_len_errors);
    stats_buf->num_crc_errors =
        serial_distance32(orig_stats->num_crc_errors, stats_buf->num_crc_errors);
    stats_buf->num_mic_errors =
        serial_distance32(orig_stats->num_mic_errors, stats_buf->num_mic_errors);

    PBL_LOG(LOG_LEVEL_INFO, "%sBytes Conn Stats: Events: %"PRIu32", Sync Errs: %"PRIu32
      ", Skipped Events: %"PRIu32" Other Errs: %"PRIu32, is_putbytes ? "Put" : "Get",
            stats_buf->num_conn_events, stats_buf->num_sync_errors,
            stats_buf->num_conn_events_skipped, prv_calc_other_errors(stats_buf));

    return true;
  }
  return false;
}

void bluetooth_analytics_handle_put_bytes_stats(bool successful, uint8_t type, uint32_t total_size,
                                                uint32_t elapsed_time_ms,
                                                const SlaveConnEventStats *orig_stats) {
  SlaveConnEventStats new_stats = {};
  prv_calc_stats_and_print(orig_stats, &new_stats, true /* is_putbytes */);

  analytics_event_put_byte_stats(
    comm_session_get_system_session(), successful, type,
    total_size, elapsed_time_ms, new_stats.num_conn_events,
    new_stats.num_sync_errors, new_stats.num_conn_events_skipped,
    prv_calc_other_errors(&new_stats));
}

void bluetooth_analytics_handle_get_bytes_stats(uint8_t type, uint32_t total_size,
                                                uint32_t elapsed_time_ms,
                                                const SlaveConnEventStats *orig_stats) {
  SlaveConnEventStats new_stats = {};
  prv_calc_stats_and_print(orig_stats, &new_stats, false /* is_putbytes */);

  analytics_event_get_bytes_stats(
      comm_session_get_system_session(), type,
      total_size, elapsed_time_ms, new_stats.num_conn_events,
      new_stats.num_sync_errors, new_stats.num_conn_events_skipped,
      prv_calc_other_errors(&new_stats));
}

void analytics_external_collect_ble_parameters(void) {
  bt_lock();
  {
    GAPLEConnection *connection = gap_le_connection_get_gateway();
    if (!connection) {
      goto unlock;
    }

    LEChannelMap le_channel_map;
    const bool success =
        bt_driver_analytics_collect_ble_parameters(&connection->device, &le_channel_map);
    if (success) {
      analytics_set(ANALYTICS_DEVICE_METRIC_BLE_CHAN_USE_COUNT,
                    count_bits_set((uint8_t *)&le_channel_map, NUM_LE_CHANNELS),
                    AnalyticsClient_System);
    }
  }
unlock:
  bt_unlock();
}

void analytics_external_collect_chip_specific_parameters(void) {
  bt_lock();
  bt_driver_analytics_external_collect_chip_specific_parameters();
  bt_unlock();
}

void analytics_external_collect_bt_chip_heartbeat(void) {
// TODO: PBL-38365: Re-enable this once it is fixed :(
#if 0
  if (bt_ctl_is_bluetooth_running()) {
    // No need for lock
    bt_driver_analytics_external_collect_bt_chip_heartbeat();
  }
#endif
}
