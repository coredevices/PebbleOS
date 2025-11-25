/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MUSIC_BUFFER_LENGTH 64

typedef enum {
  MusicPlayStateUnknown,
  MusicPlayStatePlaying,
  MusicPlayStatePaused,
  MusicPlayStateForwarding,
  MusicPlayStateRewinding,
  MusicPlayStateInvalid = 0xFF,
} MusicPlayState;

typedef enum {
  MusicCommandPlay,
  MusicCommandPause,
  MusicCommandTogglePlayPause,
  MusicCommandNextTrack,
  MusicCommandPreviousTrack,
  MusicCommandVolumeUp,
  MusicCommandVolumeDown,
  MusicCommandAdvanceRepeatMode,
  MusicCommandAdvanceShuffleMode,
  MusicCommandSkipForward,
  MusicCommandSkipBackward,
  MusicCommandLike,
  MusicCommandDislike,
  MusicCommandBookmark,

  NumMusicCommand,
} MusicCommand;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface to Music app

//! Copy out the current now playing fields into the parameters. We'll assume you've provided
//! buffers that are at least MUSIC_BUFFER_LENGTH in size.
void music_get_now_playing(char *title, char *artist, char *album);

//! @return True if the music service has Now Playing metadata.
bool music_has_now_playing(void);

//! Copy out the name of the current player. We'll assume you've provided
//! buffers that are at least MUSIC_BUFFER_LENGTH in size.
//! @return True if the name was copied successfully, or false if there was no name available.
bool music_get_player_name(char *player_name_out);

//! @return The milliseconds since the track position was last updated.
uint32_t music_get_ms_since_pos_last_updated(void);

//! Retrieve the position in the current track in the given pointers (which must not be null).
void music_get_pos(uint32_t *track_pos_ms, uint32_t *track_length_ms);

//! @return The current playback rate percentage.
int32_t music_get_playback_rate_percent(void);

//! @return The volume percentage.
uint8_t music_get_volume_percent(void);

//! Retrieve the current playback state.
MusicPlayState music_get_playback_state(void);

//! @return True if the service supports reporting of the player's playback state.
//! @see music_get_playback_state
bool music_is_playback_state_reporting_supported(void);

//! @return True if the service support reporting of the playback progress.
//! @see music_get_pos
bool music_is_progress_reporting_supported(void);

//! @return True if the service supports reporting of the current volume.
//! @see music_get_volume_percent
bool music_is_volume_reporting_supported(void);

//! Sends the command to the server. Commands are "unreliable", they are sent at "best effort".
//! @param command The command to send.
//! @see music_is_command_supported
void music_command_send(MusicCommand command);

//! @param The command to test.
//! @return True if the command is supported by the connected server.
bool music_is_command_supported(MusicCommand command);

//! @return True if playback needs to be started manually by the user from the phone.
bool music_needs_user_to_start_playback_on_phone(void);

//! Puts the underlying connection in a reduced latency mode, for better responsiveness.
void music_request_reduced_latency(bool reduced_latency);

//! Puts the underlying connection in a low latency mode, for the best responsiveness.
void music_request_low_latency_for_period(uint32_t period_seconds);

//! For testing purposes.
const char * music_get_connected_server_debug_name(void);
