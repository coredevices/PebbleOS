/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/bluetooth/gap_le_connect.h>
#include <pbl/kernel/compiler.h>

struct pbl_bt_slave_conn_event_stats;

PBL_WEAK void bluetooth_analytics_get_param_averages(uint16_t *params) {
}

PBL_WEAK void bluetooth_analytics_handle_param_update_failed(void) {
}

PBL_WEAK void bluetooth_analytics_handle_connection_params_update(
    const struct pbl_bt_conn_params *params) {
}

PBL_WEAK void bluetooth_analytics_handle_connect(const struct pbl_bt_device_internal *peer_addr,
                                                 const struct pbl_bt_conn_params *conn_params) {
}

PBL_WEAK void bluetooth_analytics_handle_disconnect(bool local_is_master) {
}

PBL_WEAK void bluetooth_analytics_handle_encryption_change(void) {
}

PBL_WEAK void bluetooth_analytics_handle_no_intent_for_connection(void) {
}

PBL_WEAK void bluetooth_analytics_handle_ble_pairing_request(void) {
}

PBL_WEAK void bluetooth_analytics_handle_ble_pairing_error(uint32_t error) {
}

PBL_WEAK void bluetooth_analytics_handle_connection_disconnection_event(
    uint8_t reason, const struct pbl_bt_remote_version_info *vers_info) {
}

PBL_WEAK void bluetooth_analytics_handle_put_bytes_stats(
    bool successful, uint8_t type, uint32_t total_size, uint32_t elapsed_time_ms,
    const struct pbl_bt_slave_conn_event_stats *orig_stats) {
}

PBL_WEAK void bluetooth_analytics_handle_get_bytes_stats(
    uint8_t type, uint32_t total_size, uint32_t elapsed_time_ms,
    const struct pbl_bt_slave_conn_event_stats *orig_stats) {
}

PBL_WEAK void bluetooth_analytics_ble_mic_error(uint32_t num_sequential_mic_errors) {
}
