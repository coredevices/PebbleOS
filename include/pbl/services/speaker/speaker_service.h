/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/speaker/note_sequence.h"
#include "pbl/services/speaker/speaker_finish_reason.h"
#include "pbl/services/speaker/speaker_pcm_format.h"
#include "pbl/services/speaker/track.h"
#include "kernel/pebble_tasks.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SpeakerPriorityApp = 0,
  SpeakerPriorityNotification,
  SpeakerPriorityCritical
} SpeakerPriority;

typedef enum {
  SpeakerStateIdle = 0,
  SpeakerStatePlaying,
  SpeakerStateDraining,    // stream closing, playing remaining buffered data
} SpeakerState;

typedef enum {
  SpeakerSourceNone = 0,
  SpeakerSourceNoteSeq,
  SpeakerSourceStream,
  SpeakerSourceTracks,
} SpeakerSourceType;

//! Initialize the speaker service. Called once at boot.
void speaker_service_init(void);

//! Play a note sequence on the speaker.
//! @param notes Array of notes (copied internally)
//! @param num_notes Number of notes
//! @param pri Priority level
//! @param vol Volume (0-100)
//! @return true if playback started, false if preempted by higher priority
bool speaker_service_play_note_seq(const SpeakerNote *notes, uint32_t num_notes,
                                   SpeakerPriority pri, uint8_t vol);

//! Play N monophonic tracks in parallel, mixed together.
//! Track arrays and any sample data are copied into kernel memory.
//! @param tracks Array of tracks. For each, its notes array and (optional)
//!               sample + sample data are copied.
//! @param num_tracks Number of tracks (<= SPEAKER_MAX_TRACKS).
//! @param pri Priority level.
//! @param vol Volume (0-100).
//! @return true if playback started; false on invalid args, exceeded limits,
//!         allocation failure, or priority blocked.
bool speaker_service_play_tracks(const SpeakerTrack *tracks, uint32_t num_tracks,
                                 SpeakerPriority pri, uint8_t vol);

//! Ask the service to post PEBBLE_SPEAKER_EVENT with the finish reason to the
//! given task whenever playback ends.
void speaker_service_register_finish(PebbleTask task);

//! Max number of parallel tracks for polyphony.
#define SPEAKER_MAX_TRACKS 4

//! Max total sample-data bytes per speaker_service_play_tracks call.
#define SPEAKER_MAX_SAMPLE_BYTES_TOTAL (16 * 1024)

//! Open a PCM stream for writing.
//! @param pri Priority level
//! @param vol Volume (0-100)
//! @param fmt PCM format (sample rate and bit depth)
//! @return true if stream opened, false if blocked by higher priority
bool speaker_service_stream_open(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt);

//! Write PCM data to the active stream.
//! @param data Source buffer
//! @param num_bytes Number of bytes to write
//! @return Number of bytes actually written (backpressure)
uint32_t speaker_service_stream_write(const void *data, uint32_t num_bytes);

//! Close the active stream. Remaining buffered data will be drained.
void speaker_service_stream_close(void);

//! Stop any active playback immediately.
void speaker_service_stop(void);

//! Set playback volume.
//! @param vol Volume 0-100
void speaker_service_set_volume(uint8_t vol);

//! Get current speaker state.
SpeakerState speaker_service_get_state(void);

//! Stop any playback initiated by the given task. Called on app exit.
void speaker_service_stop_for_task(PebbleTask task);

//! Set the task that owns the current playback session.
void speaker_service_set_owner_task(PebbleTask task);
