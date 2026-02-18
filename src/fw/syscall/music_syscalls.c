/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/music.h"
#include "syscall/syscall_internal.h"

DEFINE_SYSCALL(void, sys_music_get_now_playing, char *title, char *artist, char *album) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (title) syscall_assert_userspace_buffer(title, MUSIC_BUFFER_LENGTH);
    if (artist) syscall_assert_userspace_buffer(artist, MUSIC_BUFFER_LENGTH);
    if (album) syscall_assert_userspace_buffer(album, MUSIC_BUFFER_LENGTH);
  }
  music_get_now_playing(title, artist, album);
}

DEFINE_SYSCALL(bool, sys_music_has_now_playing, void) {
  return music_has_now_playing();
}

DEFINE_SYSCALL(bool, sys_music_get_player_name, char *player_name_out) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (player_name_out) syscall_assert_userspace_buffer(player_name_out, MUSIC_BUFFER_LENGTH);
  }
  return music_get_player_name(player_name_out);
}

DEFINE_SYSCALL(uint32_t, sys_music_get_ms_since_pos_last_updated, void) {
  return music_get_ms_since_pos_last_updated();
}

DEFINE_SYSCALL(void, sys_music_get_pos, uint32_t *track_pos_ms, uint32_t *track_length_ms) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(track_pos_ms, sizeof(*track_pos_ms));
    syscall_assert_userspace_buffer(track_length_ms, sizeof(*track_length_ms));
  }
  music_get_pos(track_pos_ms, track_length_ms);
}

DEFINE_SYSCALL(int32_t, sys_music_get_playback_rate_percent, void) {
  return music_get_playback_rate_percent();
}

DEFINE_SYSCALL(uint8_t, sys_music_get_volume_percent, void) {
  return music_get_volume_percent();
}

DEFINE_SYSCALL(MusicPlayState, sys_music_get_playback_state, void) {
  return music_get_playback_state();
}

DEFINE_SYSCALL(bool, sys_music_is_playback_state_reporting_supported, void) {
  return music_is_playback_state_reporting_supported();
}

DEFINE_SYSCALL(bool, sys_music_is_progress_reporting_supported, void) {
  return music_is_progress_reporting_supported();
}

DEFINE_SYSCALL(bool, sys_music_is_volume_reporting_supported, void) {
  return music_is_volume_reporting_supported();
}

DEFINE_SYSCALL(void, sys_music_command_send, MusicCommand command) {
  music_command_send(command);
}

DEFINE_SYSCALL(bool, sys_music_is_command_supported, MusicCommand command) {
  return music_is_command_supported(command);
}

DEFINE_SYSCALL(bool, sys_music_needs_user_to_start_playback_on_phone, void) {
  return music_needs_user_to_start_playback_on_phone();
}

DEFINE_SYSCALL(void, sys_music_request_reduced_latency, bool reduced_latency) {
  music_request_reduced_latency(reduced_latency);
}

DEFINE_SYSCALL(void, sys_music_request_low_latency_for_period, uint32_t period_seconds) {
  music_request_low_latency_for_period(period_seconds);
}

DEFINE_SYSCALL(const char *, sys_music_get_connected_server_debug_name, void) {
  return music_get_connected_server_debug_name();
}
