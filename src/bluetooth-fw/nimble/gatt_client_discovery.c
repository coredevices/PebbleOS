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

#include <bluetooth/gatt.h>

// -------------------------------------------------------------------------------------------------
// Gatt Client Discovery API calls

BTErrno bt_driver_gatt_start_discovery_range(const GAPLEConnection *connection,
                                             const ATTHandleRange *data) {
  return 0;
}

BTErrno bt_driver_gatt_stop_discovery(GAPLEConnection *connection) { return 0; }

void bt_driver_gatt_handle_discovery_abandoned(void) {}
