/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/system_version_capabilities.h"

#include "pbl/services/notifications/notification_image.h"
#include "shell/system_app_ids.auto.h"

PebbleProtocolCapabilities system_version_get_capabilities(void) {
  PebbleProtocolCapabilities capabilities = {};
  capabilities.run_state_support = 1;
  capabilities.infinite_log_dumping_support = 1;
  capabilities.extended_music_service = 1;
  capabilities.extended_notification_service = 1;
  capabilities.lang_pack_support = 1;
  capabilities.app_message_8k_support = 1;
  capabilities.activity_insights_support = 1;
  capabilities.voice_api_support = 1;
  capabilities.unread_coredump_support = 1;
#ifdef APP_ID_SEND_TEXT
  capabilities.send_text_support = (APP_ID_SEND_TEXT != INSTALL_ID_INVALID) ? 1 : 0;
#endif
  capabilities.notification_filtering_support = 1;
#ifdef APP_ID_WEATHER
  capabilities.weather_app_support = (APP_ID_WEATHER != INSTALL_ID_INVALID) ? 1 : 0;
#endif
#ifdef APP_ID_REMINDERS
  capabilities.reminders_app_support = (APP_ID_REMINDERS != INSTALL_ID_INVALID) ? 1 : 0;
#endif
#ifdef APP_ID_WORKOUT
  capabilities.workout_app_support = (APP_ID_WORKOUT != INSTALL_ID_INVALID) ? 1 : 0;
#endif
  capabilities.continue_fw_install_across_disconnect_support = 1;
  capabilities.smooth_fw_install_progress_support = 1;
  capabilities.custom_vibe_pattern_support = 1;
  capabilities.blob_db_version_support = 1;
  capabilities.weather_db_v4_support = 1;
  capabilities.notification_image_support = NOTIFICATION_IMAGE_SUPPORTED;
  capabilities.music_output_routing_support = 1;
  return capabilities;
}
