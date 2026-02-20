/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_sine_wave.h"

/* Stereo sine wave data (L, R, L, R, ...) */
int16_t sine_wave[SINE_WAVE_TOTAL_SAMPLES] = {
         0,      0,   3134,   3134,   5791,   5791,   7567,   7567,   8191,   8191,   7567,   7567,   5791,   5791,   3134,   3134,
         0,      0,  -3134,  -3134,  -5791,  -5791,  -7567,  -7567,  -8191,  -8191,  -7567,  -7567,  -5791,  -5791,  -3134,  -3134
};
