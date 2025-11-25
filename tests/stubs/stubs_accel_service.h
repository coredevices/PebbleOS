/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/accel_service_private.h"

void accel_service_state_init(AccelServiceState *state) {
}

void accel_data_service_subscribe(uint32_t samples_per_update, AccelDataHandler handler) {
}

void accel_raw_data_service_subscribe(uint32_t samples_per_update, AccelRawDataHandler handler) {
}

void accel_data_service_unsubscribe(void) {
}

AccelServiceState* accel_service_private_get_session(PebbleTask task) {
  return NULL;
}

void accel_service_cleanup_task_session(PebbleTask task) {
}
