/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "nexmo.h"
#include "ancs_notifications_util.h"

#include "comm/ble/kernel_le_client/ancs/ancs.h"
#include "comm/ble/kernel_le_client/ancs/ancs_types.h"

#include "system/logging.h"

T_STATIC char* NEXMO_REAUTH_STRING = "Pebble check-in code:";

bool nexmo_is_reauth_sms(const ANCSAttribute *app_id, const ANCSAttribute *message) {
  if (ancs_notifications_util_is_sms(app_id)) {
    if (strstr((const char *)message->value, NEXMO_REAUTH_STRING)) {
      PBL_LOG(LOG_LEVEL_INFO, "Got Nexmo Reauth SMS");
      return true;
    }
  }
  return false;
}

void nexmo_handle_reauth_sms(uint32_t uid,
                             const ANCSAttribute *app_id,
                             const ANCSAttribute *message,
                             iOSNotifPrefs *existing_notif_prefs) {
  const int num_existing_attributes = existing_notif_prefs ?
                                      existing_notif_prefs->attr_list.num_attributes : 0;

  AttributeList new_attr_list;
  attribute_list_init_list(num_existing_attributes, &new_attr_list);

  // Copy over all the existing attributes to our new list
  if (existing_notif_prefs) {
    for (int i = 0; i < num_existing_attributes; i++) {
      new_attr_list.attributes[i] = existing_notif_prefs->attr_list.attributes[i];
    }
  }

  char msg_buffer[message->length + 1];
  memcpy(msg_buffer, message->value, message->length);
  msg_buffer[message->length] = '\0';
  attribute_list_add_cstring(&new_attr_list, AttributeIdAuthCode, msg_buffer);

  // This will trigger a sync sending the auth code to the phone
  ios_notif_pref_db_store_prefs(app_id->value, app_id->length,
                                &new_attr_list, &existing_notif_prefs->action_group);

  // Dismiss the notification so the user is oblivious to this process
  ancs_perform_action(uid, ActionIDNegative);
}
