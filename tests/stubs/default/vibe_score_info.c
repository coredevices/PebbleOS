/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/vibes/vibe_score_info.h>

PBL_WEAK bool vibe_score_info_is_valid(VibeScoreId id) {
  return true;
}

PBL_WEAK const char *vibe_score_info_get_name(VibeScoreId id) {
  return "test";
}

PBL_WEAK uint32_t vibe_score_info_get_resource_id(VibeScoreId id) {
  return 0;
}
