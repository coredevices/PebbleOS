/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

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
typedef struct PBL_PACKED {
  uint8_t midi_note;
  uint8_t waveform;
  uint16_t duration_ms;
  uint8_t velocity;
  uint8_t reserved;
} SpeakerNote;

/**
 * @defgroup services_speaker_note_sequence Note synthesis
 * @ingroup services_speaker
 * @brief Synthesizes PCM from @c SpeakerNote sequences.
 *
 * Also provides the waveform primitives the track player uses for its voices. These are kernel
 * internals, not part of the app SDK.
 * @{
 */

/** @brief Playback state of a note sequence. */
typedef struct {
  /** Notes being played, not owned. */
  const SpeakerNote *notes;
  /** Number of entries in @ref notes. */
  uint32_t num_notes;
  /** Index of the note being played. */
  uint32_t current_note;
  /** Samples left in the current note. */
  uint32_t samples_remaining;
  /** 16.16 fixed-point phase accumulator. */
  uint32_t phase_acc;
  /** Per-sample phase increment, 0 for a rest. */
  uint32_t phase_inc;
  /** Waveform of the current note, a @c SpeakerWaveform. */
  uint8_t current_waveform;
  /** Velocity of the current note. */
  uint8_t current_velocity;
  /** Whether notes remain to be played. */
  bool active;
} NoteSequenceState;

/**
 * @brief Initialize a note sequence player.
 *
 * The sample rate is kept in a single global, so all sequences played at once must share it.
 *
 * @param[out] s State to initialize.
 * @param notes Notes to play. Must stay valid until playback ends.
 * @param count Number of entries in @p notes.
 * @param sample_rate Output sample rate in Hz (e.g. 16000).
 */
void note_seq_init(NoteSequenceState *s, const SpeakerNote *notes, uint32_t count,
                   uint32_t sample_rate);

/**
 * @brief Fill an output buffer with synthesized PCM samples.
 *
 * @param s Note sequence state.
 * @param[out] out Output buffer for signed 16-bit PCM samples.
 * @param max_samples Maximum number of samples to generate.
 * @return Number of samples written, 0 once the sequence is done.
 */
uint32_t note_seq_fill(NoteSequenceState *s, int16_t *out, uint32_t max_samples);

/**
 * @brief Clean up note sequence state.
 *
 * @param s State to clear.
 */
void note_seq_deinit(NoteSequenceState *s);

/**
 * @brief Compute the per-sample phase increment of a MIDI note.
 *
 * @param midi_note MIDI note number.
 * @param sample_rate Output sample rate in Hz.
 * @return 16.16 fixed-point phase increment per output sample, 0 for a rest (note 0) or
 *         out-of-range values.
 */
uint32_t note_phase_inc(uint8_t midi_note, uint32_t sample_rate);

/**
 * @brief Synthesize one PCM sample of a waveform.
 *
 * Waveforms are normalized to roughly equal loudness. The square wave applies PolyBLEP
 * anti-aliasing at its discontinuities, using @p phase_inc.
 *
 * @param waveform @c SpeakerWaveform value; unknown values produce silence.
 * @param phase_acc 16.16 fixed-point phase accumulator.
 * @param phase_inc Per-sample increment of @p phase_acc.
 * @param velocity Amplitude scale, 1 to 127; 0 means no per-sample scaling (master volume is
 *                 applied downstream).
 * @return Signed 16-bit sample.
 */
int16_t note_synth_sample(uint8_t waveform, uint32_t phase_acc, uint32_t phase_inc,
                          uint8_t velocity);

/**
 * @brief Get the frequency of a MIDI note.
 *
 * Used by the track player to compute sample playback pitch ratios.
 *
 * @param midi_note MIDI note number.
 * @return Frequency in 16.8 fixed-point Hz (i.e. Hz * 256), 0 if out of range.
 */
uint32_t note_midi_freq_x256(uint8_t midi_note);

/** @} */
