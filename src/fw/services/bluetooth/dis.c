/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/dis.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "board/board.h"
#include "process_management/pebble_process_info.h"
#include "mfg/mfg_info.h"
#include "mfg/mfg_serials.h"
#include "system/version.h"

_Static_assert(PBL_BT_DIS_MODEL_NUMBER_LEN >= MFG_HW_VERSION_SIZE + 1, "Size mismatch");
_Static_assert(PBL_BT_DIS_MANUFACTURER_LEN >= sizeof(BT_VENDOR_NAME), "Size mismatch");
_Static_assert(PBL_BT_DIS_SERIAL_NUMBER_LEN >= MFG_SERIAL_NUMBER_SIZE + 1, "Size mismatch");
_Static_assert(PBL_BT_DIS_FW_REVISION_LEN >= sizeof(TINTIN_METADATA.version_tag), "Size mismatch");

static void prv_set_model_number(struct pbl_bt_dis_info *info) {
  mfg_info_get_hw_version(info->model_number, PBL_BT_DIS_MODEL_NUMBER_LEN);
}

static void prv_set_manufacturer_name(struct pbl_bt_dis_info *info) {
  strncpy(info->manufacturer, BT_VENDOR_NAME, PBL_BT_DIS_MANUFACTURER_LEN);
}

static void prv_set_serial_number(struct pbl_bt_dis_info *info) {
  mfg_info_get_serialnumber(info->serial_number, PBL_BT_DIS_SERIAL_NUMBER_LEN);
}

static void prv_set_firmware_revision(struct pbl_bt_dis_info *info) {
  strncpy(info->fw_revision, (char *)TINTIN_METADATA.version_tag, PBL_BT_DIS_FW_REVISION_LEN);
}

static void prv_set_software_revision(struct pbl_bt_dis_info *info) {
  // Fmt: xx.xx\0
  char sdk_version[PBL_BT_DIS_SW_REVISION_LEN];
  sniprintf(sdk_version, PBL_BT_DIS_SW_REVISION_LEN, "%2u.%02u",
            PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR, PROCESS_INFO_CURRENT_SDK_VERSION_MINOR);
  strncpy(info->sw_revision, sdk_version, PBL_BT_DIS_SW_REVISION_LEN);
}

void dis_get_info(struct pbl_bt_dis_info *info) {
  prv_set_model_number(info);
  prv_set_manufacturer_name(info);
  prv_set_serial_number(info);
  prv_set_firmware_revision(info);
  prv_set_software_revision(info);
}
