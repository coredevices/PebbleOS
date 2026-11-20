/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/util/bits.h>

/**
 * @defgroup bluetooth_ancs ANCS
 * @ingroup bluetooth
 * @brief The Apple Notification Center Service: its values and formats.
 * @{
 */

/**
 * @name UUIDs
 *
 * As their bytes in the order they go over the air: for @c BLE_UUID128_INIT() or advertising
 * data.
 * @{
 */
/** The service: 7905F431-B5CE-4E99-A40F-4B1E122D00D0. */
#define PBL_BT_ANCS_UUID_SERVICE \
  0xd0, 0x00, 0x2d, 0x12, 0x1e, 0x4b, 0x0f, 0xa4, 0x99, 0x4e, 0xce, 0xb5, 0x31, 0xf4, 0x05, 0x79
/** Notification Source: 9FBF120D-6301-42D9-8C58-25E699A21DBD. */
#define PBL_BT_ANCS_UUID_NOTIFICATION_SOURCE \
  0xbd, 0x1d, 0xa2, 0x99, 0xe6, 0x25, 0x58, 0x8c, 0xd9, 0x42, 0x01, 0x63, 0x0d, 0x12, 0xbf, 0x9f
/** Control Point: 69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9. */
#define PBL_BT_ANCS_UUID_CONTROL_POINT \
  0xd9, 0xd9, 0xaa, 0xfd, 0xbd, 0x9b, 0x21, 0x98, 0xa8, 0x49, 0xe1, 0x45, 0xf3, 0xd8, 0xd1, 0x69
/** Data Source: 22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB. */
#define PBL_BT_ANCS_UUID_DATA_SOURCE \
  0xfb, 0x7b, 0x7c, 0xce, 0x6a, 0xb3, 0x44, 0xbe, 0xb5, 0x4b, 0xd6, 0x24, 0xe9, 0xc6, 0xea, 0x22
/** @} */

/** @brief What happened to a notification. */
enum pbl_bt_ancs_event {
  PBL_BT_ANCS_EVENT_ADDED = 0,
  PBL_BT_ANCS_EVENT_MODIFIED = 1,
  PBL_BT_ANCS_EVENT_REMOVED = 2,
};

/**
 * @name Event flags
 * @{
 */
#define PBL_BT_ANCS_FLAG_SILENT    PBL_BIT(0)
#define PBL_BT_ANCS_FLAG_IMPORTANT PBL_BIT(1)
/** The notification was there before the client subscribed. */
#define PBL_BT_ANCS_FLAG_PRE_EXISTING    PBL_BIT(2)
#define PBL_BT_ANCS_FLAG_POSITIVE_ACTION PBL_BIT(3)
#define PBL_BT_ANCS_FLAG_NEGATIVE_ACTION PBL_BIT(4)
/** Not in the specification: iOS sets it when the notification has an attachment. */
#define PBL_BT_ANCS_FLAG_MULTIMEDIA PBL_BIT(5)
/** @} */

/** @brief Category of a notification. */
enum pbl_bt_ancs_category {
  PBL_BT_ANCS_CATEGORY_OTHER = 0,
  PBL_BT_ANCS_CATEGORY_INCOMING_CALL = 1,
  PBL_BT_ANCS_CATEGORY_MISSED_CALL = 2,
  PBL_BT_ANCS_CATEGORY_VOICEMAIL = 3,
  PBL_BT_ANCS_CATEGORY_SOCIAL = 4,
  PBL_BT_ANCS_CATEGORY_SCHEDULE = 5,
  PBL_BT_ANCS_CATEGORY_EMAIL = 6,
  PBL_BT_ANCS_CATEGORY_NEWS = 7,
  PBL_BT_ANCS_CATEGORY_HEALTH_AND_FITNESS = 8,
  PBL_BT_ANCS_CATEGORY_BUSINESS_AND_FINANCE = 9,
  PBL_BT_ANCS_CATEGORY_LOCATION = 10,
  PBL_BT_ANCS_CATEGORY_ENTERTAINMENT = 11,
};

/** @brief Attributes of a notification. */
enum pbl_bt_ancs_notif_attr {
  PBL_BT_ANCS_NOTIF_ATTR_APP_IDENTIFIER = 0,
  /** Asked for with a maximum length. */
  PBL_BT_ANCS_NOTIF_ATTR_TITLE = 1,
  /** Asked for with a maximum length. */
  PBL_BT_ANCS_NOTIF_ATTR_SUBTITLE = 2,
  /** Asked for with a maximum length. */
  PBL_BT_ANCS_NOTIF_ATTR_MESSAGE = 3,
  PBL_BT_ANCS_NOTIF_ATTR_MESSAGE_SIZE = 4,
  /** As yyyyMMdd'T'HHmmSS, in the time zone of the device. */
  PBL_BT_ANCS_NOTIF_ATTR_DATE = 5,
  PBL_BT_ANCS_NOTIF_ATTR_POSITIVE_ACTION_LABEL = 6,
  PBL_BT_ANCS_NOTIF_ATTR_NEGATIVE_ACTION_LABEL = 7,
};

/** @brief Attributes of an app. */
enum pbl_bt_ancs_app_attr {
  PBL_BT_ANCS_APP_ATTR_DISPLAY_NAME = 0,
};

/** @brief Actions on a notification. */
enum pbl_bt_ancs_action {
  PBL_BT_ANCS_ACTION_POSITIVE = 0,
  PBL_BT_ANCS_ACTION_NEGATIVE = 1,
};

/**
 * @name ATT error codes of the Control Point
 * @{
 */
#define PBL_BT_ANCS_ERR_UNKNOWN_COMMAND 0xA0
#define PBL_BT_ANCS_ERR_INVALID_COMMAND 0xA1
/** E.g. the notification does not exist (any more). */
#define PBL_BT_ANCS_ERR_INVALID_PARAMETER 0xA2
#define PBL_BT_ANCS_ERR_ACTION_FAILED     0xA3
/** @} */

/** @brief A notification of the device, as its Notification Source tells it. */
struct pbl_bt_ancs_notif {
  /** Identifies the notification for as long as the device is connected. */
  uint32_t uid;
  /** What happened to it, an enum pbl_bt_ancs_event. */
  uint8_t event;
  /** @c PBL_BT_ANCS_FLAG_*, with the bits the specification reserves as they came. */
  uint8_t flags;
  /** An enum pbl_bt_ancs_category. */
  uint8_t category;
  /** Number of notifications the device has in the category. */
  uint8_t category_count;
};

/** @brief An attribute, as the service sends it. */
struct PBL_PACKED pbl_bt_ancs_attr {
  /** Which one: an enum pbl_bt_ancs_notif_attr or an enum pbl_bt_ancs_app_attr. */
  uint8_t id;
  /** Length of the value. */
  uint16_t len;
  /** The value: UTF-8, not terminated. */
  uint8_t value[];
};

/** @} */
