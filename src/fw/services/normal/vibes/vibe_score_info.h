/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/vibes/vibe_client.h"

#include <stdbool.h>
#include <stdint.h>

#define VIBE_DEF(identifier, enum_name, name_str, alert_types_arg, res_id)\
  VibeScoreId_##enum_name = identifier,
typedef enum VibeScoreId {
  VibeScoreId_Invalid = 0,
  #include "vibes.def"
} VibeScoreId;
#undef VIBE_DEF

#if PLATFORM_SPALDING
#define DEFAULT_VIBE_SCORE_NOTIFS (VibeScoreId_Pulse)
#define DEFAULT_VIBE_SCORE_INCOMING_CALLS (VibeScoreId_Pulse)
#define DEFAULT_VIBE_SCORE_ALARMS (VibeScoreId_Pulse)
#elif PLATFORM_ASTERIX
#define DEFAULT_VIBE_SCORE_NOTIFS (VibeScoreId_StandardShortPulseHigh)
#define DEFAULT_VIBE_SCORE_INCOMING_CALLS (VibeScoreId_Pulse)
#define DEFAULT_VIBE_SCORE_ALARMS (VibeScoreId_Reveille)
#else
#define DEFAULT_VIBE_SCORE_NOTIFS (VibeScoreId_NudgeNudge)
#define DEFAULT_VIBE_SCORE_INCOMING_CALLS (VibeScoreId_Pulse)
#define DEFAULT_VIBE_SCORE_ALARMS (VibeScoreId_Reveille)
#endif

// Returns the ResourceId for the VibeScore represented by this id.
// If the id does not exist, the ResourceId of the first vibe in S_VIBE_MAP is returned
uint32_t vibe_score_info_get_resource_id(VibeScoreId id);

// Returns the name of the VibeScore represented by this id
// If the id does not exist, the name of the first vibe in S_VIBE_MAP is returned
const char *vibe_score_info_get_name(VibeScoreId id);

// Returns the next vibe score playable by the client from the array defined by vibes.def
// Wraps around and continues searching if the end of the array is reached
// Returns current_id if there is no next vibe score
VibeScoreId vibe_score_info_cycle_next(VibeClient client, VibeScoreId curr_id);

// Checks if the vibe score id exists and if the associated VibeScoreInfo contains a valid
// resource_id
bool vibe_score_info_is_valid(VibeScoreId id);
