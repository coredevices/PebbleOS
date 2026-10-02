/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup services_comm_session_session_remote_version Remote version
 * @ingroup services_comm_session
 * @brief Phone version and capabilities exchange.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct CommSession CommSession;
/** @endcond */

/**
 * @brief Pebble Protocol capability flags.
 *
 * The watch announces its own set in its version response; the phone returns the endpoints it
 * supports over their deprecated counterparts. Bit positions match @ref CommSessionCapability.
 */
typedef struct PBL_PACKED {
  union {
    struct PBL_PACKED {
      /** App run state endpoint. */
      bool run_state_support : 1;
      /** Infinite log dumping. */
      bool infinite_log_dumping_support : 1;
      /** Extended music service. */
      bool extended_music_service : 1;
      /** Extended notification service. */
      bool extended_notification_service : 1;
      /** Language pack installation. */
      bool lang_pack_support : 1;
      /** 8 KiB AppMessage buffers. */
      bool app_message_8k_support : 1;
      /** Activity insights. */
      bool activity_insights_support : 1;
      /** Voice API. */
      bool voice_api_support : 1;
      /** Send text. */
      bool send_text_support : 1;
      /** Notification filtering. */
      bool notification_filtering_support : 1;
      /** Fetching unread coredumps. */
      bool unread_coredump_support : 1;
      /** Weather app. */
      bool weather_app_support : 1;
      /** Reminders app. */
      bool reminders_app_support : 1;
      /** Workout app. */
      bool workout_app_support : 1;
      /** Smooth firmware install progress. */
      bool smooth_fw_install_progress_support : 1;
      /** Custom vibration patterns. */
      bool custom_vibe_pattern_support : 1;
      /** JavaScript bytecode version appended to the version message. */
      uint8_t javascript_bytecode_version_appended : 1;
      /** Phone serves images (album art, ...) via the imaging endpoint. */
      bool imaging_support : 1;
      /** Watch renders notifications with an image aspect ratio attribute. */
      bool notification_image_support : 1;
      /** Reserved. */
      uint8_t more_padded_bits : 2;
      /** Firmware install continues across disconnections. */
      bool continue_fw_install_across_disconnect_support : 1;
      /** BlobDB version exchange. */
      bool blob_db_version_support : 1;
      /** Phone syncs settings through BlobDB. */
      bool settings_sync_support : 1;
      /** Phone writes the v4 (rich forecast) weather BlobDB record. */
      bool weather_db_v4_support : 1;
      /** Watch skips timeline attributes with unknown IDs. */
      bool unknown_attributes_support : 1;
    };
    /** All flags as a bitset. */
    uint64_t flags;
  };
} PebbleProtocolCapabilities;

/**
 * @brief Request the phone's version and capabilities on a newly opened session.
 *
 * The response updates the session capabilities. The caller must hold bt_lock().
 *
 * @param session Session to query.
 */
void session_remote_version_start_requests(CommSession *session);

/** @} */
