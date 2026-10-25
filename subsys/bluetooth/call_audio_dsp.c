/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "call_audio_dsp.h"

#include <string.h>

#include <pbl/util/math.h>

bool call_audio_rx_header_ok(const uint8_t *pkt, size_t len) {
  return len >= CALL_AUDIO_RX_HDR_BYTES &&
         (pkt[0] == CALL_AUDIO_AIR_BYTES || pkt[0] == CALL_AUDIO_PCM_BYTES) &&
         len == (size_t)CALL_AUDIO_RX_HDR_BYTES + pkt[0] && pkt[1] <= 3;
}

// The half-band filter's taps, Q15 (summing to 1); every other one, but the middle, is zero.
static const int16_t s_halfband[CALL_AUDIO_HALFBAND_TAPS] = {
  -16,   0, 75,    0, -201, 0, 435,  0, -842, 0, 1571, 0, -3145, 0, 10316, 16384,
  10316, 0, -3145, 0, 1571, 0, -842, 0, 435,  0, -201, 0, 75,    0, -16,
};

#define HIST (CALL_AUDIO_HALFBAND_TAPS - 1)

static int16_t prv_saturate(int32_t v) {
  return (int16_t)(v > INT16_MAX ? INT16_MAX : (v < INT16_MIN ? INT16_MIN : v));
}

// Filter output ending at sample @p k of the history followed by @p in (k counted from the
// history's start, so k >= HIST), times @p gain.
static int16_t prv_fir_at(const struct call_audio_halfband *f, const int16_t *in, size_t k,
                          int32_t gain) {
  int64_t acc = 0;
  for (size_t j = 0; j < CALL_AUDIO_HALFBAND_TAPS; j++) {
    if (s_halfband[j] == 0) {
      continue;
    }
    const size_t at = k - j;
    const int16_t x = at < HIST ? f->hist[at] : in[at - HIST];
    acc += (int64_t)s_halfband[j] * x;
  }
  return prv_saturate((int32_t)((acc * gain + (1 << 14)) >> 15));
}

// Keep the last HIST samples of the history followed by @p in (@p count samples).
static void prv_shift(struct call_audio_halfband *f, const int16_t *in, size_t count) {
  if (count >= HIST) {
    memcpy(f->hist, &in[count - HIST], sizeof(f->hist));
    return;
  }
  memmove(f->hist, &f->hist[count], (HIST - count) * sizeof(int16_t));
  memcpy(&f->hist[HIST - count], in, count * sizeof(int16_t));
}

void call_audio_down2(struct call_audio_halfband *f, const int16_t *in, size_t count,
                      int16_t *out) {
  for (size_t i = 0; i + 1 < count; i += 2) {
    out[i / 2] = prv_fir_at(f, in, HIST + i + 1, 1);
  }
  prv_shift(f, in, count);
}

void call_audio_up2(struct call_audio_halfband *f, const int16_t *in, size_t count, int16_t *out) {
  // Zero-stuffed: each input sample followed by a zero, the gain of 2 making up for them.
  for (size_t i = 0; i < count; i++) {
    int16_t pair[2] = {in[i], 0};
    for (int p = 0; p < 2; p++) {
      out[2 * i + p] = prv_fir_at(f, &pair[p], HIST, 2);
      prv_shift(f, &pair[p], 1);
    }
  }
}

void call_audio_agc_init(struct call_audio_agc *a) {
  *a = (struct call_audio_agc){
    .gain_q8 = 256,
    .quiet_q15 = 32768,
    .last_q8 = 256,
  };
}

