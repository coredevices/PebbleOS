/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <math.h>
#include <string.h>

#include <bluetooth/call_audio_dsp.h>

void test_call_audio_dsp__header(void) {
  uint8_t pkt[CALL_AUDIO_RX_BYTES] = {120, 0};
  // PCM, as the LCPU converts it: 120 bytes, packet status 0 to 3.
  cl_assert(call_audio_rx_header_ok(pkt, 124));
  pkt[1] = 3;
  cl_assert(call_audio_rx_header_ok(pkt, 124));
  pkt[1] = 4;
  cl_assert(!call_audio_rx_header_ok(pkt, 124));
  // The air bits, as it hands them over: 60 bytes.
  pkt[0] = 60, pkt[1] = 0;
  cl_assert(call_audio_rx_header_ok(pkt, 64));
  // A length other than what was taken, or than the two formats.
  cl_assert(!call_audio_rx_header_ok(pkt, 124));
  pkt[0] = 30;
  cl_assert(!call_audio_rx_header_ok(pkt, 34));
  cl_assert(!call_audio_rx_header_ok(pkt, 3));
}

void test_call_audio_dsp__steps(void) {
  struct call_audio_steps s = {};
  const int16_t a[] = {0, 10, 20, 30};
  const int16_t b[] = {1000, 1010, 1020, 1030};
  call_audio_steps_add(&s, a, 4);
  call_audio_steps_add(&s, b, 4);
  uint16_t edge, inner;
  call_audio_steps_take(&s, &edge, &inner);
  // One edge (30 to 1000), six inner steps of 10.
  cl_assert_equal_i(edge, 970);
  cl_assert_equal_i(inner, 10);
  // The edge to the next packet is still counted after a take.
  call_audio_steps_add(&s, a, 4);
  call_audio_steps_take(&s, &edge, &inner);
  cl_assert_equal_i(edge, 1030);
}

// A tone through the half-band filter, halving or doubling the rate, a frame at a time: its
// level after the start-up, in dB against its level going in.
static double prv_halfband_db(double freq_hz, bool up) {
  enum {
    FRAMES = 8,
    IN = 120
  };
  static int16_t in[FRAMES * IN], out[FRAMES * IN * 2];
  const double in_rate = up ? 8000.0 : 16000.0;
  for (int i = 0; i < FRAMES * IN; i++) {
    in[i] = (int16_t)(10000 * sin(2 * 3.14159265358979 * freq_hz * i / in_rate));
  }
  struct call_audio_halfband f = {};
  const int out_per_frame = up ? IN * 2 : IN / 2;
  for (int k = 0; k < FRAMES; k++) {
    if (up) {
      call_audio_up2(&f, &in[k * IN], IN, &out[k * out_per_frame]);
    } else {
      call_audio_down2(&f, &in[k * IN], IN, &out[k * out_per_frame]);
    }
  }
  struct call_audio_level level = {};
  const int skip = out_per_frame * 2;
  call_audio_level_add(&level, &out[skip], (size_t)(FRAMES * out_per_frame - skip));
  uint16_t rms;
  int16_t mean;
  call_audio_level_take(&level, &rms, &mean);
  return 20 * log10((rms + 0.5) / 7071.0);
}

void test_call_audio_dsp__halving(void) {
  // Passed up to 3.4 kHz, stopped from 4.6 kHz on (folding back to 3.4 kHz and below).
  cl_assert(fabs(prv_halfband_db(1000, false)) < 0.2);
  cl_assert(fabs(prv_halfband_db(3000, false)) < 0.3);
  cl_assert(prv_halfband_db(3400, false) > -1.0);
  cl_assert(prv_halfband_db(4600, false) < -20);
  cl_assert(prv_halfband_db(6000, false) < -55);
}

void test_call_audio_dsp__doubling(void) {
  // The tone at its level; its image above 4 kHz (8 kHz - f) is what the level would add.
  cl_assert(fabs(prv_halfband_db(1000, true)) < 0.2);
  cl_assert(fabs(prv_halfband_db(3000, true)) < 0.3);
}

