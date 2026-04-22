/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/normal/speaker/note_sequence.h"
#include "pbl/services/normal/speaker/speaker_pcm_format.h"

#include <stdbool.h>
#include <stdint.h>

//! A raw PCM sample that can be pitch-shifted when played by a track.
//! data: mono signed PCM in the given format.
//! num_bytes: size of data in bytes.
//! format: sample rate + bit depth (see SpeakerPcmFormat).
//! base_midi_note: the MIDI note at which the sample plays unshifted (e.g. 60 = C4).
//!                 Notes above/below this value are produced by resampling.
//! loop: if true, the sample restarts from the beginning each time it runs out,
//!       and keeps playing until the owning note's duration elapses.
typedef struct {
  const void *data;
  uint32_t num_bytes;
  SpeakerPcmFormat format;
  uint8_t base_midi_note;
  bool loop;
} SpeakerSample;

//! A single monophonic voice. Multiple tracks are mixed together by
//! speaker_play_tracks() to produce polyphony.
//! notes: array of notes to play sequentially.
//! num_notes: length of the notes array.
//! sample: if non-NULL, notes are played by pitch-shifting this sample;
//!         note.waveform is ignored. If NULL, notes use their waveform field.
typedef struct {
  const SpeakerNote *notes;
  uint32_t num_notes;
  const SpeakerSample *sample;
} SpeakerTrack;
