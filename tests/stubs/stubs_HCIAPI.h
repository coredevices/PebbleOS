/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef __has_include
  #if __has_include("SS1BTPS.h")
    #include "SS1BTPS.h"
    #define SS1BTPS_AVAILABLE
  #endif
#else
  #ifdef COMPONENT_BTSTACK
    #include "SS1BTPS.h"
    #define SS1BTPS_AVAILABLE
  #endif
#endif

#ifndef SS1BTPS_AVAILABLE
// Define the types we need if SS1BTPS is not available
typedef uint8_t Byte_t;
typedef uint16_t Word_t;
#endif

int HCI_Command_Supported(unsigned int BluetoothStackID, unsigned int SupportedCommandBitNumber) {
  return 1;
}

int HCI_Write_Default_Link_Policy_Settings(unsigned int BluetoothStackID, Word_t Link_Policy_Settings, Byte_t *StatusResult) {
  return 0;
}
