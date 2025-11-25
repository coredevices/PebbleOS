/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "GAPAPI.h"

#include <stdbool.h>

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
