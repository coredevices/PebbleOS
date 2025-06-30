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

#include <stdint.h>

#define SINE_WAVE_SAMPLE_RATE 16000
#define SINE_WAVE_FREQUENCY 1000
#define SINE_WAVE_SAMPLES_PER_PERIOD 16
#define SINE_WAVE_TOTAL_SAMPLES 32

/* Stereo sine wave data (L, R, L, R, ...) */
extern int16_t sine_wave[SINE_WAVE_TOTAL_SAMPLES];
