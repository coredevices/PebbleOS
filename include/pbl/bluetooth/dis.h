/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

/**
 * @defgroup bluetooth_dis Device Information Service
 * @ingroup bluetooth
 * @brief Values served by the GATT Device Information Service.
 *
 * The lengths are those of the values the application serves, which checks at build time that
 * they are large enough. All lengths include the terminator.
 * @{
 */

/** @brief Length of the model number. */
#define PBL_BT_DIS_MODEL_NUMBER_LEN (10)
/** @brief Length of the manufacturer name. */
#define PBL_BT_DIS_MANUFACTURER_LEN (18)
/** @brief Length of the serial number. */
#define PBL_BT_DIS_SERIAL_NUMBER_LEN (13)
/** @brief Length of the firmware revision. */
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

/**
 * @brief Set the values the service serves, before the stack starts.
 *
 * @param info The values, which must outlive the stack.
 */
void pbl_bt_dis_set_info(const struct pbl_bt_dis_info *info);

/** @} */
