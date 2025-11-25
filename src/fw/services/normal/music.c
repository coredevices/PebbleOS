/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "music_internal.h"

#include "apps/system_apps/music_app.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "process_management/app_manager.h"
#include "shell/system_app_ids.auto.h"
#include "os/mutex.h"
#include "os/tick.h"
#include "system/logging.h"
#include "util/math.h"

//! @file This module implements the music service. It provides an abstraction layer on top of the
//! various underlying music metadata and control services: the Pebble Protocol
//! music endpoint (see music_endpoint.c) and Apple Media Service (see ams.c).
//! This module also caches the last known metadata and media player state.
//! @note Only one underlying backend is supported at a time. If a second backend tries to "connect"
//! it is ignored.

#define MUSIC_NORMAL_PLAYBACK_RATE_PERCENT ((int32_t) 100)

//! Cache of the most recently received now playing data. Note that this is read and written from
//! multiple threads, so access is protected by the mutex member.
struct MusicServiceContext {
  PebbleRecursiveMutex *mutex;

  //! The connected server that provides media metadata and accepts control commands
  const MusicServerImplementation *implementation;

  //! The volume setting of the current player
  uint8_t player_volume_percent;

  char player_name[MUSIC_BUFFER_LENGTH];

  char title[MUSIC_BUFFER_LENGTH];
  char artist[MUSIC_BUFFER_LENGTH];
  char album[MUSIC_BUFFER_LENGTH];

  uint32_t track_length_ms;

  //! Position that was last communicated to Pebble by the server.
  //! @note This is not necessarily the actual position. See music_get_pos()
  uint32_t track_pos_ms;

  //! The time when track_pos_ms was last updated.
  RtcTicks track_pos_updated_at;

  //! The current playback rate in percent units.
  //! Example values:
  //!  100 = normal playback rate
  //!  0   = paused
  //!  200 = 2x playback rate (Apple's Podcast app can vary the playback rate)
  //! -100 = backwards at normal rate
  int32_t playback_rate_percent;

  //! The current playback state
  MusicPlayState playback_state;
} s_music_ctx;

void music_init(void) {
  s_music_ctx.mutex = mutex_create_recursive();
}

static void copy_and_truncate(char *dest, const char *src, size_t src_length) {
  size_t cropped_length = MIN(MUSIC_BUFFER_LENGTH - 1, src_length);
  if (src) {
    memcpy(dest, src, cropped_length);
  }
  dest[cropped_length] = 0;
}

static void prv_put_now_playing_changed_event(void) {
  PebbleEvent e = {
    .type = PEBBLE_MEDIA_EVENT,
    .media.type = PebbleMediaEventTypeNowPlayingChanged
  };
  event_put(&e);
}

bool music_set_connected_server(const MusicServerImplementation *implementation, bool connected) {
  enum {
    Disconnected = -1,
    None = 0,
    Connected = 1,
  } change_type = None;

  mutex_lock_recursive(s_music_ctx.mutex);

  if (connected) {
    if (s_music_ctx.implementation == NULL) {
      change_type = Connected;
      s_music_ctx.implementation = implementation;
      PBL_LOG(LOG_LEVEL_INFO, "Music server connected: %s", implementation->debug_name);
    } else {
      PBL_LOG(LOG_LEVEL_ERROR, "Server <0x%p> connected, but another <0x%p> is already registered",
              implementation, s_music_ctx.implementation);
    }

  } else {
    if (s_music_ctx.implementation == implementation) {
      // Previously registered server got disconnected
      change_type = Disconnected;
      s_music_ctx.implementation = NULL;
      PBL_LOG(LOG_LEVEL_INFO, "Music server disconnected: %s", implementation->debug_name);
    } else {
      PBL_LOG(LOG_LEVEL_ERROR, "Unknown server <%p> disconnected", implementation);
    }
  }

  if (change_type != None) {
    // Upon connect and disconnect, reset the cached data:

    music_update_player_volume_percent(0);
    // Taking short-cut here, music_update_now_playing already puts NowPlayingChanged event, no
    // need to put it again by calling music_update_player_name:
    s_music_ctx.player_name[0] = 0;
    music_update_now_playing(NULL, 0, NULL, 0, NULL, 0);
    music_update_track_duration(0);
    const MusicPlayerStateUpdate state = {
      .playback_state = MusicPlayStateUnknown,
      .playback_rate_percent = 0,
      .elapsed_time_ms = 0,
    };
    music_update_player_playback_state(&state);

    PebbleEvent event = {
      .type = PEBBLE_MEDIA_EVENT,
      .media = {
        .type = (change_type == Connected) ? PebbleMediaEventTypeServerConnected :
                                             PebbleMediaEventTypeServerDisconnected,
      },
    };
    event_put(&event);
  }

  mutex_unlock_recursive(s_music_ctx.mutex);

  return (change_type != None);
}

