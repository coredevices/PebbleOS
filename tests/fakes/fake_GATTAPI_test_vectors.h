/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"

#include <stdint.h>

// These structures are only used in the unit tests:

typedef struct {
  Uuid uuid;
  uint16_t handle;
} Descriptor;

typedef struct {
  Uuid uuid;
  uint8_t properties;
  uint16_t handle;
  uint8_t num_descriptors;
  Descriptor descriptors[3];
} Characteristic;

typedef struct Service {
  Uuid uuid;
  uint16_t handle;
  uint8_t num_characteristics;
  Characteristic characteristics[3];
  uint8_t num_included_services;
  struct Service *included_services[2];
} Service;

//! Simulates receiving the Bluetopia service discovery complete event
void fake_gatt_put_discovery_complete_event(uint8_t status,
                                            unsigned int connection_id);

// Health Thermometer Service 0x1809 : 0x11
// Temperature Measurement    0x2a1c : 0x13 (properties=0x02)
//                       CCCD 0x2902 : 0x15

//! Simulates receiving the Bluetopia service discovery indication event
void fake_gatt_put_discovery_indication_health_thermometer_service(
                                                    unsigned int connection_id);
//! Returns the Service data structure that can be used for reference
const Service * fake_gatt_get_health_thermometer_service(void);



// Blood Pressure Service 0x1810  : 0x01
// Pressure Characteristic 0x2a35 : 0x03 (properties=0x20)
//                    CCCD 0x2902 : 0x05
// Feature Characteristic 0x2a49  : 0x07 (properties=0x02)
//                    CCCD 0x2902 : 0x09
// Included Services              : Points to the fake Health Thermometer Service

//! Simulates receiving the Bluetopia service discovery indication event
void fake_gatt_put_discovery_indication_blood_pressure_service(
                                                    unsigned int connection_id);
//! Returns the Service data structure that can be used for reference
const Service * fake_gatt_get_blood_pressure_service(void);


// Service F768095B-1BFA-4F63-97EE-FDEDAC66F9B0 : 0x17
// Char1   F768095B-1BFA-4F63-97EE-FDEDAC66F9B1 : 0x19 (properties=0x02)
// Desc1   F768095B-1BFA-4F63-97EE-FDEDAC66F9B2 : 0x21
// Char2   F768095B-1BFA-4F63-97EE-FDEDAC66F9B3 : 0x23 (properties=0x02)
// Desc2   F768095B-1BFA-4F63-97EE-FDEDAC66F9B4 : 0x25

void fake_gatt_put_discovery_indication_random_128bit_uuid_service(
                                                    unsigned int connection_id);

// Returns the starting ATT handle (Service, 0x1) and ending ATT handle (Desc2 0x09)
// for the BP service
void fake_gatt_get_bp_att_handle_range(uint16_t *start, uint16_t *end);

const Service * fake_gatt_get_random_128bit_uuid_service(void);



//! Simulates receiving the Bluetopia service discovery indication event
void fake_gatt_put_discovery_indication_gatt_profile_service(unsigned int connection_id,
                                                           bool has_service_changed_characteristic);

uint16_t fake_gatt_gatt_profile_service_service_changed_att_handle(void);


uint16_t fake_gatt_gatt_profile_service_service_changed_cccd_att_handle(void);
