/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once 

#include "SS1BTPS.h"

int HCI_Command_Supported(unsigned int BluetoothStackID, unsigned int SupportedCommandBitNumber) {
  return 1;
}

int HCI_Write_Default_Link_Policy_Settings(unsigned int BluetoothStackID, Word_t Link_Policy_Settings, Byte_t *StatusResult) {
  return 0;
}
