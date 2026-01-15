/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "fake_GAPAPI.h"

#include <stdbool.h>

// If GAPAPI is not available, define the types we need
#ifndef GAPAPI_AVAILABLE
typedef struct {
  uint16_t Connection_Interval;
  uint16_t Slave_Latency;
  uint16_t Supervision_Timeout;
} GAP_LE_Current_Connection_Parameters_t;

typedef struct {
  uint8_t Status;
  uint8_t Address_Type;
  uint8_t Address[6];
  uint8_t Peer_Address_Type;
  uint8_t Peer_Address[6];
  uint16_t Connection_Interval;
  uint16_t Slave_Latency;
  uint16_t Supervision_Timeout;
  uint8_t Role;
  uint8_t Master_Clock_Accuracy;
} GAP_LE_Connection_Complete_Event_Data_t;
#endif

void bluetooth_analytics_get_param_averages(uint16_t *params) {
}

void bluetooth_analytics_handle_connection_params_update(
                                             const GAP_LE_Current_Connection_Parameters_t *params) {
}

void bluetooth_analytics_handle_connect(unsigned int stack_id,
                                             const GAP_LE_Connection_Complete_Event_Data_t *event) {
}

void bluetooth_analytics_handle_disconnect(bool local_is_master) {
}

void bluetooth_analytics_handle_encryption_change(void) {
}

void bluetooth_analytics_handle_no_intent_for_connection(void) {
}