void call_audio_agc_run(struct call_audio_agc *a, int16_t *samples, size_t count) {
  if (count == 0) {
    return;
  }
  // High-pass: y = x - x[-1] + 0.9 y[-1] (Q15 pole), about 135 Hz at 8 kHz.
  uint64_t sum_sq = 0;
  uint32_t peak = 1;
  for (size_t i = 0; i < count; i++) {
    const int32_t x = samples[i];
    const int32_t y = x - a->hp_x + (int32_t)(((int64_t)a->hp_y * 29491) >> 15);
    a->hp_x = x;
    a->hp_y = y;
    samples[i] = prv_saturate(y);
    const int32_t v = samples[i];
    sum_sq += (uint64_t)(v * v);
    const uint32_t mag = (uint32_t)(v < 0 ? -v : v);
    peak = mag > peak ? mag : peak;
  }
  const uint32_t rms = (uint32_t)integer_sqrt((int64_t)(sum_sq / count));

  // The noise floor: the first frame's, then down to a quieter frame at once, up slowly. Not
  // from a held frame (the far end's echo).
  if (!a->hold) {
    if (a->floor_q8 == 0) {
      // Not the level of someone already talking.
      a->floor_q8 = MIN(rms, CALL_AUDIO_AGC_FLOOR_FIRST) << 8;
    }
    if ((rms << 8) < a->floor_q8) {
      a->floor_q8 = rms << 8;
    } else {
      a->floor_q8 += a->floor_q8 / 256 + 1;
    }
    // Not below what it can rise from in a few seconds.
    a->floor_q8 = MAX(a->floor_q8, CALL_AUDIO_AGC_FLOOR_MIN << 8);
  }
  const bool speech =
      rms >= CALL_AUDIO_AGC_GATE && (rms << 8) > CALL_AUDIO_AGC_SPEECH_X * a->floor_q8;

  if (speech && !a->hold) {
    uint32_t want = CALL_AUDIO_AGC_TARGET * 256U / rms;
    want = CLIP(want, CALL_AUDIO_AGC_MIN_Q8, CALL_AUDIO_AGC_MAX_Q8);
    if (want > a->gain_q8) {
      // 0.5 dB up.
      const uint32_t up = a->gain_q8 * 271 / 256;
      a->gain_q8 = up > want ? want : up;
    } else {
      // 3 dB down.
      const uint32_t down = a->gain_q8 * 181 / 256;
      a->gain_q8 = down < want ? want : down;
    }
  }
  // Whatever the frame is, its peak must not clip: down to that at once. A held frame's only
  // for itself: the echo's peaks do not set the gain for the user's voice.
  bool limited = false;
  uint32_t gain_q8 = a->gain_q8;
  if (gain_q8 > CALL_AUDIO_AGC_MIN_Q8 && gain_q8 * peak > CALL_AUDIO_AGC_PEAK * 256U) {
    gain_q8 = MAX(CALL_AUDIO_AGC_PEAK * 256U / peak, CALL_AUDIO_AGC_MIN_Q8);
    limited = true;
    if (!a->hold) {
      a->gain_q8 = gain_q8;
    }
  }

  // Between words, down to the quiet level.
  if (speech) {
    a->since_speech = 0;
    a->quiet_q15 = 32768;
  } else if (++a->since_speech > CALL_AUDIO_AGC_HANG) {
    // 1.5 dB down.
    const int32_t down = (int32_t)((a->quiet_q15 * 27554) >> 15);
    a->quiet_q15 = down < CALL_AUDIO_AGC_QUIET_Q15 ? CALL_AUDIO_AGC_QUIET_Q15 : down;
  }

  // The total gain, ramped from the last frame's (but for a peak's limit, at once).
  const int32_t to = (int32_t)(((uint64_t)gain_q8 * (uint32_t)a->quiet_q15) >> 15);
  const int32_t from = limited ? to : (int32_t)a->last_q8;
  for (size_t i = 0; i < count; i++) {
    const int32_t g = from + (to - from) * (int32_t)(i + 1) / (int32_t)count;
    samples[i] = prv_saturate((int32_t)(((int64_t)samples[i] * g) >> 8));
  }
  a->last_q8 = (uint32_t)to;
}

void call_audio_es_init(struct call_audio_es *es) {
  *es = (struct call_audio_es){.gain_q15 = 32768};
}

void call_audio_es_far(struct call_audio_es *es, const int16_t *samples, size_t count) {
  for (size_t i = 0; i < count; i++) {
    const int32_t mag = samples[i] < 0 ? -samples[i] : samples[i];
    // Peak follower, falling by 1/256 a sample (about 30 ms at 8 kHz).
    es->far_env = mag > es->far_env ? mag : es->far_env - es->far_env / 256;
  }
  if (es->far_env > CALL_AUDIO_ES_FAR_ON) {
    es->hang = CALL_AUDIO_ES_HANG;
  }
}