void test_call_audio_dsp__doubling_image(void) {
  // A 1 kHz tone doubled: the output at 7 kHz (its image) is 70 dB down. Measured by
  // correlating with 7 kHz.
  enum {
    IN = 960
  };
  static int16_t in[IN], out[IN * 2];
  for (int i = 0; i < IN; i++) {
    in[i] = (int16_t)(10000 * sin(2 * 3.14159265358979 * 1000 * i / 8000.0));
  }
  struct call_audio_halfband f = {};
  call_audio_up2(&f, in, IN, out);
  double s7 = 0, c7 = 0, s1 = 0, c1 = 0;
  for (int i = 200; i < IN * 2; i++) {
    const double t = (double)i / 16000.0;
    s7 += out[i] * sin(2 * 3.14159265358979 * 7000 * t);
    c7 += out[i] * cos(2 * 3.14159265358979 * 7000 * t);
    s1 += out[i] * sin(2 * 3.14159265358979 * 1000 * t);
    c1 += out[i] * cos(2 * 3.14159265358979 * 1000 * t);
  }
  const double image_db = 10 * log10((s7 * s7 + c7 * c7) / (s1 * s1 + c1 * c1));
  cl_assert(image_db < -60);
}

void test_call_audio_dsp__level(void) {
  struct call_audio_level level = {};
  const int16_t a[] = {3, -3, 3, -3};
  const int16_t b[] = {100, 100};
  call_audio_level_add(&level, a, 4);
  uint16_t rms;
  int16_t mean;
  call_audio_level_take(&level, &rms, &mean);
  cl_assert_equal_i(rms, 3);
  cl_assert_equal_i(mean, 0);
  call_audio_level_add(&level, b, 2);
  call_audio_level_take(&level, &rms, &mean);
  cl_assert_equal_i(rms, 100);
  cl_assert_equal_i(mean, 100);
  // Nothing added: zeros.
  call_audio_level_take(&level, &rms, &mean);
  cl_assert_equal_i(rms, 0);
}

void test_call_audio_dsp__rx_decide(void) {
  cl_assert_equal_i(call_audio_rx_decide(0, 0), CALL_AUDIO_RX_WRITE);
  cl_assert_equal_i(call_audio_rx_decide(240, 8), CALL_AUDIO_RX_WRITE);
  // Too much queued: drop.
  cl_assert_equal_i(call_audio_rx_decide(CALL_AUDIO_RX_MAX_BUFFERED + 2, 7), CALL_AUDIO_RX_DROP);
  // Dry after a gap: silence first.
  cl_assert_equal_i(call_audio_rx_decide(0, 30), CALL_AUDIO_RX_FILL_THEN_WRITE);
  // A gap with some still queued is fine.
  cl_assert_equal_i(call_audio_rx_decide(120, 30), CALL_AUDIO_RX_WRITE);
}

void test_call_audio_dsp__gain_saturates(void) {
  int16_t s[] = {100, -100, 20000, -20000};
  call_audio_gain(s, 4, 2);
  cl_assert_equal_i(s[0], 400);
  cl_assert_equal_i(s[1], -400);
  cl_assert_equal_i(s[2], 32767);
  cl_assert_equal_i(s[3], -32768);
}

// A tone of @p amplitude through the automatic gain, a frame (120 samples, as the mic's) at a
// time for @p frames, talked as speech is: @p on frames of it, then @p off frames of
// silence (on 0: all of it); the output's RMS over the last frame of the tone.
#define FRAME 120
static uint16_t prv_agc_talk(struct call_audio_agc *a, double amplitude, int frames, int on,
                             int off) {
  int16_t pkt[FRAME];
  uint16_t rms = 0;
  for (int p = 0; p < frames; p++) {
    const bool talking = on == 0 || p % (on + off) < on;
    for (int i = 0; i < FRAME; i++) {
      pkt[i] =
          talking
              ? (int16_t)(amplitude * sin(2 * 3.14159265358979 * 500 * (p * FRAME + i) / 8000.0))
              : 0;
    }
    call_audio_agc_run(a, pkt, FRAME);
    if (talking) {
      struct call_audio_level level = {};
      call_audio_level_add(&level, pkt, FRAME);
      int16_t mean;
      call_audio_level_take(&level, &rms, &mean);
    }
  }
  return rms;
}

