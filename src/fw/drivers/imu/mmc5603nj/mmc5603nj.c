/*
 * Copyright 2025 Matthew Wardrop
 * Copyright 2025 Bob Wei
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
 *
 */

#include "drivers/mag.h"

#include "mmc5603nj.h"

// MMC5603NJ entrypoints

void mmc5603nj_init(void) {}

// mag.h implementation

void mag_use(void) {}

void mag_start_sampling(void) {}

void mag_release(void) {}

MagReadStatus mag_read_data(MagData *data) {
    return MagReadCommunicationFail;
}
  
bool mag_change_sample_rate(MagSampleRate rate) {
    return false;
}
