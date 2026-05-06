/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/speaker/note_sequence.h"

#include <string.h>

// 256-entry sine wave lookup table (one full cycle, 16-bit signed amplitude)
// Values represent sin(2*pi*i/256) * 32767
static const int16_t s_sine_lut[256] = {
      0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
   6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
  12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
  18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
  23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
  27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
  30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
  32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
  32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
  32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
  30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
  27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
  23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
  18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
  12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
   6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
      0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
  -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
 -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
 -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
 -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
 -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
 -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
 -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
 -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
 -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
 -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
 -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
 -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
 -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
 -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
  -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

// MIDI note number to frequency (Hz * 256) lookup table for notes 0-127
// Stored as 16.8 fixed-point: freq_hz_x256 = round(440 * 2^((note-69)/12) * 256)
// We use uint32_t to hold values for higher notes
static const uint32_t s_midi_freq_x256[128] = {
      2093,     2217,     2349,     2489,     2637,     2794,
      2960,     3136,     3322,     3520,     3729,     3951,
      4186,     4435,     4699,     4978,     5274,     5588,
      5920,     6272,     6645,     7040,     7459,     7902,
      8372,     8870,     9397,     9956,    10548,    11175,
     11840,    12544,    13290,    14080,    14917,    15804,
     16744,    17740,    18795,    19912,    21096,    22351,
     23680,    25088,    26580,    28160,    29834,    31609,
     33488,    35479,    37589,    39824,    42192,    44701,
     47359,    50175,    53159,    56320,    59669,    63217,
     66976,    70959,    75178,    79649,    84385,    89402,
     94719,   100351,   106318,   112640,   119338,   126434,
    133952,   141918,   150356,   159297,   168769,   178805,
    189437,   200702,   212636,   225280,   238676,   252868,
    267905,   283835,   300713,   318594,   337539,   357610,
    378874,   401403,   425272,   450560,   477352,   505737,
    535809,   567670,   601425,   637188,   675077,   715219,
    757749,   802807,   850544,   901120,   954703,  1011473,
   1071618,  1135340,  1202851,  1274376,  1350154,  1430439,
   1515497,  1605613,  1701088,  1802240,  1909407,  2022946,
   2143237,  2270680,  2405702,  2548752,  2700309,  2860878,
   3030994,  3211227,
};

static uint32_t s_sample_rate;

uint32_t note_midi_freq_x256(uint8_t midi_note) {
  if (midi_note >= 128) {
    return 0;
  }
  return s_midi_freq_x256[midi_note];
}

uint32_t note_phase_inc(uint8_t midi_note, uint32_t sample_rate) {
  if (midi_note == 0 || midi_note > 127 || sample_rate == 0) {
    return 0;
  }
  // phase_inc = freq_x256 * 256 / sample_rate (16.16 per output sample)
  return (s_midi_freq_x256[midi_note] << 8) / sample_rate;
}

//! PolyBLEP correction value at phase t with per-sample phase increment dt.
//! Both are in 16.0 phase units where one cycle = 65536. Returns a Q15 value
//! (32768 = 1.0). Caller must ensure dt > 0 and dt < 32768. For phases more
//! than dt away from any discontinuity the correction is zero.
static int32_t prv_polyblep_q15(uint32_t t, uint32_t dt) {
  if (t < dt) {
    // Trailing side of the discontinuity: tp = t/dt in [0, 1)
    // Correction = 2*tp - tp^2 - 1, in [-1, 0)
    uint32_t tp = (t << 15) / dt;  // Q15 in [0, 32768)
    int32_t tp_s = (int32_t)tp;
    int32_t tp_sq = (tp_s * tp_s) >> 15;
    return (tp_s << 1) - tp_sq - 32768;
  }
  if (t + dt > 65536) {
    // Leading side of the next discontinuity: u = (1 - t)/dt in (0, 1]
    // Correction = (1 - u)^2, in [0, 1)
    uint32_t u = ((65536u - t) << 15) / dt;  // Q15 in (0, 32768]
    int32_t one_minus_u = 32768 - (int32_t)u;
    return (one_minus_u * one_minus_u) >> 15;
  }
  return 0;
}

int16_t note_synth_sample(uint8_t waveform, uint32_t phase_acc, uint32_t phase_inc,
                          uint8_t velocity) {
  uint8_t phase_idx = (phase_acc >> 8) & 0xFF;
  int32_t sample = 0;

  switch (waveform) {
    case SpeakerWaveformSine:
      sample = s_sine_lut[phase_idx];
      break;

    case SpeakerWaveformSquare: {
      // Naive square plus PolyBLEP corrections at the rising edge (t=0) and
      // falling edge (t=0.5) to suppress the aliasing that otherwise mangles
      // every frequency where the half-period isn't an integer number of
      // output samples.
      uint32_t t = phase_acc & 0xFFFFu;
      uint32_t dt = phase_inc & 0xFFFFu;
      sample = (t < 32768u) ? 32767 : -32767;
      // Correction is meaningless at and above Nyquist; skip rather than
      // generate nonsense when the caller asked for an out-of-range frequency.
      if (dt > 0 && dt < 32768u) {
        sample += prv_polyblep_q15(t, dt);
        sample -= prv_polyblep_q15(t ^ 0x8000u, dt);
        if (sample > 32767) sample = 32767;
        else if (sample < -32768) sample = -32768;
      }
      break;
    }

    case SpeakerWaveformTriangle:
      if (phase_idx < 64) {
        sample = (int32_t)phase_idx * 32767 / 64;
      } else if (phase_idx < 192) {
        sample = (128 - (int32_t)phase_idx) * 32767 / 64;
      } else {
        sample = ((int32_t)phase_idx - 256) * 32767 / 64;
      }
      break;

    case SpeakerWaveformSawtooth:
      sample = ((int32_t)phase_idx - 128) * 32767 / 128;
      break;

    default:
      sample = 0;
      break;
  }

  if (velocity > 0 && velocity < 127) {
    sample = (sample * velocity) / 127;
  }

  return (int16_t)sample;
}

static void prv_advance_to_note(NoteSequenceState *s) {
  if (s->current_note >= s->num_notes) {
    s->active = false;
    return;
  }

  const SpeakerNote *note = &s->notes[s->current_note];
  s->samples_remaining = ((uint32_t)note->duration_ms * s_sample_rate) / 1000;
  s->current_waveform = note->waveform;
  s->current_velocity = note->velocity;
  s->phase_acc = 0;
  s->phase_inc = note_phase_inc(note->midi_note, s_sample_rate);
}

static int16_t prv_generate_sample(NoteSequenceState *s) {
  if (s->phase_inc == 0) {
    return 0;
  }
  int16_t sample = note_synth_sample(s->current_waveform, s->phase_acc,
                                     s->phase_inc, s->current_velocity);
  s->phase_acc += s->phase_inc;
  return sample;
}

void note_seq_init(NoteSequenceState *s, const SpeakerNote *notes, uint32_t count,
                   uint32_t sample_rate) {
  memset(s, 0, sizeof(*s));
  s->notes = notes;
  s->num_notes = count;
  s->active = (count > 0);
  s_sample_rate = sample_rate;

  if (s->active) {
    prv_advance_to_note(s);
  }
}

uint32_t note_seq_fill(NoteSequenceState *s, int16_t *out, uint32_t max_samples) {
  uint32_t written = 0;

  while (written < max_samples && s->active) {
    if (s->samples_remaining == 0) {
      s->current_note++;
      prv_advance_to_note(s);
      continue;
    }

    uint32_t to_generate = max_samples - written;
    if (to_generate > s->samples_remaining) {
      to_generate = s->samples_remaining;
    }

    for (uint32_t i = 0; i < to_generate; i++) {
      out[written + i] = prv_generate_sample(s);
    }

    written += to_generate;
    s->samples_remaining -= to_generate;
  }

  return written;
}

void note_seq_deinit(NoteSequenceState *s) {
  memset(s, 0, sizeof(*s));
}
