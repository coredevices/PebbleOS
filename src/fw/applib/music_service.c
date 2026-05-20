/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "music_service.h"
#include "syscall/syscall.h"
#include "pbl/services/music.h"

void music_volume_up(void) {
  sys_music_command_send(MusicCommandVolumeUp);
}

void music_volume_down(void) {
  sys_music_command_send(MusicCommandVolumeDown);
}

uint8_t music_get_volume_percent(void) {
  return sys_music_get_volume_percent();
}
