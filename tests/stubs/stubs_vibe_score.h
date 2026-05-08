/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/vibes/vibe_score.h"

VibeScore *vibe_score_create_with_resource_system(ResAppNum app_num, uint32_t resource_id) {
  return NULL;
}

void vibe_score_do_vibe(VibeScore *score) {}

void vibe_score_destroy(VibeScore *score) {}
