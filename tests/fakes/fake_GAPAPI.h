/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef __has_include
  #if __has_include("GAPAPI.h")
    #include "GAPAPI.h"
    #define GAPAPI_AVAILABLE
  #endif
#else
  #ifdef COMPONENT_BTSTACK
    #include "GAPAPI.h"
    #define GAPAPI_AVAILABLE
  #endif
#endif

#include <bluetooth/bluetooth_types.h>

#include <stdbool.h>
#include <stdint.h>

// If GAPAPI.h is not available, provide dummy type definitions
#ifndef GAPAPI_AVAILABLE
typedef struct GAPLEConnection GAPLEConnection;  // Forward declaration (already defined elsewhere)
typedef struct GAP_LE_Event_Data_t GAP_LE_Event_Data_t;

// Advertising and scan response data are 31-byte arrays per Bluetooth spec
#define GAP_ADVERTISING_DATA_SIZE 31
typedef uint8_t Advertising_Data_t[GAP_ADVERTISING_DATA_SIZE];
typedef uint8_t Scan_Response_Data_t[GAP_ADVERTISING_DATA_SIZE];

// HCI error codes
#define HCI_ERROR_CODE_SUCCESS 0x00
#define HCI_ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST 0x16

// Boolean constants
#define TRUE true
#define FALSE false

// Bluetooth address types
typedef uint8_t BD_ADDR_t[6];
typedef uint8_t Encryption_Key_t[16];

// GAP API types
typedef struct {
  uint8_t IO_Capability;
  uint8_t OOB_Data_Flag;
  uint8_t Authentication_Requirements;
  uint8_t Max_Encryption_Key_Size;
  uint8_t Link_Key_Request_Notification_Flag;
} GAP_LE_Pairing_Capabilities_t;

// GAP API function declarations
int GAP_LE_Advertising_Enable(unsigned int BluetoothStackID, unsigned int Enable,
                              Advertising_Data_t *Advertising_Data,
                              Scan_Response_Data_t *Scan_Response_Data,
                              void (*ConnectionCallback)(unsigned int, void *, unsigned long),
                              unsigned int CallbackParameter);

const GAP_LE_Pairing_Capabilities_t* gap_le_pairing_capabilities(void);

#endif // GAPAPI_AVAILABLE

// These functions are always available (either from GAPAPI or from the fake)
void gap_le_set_advertising_disabled(void);
bool gap_le_is_advertising_enabled(void);
void gap_le_assert_advertising_interval(uint16_t expected_min_slots, uint16_t expected_max_slots);
unsigned int gap_le_get_advertising_data(Advertising_Data_t *ad_data_out);
unsigned int gap_le_get_scan_response_data(Scan_Response_Data_t *scan_resp_data_out);
void fake_GAPAPI_init(void);

// Fake GAP API functions (available even when real GAPAPI is not)
void fake_gap_put_connection_event(uint8_t status, bool is_master, const BTDeviceInternal *device);
void fake_gap_put_disconnection_event(uint8_t status, uint8_t reason, bool is_master,
                                      const BTDeviceInternal *device);
void fake_GAPAPI_put_encryption_change_event(bool encrypted, uint8_t status, bool is_master,
                                             const BTDeviceInternal *device);
void fake_gap_le_put_cancel_create_event(const BTDeviceInternal *device, bool is_master);
void fake_GAPAPI_set_encrypted_for_device(const BTDeviceInternal *device);
const Encryption_Key_t *fake_GAPAPI_get_fake_irk(void);
const BD_ADDR_t *fake_GAPAPI_get_bd_addr_not_resolving_to_fake_irk(void);
const BTDeviceInternal *fake_GAPAPI_get_device_not_resolving_to_fake_irk(void);
const BD_ADDR_t *fake_GAPAPI_get_bd_addr_resolving_to_fake_irk(void);
const BTDeviceInternal *fake_GAPAPI_get_device_resolving_to_fake_irk(void);

#ifdef GAPAPI_AVAILABLE
// Additional functions when GAPAPI is available
// (none needed currently)
#endif
