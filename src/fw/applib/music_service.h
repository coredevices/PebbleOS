/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup MusicService
//!
//! \brief Read-only access to the now-playing music metadata
//!
//! The MusicService API lets watchfaces and apps read the now-playing music
//! metadata (track title, artist and album) and playback state that the
//! connected phone reports to the watch. It is the same data shown by the
//! built-in Music app. This API is read-only: it does not provide any
//! playback control.
//! @{

//! Required size in bytes, including the null terminator, of each buffer
//! passed to \ref music_service_get_now_playing().
#define MUSIC_SERVICE_BUFFER_LENGTH 64

//! Playback state of the connected music player
typedef enum {
  //! The playback state is not known
  MusicServicePlaybackStateUnknown = 0,
  //! The player is playing
  MusicServicePlaybackStatePlaying,
  //! The player is paused
  MusicServicePlaybackStatePaused,
  //! The player is fast-forwarding
  MusicServicePlaybackStateForwarding,
  //! The player is rewinding
  MusicServicePlaybackStateRewinding,
} MusicServicePlaybackState;

//! Type of music service event
typedef enum {
  //! The now-playing metadata (title, artist or album) changed. Also emitted
  //! when a music server connects or disconnects, since the metadata is
  //! (re)set at those points.
  MusicServiceEventNowPlayingChanged = 0,
  //! The playback state changed
  MusicServiceEventPlaybackStateChanged,
} MusicServiceEventType;

//! Callback type for music service events
//! @param event_type The type of event that occurred
typedef void (*MusicServiceEventHandler)(MusicServiceEventType event_type);

//! @return True if now-playing metadata is currently available.
bool music_service_has_now_playing(void);

//! Copy the current now-playing metadata into the provided buffers. Each
//! buffer must be at least \ref MUSIC_SERVICE_BUFFER_LENGTH bytes; fields
//! longer than the buffer are truncated. Fields that are unknown are set to
//! the empty string.
//! @param title Buffer to receive the track title. Must not be NULL.
//! @param artist Buffer to receive the artist name. Must not be NULL.
//! @param album Buffer to receive the album name. Must not be NULL.
void music_service_get_now_playing(char *title, char *artist, char *album);

//! @return The current playback state of the connected music player.
MusicServicePlaybackState music_service_get_playback_state(void);

//! Subscribe to the music event service. Once subscribed, the handler gets
//! called whenever the now-playing metadata or playback state changes.
//! @param handler A callback to be executed on music service events
void music_service_subscribe(MusicServiceEventHandler handler);

//! Unsubscribe from the music event service. Once unsubscribed, the
//! previously registered handler will no longer be called.
void music_service_unsubscribe(void);

//!     @} // end addtogroup MusicService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation
