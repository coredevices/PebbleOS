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

#include <bluetooth/init.h>
#include <stdlib.h>

#include "comm/bt_lock.h"
#include "kernel/event_loop.h"
#include "pebble_errors.h"
#include "system/logging.h"

void bt_driver_init(void) { bt_lock_init(); }

bool bt_driver_start(BTDriverConfig *config) { return true; }

void bt_driver_stop(void) { }

void bt_driver_power_down_controller_on_boot(void) {
  // no-op
}
