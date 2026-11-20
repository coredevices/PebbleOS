/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/ancs.h>
#include <pbl/logging/logging.h>
#include <pbl/services/notifications/ancs/ancs_notifications_util.h>
#include <pbl/services/notifications/ancs/nexmo.h>
#include <pbl/util/testing.h>

#include <comm/ble/kernel_le_client/ancs/ancs.h>

PBL_LOG_MODULE_DECLARE(service_notifications, CONFIG_SERVICE_NOTIFICATIONS_LOG_LEVEL);

PBL_T_STATIC char *NEXMO_REAUTH_STRING = "Pebble check-in code:";

bool nexmo_is_reauth_sms(const struct pbl_bt_ancs_attr *app_id,
                         const struct pbl_bt_ancs_attr *message) {
  if (ancs_notifications_util_is_sms(app_id)) {
    if (strstr((const char *)message->value, NEXMO_REAUTH_STRING)) {
      PBL_LOG_INFO("Got Nexmo Reauth SMS");
      return true;
    }
  }
  return false;
}

void nexmo_handle_reauth_sms(uint32_t uid, const struct pbl_bt_ancs_attr *app_id,
                             const struct pbl_bt_ancs_attr *message,
                             iOSNotifPrefs *existing_notif_prefs) {
  const int num_existing_attributes =
      existing_notif_prefs ? existing_notif_prefs->attr_list.num_attributes : 0;

  AttributeList new_attr_list;
  attribute_list_init_list(num_existing_attributes, &new_attr_list);

  // Copy over all the existing attributes to our new list
  if (existing_notif_prefs) {
    for (int i = 0; i < num_existing_attributes; i++) {
      new_attr_list.attributes[i] = existing_notif_prefs->attr_list.attributes[i];
    }
  }

  char msg_buffer[message->len + 1];
  memcpy(msg_buffer, message->value, message->len);
  msg_buffer[message->len] = '\0';
  attribute_list_add_cstring(&new_attr_list, AttributeIdAuthCode, msg_buffer);

  // This will trigger a sync sending the auth code to the phone
  ios_notif_pref_db_store_prefs(app_id->value, app_id->len, &new_attr_list,
                                &existing_notif_prefs->action_group);

  // Dismiss the notification so the user is oblivious to this process
  ancs_perform_action(uid, PBL_BT_ANCS_ACTION_NEGATIVE);
}
