/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/notifications/ancs/nexmo.h>

#include <comm/ble/kernel_le_client/ancs/ancs_types.h>

PBL_WEAK bool nexmo_is_reauth_sms(const ANCSAttribute *app_id, const ANCSAttribute *message) {
  return false;
}

PBL_WEAK void nexmo_handle_reauth_sms(uint32_t uid, const ANCSAttribute *app_id,
                                      const ANCSAttribute *message,
                                      iOSNotifPrefs *existing_notif_prefs) {
  return;
}
