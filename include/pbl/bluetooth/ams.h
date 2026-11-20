/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/util/bits.h>

/**
 * @defgroup bluetooth_ams AMS
 * @ingroup bluetooth
 * @brief The Apple Media Service: its values and formats.
 * @{
 */

/**
 * @name UUIDs
 *
 * As their bytes in the order they go over the air: for @c BLE_UUID128_INIT() or advertising
 * data.
 * @{
 */
/** The service: 89D3502B-0F36-433A-8EF4-C502AD55F8DC. */
#define PBL_BT_AMS_UUID_SERVICE \
  0xdc, 0xf8, 0x55, 0xad, 0x02, 0xc5, 0xf4, 0x8e, 0x3a, 0x43, 0x36, 0x0f, 0x2b, 0x50, 0xd3, 0x89
/** Remote Command: 9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2. */
#define PBL_BT_AMS_UUID_REMOTE_COMMAND \
  0xc2, 0x51, 0xca, 0xf7, 0x56, 0x0e, 0xdf, 0xb8, 0x8a, 0x4a, 0xb1, 0x57, 0xd8, 0x81, 0x3c, 0x9b
/** Entity Update: 2F7CABCE-808D-411F-9A0C-BB92BA96C102. */
#define PBL_BT_AMS_UUID_ENTITY_UPDATE \
  0x02, 0xc1, 0x96, 0xba, 0x92, 0xbb, 0x0c, 0x9a, 0x1f, 0x41, 0x8d, 0x80, 0xce, 0xab, 0x7c, 0x2f
/** Entity Attribute: C6B2F38C-23AB-46D8-A6AB-A3A870BBD5D7. */
#define PBL_BT_AMS_UUID_ENTITY_ATTRIBUTE \
  0xd7, 0xd5, 0xbb, 0x70, 0xa8, 0xa3, 0xab, 0xa6, 0xd8, 0x46, 0xab, 0x23, 0x8c, 0xf3, 0xb2, 0xc6
/** @} */

/** @brief Commands for the media player. */
enum pbl_bt_ams_command {
  PBL_BT_AMS_COMMAND_PLAY = 0,
  PBL_BT_AMS_COMMAND_PAUSE = 1,
  PBL_BT_AMS_COMMAND_TOGGLE_PLAY_PAUSE = 2,
  PBL_BT_AMS_COMMAND_NEXT_TRACK = 3,
  PBL_BT_AMS_COMMAND_PREVIOUS_TRACK = 4,
  PBL_BT_AMS_COMMAND_VOLUME_UP = 5,
  PBL_BT_AMS_COMMAND_VOLUME_DOWN = 6,
  PBL_BT_AMS_COMMAND_ADVANCE_REPEAT_MODE = 7,
  PBL_BT_AMS_COMMAND_ADVANCE_SHUFFLE_MODE = 8,
  PBL_BT_AMS_COMMAND_SKIP_FORWARD = 9,
  PBL_BT_AMS_COMMAND_SKIP_BACKWARD = 10,
  PBL_BT_AMS_COMMAND_LIKE_TRACK = 11,
  PBL_BT_AMS_COMMAND_DISLIKE_TRACK = 12,
  PBL_BT_AMS_COMMAND_BOOKMARK_TRACK = 13,
};

/** @brief What the service tells about. */
enum pbl_bt_ams_entity {
  /** The media player: its attributes are an enum pbl_bt_ams_player_attr. */
  PBL_BT_AMS_ENTITY_PLAYER = 0,
  /** The playback queue: its attributes are an enum pbl_bt_ams_queue_attr. */
  PBL_BT_AMS_ENTITY_QUEUE = 1,
  /** The track: its attributes are an enum pbl_bt_ams_track_attr. */
  PBL_BT_AMS_ENTITY_TRACK = 2,
};

