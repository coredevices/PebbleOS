/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_notifications_notification_constants Notification constants
 * @ingroup services_notifications
 * @brief Constants shared by the SMS reply and phone call UIs.
 * @{
 */

/** @brief Highlight color of SMS reply menus. */
#define SMS_REPLY_COLOR GColorIslamicGreen

/** @brief Notification preferences key holding the Send Text app's reply actions. */
#define SEND_TEXT_NOTIF_PREF_KEY "com.pebble.sendText"

/** @brief Notification preferences key holding incoming call replies on Android. */
#define ANDROID_PHONE_KEY "com.pebble.android.phone"
/** @brief Notification preferences key holding incoming call replies on iOS. */
#define IOS_PHONE_KEY "com.apple.mobilephone"

/** @} */
