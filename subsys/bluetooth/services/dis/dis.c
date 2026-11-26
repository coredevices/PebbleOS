/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/dis.h>
#include <pbl/bluetooth/gatts.h>

#include <services/dis/ble_svc_dis.h>

static const struct pbl_bt_dis_info *s_info;

void pbl_bt_dis_set_info(const struct pbl_bt_dis_info *info) {
  s_info = info;
}

static int prv_add(void) {
  if (s_info != nullptr) {
    ble_svc_dis_model_number_set(s_info->model_number);
    ble_svc_dis_serial_number_set(s_info->serial_number);
    ble_svc_dis_firmware_revision_set(s_info->fw_revision);
    ble_svc_dis_software_revision_set(s_info->sw_revision);
    ble_svc_dis_manufacturer_name_set(s_info->manufacturer);
  }

  ble_svc_dis_init();

  return 0;
}

PBL_BT_GATTS_SERVICE_DEFINE(dis, 10, prv_add);
