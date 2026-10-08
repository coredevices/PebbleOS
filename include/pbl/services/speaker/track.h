/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/speaker/note_sequence.h>
#include <pbl/services/speaker/speaker_pcm_format.h>

#include <stdbool.h>
#include <stdint.h>

/** @brief A raw PCM sample that can be pitch-shifted when played by a track. */
typedef struct {
  /** Mono signed PCM in the given format. */
  const void *data;
  /** Size of data in bytes. */
  uint32_t num_bytes;
  /** Sample rate + bit depth (see SpeakerPcmFormat). */
  SpeakerPcmFormat format;
  /**
   * The MIDI note at which the sample plays unshifted (e.g. 60 = C4).
   * Notes above/below this value are produced by resampling.
   */
  uint8_t base_midi_note;
  /**
   * If true, the sample restarts from the beginning each time it runs out,
   * and keeps playing until the owning note's duration elapses.
   */
  bool loop;
} SpeakerSample;

/**
 * @brief A single monophonic voice.
 *
 * Multiple tracks are mixed together by speaker_play_tracks() to produce polyphony.
 */
typedef struct {
  /** Array of notes to play sequentially. */
  const SpeakerNote *notes;
  /** Length of the notes array. */
  uint32_t num_notes;
  /**
   * If non-NULL, notes are played by pitch-shifting this sample;
   * note.waveform is ignored. If NULL, notes use their waveform field.
   */
  const SpeakerSample *sample;
} SpeakerTrack;
