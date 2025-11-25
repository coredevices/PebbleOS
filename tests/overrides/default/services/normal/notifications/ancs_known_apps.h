/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// @nolint
// please don't change these values manually, they are derived from the spreadsheet
// "Notification Colors"

#if PLATFORM_TINTIN
// Tintin does not have the color arg in its App Metadata. Remove it.
#define APP(id, icon, color) { id, icon }
#else
#define APP(id, icon, color) { id, icon, color }
#endif
    APP(IOS_SMS_APP_ID, TIMELINE_RESOURCE_GENERIC_SMS, GColorIslamicGreenARGB8),

#undef APP
