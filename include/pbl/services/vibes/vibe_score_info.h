/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/services/vibes/vibe_client.h>

/**
 * @defgroup services_vibes_vibe_score_info Vibe score catalog
 * @ingroup services_vibes
 * @brief Built-in vibe scores, their names and the alerts they can be used for.
 *
 * The catalog is defined in @c vibes.def. Score IDs are persisted in preferences, so they are
 * never reused.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define VIBE_DEF(identifier, enum_name, name_str, alert_types_arg, res_id) \
  VibeScoreId_##enum_name = identifier,
/** @endcond */
/**
 * @brief ID of a built-in vibe score.
 *
 * Generated from @c vibes.def: besides @ref VibeScoreId_Invalid, each entry defines a value named
 * after the score, e.g. @c VibeScoreId_Disabled or @c VibeScoreId_Pulse.
 */
typedef enum VibeScoreId {
  /** Invalid ID, never used by a score. */
  VibeScoreId_Invalid = 0,
/** @cond INTERNAL_HIDDEN */
#include "vibes.def"
  /** @endcond */
} VibeScoreId;
/** @cond INTERNAL_HIDDEN */
#undef VIBE_DEF
/** @endcond */

#ifdef CONFIG_BOARD_ASTERIX
#define DEFAULT_VIBE_SCORE_NOTIFS         (VibeScoreId_StandardShortPulseHigh)
#define DEFAULT_VIBE_SCORE_INCOMING_CALLS (VibeScoreId_Pulse)
#define DEFAULT_VIBE_SCORE_ALARMS         (VibeScoreId_Reveille)
#define DEFAULT_VIBE_SCORE_HOURLY         (VibeScoreId_Disabled)
#define DEFAULT_VIBE_SCORE_ON_DISCONNECT  (VibeScoreId_Disabled)
#else
/** @brief Default score for notifications; @c VibeScoreId_StandardShortPulseHigh on Asterix. */
#define DEFAULT_VIBE_SCORE_NOTIFS         (VibeScoreId_NudgeNudge)
/** @brief Default score for incoming calls. */
#define DEFAULT_VIBE_SCORE_INCOMING_CALLS (VibeScoreId_Pulse)
/** @brief Default score for alarms. */
#define DEFAULT_VIBE_SCORE_ALARMS         (VibeScoreId_Reveille)
/** @brief Default score for the hourly vibration. */
#define DEFAULT_VIBE_SCORE_HOURLY         (VibeScoreId_Disabled)
/** @brief Default score for phone disconnection. */
#define DEFAULT_VIBE_SCORE_ON_DISCONNECT  (VibeScoreId_Disabled)
#endif

/**
 * @brief Get the resource of a vibe score.
 *
 * @param id Score ID.
 * @return System resource ID of the score, @c RESOURCE_ID_INVALID if unknown or for
 *         @c VibeScoreId_Disabled.
 */
uint32_t vibe_score_info_get_resource_id(VibeScoreId id);

/**
 * @brief Get the display name of a vibe score.
 *
 * @param id Score ID.
 * @return Name (untranslated where marked for translation), or "" if unknown.
 */
const char *vibe_score_info_get_name(VibeScoreId id);

/**
 * @brief Get the next vibe score available to an alert, in catalog order.
 *
 * Wraps around at the end of the catalog.
 *
 * @param client Kind of alert; @ref VibeClient_AlarmsLPM is not allowed.
 * @param curr_id Current score.
 * @return Next score, or @p curr_id if there is no other.
 */
VibeScoreId vibe_score_info_cycle_next(VibeClient client, VibeScoreId curr_id);

/**
 * @brief Check whether a vibe score ID can be used.
 *
 * @param id Score ID.
 * @return true if the score exists and has a resource, or is @c VibeScoreId_Disabled.
 */
bool vibe_score_info_is_valid(VibeScoreId id);

/** @} */
