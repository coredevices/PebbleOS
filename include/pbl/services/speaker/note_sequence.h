/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SpeakerWaveformSine = 0,
  SpeakerWaveformSquare,
  SpeakerWaveformTriangle,
  SpeakerWaveformSawtooth,
  SpeakerWaveformCount
} SpeakerWaveform;

//! A single note in a sequence.
//! midi_note: MIDI note number (0-127, 60=C4). 0 = rest (silence).
//! waveform: SpeakerWaveform value.
//! duration_ms: Note duration in ms (max 10000).
//! velocity: Volume 0-127 (0 = use global volume).
typedef struct PACKED {
  uint8_t midi_note;
  uint8_t waveform;
  uint16_t duration_ms;
  uint8_t velocity;
  uint8_t reserved;
} SpeakerNote;

typedef struct {
  const SpeakerNote *notes;
  uint32_t num_notes;
  uint32_t current_note;
  uint32_t samples_remaining;
  uint32_t phase_acc;       // 16.16 fixed-point phase accumulator
  uint32_t phase_inc;       // per-sample phase increment
  uint8_t current_waveform;
  uint8_t current_velocity;
  bool active;
} NoteSequenceState;

//! Initialize a note sequence player.
//! @param s State to initialize
//! @param notes Array of notes to play
//! @param count Number of notes
//! @param sample_rate Output sample rate in Hz (e.g. 16000)
void note_seq_init(NoteSequenceState *s, const SpeakerNote *notes, uint32_t count,
                   uint32_t sample_rate);

//! Fill output buffer with synthesized PCM samples.
//! @param s Note sequence state
//! @param out Output buffer for 16-bit PCM samples
//! @param max_samples Maximum number of samples to generate
//! @return Number of samples actually written. 0 means sequence is done.
uint32_t note_seq_fill(NoteSequenceState *s, int16_t *out, uint32_t max_samples);

//! Clean up note sequence state.
void note_seq_deinit(NoteSequenceState *s);

//! @internal Shared synth primitives used by track_player for waveform voices.
//! These are not part of the public SDK.

//! Compute the 16.16 fixed-point phase increment per output sample for a
//! MIDI note at the given sample rate. Returns 0 for rests (midi_note == 0) or
//! out-of-range values.
uint32_t note_phase_inc(uint8_t midi_note, uint32_t sample_rate);

//! Synthesize one 16-bit signed PCM sample for a given waveform at the given
//! 16.16 phase accumulator value. phase_inc is the per-sample increment for
//! the same accumulator and is used by the square wave generator to apply
//! PolyBLEP anti-aliasing at the discontinuities. velocity scales the
//! amplitude (1..127); velocity 0 means no per-sample scaling (master volume
//! is applied downstream).
int16_t note_synth_sample(uint8_t waveform, uint32_t phase_acc, uint32_t phase_inc,
                          uint8_t velocity);

//! Return the MIDI-note frequency in 16.8 fixed-point Hz (i.e. freq_hz * 256).
//! Used by track_player to compute sample-playback pitch ratios.
uint32_t note_midi_freq_x256(uint8_t midi_note);
