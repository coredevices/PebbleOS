/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef __has_include
  #if __has_include("GATTAPI.h")
    #include "GATTAPI.h"
    #define GATTAPI_AVAILABLE
  #endif
#else
  #ifdef COMPONENT_BTSTACK
    #include "GATTAPI.h"
    #define GATTAPI_AVAILABLE
  #endif
#endif

#include <stdbool.h>
#include <stdint.h>

// If GATTAPI.h is not available, provide dummy type definitions
#ifndef GATTAPI_AVAILABLE
typedef struct {
  int dummy;
} GATT_Service_Discovery_Event_Data_t;
#endif

bool fake_gatt_is_service_discovery_running(void);

//! @return Number of times GATT_Start_Service_Discovery has been called since fake_gatt_init()
int fake_gatt_is_service_discovery_start_count(void);

//! @return Number of times GATT_Stop_Service_Discovery has been called since fake_gatt_init()
int fake_gatt_is_service_discovery_stop_count(void);

//! Sets the value that the GATT_Start_Service_Discovery fake should return
//! @note fake_gatt_init() will reset this to 0
void fake_gatt_set_start_return_value(int ret_value);

//! Sets the value that the GATT_Stop_Service_Discovery fake should return
//! @note fake_gatt_init() will reset this to 0
void fake_gatt_set_stop_return_value(int ret_value);

int fake_gatt_get_service_changed_indication_count(void);

void fake_gatt_put_service_discovery_event(GATT_Service_Discovery_Event_Data_t *event);

uint16_t fake_gatt_write_last_written_handle(void);

void fake_gatt_put_write_response_for_last_write(void);

void fake_gatt_init(void);
