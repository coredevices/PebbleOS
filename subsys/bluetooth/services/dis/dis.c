/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <bluetooth/services/services.h>
#include <services/dis/ble_svc_dis.h>

// NimBLE keeps pointers to the strings.
static struct pbl_bt_dis_info s_dis_info;

void pbl_bt__dis_init(const struct pbl_bt_dis_info *info) {
  s_dis_info = *info;
  ble_svc_dis_model_number_set(s_dis_info.model_number);
  ble_svc_dis_serial_number_set(s_dis_info.serial_number);
  ble_svc_dis_firmware_revision_set(s_dis_info.fw_revision);
  ble_svc_dis_software_revision_set(s_dis_info.sw_revision);
  ble_svc_dis_manufacturer_name_set(s_dis_info.manufacturer);

  ble_svc_dis_init();
}
