/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "comm/ble/kernel_le_client/ancs/ancs_types.h"
#include "util/attributes.h"
#include "util/time/time.h"

#define IOS_PHONE_APP_ID "com.apple.mobilephone"
#define IOS_CALENDAR_APP_ID "com.apple.mobilecal"
#define IOS_REMINDERS_APP_ID "com.apple.reminders"
#define IOS_MAIL_APP_ID "com.apple.mobilemail"
#define IOS_SMS_APP_ID "com.apple.MobileSMS"
#define IOS_FACETIME_APP_ID "com.apple.facetime"

typedef struct PACKED ANCSAppMetadata {
  const char *app_id;
  uint32_t icon_id;
#if PBL_COLOR
  uint8_t app_color;
#endif
  bool is_blocked:1; //<! Whether the app's notifications should always be ignored
  bool is_unblockable:1; //<! Whether the app's notifications should never be ignored
} ANCSAppMetadata;

const ANCSAppMetadata* ancs_notifications_util_get_app_metadata(const ANCSAttribute *app_id);

time_t ancs_notifications_util_parse_timestamp(const ANCSAttribute *timestamp_attr);

// Returns true if given app id is the iOS phone app
bool ancs_notifications_util_is_phone(const ANCSAttribute *app_id);

// Returns true if given app id is the iOS sms app
bool ancs_notifications_util_is_sms(const ANCSAttribute *app_id);

// Returns true if given app id and subtitle denote a group sms
bool ancs_notifications_util_is_group_sms(const ANCSAttribute *app_id,
                                          const ANCSAttribute *subtitle);
