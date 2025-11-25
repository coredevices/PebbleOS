/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "vibe_client.h"

#include "applib/ui/vibes.h"
#include "services/normal/notifications/alerts_preferences_private.h"
#include "services/normal/vibes/vibe_score.h"
#include "services/normal/vibes/vibe_score_info.h"
#include "system/logging.h"

static VibeScoreId prv_get_resource_for_client(VibeClient client) {
  if (client == VibeClient_AlarmsLPM) {
    return VibeScoreId_AlarmsLPM;
  }
  return alerts_preferences_get_vibe_score_for_client(client);
}

VibeScore *vibe_client_get_score(VibeClient client) {
  VibeScoreId id = prv_get_resource_for_client(client);
  if (id == VibeScoreId_Disabled) {
    return NULL;
  }
  VibeScore *score = vibe_score_create_with_resource(vibe_score_info_get_resource_id(id));
  if (!score) {
    PBL_LOG(LOG_LEVEL_ERROR, "Got a null VibeScore resource!");
  }
  return score;
}
