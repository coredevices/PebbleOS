/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "music.h"

#include <stdbool.h>

#include <kernel/events.h>

/**
 * @addtogroup services_music
 * @{
 */

/** @brief Initialize the music service. */
void music_init(void);

/**
 * @brief Mark a media event as taken by KernelMain.
 *
 * Now-playing and track position events are coalesced: a new one is only posted once the previous
 * one has been taken. Consumers must read the current state through the music_get_*() functions.
 * Must be called before the event is dispatched.
 *
 * @param event Media event taken from the KernelMain queue.
 */
void music_handle_media_event(const PebbleMediaEvent *event);

/** @brief Optional features of a music backend, as a bitset. */
typedef enum {
  /** No optional feature. */
  MusicServerCapabilityNone = 0,
  /** Reports the playback state. */
  MusicServerCapabilityPlaybackStateReporting = (1 << 0),
  /** Reports the track position. */
  MusicServerCapabilityProgressReporting = (1 << 1),
  /** Reports the player volume. */
  MusicServerCapabilityVolumeReporting = (1 << 2),
} MusicServerCapability;

/** @brief Operations implemented by a music backend. */
typedef struct {
  /** Name used in logs and tests. */
  const char *debug_name;
  /** Check whether a command is supported. */
  bool (*is_command_supported)(MusicCommand command);
  /** Send a command to the player. */
  void (*command_send)(MusicCommand command);
  /** Check whether playback must be started from the phone. */
  bool (*needs_user_to_start_playback_on_phone)(void);
  /** Get the supported MusicServerCapability bits. */
  MusicServerCapability (*get_capability_bitset)(void);
  /** Enable or disable reduced connection latency. */
  void (*request_reduced_latency)(bool reduced_latency);
  /** Request the lowest connection latency for @c period_ms milliseconds. */
  void (*request_low_latency_for_period)(uint32_t period_ms);
} MusicServerImplementation;

/**
 * @brief Report that a backend connected or disconnected.
 *
 * Only one backend can be connected at a time; a second one is rejected. Every connection change
 * resets the cached metadata and state. Only one instance of each backend type exists, so the
 * implementation pointer identifies it.
 *
 * @param implementation Backend operations.
 * @param connected True on connection, false on disconnection.
 * @return True if the state changed. On false the backend must not call any music_update_*()
 *         function.
 */
bool music_set_connected_server(const MusicServerImplementation *implementation, bool connected);

/**
 * @brief Update the current track metadata.
 *
 * Strings need not be NUL terminated and are truncated to fit #MUSIC_BUFFER_LENGTH. A change of
 * any field bumps the now-playing generation.
 *
 * @param title Track title.
 * @param title_length Length of @p title in bytes.
 * @param artist Track artist.
 * @param artist_length Length of @p artist in bytes.
 * @param album Track album.
 * @param album_length Length of @p album in bytes.
 */
void music_update_now_playing(const char *title, size_t title_length, const char *artist,
                              size_t artist_length, const char *album, size_t album_length);

/**
 * @brief Update the name of the current player.
 *
 * @param player_name Player name, need not be NUL terminated.
 * @param player_name_length Length of @p player_name in bytes.
 */
void music_update_player_name(const char *player_name, size_t player_name_length);

/**
 * @brief Playback state update.
 * @see music_update_player_playback_state
 */
typedef struct {
  /** Playback state. */
  MusicPlayState playback_state;
  /** Playback rate in percent, 100 being normal speed. */
  int32_t playback_rate_percent;
  /** Track position in milliseconds. */
  uint32_t elapsed_time_ms;
  /** See music_skip_seeks_within_track(). */
  bool skip_seeks_within_track;
} MusicPlayerStateUpdate;

/**
 * @brief Update playback state, rate and track position at once.
 *
 * @param state New state.
 */
void music_update_player_playback_state(const MusicPlayerStateUpdate *state);

/**
 * @brief Update the volume of the current player.
 *
 * @param volume_percent Volume, 0 to 100.
 */
void music_update_player_volume_percent(uint8_t volume_percent);

/**
 * @brief Update the title of the current track.
 *
 * Unlike music_update_now_playing(), this does not bump the now-playing generation.
 *
 * @param title Title, need not be NUL terminated.
 * @param title_length Length of @p title in bytes.
 */
void music_update_track_title(const char *title, size_t title_length);

/**
 * @brief Update the artist of the current track.
 *
 * Does not bump the now-playing generation.
 *
 * @param artist Artist, need not be NUL terminated.
 * @param artist_length Length of @p artist in bytes.
 */
void music_update_track_artist(const char *artist, size_t artist_length);

/**
 * @brief Update the album of the current track.
 *
 * Does not bump the now-playing generation.
 *
 * @param album Album, need not be NUL terminated.
 * @param album_length Length of @p album in bytes.
 */
void music_update_track_album(const char *album, size_t album_length);

/**
 * @brief Update the position in the current track.
 *
 * @param track_pos_ms Position in milliseconds.
 */
void music_update_track_position(uint32_t track_pos_ms);

/**
 * @brief Update the duration of the current track.
 *
 * @param track_duration_ms Duration in milliseconds.
 */
void music_update_track_duration(uint32_t track_duration_ms);

/**
 * @brief Hand the album art of the current track to the service.
 *
 * Ownership of the bitmap, its pixel data and its palette, all allocated on the kernel heap,
 * passes to the service, which frees the previous art. Art whose @p token does not match the
 * current now-playing generation is stale and is freed immediately.
 *
 * @param bitmap Album art, or NULL to report that the track has none.
 * @param token Now-playing generation the art was requested for.
 */
void music_set_album_art(struct GBitmap *bitmap, uint8_t token);

/**
 * @brief Report that an album art transfer ended without an image.
 *
 * @param token Now-playing generation the art was requested for.
 */
void music_album_art_transfer_failed(uint8_t token);

/** @} */
