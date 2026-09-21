/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

// The reason the headers that define these lengths aren't included is because this header
// is included by the various number of backend implementations. They don't know what "mfg"
// is, etc.
// NOTE: These sizes are asserted in a .c file to be in sync with the FW
#define PBL_BT_DIS_MODEL_NUMBER_LEN  (10) // MFG_HW_VERSION_SIZE + 1
#define PBL_BT_DIS_MANUFACTURER_LEN  (18) // sizeof("Pebble Technology")
#define PBL_BT_DIS_SERIAL_NUMBER_LEN (13) // MFG_SERIAL_NUMBER_SIZE + 1
#define PBL_BT_DIS_FW_REVISION_LEN   (32) // FW_METADATA_VERSION_TAG_BYTES)
#define PBL_BT_DIS_SW_REVISION_LEN   (8)  // Fmt: xx.xxx\0

struct PBL_PACKED pbl_bt_dis_info {
  char model_number[PBL_BT_DIS_MODEL_NUMBER_LEN];
  char manufacturer[PBL_BT_DIS_MANUFACTURER_LEN];
  char serial_number[PBL_BT_DIS_SERIAL_NUMBER_LEN];
  char fw_revision[PBL_BT_DIS_FW_REVISION_LEN];
  char sw_revision[PBL_BT_DIS_SW_REVISION_LEN];
};
