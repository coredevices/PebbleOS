/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @defgroup services_music Music
 * @ingroup services
 * @brief Now-playing metadata cache and media control.
 *
 * Abstracts the music backend (the Pebble Protocol music endpoint or the Apple Media Service)
 * behind a single interface used by the Music app. Only one backend is connected at a time. The
 * service caches the last reported metadata and player state, and emits @c PEBBLE_MEDIA_EVENT
 * events when they change. All functions are thread safe.
 *
 * @code{.c}
 * char title[MUSIC_BUFFER_LENGTH], artist[MUSIC_BUFFER_LENGTH], album[MUSIC_BUFFER_LENGTH];
 *
 * if (music_has_now_playing()) {
 *   music_get_now_playing(title, artist, album);
 * }
 * if (music_is_command_supported(MusicCommandTogglePlayPause)) {
 *   music_command_send(MusicCommandTogglePlayPause);
 * }
 * @endcode
 * @{
 */

/** @brief Size of the string buffers used for metadata, including the terminator. */
#define MUSIC_BUFFER_LENGTH 64

/** @brief Playback state of the player. */
typedef enum {
  /** State not known or not reported. */
  MusicPlayStateUnknown,
  /** Playing. */
  MusicPlayStatePlaying,
  /** Paused. */
  MusicPlayStatePaused,
  /** Fast-forwarding. */
  MusicPlayStateForwarding,
  /** Rewinding. */
  MusicPlayStateRewinding,
  /** Backend reported an unrecognized state. */
  MusicPlayStateInvalid = 0xFF,
} MusicPlayState;

/** @brief Control command sent to the player. */
typedef enum {
  /** Start playback. */
  MusicCommandPlay,
  /** Pause playback. */
  MusicCommandPause,
  /** Toggle between play and pause. */
  MusicCommandTogglePlayPause,
  /** Next track, or seek forward when music_skip_seeks_within_track() is true. */
  MusicCommandNextTrack,
  /** Previous track, or seek backward when music_skip_seeks_within_track() is true. */
  MusicCommandPreviousTrack,
  /** Raise the volume. */
  MusicCommandVolumeUp,
  /** Lower the volume. */
  MusicCommandVolumeDown,
  /** Cycle the repeat mode. */
  MusicCommandAdvanceRepeatMode,
  /** Cycle the shuffle mode. */
  MusicCommandAdvanceShuffleMode,
  /** Skip forward within the track. */
  MusicCommandSkipForward,
  /** Skip backward within the track. */
  MusicCommandSkipBackward,
  /** Like the current track. */
  MusicCommandLike,
  /** Dislike the current track. */
  MusicCommandDislike,
  /** Bookmark the current track. */
  MusicCommandBookmark,

  /** Number of commands. */
  NumMusicCommand,
} MusicCommand;

/**
 * @brief Copy the current track metadata.
 *
 * @param[out] title Title buffer of at least #MUSIC_BUFFER_LENGTH bytes, or NULL.
 * @param[out] artist Artist buffer of at least #MUSIC_BUFFER_LENGTH bytes, or NULL.
 * @param[out] album Album buffer of at least #MUSIC_BUFFER_LENGTH bytes, or NULL.
 */
void music_get_now_playing(char *title, char *artist, char *album);

/**
 * @brief Check whether now-playing metadata is available.
 *
 * @return True if a title or an artist is known.
 */
bool music_has_now_playing(void);

/**
 * @brief Copy the name of the current player.
 *
 * @param[out] player_name_out Buffer of at least #MUSIC_BUFFER_LENGTH bytes, or NULL.
 * @return True if a player name is known.
 */
bool music_get_player_name(char *player_name_out);

/**
 * @brief Time since the backend last reported the track position.
 *
 * @return Elapsed time in milliseconds.
 */
uint32_t music_get_ms_since_pos_last_updated(void);

/**
 * @brief Get the estimated track position and the track length.
 *
 * The position is extrapolated from the last reported one using the playback rate, and clamped
 * to the track length.
 *
 * @param[out] track_pos_ms Position in milliseconds. Must not be NULL.
 * @param[out] track_length_ms Track length in milliseconds. Must not be NULL.
 */
void music_get_pos(uint32_t *track_pos_ms, uint32_t *track_length_ms);

/**
 * @brief Get the playback rate.
 *
 * @return Rate in percent: 100 is normal speed, 0 paused, negative values play backwards.
 */
int32_t music_get_playback_rate_percent(void);

/**
 * @brief Get the player volume.
 *
 * @return Volume in percent, 0 to 100.
 */
uint8_t music_get_volume_percent(void);

/**
 * @brief Get the playback state.
 *
 * @return Current state, or #MusicPlayStateUnknown if the backend does not report it.
 */
MusicPlayState music_get_playback_state(void);

/**
 * @brief Check whether the backend reports the playback state.
 *
 * @return True if supported.
 * @see music_get_playback_state
 */
bool music_is_playback_state_reporting_supported(void);

/**
 * @brief Check whether the backend reports playback progress.
 *
 * @return True if supported and the current track has a non-zero length.
 * @see music_get_pos
 */
bool music_is_progress_reporting_supported(void);

/**
 * @brief Check whether the backend reports the player volume.
 *
 * @return True if supported.
 * @see music_get_volume_percent
 */
bool music_is_volume_reporting_supported(void);

/**
 * @brief Send a command to the player, best effort.
 *
 * Does nothing when no backend is connected. Delivery is not confirmed.
 *
 * @param command Command to send.
 * @see music_is_command_supported
 */
void music_command_send(MusicCommand command);

/**
 * @brief Check whether the connected backend supports a command.
 *
 * @param command Command to test.
 * @return True if supported, false if not or when no backend is connected.
 */
bool music_is_command_supported(MusicCommand command);

/**
 * @brief Check whether next/previous track commands seek within the track.
 *
 * True for podcasts and audiobooks. The commands to send are the same either way; this only
 * tells the Music app which icons to show.
 *
 * @return True if #MusicCommandNextTrack and #MusicCommandPreviousTrack seek within the track.
 */
bool music_skip_seeks_within_track(void);

/**
 * @brief Check whether playback must be started by the user on the phone.
 *
 * @return True if so, false otherwise or when no backend is connected.
 */
bool music_needs_user_to_start_playback_on_phone(void);

/**
 * @brief Enable or disable a reduced latency mode on the backend connection.
 *
 * @param reduced_latency True to request reduced latency, false to release the request.
 */
void music_request_reduced_latency(bool reduced_latency);

/**
 * @brief Request the lowest connection latency for a limited time.
 *
 * @param period_seconds Duration in milliseconds, despite the name.
 */
void music_request_low_latency_for_period(uint32_t period_seconds);

/**
 * @brief Get the debug name of the connected backend, for tests.
 *
 * @return Debug name, or NULL when no backend is connected.
 */
const char *music_get_connected_server_debug_name(void);

struct GBitmap;

/**
 * @brief Get the now-playing generation token.
 *
 * The 8-bit token changes whenever the title, artist or album changes. The Music app re-requests
 * album art when it changes, and backends echo it in album art transfers so that art arriving
 * after a track change can be discarded.
 *
 * @return Current generation.
 * @see music_set_album_art
 */
uint8_t music_get_now_playing_generation(void);

/**
 * @brief Check whether the held album art belongs to the current track.
 *
 * True once a response (art or "no art") was received for the current generation. The previous
 * track's art is kept until then, so use this rather than the presence of art to decide whether
 * to request art for the current track.
 *
 * @return True if the album art is current.
 */
bool music_album_art_is_current(void);

/**
 * @brief Borrow the current album art for drawing.
 *
 * Holds the service lock until music_album_art_unlock(), which must always be called, even when
 * NULL is returned. The pointer must not be used after unlocking.
 *
 * @return Album art owned by the service, or NULL if there is none.
 */
const struct GBitmap *music_album_art_lock(void);

/** @brief Release the album art borrowed with music_album_art_lock(). */
void music_album_art_unlock(void);

/** @} */
