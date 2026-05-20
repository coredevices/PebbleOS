/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/music.h"
#include "syscall/syscall_internal.h"

DEFINE_SYSCALL(void, sys_music_command_send, MusicCommand command) {
  music_command_send(command);
}

DEFINE_SYSCALL(uint8_t, sys_music_get_volume_percent, void) {
  return music_service_get_volume_percent();
}
