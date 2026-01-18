/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef __has_include
  #if __has_include("L2CAPAPI.h")
    #include "L2CAPAPI.h"
    #define L2CAPAPI_AVAILABLE
  #endif
#else
  #ifdef COMPONENT_BTSTACK
    #include "L2CAPAPI.h"
    #define L2CAPAPI_AVAILABLE
  #endif
#endif

#ifndef L2CAPAPI_AVAILABLE
// Define the types we need if L2CAPAPI is not available
typedef struct {
  uint16_t dummy;
} L2CA_Link_Connect_Params_t;
#endif

int L2CA_Set_Link_Connection_Configuration(unsigned int BluetoothStackID, L2CA_Link_Connect_Params_t *L2CA_Link_Connect_Params) {
  return 0;
}
