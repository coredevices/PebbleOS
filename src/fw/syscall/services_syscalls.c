/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall_internal.h"

#include "pbl/services/alarms/alarm.h"
#include "pbl/services/music.h"

DEFINE_SYSCALL(bool, sys_alarm_get_next_enabled, time_t *timestamp_out) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(timestamp_out, sizeof(*timestamp_out));
  }
  return alarm_get_next_enabled_alarm(timestamp_out);
}

DEFINE_SYSCALL(bool, sys_hrm_manager_is_hrm_present) {
#ifdef CONFIG_SERVICE_HRM
  return true;
#else
  return false;
#endif
}

DEFINE_SYSCALL(bool, sys_music_has_now_playing) {
#ifdef CONFIG_SERVICE_MUSIC
  return music_has_now_playing();
#else
  return false;
#endif
}

DEFINE_SYSCALL(void, sys_music_get_now_playing, char *title, char *artist, char *album) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(title, MUSIC_BUFFER_LENGTH);
    syscall_assert_userspace_buffer(artist, MUSIC_BUFFER_LENGTH);
    syscall_assert_userspace_buffer(album, MUSIC_BUFFER_LENGTH);
  }
#ifdef CONFIG_SERVICE_MUSIC
  music_get_now_playing(title, artist, album);
#else
  title[0] = '\0';
  artist[0] = '\0';
  album[0] = '\0';
#endif
}

DEFINE_SYSCALL(MusicPlayState, sys_music_get_playback_state) {
#ifdef CONFIG_SERVICE_MUSIC
  return music_get_playback_state();
#else
  return MusicPlayStateUnknown;
#endif
}