const char * music_get_connected_server_debug_name(void) {
  const char *debug_name = NULL;
  mutex_lock_recursive(s_music_ctx.mutex);
  if (s_music_ctx.implementation) {
    debug_name = s_music_ctx.implementation->debug_name;
  }
  mutex_unlock_recursive(s_music_ctx.mutex);
  return debug_name;
}

void music_update_now_playing(const char *title, size_t title_length,
                              const char *artist, size_t artist_length,
                              const char *album, size_t album_length) {
  mutex_lock_recursive(s_music_ctx.mutex);

  copy_and_truncate(s_music_ctx.title, title, title_length);
  copy_and_truncate(s_music_ctx.artist, artist, artist_length);
  copy_and_truncate(s_music_ctx.album, album, album_length);

  mutex_unlock_recursive(s_music_ctx.mutex);

  prv_put_now_playing_changed_event();
}

static void prv_update_string_and_put_event(const char *value, size_t value_length, off_t offset) {
  mutex_lock_recursive(s_music_ctx.mutex);
  char *buffer = ((char *) &s_music_ctx) + offset;
  copy_and_truncate(buffer, value, value_length);
  mutex_unlock_recursive(s_music_ctx.mutex);
  prv_put_now_playing_changed_event();
}

void music_update_player_name(const char *player_name, size_t player_name_length) {
  // TODO: actually do something with this
  off_t o = offsetof(__typeof__(s_music_ctx), player_name);
  prv_update_string_and_put_event(player_name, player_name_length, o);
}

void music_update_track_title(const char *title, size_t title_length) {
  off_t o = offsetof(__typeof__(s_music_ctx), title);
  prv_update_string_and_put_event(title, title_length, o);
}

void music_update_track_artist(const char *artist, size_t artist_length) {
  off_t o = offsetof(__typeof__(s_music_ctx), artist);
  prv_update_string_and_put_event(artist, artist_length, o);
}

void music_update_track_album(const char *album, size_t album_length) {
  off_t o = offsetof(__typeof__(s_music_ctx), album);
  prv_update_string_and_put_event(album, album_length, o);
}

static void prv_put_pos_changed_event(void) {
  PebbleEvent e = {
    .type = PEBBLE_MEDIA_EVENT,
    .media.type = PebbleMediaEventTypeTrackPosChanged,
  };
  event_put(&e);
}

void music_update_track_position(uint32_t track_pos_ms) {
  mutex_lock_recursive(s_music_ctx.mutex);

  s_music_ctx.track_pos_ms = track_pos_ms;
  s_music_ctx.track_pos_updated_at = rtc_get_ticks();

  mutex_unlock_recursive(s_music_ctx.mutex);

  prv_put_pos_changed_event();
}

void music_update_track_duration(uint32_t track_duration_ms) {
  mutex_lock_recursive(s_music_ctx.mutex);

  s_music_ctx.track_length_ms = track_duration_ms;

  mutex_unlock_recursive(s_music_ctx.mutex);

  prv_put_pos_changed_event();
}

void music_get_now_playing(char *title, char *artist, char *album) {
  mutex_lock_recursive(s_music_ctx.mutex);

  if (title) {
    strcpy(title, s_music_ctx.title);
  }
  if (artist) {
    strcpy(artist, s_music_ctx.artist);
  }
  if (album) {
    strcpy(album, s_music_ctx.album);
  }

  mutex_unlock_recursive(s_music_ctx.mutex);
}

