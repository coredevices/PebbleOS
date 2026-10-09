/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <applib/ui/kino/kino_player.h>

void PBL_WEAK kino_player_deinit(KinoPlayer *player) {
}

void PBL_WEAK kino_player_set_callbacks(KinoPlayer *player, KinoPlayerCallbacks callbacks,
                                        void *context) {
}

KinoReel *PBL_WEAK kino_player_get_reel(KinoPlayer *player) {
  return nullptr;
}

void PBL_WEAK kino_player_play(KinoPlayer *player) {
}

void PBL_WEAK kino_player_pause(KinoPlayer *player) {
}

void PBL_WEAK kino_player_rewind(KinoPlayer *player) {
}

void PBL_WEAK kino_player_set_reel(KinoPlayer *player, KinoReel *reel, bool take_ownership) {
}
