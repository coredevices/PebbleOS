/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_bt_gatt.h"

// Service Changed indications the firmware has pushed to the controller: a count
// plus a faithful copy of the last call's arguments.
static int s_service_changed_indication_count;
static struct pbl_bt_device_internal s_service_changed_last_device;
static struct pbl_bt_att_handle_range s_service_changed_last_range;

void pbl_bt_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code) {
  // The response is consumed by the controller; nothing observes it in tests.
}

void pbl_bt_gatt_send_changed_indication(const struct pbl_bt_device_internal *device,
                                         const struct pbl_bt_att_handle_range *data) {
  ++s_service_changed_indication_count;
  s_service_changed_last_device = *device;
  s_service_changed_last_range = *data;
}

int fake_gatt_get_service_changed_indication_count(void) {
  return s_service_changed_indication_count;
}

const struct pbl_bt_device_internal *fake_gatt_get_service_changed_last_device(void) {
  return &s_service_changed_last_device;
}

struct pbl_bt_att_handle_range fake_gatt_get_service_changed_last_range(void) {
  return s_service_changed_last_range;
}

void fake_gatt_init(void) {
  s_service_changed_indication_count = 0;
  s_service_changed_last_device = (struct pbl_bt_device_internal){};
  s_service_changed_last_range = (struct pbl_bt_att_handle_range){};
}
