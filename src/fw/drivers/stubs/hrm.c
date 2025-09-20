/*
 * Copyright 2025 Core Devices LLC
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

#include "hrm.h"

void hrm_init(HRMDevice *dev) {
}

void hrm_enable(HRMDevice *dev) {
    dev->state->enabled = true;
}

void hrm_disable(HRMDevice *dev) {
    dev->state->enabled = false;
}

bool hrm_is_enabled(HRMDevice *dev) {
    return dev->state->enabled;
}