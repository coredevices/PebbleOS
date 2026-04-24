/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "vibe_score.h"

typedef enum VibeClient {
  VibeClient_Notifications = 0,
  VibeClient_PhoneCalls,
  VibeClient_Alarms,
  VibeClient_AlarmsLPM
} VibeClient;

// Returns the appropriate vibe score for the client.
// This is determined from alert preferences.
VibeScore *vibe_client_get_score(VibeClient client);