bool call_audio_es_far_active(const struct call_audio_es *es) {
  return es->hang > 0;
}

void call_audio_es_near(struct call_audio_es *es, int16_t *samples, size_t count) {
  const int32_t target = es->hang > 0 ? CALL_AUDIO_ES_DUCK_Q15 : 32768;
  // Down over 40 samples (5 ms), up over 320 (40 ms).
  const int32_t down = (32768 - CALL_AUDIO_ES_DUCK_Q15) / 40;
  const int32_t up = (32768 - CALL_AUDIO_ES_DUCK_Q15) / 320;
  for (size_t i = 0; i < count; i++) {
    if (es->gain_q15 > target) {
      es->gain_q15 = es->gain_q15 - down < target ? target : es->gain_q15 - down;
    } else if (es->gain_q15 < target) {
      es->gain_q15 = es->gain_q15 + up > target ? target : es->gain_q15 + up;
    }
    samples[i] = (int16_t)((samples[i] * es->gain_q15) >> 15);
  }
  es->hang = es->hang > count ? es->hang - (uint32_t)count : 0;
}

int call_audio_agc_gain_db10(const struct call_audio_agc *a) {
  // 20 log10(g / 256), in tenths of a dB: about 60.2 per doubling, interpolated.
  int db10 = 0;
  uint32_t g = a->gain_q8;
  while (g >= 512) {
    g >>= 1;
    db10 += 60;
  }
  while (g < 256) {
    g <<= 1;
    db10 -= 60;
  }
  return db10 + (int)((g - 256) * 60 / 256);
}

void call_audio_gain(int16_t *samples, size_t count, unsigned int shift) {
  for (size_t i = 0; i < count; i++) {
    samples[i] = prv_saturate((int32_t)samples[i] * (1 << shift));
  }
}

static uint32_t prv_abs_diff(int16_t a, int16_t b) {
  const int32_t d = (int32_t)a - b;
  return (uint32_t)(d < 0 ? -d : d);
}

void call_audio_steps_add(struct call_audio_steps *s, const int16_t *samples, size_t count) {
  if (count == 0) {
    return;
  }
  if (s->have_last) {
    s->edge_sum += prv_abs_diff(samples[0], s->last);
    s->edges++;
  }
  for (size_t i = 1; i < count; i++) {
    s->inner_sum += prv_abs_diff(samples[i], samples[i - 1]);
  }
  s->inners += (uint32_t)(count - 1);
  s->last = samples[count - 1];
  s->have_last = true;
}

void call_audio_steps_take(struct call_audio_steps *s, uint16_t *edge, uint16_t *inner) {
  const uint64_t e = s->edges ? s->edge_sum / s->edges : 0;
  const uint64_t n = s->inners ? s->inner_sum / s->inners : 0;
  *edge = (uint16_t)(e > UINT16_MAX ? UINT16_MAX : e);
  *inner = (uint16_t)(n > UINT16_MAX ? UINT16_MAX : n);
  s->edge_sum = s->inner_sum = 0;
  s->edges = s->inners = 0;
}

void call_audio_level_add(struct call_audio_level *l, const int16_t *samples, size_t count) {
  for (size_t i = 0; i < count; i++) {
    l->sum_sq += (uint64_t)((int32_t)samples[i] * (int32_t)samples[i]);
    l->sum += samples[i];
  }
  l->count += count;
}

void call_audio_level_take(struct call_audio_level *l, uint16_t *rms, int16_t *mean) {
  if (l->count == 0) {
    *rms = 0;
    *mean = 0;
    return;
  }
  *rms = (uint16_t)integer_sqrt((int64_t)(l->sum_sq / l->count));
  *mean = (int16_t)(l->sum / (int64_t)l->count);
  *l = (struct call_audio_level){};
}

enum call_audio_rx_action call_audio_rx_decide(uint32_t buffered_bytes, uint32_t gap_ms) {
  if (buffered_bytes > CALL_AUDIO_RX_MAX_BUFFERED) {
    return CALL_AUDIO_RX_DROP;
  }
  if (buffered_bytes == 0 && gap_ms > CALL_AUDIO_RX_GAP_MS) {
    return CALL_AUDIO_RX_FILL_THEN_WRITE;
  }
  return CALL_AUDIO_RX_WRITE;
}
