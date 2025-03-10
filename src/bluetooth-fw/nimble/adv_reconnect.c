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

const GAPLEAdvertisingJobTerm *bt_driver_adv_reconnect_get_job_terms(size_t *num_terms_out) {
  static const GAPLEAdvertisingJobTerm terms[] = {
      // burst to attempt to reconnect quickly...
      [0] =
          {
              .duration_secs = 25,
              .min_interval_slots = 244,  // one slot is 625us; Apple recommends 152.5ms interval,
                                          // though they really say 20ms
              .max_interval_slots = 256,  // 160.0ms
          },

      // ... otherwise, if we don't make it in time, go back and forth between bursting and low duty
      // cycle advertising
      [1] =
          {
              .duration_secs = 5,
              .min_interval_slots = 244,  // 152.5 ms
              .max_interval_slots = 256,  // 160.0 ms
          },
      [2] =
          {
              .duration_secs = 20,
              .min_interval_slots =
                  1636,  // 1022.5 ms is also an Apple-recommended number
                         // (https://stackoverflow.com/questions/34157934/ios-background-bluetooth-low-energy-ble-scanning-rules)
              .max_interval_slots = 1656,  // 1035.0 ms
          },
      [3] =
          {
              .duration_secs = GAPLE_ADVERTISING_DURATION_LOOP_AROUND,
              .loop_around_index = 1,
          },
  };
  *num_terms_out = sizeof(terms) / sizeof(terms[0]);
  return terms;
}
