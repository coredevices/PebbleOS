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
  unsigned int Event_Data_Type;
  unsigned int Event_Data_Size;
  union {
    void *GATT_Service_Discovery_Complete_Data;
    void *GATT_Service_Discovery_Indication_Data;
  } Event_Data;
} GATT_Service_Discovery_Event_Data_t;

typedef struct {
  unsigned int ConnectionID;
  uint8_t Status;
} GATT_Service_Discovery_Complete_Data_t;

typedef struct {
  unsigned int ConnectionID;
  struct {
    unsigned int NumberOfCharacteristics;
    void *CharacteristicInformationList;
  } ServiceInformation;
  unsigned int NumberOfCharacteristics;
  void *CharacteristicInformationList;
} GATT_Service_Discovery_Indication_Data_t;

typedef struct {
  unsigned int Characteristic_Descriptor_Handle;
  unsigned int Characteristic_Descriptor_UUID;
  unsigned int UUID_Type;
} GATT_Characteristic_Descriptor_Information_t;

typedef struct {
  unsigned int Characteristic_Handle;
  unsigned int Characteristic_UUID;
  unsigned int Characteristic_Properties;
  unsigned int NumberOfDescriptors;
  GATT_Characteristic_Descriptor_Information_t *DescriptorList;
  unsigned int UUID_Type;
} GATT_Characteristic_Information_t;

typedef void (*GATT_Connection_Event_Callback_t)(unsigned int, void *, unsigned long);

typedef struct {
  int dummy;
} GATT_Connection_Event_Data_t;

typedef struct {
  unsigned int UUID_Type;
  unsigned int UUID_16;
  unsigned char UUID_128[16];
} GATT_UUID_t;

typedef void (*GATT_Service_Discovery_Event_Callback_t)(unsigned int, GATT_Service_Discovery_Event_Data_t *, unsigned long);

typedef struct {
  unsigned int Starting_Handle;
  unsigned int Ending_Handle;
  unsigned int Service_Handle;
  unsigned int End_Group_Handle;
  GATT_UUID_t UUID;
} GATT_Attribute_Handle_Group_t;

typedef struct {
  int dummy;
} GATT_Service_Changed_Data_t;

typedef void (*GATT_Client_Event_Callback_t)(unsigned int, void *, unsigned long);

typedef struct {
  unsigned int ConnectionID;
  unsigned int TransactionID;
  unsigned int ConnectionType;
  unsigned int BytesWritten;
} GATT_Write_Response_Data_t;

typedef struct {
  unsigned int Event_Data_Type;
  unsigned int Event_Data_Size;
  union {
    GATT_Write_Response_Data_t *GATT_Write_Response_Data;
    void *GATT_Service_Changed_Data;
  } Event_Data;
} GATT_Client_Event_Data_t;

typedef struct {
  unsigned int Service_Handle;
  unsigned int End_Group_Handle;
  GATT_UUID_t UUID;
} GATT_Service_Information_t;

#define inc_service_list 0
#define guUUID_128 1
#ifndef NULL
#define NULL ((void *)0)
#endif

typedef uint16_t Word_t;

// Enum values
#define etGATT_Service_Discovery_Complete 0
#define etGATT_Service_Discovery_Indication 1
#define etGATT_Client_Write_Response 2
#define guUUID_16 0
#define gctLE 0

// Size constants
#define GATT_SERVICE_DISCOVERY_COMPLETE_DATA_SIZE sizeof(GATT_Service_Discovery_Complete_Data_t)
#define GATT_SERVICE_DISCOVERY_INDICATION_DATA_SIZE sizeof(GATT_Service_Discovery_Indication_Data_t)

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
