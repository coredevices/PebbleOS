/*
 * Copyright 2025 Google LLC
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

#include <bluetooth/adv_reconnect.h>

#include "comm/ble/gap_le_advert.h"

const GAPLEAdvertisingJobTerm *bt_driver_adv_reconnect_get_job_terms(size_t *num_terms_out) {
  *num_terms_out = 0;
  return NULL;
}
