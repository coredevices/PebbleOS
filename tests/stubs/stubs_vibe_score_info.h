/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/normal/vibes/vibe_score_info.h"

bool vibe_score_info_is_valid(VibeScoreId id) {
  return true;
}

const char *vibe_score_info_get_name(VibeScoreId id) {
  return "test";
}