void test_call_audio_dsp__agc_brings_quiet_speech_up(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  // RMS 140 in, talked: 31 dB up to the target at 0.5 dB a frame of speech.
  const uint16_t rms = prv_agc_talk(&a, 200, 150, 20, 10);
  cl_assert(rms > CALL_AUDIO_AGC_TARGET * 9 / 10 && rms < CALL_AUDIO_AGC_TARGET * 11 / 10);
  cl_assert(call_audio_agc_gain_db10(&a) > 200);
}

void test_call_audio_dsp__agc_brings_loud_speech_down(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  // RMS 14000 in: down to the target, the codec's range (12 dB at most).
  const uint16_t rms = prv_agc_talk(&a, 20000, 60, 20, 10);
  cl_assert(rms > CALL_AUDIO_AGC_TARGET * 9 / 10 && rms < CALL_AUDIO_AGC_TARGET * 15 / 10);
}

void test_call_audio_dsp__agc_comes_down_fast(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  prv_agc_talk(&a, 200, 120, 20, 10);
  // Loud speech after quiet: from its first frame below the peak limit, and within 10 frames
  // (150 ms) at 3 dB each, near the target again.
  int16_t pkt[FRAME];
  for (int i = 0; i < FRAME; i++) {
    pkt[i] = (int16_t)(8000 * sin(2 * 3.14159265358979 * 500 * i / 8000.0));
  }
  call_audio_agc_run(&a, pkt, FRAME);
  for (int i = 0; i < FRAME; i++) {
    cl_assert(abs(pkt[i]) <= CALL_AUDIO_AGC_PEAK);
  }
  const uint16_t rms = prv_agc_talk(&a, 8000, 9, 0, 0);
  cl_assert(rms < CALL_AUDIO_AGC_TARGET * 12 / 10);
}

// Steady noise of RMS about @p rms (a fixed pseudo-random sequence) for @p frames; the output's
// RMS over the last frame.
static uint16_t prv_agc_noise(struct call_audio_agc *a, int rms, int frames) {
  int16_t pkt[FRAME];
  uint32_t seed = 1;
  uint16_t out = 0;
  for (int p = 0; p < frames; p++) {
    for (int i = 0; i < FRAME; i++) {
      seed = seed * 1103515245 + 12345;
      // Uniform in +-1.73 rms: that RMS.
      pkt[i] = (int16_t)((int32_t)((seed >> 16) % (2 * 173 * rms / 100 + 1)) - 173 * rms / 100);
    }
    call_audio_agc_run(a, pkt, FRAME);
    struct call_audio_level level = {};
    call_audio_level_add(&level, pkt, FRAME);
    int16_t mean;
    call_audio_level_take(&level, &out, &mean);
  }
  return out;
}

void test_call_audio_dsp__agc_does_not_bring_noise_up(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  // A room's noise for 3 s, nobody talking: the gain stays within 3 dB and the noise goes down.
  const uint16_t out = prv_agc_noise(&a, 140, 200);
  cl_assert(call_audio_agc_gain_db10(&a) < 30);
  cl_assert(out < 140 / 2);
}

void test_call_audio_dsp__agc_speech_over_noise(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  prv_agc_noise(&a, 60, 100);
  // Then speech well above it: brought to the target, at full level.
  const uint16_t rms = prv_agc_talk(&a, 400, 90, 20, 10);
  cl_assert(rms > CALL_AUDIO_AGC_TARGET * 8 / 10 && rms < CALL_AUDIO_AGC_TARGET * 12 / 10);
}