bool music_get_player_name(char *player_name_out) {
  mutex_lock_recursive(s_music_ctx.mutex);

  const bool has_player_name = (s_music_ctx.player_name[0] != 0);

  if (player_name_out) {
    strcpy(player_name_out, s_music_ctx.player_name);
  }

  mutex_unlock_recursive(s_music_ctx.mutex);

  return has_player_name;
}

bool music_has_now_playing(void) {
  bool has_now_playing = false;
  mutex_lock_recursive(s_music_ctx.mutex);
  if (s_music_ctx.title[0] != 0 ||
      s_music_ctx.artist[0] != 0) {
    has_now_playing = true;
  }
  mutex_unlock_recursive(s_music_ctx.mutex);
  return has_now_playing;
}

uint32_t music_get_ms_since_pos_last_updated(void) {
  mutex_lock_recursive(s_music_ctx.mutex);
  const RtcTicks time_elapsed_ticks = rtc_get_ticks() - s_music_ctx.track_pos_updated_at;
  const uint32_t time_elapsed_ms = ticks_to_milliseconds(time_elapsed_ticks);
  mutex_unlock_recursive(s_music_ctx.mutex);
  return time_elapsed_ms;
}

void music_get_pos(uint32_t *track_pos_ms, uint32_t *track_length_ms) {
  mutex_lock_recursive(s_music_ctx.mutex);

  const int32_t time_elapsed_ms = music_get_ms_since_pos_last_updated();
  const int32_t track_time_elapsed =
      (time_elapsed_ms * s_music_ctx.playback_rate_percent) / MUSIC_NORMAL_PLAYBACK_RATE_PERCENT;
  const int32_t pos_ms = s_music_ctx.track_pos_ms + track_time_elapsed;
  const int32_t length_ms = s_music_ctx.track_length_ms;

  *track_pos_ms = CLIP(pos_ms, 0, length_ms);
  *track_length_ms = length_ms;

  mutex_unlock_recursive(s_music_ctx.mutex);
}

int32_t music_get_playback_rate_percent(void) {
  mutex_lock_recursive(s_music_ctx.mutex);
  int32_t playback_rate_percent = s_music_ctx.playback_rate_percent;
  mutex_unlock_recursive(s_music_ctx.mutex);
  return playback_rate_percent;
}

uint8_t music_get_volume_percent(void) {
  mutex_lock_recursive(s_music_ctx.mutex);
  int32_t player_volume_percent = s_music_ctx.player_volume_percent;
  mutex_unlock_recursive(s_music_ctx.mutex);
  return player_volume_percent;
}

static void prv_put_state_changed_event(MusicPlayState playback_state) {
  PebbleEvent event = {
    .type = PEBBLE_MEDIA_EVENT,
    .media = {
      .type = PebbleMediaEventTypePlaybackStateChanged,
      .playback_state = playback_state,
    },
  };
  event_put(&event);
}

void music_update_player_playback_state(const MusicPlayerStateUpdate *state) {
  mutex_lock_recursive(s_music_ctx.mutex);
  s_music_ctx.playback_state = state->playback_state;
  s_music_ctx.playback_rate_percent = state->playback_rate_percent;
  s_music_ctx.track_pos_ms = state->elapsed_time_ms;
  s_music_ctx.track_pos_updated_at = rtc_get_ticks();
  mutex_unlock_recursive(s_music_ctx.mutex);

  prv_put_state_changed_event(state->playback_state);
  prv_put_pos_changed_event();
}

void music_update_player_volume_percent(uint8_t volume_percent) {
  mutex_lock_recursive(s_music_ctx.mutex);
  s_music_ctx.player_volume_percent = volume_percent;
  mutex_unlock_recursive(s_music_ctx.mutex);

  PebbleEvent event = {
    .type = PEBBLE_MEDIA_EVENT,
    .media = {
      .type = PebbleMediaEventTypeVolumeChanged,
      .volume_percent = volume_percent,
    },
  };
  event_put(&event);
}

