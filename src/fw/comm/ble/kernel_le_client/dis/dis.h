/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/bluetooth/ble_client.h"

//! @file dis.h Module implementing an DIS client.
//! See https://developer.bluetooth.org/TechnologyOverview/Pages/DIS.aspx

//! Enum indexing the DIS characteristics
typedef enum {
  // We need at least one characteristic to look up the GAPLEConnection & flag the presence of DIS
  // since Apple doesn't expose the SW version yet
  DISCharacteristicManufacturerNameString = 0,
  NumDISCharacteristic,

  DISCharacteristicInvalid = NumDISCharacteristic,
} DISCharacteristic;

//! Updates the /ref GAPLEConnection to register that the DIS service has been discovered
//! @param characteristics Matrix of characteristics references of the DIS service
void dis_handle_service_discovered(BLECharacteristic *characteristics);

void dis_invalidate_all_references(void);

void dis_handle_service_removed(BLECharacteristic *characteristics, uint8_t num_characteristics);