void test_call_audio_dsp__agc_keeps_peaks_from_clipping(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  // A click in quiet: its RMS asks for more gain than its peak allows.
  int16_t pkt[60];
  for (int p = 0; p < 60; p++) {
    for (int i = 0; i < 60; i++) {
      pkt[i] = (int16_t)(i == 30 ? 10000 : ((i & 1) ? 100 : -100));
    }
    call_audio_agc_run(&a, pkt, 60);
  }
  cl_assert(pkt[30] <= CALL_AUDIO_AGC_PEAK && pkt[30] > CALL_AUDIO_AGC_PEAK * 8 / 10);
}

static void prv_tone(int16_t *pkt, size_t n, int amplitude) {
  for (size_t i = 0; i < n; i++) {
    pkt[i] = (int16_t)((i % 8) < 4 ? amplitude : -amplitude);
  }
}

void test_call_audio_dsp__es_ducks_the_mic_while_the_far_end_talks(void) {
  struct call_audio_es es;
  call_audio_es_init(&es);
  int16_t far[60];
  int16_t near[120];
  // Nobody on the far end: the mic passes.
  prv_tone(near, 120, 4000);
  call_audio_es_near(&es, near, 120);
  cl_assert_equal_i(near[119], -4000);
  cl_assert(!call_audio_es_far_active(&es));

  // The far end talks: after a few ms the mic is down 18 dB.
  prv_tone(far, 60, 3000);
  call_audio_es_far(&es, far, 60);
  cl_assert(call_audio_es_far_active(&es));
  prv_tone(near, 120, 4000);
  call_audio_es_near(&es, near, 120);
  cl_assert(near[119] > -4000 / 7 && near[119] < 0);
}

void test_call_audio_dsp__es_lets_go_after_the_far_end(void) {
  struct call_audio_es es;
  call_audio_es_init(&es);
  int16_t far[60];
  int16_t near[120];
  prv_tone(far, 60, 3000);
  call_audio_es_far(&es, far, 60);
  // Quiet far end; within the hang (300 ms) the mic stays down, after it comes back.
  memset(far, 0, sizeof(far));
  for (int p = 0; p < 10; p++) {
    call_audio_es_far(&es, far, 60);
    prv_tone(near, 120, 4000);
    call_audio_es_near(&es, near, 120);
  }
  cl_assert(near[119] > -4000 / 7);
  for (int p = 0; p < 30; p++) {
    call_audio_es_far(&es, far, 60);
    prv_tone(near, 120, 4000);
    call_audio_es_near(&es, near, 120);
  }
  cl_assert(!call_audio_es_far_active(&es));
  cl_assert_equal_i(near[119], -4000);
}

void test_call_audio_dsp__agc_holds_while_told_to(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  a.hold = true;
  prv_agc_talk(&a, 200, 120, 20, 10);
  cl_assert_equal_i(call_audio_agc_gain_db10(&a), 0);
}

void test_call_audio_dsp__agc_echo_does_not_take_the_gain_down(void) {
  struct call_audio_agc a;
  call_audio_agc_init(&a);
  // The user's quiet voice brought up.
  prv_agc_talk(&a, 200, 150, 20, 10);
  const int gain = call_audio_agc_gain_db10(&a);
  cl_assert(gain > 200);
  // The far end's loud echo, held: kept from clipping, but only while it lasts.
  a.hold = true;
  int16_t pkt[FRAME];
  for (int p = 0; p < 50; p++) {
    prv_tone(pkt, FRAME, 20000);
    call_audio_agc_run(&a, pkt, FRAME);
    for (int i = 0; i < FRAME; i++) {
      cl_assert(pkt[i] <= CALL_AUDIO_AGC_PEAK && pkt[i] >= -CALL_AUDIO_AGC_PEAK);
    }
  }
  // The far end's hang (300 ms): quiet, still held.
  memset(pkt, 0, sizeof(pkt));
  for (int p = 0; p < 20; p++) {
    call_audio_agc_run(&a, pkt, FRAME);
  }
  cl_assert_equal_i(call_audio_agc_gain_db10(&a), gain);
  // The user again: as loud as before, at once.
  a.hold = false;
  const uint16_t rms = prv_agc_talk(&a, 200, 2, 0, 0);
  cl_assert(rms > CALL_AUDIO_AGC_TARGET * 8 / 10);
}