MusicPlayState music_get_playback_state(void) {
  if (!music_is_playback_state_reporting_supported()) {
    return MusicPlayStateUnknown;
  }
  MusicPlayState result;
  mutex_lock_recursive(s_music_ctx.mutex);
  result = s_music_ctx.playback_state;
  mutex_unlock_recursive(s_music_ctx.mutex);
  return result;
}

static void * prv_implementation_function_for_offset(off_t offset) {
  typedef void (*FuncPtr)(void);
  FuncPtr func_ptr = NULL;
  mutex_lock_recursive(s_music_ctx.mutex);
  if (s_music_ctx.implementation) {
    func_ptr = *(FuncPtr *) (((const uint8_t *)s_music_ctx.implementation) + offset);
  }
  mutex_unlock_recursive(s_music_ctx.mutex);
  return func_ptr;
}

void music_command_send(MusicCommand command) {
  const off_t o = offsetof(__typeof__(*s_music_ctx.implementation), command_send);
  void (*command_send)(MusicCommand) = prv_implementation_function_for_offset(o);
  if (command_send) {
    command_send(command);
  }
}

void music_request_reduced_latency(bool reduced_latency) {
  const off_t o = offsetof(__typeof__(*s_music_ctx.implementation), request_reduced_latency);
  void (*request_reduced_latency)(bool) = prv_implementation_function_for_offset(o);
  if (request_reduced_latency) {
    request_reduced_latency(reduced_latency);
  }
}

void music_request_low_latency_for_period(uint32_t period_ms) {
  const off_t o = offsetof(__typeof__(*s_music_ctx.implementation), request_low_latency_for_period);
  void (*request_low_latency_for_period)(uint32_t) = prv_implementation_function_for_offset(o);
  if (request_low_latency_for_period) {
    request_low_latency_for_period(period_ms);
  }
}

bool music_is_command_supported(MusicCommand command) {
  const off_t o = offsetof(__typeof__(*s_music_ctx.implementation),
                           is_command_supported);
  bool (*func_ptr)(MusicCommand) = prv_implementation_function_for_offset(o);
  if (!func_ptr) {
    return false;
  }
  return func_ptr(command);
}

static bool prv_call_implementation_bool_return_void_args(off_t offset) {
  bool (*func_ptr)(void) = prv_implementation_function_for_offset(offset);
  if (!func_ptr) {
    return false;  // defaults to false when there is no "connected" implementation
  }
  return func_ptr();
}

bool music_needs_user_to_start_playback_on_phone(void) {
  const off_t o = offsetof(__typeof__(*s_music_ctx.implementation),
                           needs_user_to_start_playback_on_phone);
  return prv_call_implementation_bool_return_void_args(o);
}

static bool prv_is_capability_supported(MusicServerCapability capability) {
  const off_t o = offsetof(__typeof__(*s_music_ctx.implementation), get_capability_bitset);
  MusicServerCapability (*func_ptr)(void) = prv_implementation_function_for_offset(o);
  if (!func_ptr) {
    return false;
  }
  return (func_ptr() & capability);
}

bool music_is_playback_state_reporting_supported(void) {
  return prv_is_capability_supported(MusicServerCapabilityPlaybackStateReporting);
}

bool music_is_progress_reporting_supported(void) {
  // Check capability and that track length is greater than 0
  uint32_t track_length_ms;
  mutex_lock_recursive(s_music_ctx.mutex);
  track_length_ms = s_music_ctx.track_length_ms;
  mutex_unlock_recursive(s_music_ctx.mutex);
  return (prv_is_capability_supported(MusicServerCapabilityProgressReporting) && track_length_ms);
}

bool music_is_volume_reporting_supported(void) {
  return prv_is_capability_supported(MusicServerCapabilityVolumeReporting);
}

void command_print_now_playing(void) {
  char title[MUSIC_BUFFER_LENGTH];
  char artist[MUSIC_BUFFER_LENGTH];
  char album[MUSIC_BUFFER_LENGTH];

  music_get_now_playing(title, artist, album);

  char buffer[128];
  dbgserial_putstr_fmt(buffer, 128, "title=%s; artist=%s; album=%s", title, artist, album);
}

