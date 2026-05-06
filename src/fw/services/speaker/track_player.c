/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "track_player.h"

#include <string.h>

static uint32_t s_sample_rate;

static uint32_t prv_sample_input_rate(SpeakerPcmFormat fmt) {
  return (fmt & 1) ? 16000 : 8000;
}

static uint32_t prv_sample_bytes_per(SpeakerPcmFormat fmt) {
  return (fmt & 2) ? 2 : 1;
}

static int16_t prv_decode_sample_at(const uint8_t *raw, uint32_t idx, bool is_16bit) {
  if (is_16bit) {
    return (int16_t)(raw[idx * 2] | (raw[idx * 2 + 1] << 8));
  }
  // 8-bit signed to 16-bit signed
  return (int16_t)((int8_t)raw[idx]) << 8;
}

static void prv_reset_sample_stride(TrackState *s, uint8_t target_note) {
  const SpeakerSample *smp = s->sample;
  uint32_t input_rate = prv_sample_input_rate(smp->format);
  uint32_t tgt_x256 = note_midi_freq_x256(target_note);
  uint32_t base_x256 = note_midi_freq_x256(smp->base_midi_note);

  s->sample_pos_q32 = 0;
  s->sample_exhausted = false;

  if (tgt_x256 == 0 || base_x256 == 0 || s->sample_num_input == 0) {
    // Rest or invalid base note: hold silence
    s->sample_stride_q32 = 0;
    s->sample_exhausted = true;
    return;
  }

  // stride = (input_rate / output_rate) * (freq_target / freq_base), 32.32 fixed.
  // Compute in two 16.16 multiplies to stay within uint64.
  uint64_t pitch_q16 = ((uint64_t)tgt_x256 << 16) / base_x256;
  uint64_t rate_q16 = ((uint64_t)input_rate << 16) / s_sample_rate;
  s->sample_stride_q32 = pitch_q16 * rate_q16;  // 32.32
}

static void prv_advance_to_note(TrackState *s) {
  if (s->current_note >= s->num_notes) {
    s->active = false;
    return;
  }

  const SpeakerNote *note = &s->notes[s->current_note];
  s->samples_remaining = ((uint32_t)note->duration_ms * s_sample_rate) / 1000;
  s->current_velocity = note->velocity;
  s->phase_acc = 0;

  if (s->sample != NULL) {
    prv_reset_sample_stride(s, note->midi_note);
  } else {
    s->current_waveform = note->waveform;
    s->phase_inc = note_phase_inc(note->midi_note, s_sample_rate);
  }
}

static int16_t prv_gen_sample_mode(TrackState *s) {
  if (s->sample_exhausted || s->sample_stride_q32 == 0) {
    return 0;
  }

  uint32_t idx = (uint32_t)(s->sample_pos_q32 >> 32);
  if (idx >= s->sample_num_input) {
    if (s->sample->loop) {
      uint64_t span = (uint64_t)s->sample_num_input << 32;
      s->sample_pos_q32 %= span;
      idx = (uint32_t)(s->sample_pos_q32 >> 32);
    } else {
      s->sample_exhausted = true;
      return 0;
    }
  }

  bool is_16bit = (s->sample->format & 2);
  int16_t raw_sample = prv_decode_sample_at((const uint8_t *)s->sample->data,
                                             idx, is_16bit);

  s->sample_pos_q32 += s->sample_stride_q32;

  int32_t out = raw_sample;
  if (s->current_velocity > 0 && s->current_velocity < 127) {
    out = (out * s->current_velocity) / 127;
  }
  return (int16_t)out;
}

static int16_t prv_gen_waveform_mode(TrackState *s) {
  if (s->phase_inc == 0) {
    return 0;
  }
  int16_t v = note_synth_sample(s->current_waveform, s->phase_acc, s->phase_inc,
                                s->current_velocity);
  s->phase_acc += s->phase_inc;
  return v;
}

void track_init(TrackState *s, const SpeakerTrack *track, uint32_t sample_rate) {
  memset(s, 0, sizeof(*s));
  s->notes = track->notes;
  s->num_notes = track->num_notes;
  s->sample = track->sample;
  s_sample_rate = sample_rate;
  s->active = (track->num_notes > 0);

  if (s->sample != NULL) {
    s->sample_num_input = s->sample->num_bytes / prv_sample_bytes_per(s->sample->format);
  }

  if (s->active) {
    prv_advance_to_note(s);
  }
}

uint32_t track_fill(TrackState *s, int16_t *out, uint32_t max_samples) {
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

    if (s->sample != NULL) {
      for (uint32_t i = 0; i < to_generate; i++) {
        out[written + i] = prv_gen_sample_mode(s);
      }
    } else {
      for (uint32_t i = 0; i < to_generate; i++) {
        out[written + i] = prv_gen_waveform_mode(s);
      }
    }

    written += to_generate;
    s->samples_remaining -= to_generate;
  }

  return written;
}

void track_deinit(TrackState *s) {
  memset(s, 0, sizeof(*s));
}
