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

#include <stdint.h>
#include <stdbool.h>

void bt_driver_reconnect_pause(void) {
}

void bt_driver_reconnect_resume(void) {
}

void bt_driver_reconnect_try_now(bool ignore_paused) {
}

void bt_driver_reconnect_reset_interval(void) {
}

void bt_driver_reconnect_notify_platform_bitfield(uint32_t platform_bitfield) {
  // Don't care
}
