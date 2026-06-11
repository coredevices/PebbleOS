/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>
#include <bluetooth/gatt_service_types.h>
#include "pbl/services/new_timer/new_timer.h"
#include "util/uuid.h"

#include <stdbool.h>
#include <stdint.h>

//! High-level description of a remote GATT service used by the unit tests. The
//! fake converts these into the packed GATTService blobs the firmware expects
//! and pushes them through the bt_driver_cb_gatt_client_discovery_* contract.
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

//! Status codes the tests pass to fake_gatt_put_discovery_complete_event. They
//! mirror the Bluetopia discovery status values the firmware was originally
//! tested against; the fake maps them onto the bt_driver BTErrno contract.
#define GATT_SERVICE_DISCOVERY_STATUS_SUCCESS (0x00)
#define GATT_SERVICE_DISCOVERY_STATUS_RESPONSE_TIMEOUT (0x01)

//! An opaque controller-level GATT error code. The driver surfaces it into the
//! BTErrno space (via BTErrnoWithBluetopiaError) when returned from a discovery
//! start/stop, just as the real driver maps internal error codes.
#define BTGATT_ERROR_INVALID_PARAMETER (0x07)

//! Resets all simulated discovery state. Call from test initialize().
void fake_gatt_init(void);

//! @return true while a discovery started through bt_driver_gatt_start_discovery_range
//! has not yet completed.
bool fake_gatt_is_service_discovery_running(void);

//! @return the number of bt_driver_gatt_start_discovery_range calls so far.
int fake_gatt_is_service_discovery_start_count(void);

//! @return the number of bt_driver_gatt_stop_discovery calls so far.
int fake_gatt_is_service_discovery_stop_count(void);

//! Sets the controller error code that bt_driver_gatt_start_discovery_range
//! reports. A non-zero code is surfaced as BTErrnoWithBluetopiaError(code); zero
//! means success (BTErrnoOK).
void fake_gatt_set_start_return_value(int ret_value);

//! As fake_gatt_set_start_return_value, but for bt_driver_gatt_stop_discovery.
void fake_gatt_set_stop_return_value(int ret_value);

//! @return the TimerID of the simulated discovery watchdog. Firing it (via
//! stub_new_timer_fire) makes the controller report a discovery timeout.
TimerID bt_driver_gatt_get_watchdog_timer_id(void);

//! @return the number of Service Changed indications the firmware has pushed to
//! the controller through bt_driver_gatt_send_changed_indication.
int fake_gatt_get_service_changed_indication_count(void);

//! Feeds a single discovered service to the firmware, as the driver would.
void fake_gatt_put_discovery_indication_service(unsigned int connection_id,
                                                const Service *service);

//! Simulates the driver reporting service discovery completion with the given
//! status (see GATT_SERVICE_DISCOVERY_STATUS_*).
void fake_gatt_put_discovery_complete_event(uint8_t status, unsigned int connection_id);

// Health Thermometer Service 0x1809 : 0x11
// Temperature Measurement    0x2a1c : 0x13 (properties=0x02)
//                       CCCD 0x2902 : 0x15
void fake_gatt_put_discovery_indication_health_thermometer_service(unsigned int connection_id);
const Service *fake_gatt_get_health_thermometer_service(void);

// Blood Pressure Service 0x1810  : 0x01
// Pressure Characteristic 0x2a35 : 0x03 (properties=0x20)
//                    CCCD 0x2902 : 0x05
// Feature Characteristic 0x2a49  : 0x07 (properties=0x02)
//                    CCCD 0x2902 : 0x09
// Included Services              : Points to the fake Health Thermometer Service
void fake_gatt_put_discovery_indication_blood_pressure_service(unsigned int connection_id);
const Service *fake_gatt_get_blood_pressure_service(void);

// Service F768095B-1BFA-4F63-97EE-FDEDAC66F9B0 : 0x17
// Char1   F768095B-1BFA-4F63-97EE-FDEDAC66F9B1 : 0x19 (properties=0x02)
// Desc1   F768095B-1BFA-4F63-97EE-FDEDAC66F9B2 : 0x21
// Char2   F768095B-1BFA-4F63-97EE-FDEDAC66F9B3 : 0x23 (properties=0x02)
// Desc2   F768095B-1BFA-4F63-97EE-FDEDAC66F9B4 : 0x25
void fake_gatt_put_discovery_indication_random_128bit_uuid_service(unsigned int connection_id);
const Service *fake_gatt_get_random_128bit_uuid_service(void);

//! Returns the starting ATT handle (Service, 0x1) and ending ATT handle (Desc2 0x09)
//! for the BP service.
void fake_gatt_get_bp_att_handle_range(uint16_t *start, uint16_t *end);

// GATT Profile Service 0x1801 : 0x01
// Service Changed 0x2a05      : 0x03 (properties=0x20)
//            CCCD 0x2902      : 0x05
void fake_gatt_put_discovery_indication_gatt_profile_service(
    unsigned int connection_id, bool has_service_changed_characteristic);
uint16_t fake_gatt_gatt_profile_service_service_changed_att_handle(void);
uint16_t fake_gatt_gatt_profile_service_service_changed_cccd_att_handle(void);
