/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup pp_protocol Wire protocol
 * @ingroup pp
 * @brief Pebble protocol message framing, endpoint IDs and capabilities.
 * @{
 */

/** @brief Message header, big endian on the wire. */
struct PBL_PACKED pbl_pp_header {
  /** Payload length in bytes. */
  uint16_t length;
  /** Endpoint ID. */
  uint16_t endpoint_id;
};

/** @brief Largest inbound payload accepted from the Pebble mobile app. */
#define PBL_PP_MAX_INBOUND_PAYLOAD_PRIVATE 2044
/** @brief Largest inbound payload accepted from third party phone apps. */
#define PBL_PP_MAX_INBOUND_PAYLOAD_PUBLIC 144
/** @brief Largest outbound payload the phone accepts. */
#define PBL_PP_MAX_OUTBOUND_PAYLOAD 656

/**
 * @name Endpoint IDs
 *
 * Written as four hex digits: PBL_PP_ENDPOINT_DEFINE() turns the ID into a symbol name, so that
 * defining an endpoint twice fails to link.
 * @{
 */
#define PBL_PP_ENDPOINT_META                 0x0000
#define PBL_PP_ENDPOINT_CLOCK                0x000b
#define PBL_PP_ENDPOINT_SYSTEM_VERSION       0x0010
#define PBL_PP_ENDPOINT_REMOTE_VERSION       0x0011
#define PBL_PP_ENDPOINT_SYSTEM_MESSAGE       0x0012
#define PBL_PP_ENDPOINT_MUSIC                0x0020
#define PBL_PP_ENDPOINT_PHONE                0x0021
#define PBL_PP_ENDPOINT_APP_MESSAGE          0x0030
#define PBL_PP_ENDPOINT_LAUNCHER_APP_MESSAGE 0x0031
#define PBL_PP_ENDPOINT_APP_CUSTOMIZE        0x0032
#define PBL_PP_ENDPOINT_BLE_CONTROL          0x0033
#define PBL_PP_ENDPOINT_APP_RUN_STATE        0x0034
#define PBL_PP_ENDPOINT_IMAGING              0x0035
#define PBL_PP_ENDPOINT_HEALTH_SYNC          0x038f
#define PBL_PP_ENDPOINT_PING                 0x07d1
#define PBL_PP_ENDPOINT_LOG_DUMP             0x07d2
#define PBL_PP_ENDPOINT_RESET                0x07d3
#define PBL_PP_ENDPOINT_APP_LOG              0x07d6
#define PBL_PP_ENDPOINT_FACTORY_REGISTRY     0x1389
#define PBL_PP_ENDPOINT_APP_FETCH            0x1771
#define PBL_PP_ENDPOINT_DATA_LOGGING         0x1a7a
#define PBL_PP_ENDPOINT_SCREENSHOT           0x1f40
#define PBL_PP_ENDPOINT_GET_BYTES            0x2328
#define PBL_PP_ENDPOINT_AUDIO                0x2710
#define PBL_PP_ENDPOINT_VOICE                0x2af8
#define PBL_PP_ENDPOINT_TIMELINE_ACTION      0x2cb0
#define PBL_PP_ENDPOINT_APP_ORDER            0xabcd
#define PBL_PP_ENDPOINT_BLOB_DB              0xb1db
#define PBL_PP_ENDPOINT_BLOB_DB2             0xb2db
#define PBL_PP_ENDPOINT_PUT_BYTES            0xbeef
#define PBL_PP_ENDPOINT_POLL_REMOTE          0xcafe
#define PBL_PP_ENDPOINT_XSBUG                0xcaff
#define PBL_PP_ENDPOINT_REMOTE_INPUT         0xf00d
/** @} */

/** @brief Protocol capabilities announced by the phone, one bit each. */
enum pbl_pp_capability : uint32_t {
  /** App run state endpoint. */
  PBL_PP_CAP_RUN_STATE = 1U << 0,
  /** Infinite log dumping. */
  PBL_PP_CAP_INFINITE_LOG_DUMPING = 1U << 1,
  /** Extended music service. */
  PBL_PP_CAP_EXTENDED_MUSIC = 1U << 2,
  /** Extended notification service. */
  PBL_PP_CAP_EXTENDED_NOTIFICATIONS = 1U << 3,
  /** Language pack installation. */
  PBL_PP_CAP_LANGUAGE_PACK = 1U << 4,
  /** 8 KiB AppMessage buffers. */
  PBL_PP_CAP_APP_MESSAGE_8K = 1U << 5,
  /** Activity insights. */
  PBL_PP_CAP_ACTIVITY_INSIGHTS = 1U << 6,
  /** Voice API. */
  PBL_PP_CAP_VOICE_API = 1U << 7,
  /** Send text. */
  PBL_PP_CAP_SEND_TEXT = 1U << 8,
  /** Notification filtering. */
  PBL_PP_CAP_NOTIFICATION_FILTERING = 1U << 9,
  /** Fetching unread coredumps. */
  PBL_PP_CAP_UNREAD_COREDUMP = 1U << 10,
  /** Weather app. */
  PBL_PP_CAP_WEATHER_APP = 1U << 11,
  /** Reminders app. */
  PBL_PP_CAP_REMINDERS_APP = 1U << 12,
  /** Workout app. */
  PBL_PP_CAP_WORKOUT_APP = 1U << 13,
  /** Smooth firmware install progress. */
  PBL_PP_CAP_SMOOTH_FW_INSTALL_PROGRESS = 1U << 14,
  /** Phone serves images through the imaging endpoint. */
  PBL_PP_CAP_IMAGING = 1U << 17,
  /** Phone syncs settings through BlobDB. */
  PBL_PP_CAP_SETTINGS_SYNC = 1U << 23,
  /** Phone tracks which notification the watch is displaying. */
  PBL_PP_CAP_NOTIFICATION_WINDOW_STATE = 1U << 26,
};

/** @brief Capabilities as sent by the phone, bit for bit as @ref pbl_pp_capability. */
struct PBL_PACKED pbl_pp_capabilities {
  union {
    struct PBL_PACKED {
      bool run_state_support : 1;
      bool infinite_log_dumping_support : 1;
      bool extended_music_service : 1;
      bool extended_notification_service : 1;
      bool lang_pack_support : 1;
      bool app_message_8k_support : 1;
      bool activity_insights_support : 1;
      bool voice_api_support : 1;
      bool send_text_support : 1;
      bool notification_filtering_support : 1;
      bool unread_coredump_support : 1;
      bool weather_app_support : 1;
      bool reminders_app_support : 1;
      bool workout_app_support : 1;
      bool smooth_fw_install_progress_support : 1;
      bool custom_vibe_pattern_support : 1;
      uint8_t javascript_bytecode_version_appended : 1;
      bool imaging_support : 1;
      bool notification_image_support : 1;
      uint8_t more_padded_bits : 2;
      bool continue_fw_install_across_disconnect_support : 1;
      bool blob_db_version_support : 1;
      bool settings_sync_support : 1;
      bool weather_db_v4_support : 1;
      bool unknown_attributes_support : 1;
      bool notification_window_state_support : 1;
      bool app_permissions_support : 1;
      bool multiple_language_packs_support : 1;
    };
    /** All capability bits. */
    uint64_t flags;
  };
};

/** @brief Operating system of the phone, as reported by the Pebble mobile app. */
enum pbl_pp_remote_os : uint8_t {
  PBL_PP_REMOTE_OS_UNKNOWN = 0,
  PBL_PP_REMOTE_OS_IOS = 1,
  PBL_PP_REMOTE_OS_ANDROID = 2,
  PBL_PP_REMOTE_OS_OSX = 3,
  PBL_PP_REMOTE_OS_LINUX = 4,
  PBL_PP_REMOTE_OS_WINDOWS = 5,
};

/** @brief Bits of the reported platform bitfield that hold a @ref pbl_pp_remote_os. */
#define PBL_PP_REMOTE_OS_MASK 0x7U

/** @} */
