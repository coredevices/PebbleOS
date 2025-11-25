/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "comm/ble/gap_le_connection.h"
#include "comm/bt_conn_mgr.h"

void conn_mgr_set_ble_conn_response_time(
    GAPLEConnection *hdl, BtConsumer consumer, ResponseTimeState state,
    uint16_t max_period_secs) { }

void conn_mgr_set_ble_conn_response_time_ext(
    GAPLEConnection *hdl, BtConsumer consumer, ResponseTimeState state,
    uint16_t max_period_secs, ResponsivenessGrantedHandler granted_handler) { }


void conn_mgr_set_bt_classic_conn_response_time(
    struct Remote *remote, BtConsumer consumer, ResponseTimeState state,
    uint16_t max_period_secs) { }

void conn_mgr_set_bt_classic_conn_response_time_ext(
    struct Remote *remote, BtConsumer consumer, ResponseTimeState state,
    uint16_t max_period_secs, ResponsivenessGrantedHandler granted_handler) { }
