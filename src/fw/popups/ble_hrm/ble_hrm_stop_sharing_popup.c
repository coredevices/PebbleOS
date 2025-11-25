/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_hrm_stop_sharing_popup.h"

#include "applib/ui/dialogs/simple_dialog.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "util/time/time.h"

#define BLE_HRM_CONFIRMATION_TIMEOUT_MS (2 * MS_PER_SECOND)

SimpleDialog *ble_hrm_stop_sharing_popup_create(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Stopped Sharing");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);

  const char *msg = i18n_get("Heart Rate Not Shared", dialog);
  dialog_set_text(dialog, msg);
  dialog_set_icon(dialog, RESOURCE_ID_BLE_HRM_NOT_SHARED);
  dialog_set_timeout(dialog, BLE_HRM_CONFIRMATION_TIMEOUT_MS);
  simple_dialog_set_icon_animated(simple_dialog, false);
  i18n_free(msg, dialog);
  return simple_dialog;
}
