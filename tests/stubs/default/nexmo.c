/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/ancs.h>
#include <pbl/kernel/compiler.h>
#include <pbl/services/notifications/ancs/nexmo.h>

PBL_WEAK bool nexmo_is_reauth_sms(const struct pbl_bt_ancs_attr *app_id,
                                  const struct pbl_bt_ancs_attr *message) {
  return false;
}

PBL_WEAK void nexmo_handle_reauth_sms(uint32_t uid, const struct pbl_bt_ancs_attr *app_id,
                                      const struct pbl_bt_ancs_attr *message,
                                      iOSNotifPrefs *existing_notif_prefs) {
  return;
}
