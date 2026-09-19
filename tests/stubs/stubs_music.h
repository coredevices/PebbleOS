/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/music.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK music_get_now_playing(char *title, char *artist, char *album) {
}

MusicPlayState PBL_WEAK music_get_playback_state(void) {
  return MusicPlayStateUnknown;
}

uint32_t PBL_WEAK music_get_ms_since_pos_last_updated(void) {
  return 0;
}
