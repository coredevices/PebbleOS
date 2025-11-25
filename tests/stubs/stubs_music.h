/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/music.h"
#include "util/attributes.h"

void WEAK music_get_now_playing(char* title, char* artist, char* album) {}

MusicPlayState WEAK music_get_playback_state(void) {
  return MusicPlayStateUnknown;
}

uint32_t WEAK music_get_ms_since_pos_last_updated(void) {
  return 0;
}
