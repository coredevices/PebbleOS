/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "services/common/accel_manager.h"

AccelManagerState* sys_accel_manager_data_subscribe(
    AccelSamplingRate rate, AccelDataReadyCallback data_cb, void* context,
    PebbleTask handler_task) {
  return NULL;
}

uint8_t sys_accel_manager_data_unsubscribe(AccelManagerState *state) {
  return 0;
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
