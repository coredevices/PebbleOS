/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/accel_service_private.h>

PBL_WEAK void accel_service_state_init(AccelServiceState *state) {
}

PBL_WEAK void accel_data_service_subscribe(uint32_t samples_per_update, AccelDataHandler handler) {
}

PBL_WEAK void accel_raw_data_service_subscribe(uint32_t samples_per_update,
                                               AccelRawDataHandler handler) {
}

PBL_WEAK void accel_data_service_unsubscribe(void) {
}

PBL_WEAK AccelServiceState *accel_service_private_get_session(PebbleTask task) {
  return nullptr;
}

PBL_WEAK void accel_service_cleanup_task_session(PebbleTask task) {
}
