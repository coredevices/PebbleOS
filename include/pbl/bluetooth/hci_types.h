/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup bluetooth_hci_types HCI types
 * @ingroup bluetooth
 * @brief HCI status codes.
 * @{
 */

/**
 * @brief HCI status or error code, see Bluetooth Core Specification Vol 1, Part F.
 *
 * Two bytes in size, as packed event structures expect.
 */
enum pbl_bt_hci_status {
  /** Success. */
  PBL_BT_HCI_STATUS_SUCCESS = 0x00,
  /** Unknown Connection Identifier. */
  PBL_BT_HCI_STATUS_UNKNOWN_CONNECTION_IDENTIFIER = 0x02,
  /** First vendor specific status code. */
  PBL_BT_HCI_STATUS_VS_BASE = 0x50,
  /** Forces the enum to two bytes. */
  PBL_BT_HCI_STATUS_MAX = UINT16_MAX
};

#ifndef __clang__
_Static_assert(sizeof(enum pbl_bt_hci_status) == 2,
               "packed structs expect the status code to be 2 bytes!");
#endif

/** @} */
