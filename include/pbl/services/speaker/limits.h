/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Maximum number of notes in a single speaker_play_notes() call.
#define SPEAKER_MAX_NOTES 256

//! Maximum number of parallel tracks in a single speaker_play_tracks() call.
#define SPEAKER_MAX_TRACKS 4

//! Maximum total sample-data bytes across all tracks in one speaker_play_tracks()
//! call.
#define SPEAKER_MAX_SAMPLE_BYTES_TOTAL (16 * 1024)
