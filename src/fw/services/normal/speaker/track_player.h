/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/normal/speaker/note_sequence.h"
#include "pbl/services/normal/speaker/track.h"

#include <stdbool.h>
#include <stdint.h>

//! Per-voice state for a polyphonic track. Plays either waveform-synth notes
//! (when SpeakerTrack::sample is NULL) or pitch-shifted PCM samples.
typedef struct {
  const SpeakerNote *notes;
  uint32_t num_notes;
  const SpeakerSample *sample;

  uint32_t current_note;
  uint32_t samples_remaining;   // at output sample rate

  // Waveform-mode state (used when sample == NULL)
  uint32_t phase_acc;           // 16.16
  uint32_t phase_inc;           // 16.16

  // Sample-mode state (used when sample != NULL)
  uint64_t sample_pos_q32;      // 32.32 position within the PCM source
  uint64_t sample_stride_q32;   // advance per output sample (32.32)
  uint32_t sample_num_input;    // total input samples in sample->data
  bool     sample_exhausted;    // reached end and loop=false

  uint8_t current_waveform;
  uint8_t current_velocity;
  bool active;
} TrackState;

//! Initialize a track voice.
//! @param s State to initialize
//! @param track Track configuration (notes, optional sample)
//! @param sample_rate Output sample rate in Hz
void track_init(TrackState *s, const SpeakerTrack *track, uint32_t sample_rate);

//! Generate up to max_samples 16-bit signed output samples for this voice.
//! @return Number of samples actually produced. 0 means the voice has finished.
uint32_t track_fill(TrackState *s, int16_t *out, uint32_t max_samples);

//! Clean up track state.
void track_deinit(TrackState *s);
