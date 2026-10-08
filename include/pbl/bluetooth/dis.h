/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_dis Device Information Service
 * @ingroup bluetooth
 * @brief Values served by the GATT Device Information Service.
 *
 * The lengths are spelled out instead of derived from the firmware headers that define them, so
 * this header stays usable by the stack. The firmware checks at build time that they are large
 * enough. All lengths include the terminator.
 * @{
 */

/** @brief Length of the model number, MFG_HW_VERSION_SIZE + 1. */
#define PBL_BT_DIS_MODEL_NUMBER_LEN (10)
/** @brief Length of the manufacturer name, at least sizeof(BT_VENDOR_NAME). */
#define PBL_BT_DIS_MANUFACTURER_LEN (18)
/** @brief Length of the serial number, MFG_SERIAL_NUMBER_SIZE + 1. */
#define PBL_BT_DIS_SERIAL_NUMBER_LEN (13)
/** @brief Length of the firmware revision, FW_METADATA_VERSION_TAG_BYTES. */
#define PBL_BT_DIS_FW_REVISION_LEN (32)
/** @brief Length of the software revision, formatted as @c xx.xxx. */
#define PBL_BT_DIS_SW_REVISION_LEN (8)

/** @brief Device Information Service values, NUL-terminated strings. */
struct PBL_PACKED pbl_bt_dis_info {
  /** Model Number String. */
  char model_number[PBL_BT_DIS_MODEL_NUMBER_LEN];
  /** Manufacturer Name String. */
  char manufacturer[PBL_BT_DIS_MANUFACTURER_LEN];
  /** Serial Number String. */
  char serial_number[PBL_BT_DIS_SERIAL_NUMBER_LEN];
  /** Firmware Revision String. */
  char fw_revision[PBL_BT_DIS_FW_REVISION_LEN];
  /** Software Revision String. */
  char sw_revision[PBL_BT_DIS_SW_REVISION_LEN];
};

/** @} */
