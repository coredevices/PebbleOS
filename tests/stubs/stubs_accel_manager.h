/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/accel_manager.h"

AccelManagerState* sys_accel_manager_data_subscribe(
    AccelSamplingRate rate, AccelDataReadyCallback data_cb, void* context,
    PebbleTask handler_task) {
  return NULL;
}

bool sys_accel_manager_data_unsubscribe(AccelManagerState *state) {
  return false;
}

uint32_t accel_manager_set_jitterfree_sampling_rate(AccelManagerState *state, uint32_t min_rate_mhz) {
  return 0;
}

int sys_accel_manager_set_sample_buffer(AccelManagerState *state, AccelRawData *buffer,
                                        uint32_t samples_per_update) {
  return 0;
}

uint32_t sys_accel_manager_get_num_samples(AccelManagerState *state, uint64_t *timestamp_ms) {
  return 0;
}

bool sys_accel_manager_consume_samples(AccelManagerState *state, uint32_t samples) {
  return false;
}
