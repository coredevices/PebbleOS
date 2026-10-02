/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "vibe_score.h"

/**
 * @defgroup services_vibes_vibe_client Vibe clients
 * @ingroup services_vibes
 * @brief Vibe score selected by the user for each kind of alert.
 * @{
 */

/** @brief Kind of alert that vibrates. */
typedef enum VibeClient {
  /** Notifications. */
  VibeClient_Notifications = 0,
  /** Incoming phone calls. */
  VibeClient_PhoneCalls,
  /** Alarms. */
  VibeClient_Alarms,
  /** Alarms in low power mode, which always use a fixed score. */
  VibeClient_AlarmsLPM,
  /** Hourly vibration. */
  VibeClient_Hourly,
  /** Phone disconnection. */
  VibeClient_OnDisconnect,
} VibeClient;

/**
 * @brief Load the vibe score for an alert, as set in the alert preferences.
 *
 * @param client Kind of alert.
 * @return Score to be freed with vibe_score_destroy(), or NULL if vibration is disabled for the
 *         client or the score could not be loaded.
 */
VibeScore *vibe_client_get_score(VibeClient client);

/** @} */
