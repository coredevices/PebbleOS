/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <string.h>
#include <time.h>

#include <pbl/bluetooth/ancs.h>

#include <applib/graphics/gtypes.h>

/**
 * @defgroup services_notifications_ancs_ancs_notifications_util ANCS utilities
 * @ingroup services_notifications_ancs
 * @brief App identification and timestamp parsing for ANCS notifications.
 * @{
 */

/** @brief Bundle identifier of the iOS Phone app. */
#define IOS_PHONE_APP_ID "com.apple.mobilephone"
/** @brief Bundle identifier of the iOS Calendar app. */
#define IOS_CALENDAR_APP_ID "com.apple.mobilecal"
/** @brief Bundle identifier of the iOS Reminders app. */
#define IOS_REMINDERS_APP_ID "com.apple.reminders"
/** @brief Bundle identifier of the iOS Mail app. */
#define IOS_MAIL_APP_ID "com.apple.mobilemail"
/** @brief Bundle identifier of the iOS Messages app. */
#define IOS_SMS_APP_ID "com.apple.MobileSMS"
/** @brief Bundle identifier of the iOS FaceTime app. */
#define IOS_FACETIME_APP_ID "com.apple.facetime"

/** @brief Presentation of a known iOS app. */
typedef struct ANCSAppMetadata {
  /** Bundle identifier, NULL for the generic entry used for unknown apps. */
  const char *app_id;
  /** Timeline resource id of the app icon. */
  uint32_t icon_id;
#if PBL_COLOR
  /** Background color, as GColor8 ARGB, or clear for none. */
  uint8_t app_color;
#endif
  /** Whether the app's notifications should always be ignored. */
  bool is_blocked : 1;
  /** Whether the app's notifications should never be ignored. */
  bool is_unblockable : 1;
} ANCSAppMetadata;

/**
 * @brief Copy the value of an attribute as a string.
 *
 * @param attr Attribute.
 * @param[out] str The string, with room for the value and its terminator.
 */
static inline void ancs_attr_to_cstring(const struct pbl_bt_ancs_attr *attr, char *str) {
  memcpy(str, attr->value, attr->len);
  str[attr->len] = '\0';
}

/** @brief Whether the value of an attribute is a string; false if either is NULL. */
static inline bool ancs_attr_equal_cstring(const struct pbl_bt_ancs_attr *attr, const char *str) {
  return attr && str && (attr->len == strlen(str)) && (memcmp(attr->value, str, attr->len) == 0);
}

/** @brief Whether two attributes have the same value; false if either is NULL. */
static inline bool ancs_attr_equal(const struct pbl_bt_ancs_attr *a,
                                   const struct pbl_bt_ancs_attr *b) {
  return a && b && (a->len == b->len) && (memcmp(a->value, b->value, a->len) == 0);
}

/**
 * @brief Look up the presentation of an app.
 *
 * @param app_id App identifier attribute.
 * @return Metadata of a known app, or a generic entry (NULL app_id, generic icon) otherwise.
 */
const ANCSAppMetadata *ancs_notifications_util_get_app_metadata(
    const struct pbl_bt_ancs_attr *app_id);

/**
 * @brief Parse an ANCS date attribute.
 *
 * The date is @c yyyyMMdd'T'HHmmSS in the phone's local time, which is assumed to match the
 * watch's timezone.
 *
 * @param timestamp_attr Date attribute.
 * @return UTC timestamp, or 0 if the attribute is malformed.
 */
time_t ancs_notifications_util_parse_timestamp(const struct pbl_bt_ancs_attr *timestamp_attr);

/**
 * @brief Check whether an app identifier is the iOS Phone app.
 *
 * @param app_id App identifier attribute, may be NULL.
 * @return true if it is the Phone app.
 */
bool ancs_notifications_util_is_phone(const struct pbl_bt_ancs_attr *app_id);

/**
 * @brief Check whether an app identifier is the iOS Messages app.
 *
 * @param app_id App identifier attribute, may be NULL.
 * @return true if it is the Messages app.
 */
bool ancs_notifications_util_is_sms(const struct pbl_bt_ancs_attr *app_id);

/**
 * @brief Check whether a notification is a group message.
 *
 * @param app_id App identifier attribute.
 * @param subtitle Subtitle attribute; group messages from the Messages app have one.
 * @return true for a Messages notification with a non-empty subtitle.
 */
bool ancs_notifications_util_is_group_sms(const struct pbl_bt_ancs_attr *app_id,
                                          const struct pbl_bt_ancs_attr *subtitle);

/** @} */
