/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define SINE_WAVE_SAMPLE_RATE 16000
#define SINE_WAVE_FREQUENCY 1000
#define SINE_WAVE_SAMPLES_PER_PERIOD 16
#define SINE_WAVE_TOTAL_SAMPLES 32

/* Stereo sine wave data (L, R, L, R, ...) */
extern int16_t sine_wave[SINE_WAVE_TOTAL_SAMPLES];