/** @brief Attributes of the media player. */
enum pbl_bt_ams_player_attr {
  /** The name of the app. */
  PBL_BT_AMS_PLAYER_ATTR_NAME = 0,
  /**
   * Three values with commas in between: the playback state (an enum
   * pbl_bt_ams_playback_state), the playback rate, and the elapsed time in seconds when the
   * device sent it, the last two with decimals.
   */
  PBL_BT_AMS_PLAYER_ATTR_PLAYBACK_INFO = 1,
  /** From 0 (silent) to 1 (full volume), with decimals. */
  PBL_BT_AMS_PLAYER_ATTR_VOLUME = 2,
  /** Not in the specification: the bundle identifier of the app, which iOS has since 8.3. */
  PBL_BT_AMS_PLAYER_ATTR_BUNDLE_IDENTIFIER = 3,
};

/** @brief Playback states, as the playback info gives them. */
enum pbl_bt_ams_playback_state {
  PBL_BT_AMS_PLAYBACK_STATE_PAUSED = 0,
  PBL_BT_AMS_PLAYBACK_STATE_PLAYING = 1,
  PBL_BT_AMS_PLAYBACK_STATE_REWINDING = 2,
  PBL_BT_AMS_PLAYBACK_STATE_FAST_FORWARDING = 3,
};

/** @brief Attributes of the playback queue: each an integer. */
enum pbl_bt_ams_queue_attr {
  /** Index of the track in the queue, from 0. */
  PBL_BT_AMS_QUEUE_ATTR_INDEX = 0,
  /** Number of tracks in the queue. */
  PBL_BT_AMS_QUEUE_ATTR_COUNT = 1,
  /** An enum pbl_bt_ams_shuffle_mode. */
  PBL_BT_AMS_QUEUE_ATTR_SHUFFLE_MODE = 2,
  /** An enum pbl_bt_ams_repeat_mode. */
  PBL_BT_AMS_QUEUE_ATTR_REPEAT_MODE = 3,
};

/** @brief Shuffle modes of the queue. */
enum pbl_bt_ams_shuffle_mode {
  PBL_BT_AMS_SHUFFLE_MODE_OFF = 0,
  PBL_BT_AMS_SHUFFLE_MODE_ONE = 1,
  PBL_BT_AMS_SHUFFLE_MODE_ALL = 2,
};

/** @brief Repeat modes of the queue. */
enum pbl_bt_ams_repeat_mode {
  PBL_BT_AMS_REPEAT_MODE_OFF = 0,
  PBL_BT_AMS_REPEAT_MODE_ONE = 1,
  PBL_BT_AMS_REPEAT_MODE_ALL = 2,
};

/** @brief Attributes of the track. */
enum pbl_bt_ams_track_attr {
  PBL_BT_AMS_TRACK_ATTR_ARTIST = 0,
  PBL_BT_AMS_TRACK_ATTR_ALBUM = 1,
  PBL_BT_AMS_TRACK_ATTR_TITLE = 2,
  /** In seconds, with decimals. */
  PBL_BT_AMS_TRACK_ATTR_DURATION = 3,
};

/**
 * @name Flags of an update
 * @{
 */
/** The value did not fit and was cut: the Entity Attribute has all of it. */
#define PBL_BT_AMS_FLAG_TRUNCATED PBL_BIT(0)
/** @} */

/**
 * @name ATT error codes of the service
 * @{
 */
/** E.g. the Entity Update was written to before subscribing to it. */
#define PBL_BT_AMS_ERR_INVALID_STATE   0xA0
#define PBL_BT_AMS_ERR_INVALID_COMMAND 0xA1
/** The attribute is empty. */
#define PBL_BT_AMS_ERR_ABSENT_ATTRIBUTE 0xA2
/** @} */

/** @brief The value of an attribute, as the Entity Update tells it. */
struct pbl_bt_ams_update {
  /** An enum pbl_bt_ams_entity. */
  uint8_t entity;
  /** The attribute of the entity. */
  uint8_t attr;
  /** @c PBL_BT_AMS_FLAG_*, with the bits the specification reserves as they came. */
  uint8_t flags;
  /** Length of the value. */
  uint16_t len;
  /** The value: UTF-8, not terminated. */
  const uint8_t *value;
};

/** @} */
