/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/vibes/vibe_client.h>

PBL_WEAK VibeScore *vibe_client_get_score(VibeClient client) {
  return nullptr;
}
