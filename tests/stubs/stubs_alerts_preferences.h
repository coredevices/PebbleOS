/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/notifications/alerts_preferences_private.h"
#include "pbl/kernel/compiler.h"

VibeScoreId PBL_WEAK alerts_preferences_get_vibe_score_for_client(VibeClient client) {
  return VibeScoreId_Invalid;
}

VibeIntensity PBL_WEAK alerts_preferences_get_vibe_intensity(void) {
  return VibeIntensityLow;
}

bool PBL_WEAK alerts_preferences_get_notification_alternative_design(void) {
  return false;
}

DndNotificationMode PBL_WEAK alerts_preferences_dnd_get_show_notifications(void) {
  return DndNotificationModeShow;
}

bool PBL_WEAK alerts_preferences_dnd_get_auto_dismiss(void) {
  return false;
}

bool PBL_WEAK alerts_preferences_get_notification_vibe_delay(void) {
  return false;
}

NotificationStatusBarStyle PBL_WEAK alerts_preferences_get_notification_status_bar_style(void) {
  return NotificationStatusBarStyle_Default;
}

PreferredContentSize PBL_WEAK alerts_preferences_get_notification_content_size(void) {
  return NotificationContentSizeSystem;
}

NotificationHoldSelectAction PBL_WEAK alerts_preferences_get_notification_hold_select_action(void) {
  return NotificationHoldSelectAction_DismissAll;
}
