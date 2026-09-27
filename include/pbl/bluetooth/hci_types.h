/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

enum pbl_bt_hci_status {
  PBL_BT_HCI_STATUS_SUCCESS = 0x00,
  PBL_BT_HCI_STATUS_UNKNOWN_CONNECTION_IDENTIFIER = 0x02,
  PBL_BT_HCI_STATUS_VS_BASE = 0x50,
  PBL_BT_HCI_STATUS_MAX = UINT16_MAX
};

#ifndef __clang__
_Static_assert(sizeof(enum pbl_bt_hci_status) == 2,
               "packed structs expect the status code to be 2 bytes!");
#endif
