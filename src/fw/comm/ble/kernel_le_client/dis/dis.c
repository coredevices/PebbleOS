/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dis.h"

#include "comm/ble/ble_log.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/kernel_le_client/ancs/ancs.h"
#include "comm/bt_lock.h"
#include "system/logging.h"
#include "system/passert.h"

// -------------------------------------------------------------------------------------------------
// Interface towards kernel_le_client.c

void dis_invalidate_all_references(void) {
}

void dis_handle_service_removed(BLECharacteristic *characteristics, uint8_t num_characteristics) {
  // dis_service_discovered doesn't get set to false here, since services can temporarily disappear
  // and we're just using this to detect whether or not we're on iOS 9
}

void dis_handle_service_discovered(BLECharacteristic *characteristics) {
  BLE_LOG_DEBUG("In DIS service discovery CB");
  PBL_ASSERTN(characteristics);

  ancs_handle_ios9_or_newer_detected();
}
