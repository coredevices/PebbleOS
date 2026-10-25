/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <bluetooth/call_audio_cvsd.h>

#define FRAMES 100
#define N      (FRAMES * CALL_AUDIO_CVSD_SAMPLES)

static struct call_audio_cvsd s_tx;
static struct call_audio_cvsd s_rx;
static int16_t s_in[N];
static int16_t s_out[N];

// Encode and decode, a packet at a time, as on a call: the state carries over.
static void prv_round_trip(void) {
  call_audio_cvsd_init(&s_tx);
  call_audio_cvsd_init(&s_rx);
  for (int f = 0; f < FRAMES; f++) {
    uint32_t air[CALL_AUDIO_CVSD_BYTES / 4];
    call_audio_cvsd_encode(&s_tx, &s_in[f * CALL_AUDIO_CVSD_SAMPLES], (uint8_t *)air);
    call_audio_cvsd_decode(&s_rx, (const uint8_t *)air, &s_out[f * CALL_AUDIO_CVSD_SAMPLES]);
  }
}

// Signal to noise ratio of the output, as a sine of @p freq fitted to it (any delay and gain)
// over the second half (past the start-up), against what is left; and the sine's gain.
static double prv_tone_snr(double freq, double amplitude, double *gain) {
  for (int i = 0; i < N; i++) {
    s_in[i] = (int16_t)(amplitude * sin(2 * 3.14159265358979 * freq * i / 8000.0));
  }
  prv_round_trip();
  double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0, yy = 0;
  for (int i = N / 2; i < N; i++) {
    const double w = 2 * 3.14159265358979 * freq * i / 8000.0;
    const double s = sin(w), c = cos(w), y = s_out[i];
    ss += s * s, cc += c * c, sc += s * c, ys += y * s, yc += y * c, yy += y * y;
  }
  const double det = ss * cc - sc * sc;
  const double a = (ys * cc - yc * sc) / det, b = (yc * ss - ys * sc) / det;
  const double fit = a * a * ss + 2 * a * b * sc + b * b * cc;
  *gain = sqrt(a * a + b * b) / amplitude;
  return 10 * log10(fit / (yy - fit));
}

void test_call_audio_cvsd__tones_come_back(void) {
  const double freqs[] = {300, 1000, 2000, 3000};
  for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
    double gain;
    const double snr = prv_tone_snr(freqs[i], 8000, &gain);
    cl_assert_(snr > 15, "SNR below 15 dB");
    // The filters pass up to 3.4 kHz: 3 kHz comes back 3.3 dB down.
    cl_assert_(gain > 0.6 && gain < 1.15, "gain off");
  }
}

void test_call_audio_cvsd__silence_stays_quiet(void) {
  memset(s_in, 0, sizeof(s_in));
  prv_round_trip();
  int peak = 0;
  for (int i = N / 2; i < N; i++) {
    peak = fmax(peak, abs(s_out[i]));
  }
  cl_assert(peak < 200);
}
