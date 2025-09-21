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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  bool enabled;
  int32_t gain_q12;
  int32_t last_applied_gain_q12;
  uint32_t smoothed_level;
  uint16_t silence_run;
} VoiceAgcState;

void voice_agc_init(VoiceAgcState *agc);
void voice_agc_process_frame(VoiceAgcState *agc, int16_t *samples, size_t sample_count);
