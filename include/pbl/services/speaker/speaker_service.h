/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/speaker/limits.h>
#include <pbl/services/speaker/note_sequence.h>
#include <pbl/services/speaker/speaker_finish_reason.h>
#include <pbl/services/speaker/speaker_pcm_format.h>
#include <pbl/services/speaker/track.h>
#include <kernel/pebble_tasks.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup services_speaker Speaker
 * @ingroup services
 * @brief Plays notes, tones, polyphonic tracks and PCM streams on the speaker.
 *
 * One source plays at a time, mixed to 16 kHz mono and fed to the audio driver from KernelBG.
 * Starting playback preempts the current source when the new priority is higher, or when the
 * current source has finished and only its tail is draining; otherwise the request is rejected.
 * Volumes are 0-100 and are scaled by the user's speaker volume preference; output is silent
 * while the speaker is muted (always, or during Do Not Disturb if so configured). Without
 * @c CONFIG_SPEAKER every call is a no-op that reports failure.
 *
 * The note, sample and PCM types are shared with the app SDK.
 *
 * @code{.c}
 * static const SpeakerNote s_chime[] = {
 *   {.midi_note = 72, .waveform = SpeakerWaveformSine, .duration_ms = 150},
 *   {.midi_note = 0, .duration_ms = 50},
 *   {.midi_note = 79, .waveform = SpeakerWaveformSine, .duration_ms = 300},
 * };
 *
 * speaker_service_play_note_seq(s_chime, ARRAY_LENGTH(s_chime), SpeakerPriorityNotification, 80);
 * @endcode
 *
 * Streaming PCM:
 *
 * @code{.c}
 * if (speaker_service_stream_open(SpeakerPriorityApp, 100, SpeakerPcmFormat_16kHz_16bit)) {
 *   uint32_t written = speaker_service_stream_write(pcm, pcm_len);
 *   // Retry the remaining pcm_len - written bytes later: the queue is full.
 *   speaker_service_stream_close();
 * }
 * @endcode
 * @{
 */

/** @brief Playback priority; a higher priority preempts a lower one. */
typedef enum {
  /** Sounds started by apps. */
  SpeakerPriorityApp = 0,
  /** Notification sounds. */
  SpeakerPriorityNotification,
  /** Sounds that must not be interrupted, e.g. alarms. */
  SpeakerPriorityCritical
} SpeakerPriority;

/** @brief Playback state. */
typedef enum {
  /** Nothing is playing. */
  SpeakerStateIdle = 0,
  /** A source is playing. */
  SpeakerStatePlaying,
  /** The stream was closed; its remaining queued data is playing. */
  SpeakerStateDraining,
} SpeakerState;

/** @brief Kind of source being played. */
typedef enum {
  /** No source. */
  SpeakerSourceNone = 0,
  /** Note sequence, see speaker_service_play_note_seq(). */
  SpeakerSourceNoteSeq,
  /** PCM stream, see speaker_service_stream_open(). */
  SpeakerSourceStream,
  /** Polyphonic tracks, see speaker_service_play_tracks(). */
  SpeakerSourceTracks,
  /** Single tone, see speaker_service_play_tone(). */
  SpeakerSourceTone,
} SpeakerSourceType;

/** @brief Initialize the speaker service. Called once at boot. */
void speaker_service_init(void);

/**
 * @brief Play a note sequence.
 *
 * @param notes Notes to play, copied internally.
 * @param num_notes Number of entries in @p notes.
 * @param pri Priority.
 * @param vol Volume, 0-100.
 * @return true if playback started; false on invalid arguments, allocation failure, or if
 *         blocked by a higher or equal priority source.
 */
bool speaker_service_play_note_seq(const SpeakerNote *notes, uint32_t num_notes,
                                   SpeakerPriority pri, uint8_t vol);

/**
 * @brief Play a single tone at an exact frequency.
 *
 * @param freq_hz Tone frequency in Hz, 0 for silence.
 * @param duration_ms Tone duration in milliseconds, must be non-zero.
 * @param waveform @c SpeakerWaveform value.
 * @param velocity Amplitude scale 0-127, 0 for full amplitude.
 * @param pri Priority.
 * @param vol Volume, 0-100.
 * @return true if playback started; false on invalid arguments or if blocked by a higher or
 *         equal priority source.
 */
bool speaker_service_play_tone(uint16_t freq_hz, uint16_t duration_ms, uint8_t waveform,
                               uint8_t velocity, SpeakerPriority pri, uint8_t vol);

/**
 * @brief Play a short tone at an absolute output volume.
 *
 * Bypasses the user's speaker volume preference, so settings UIs can preview a candidate volume
 * before it is saved. Mute still applies. Plays at @ref SpeakerPriorityApp.
 *
 * @param vol Absolute output volume, 0-100.
 * @return true if playback started.
 */
bool speaker_service_play_volume_preview(uint8_t vol);

/**
 * @brief Play monophonic tracks in parallel, mixed together.
 *
 * Track notes, samples and sample data are copied into kernel memory.
 *
 * @param tracks Tracks to play.
 * @param num_tracks Number of tracks, at most @c SPEAKER_MAX_TRACKS.
 * @param pri Priority.
 * @param vol Volume, 0-100.
 * @return true if playback started; false on invalid arguments, exceeded limits (see
 *         @c SPEAKER_MAX_SAMPLE_BYTES_TOTAL), allocation failure, or if blocked by a higher or
 *         equal priority source.
 */
bool speaker_service_play_tracks(const SpeakerTrack *tracks, uint32_t num_tracks,
                                 SpeakerPriority pri, uint8_t vol);

/**
 * @brief Post a @c PEBBLE_SPEAKER_EVENT with the finish reason whenever playback ends.
 *
 * Stays enabled until speaker_service_stop_for_task() is called for @p task.
 *
 * @param task Task interested in the events.
 */
void speaker_service_register_finish(PebbleTask task);

/**
 * @brief Open a PCM stream for writing.
 *
 * @param pri Priority.
 * @param vol Volume, 0-100.
 * @param fmt PCM format of the data to be written.
 * @return true if the stream opened; false on allocation failure or if blocked by a higher or
 *         equal priority source.
 */
bool speaker_service_stream_open(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt);

/**
 * @brief Open a PCM stream owned by a task.
 *
 * Ownership is assigned atomically with the open, so the owner's later writes, closes and volume
 * changes cannot affect a stream that has preempted it.
 *
 * @param pri Priority.
 * @param vol Volume, 0-100.
 * @param fmt PCM format of the data to be written.
 * @param owner Owning task.
 * @return true if the stream opened.
 */
bool speaker_service_stream_open_owned(SpeakerPriority pri, uint8_t vol, SpeakerPcmFormat fmt,
                                       PebbleTask owner);

/**
 * @brief Open an owned PCM stream for internal live audio.
 *
 * Writes feed complete driver blocks directly on the producer task. Driver callbacks retry on
 * backpressure instead of inserting silence into the PCM queue, and closing always drains.
 *
 * @param pri Priority.
 * @param vol Volume, 0-100.
 * @param fmt PCM format of the data to be written.
 * @param owner Owning task.
 * @return true if the stream opened.
 */
bool speaker_service_stream_open_realtime_owned(SpeakerPriority pri, uint8_t vol,
                                                SpeakerPcmFormat fmt, PebbleTask owner);

/**
 * @brief Write PCM data to the stream owned by a task.
 *
 * @param owner Owning task, or @c PebbleTask_Unknown to write to any stream.
 * @param data Source buffer.
 * @param num_bytes Number of bytes to write; a trailing partial sample is dropped.
 * @return Number of bytes accepted, 0 if no stream is open or it belongs to another task.
 */
uint32_t speaker_service_stream_write_owned(PebbleTask owner, const void *data, uint32_t num_bytes);

/**
 * @brief Close the stream owned by a task, draining its buffered data.
 *
 * @param owner Owning task, or @c PebbleTask_Unknown to close any stream.
 */
void speaker_service_stream_close_owned(PebbleTask owner);

/**
 * @brief Write PCM data to the active stream.
 *
 * @param data Source buffer.
 * @param num_bytes Number of bytes to write; a trailing partial sample is dropped.
 * @return Number of bytes accepted, fewer than @p num_bytes when the queue is full.
 */
uint32_t speaker_service_stream_write(const void *data, uint32_t num_bytes);

/** @brief Close the active stream. Remaining buffered data is drained. */
void speaker_service_stream_close(void);

/** @brief Stop any active playback immediately. */
void speaker_service_stop(void);

/**
 * @brief Set the playback volume.
 *
 * @param vol Volume, 0-100.
 */
void speaker_service_set_volume(uint8_t vol);

/**
 * @brief Set the playback volume, only if the playback belongs to a task.
 *
 * @param owner Owning task, or @c PebbleTask_Unknown to apply unconditionally.
 * @param vol Volume, 0-100.
 */
void speaker_service_set_volume_owned(PebbleTask owner, uint8_t vol);

/**
 * @brief Get the playback state.
 *
 * @return Current state.
 */
SpeakerState speaker_service_get_state(void);

/**
 * @brief Stop any playback owned by a task and drop its finish events. Called on app exit.
 *
 * @param task Exiting task.
 */
void speaker_service_stop_for_task(PebbleTask task);

/**
 * @brief Set the task that owns the current playback.
 *
 * @param task Owning task.
 */
void speaker_service_set_owner_task(PebbleTask task);

/**
 * @brief Check whether the speaker is muted.
 *
 * @return true if always-on mute is set, or the Do Not Disturb mute is set while Do Not Disturb
 *         is active.
 */
bool speaker_service_is_muted(void);

/**
 * @brief Notify that the audio preferences (mute or system volume cap) changed.
 *
 * The output volume of a playing source is re-applied immediately.
 */
void speaker_service_handle_audio_prefs_changed(void);

/** @} */
