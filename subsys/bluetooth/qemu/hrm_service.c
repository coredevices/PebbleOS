/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/hrm_service.h>

bool pbl_bt_is_hrm_service_supported(void) {
  return false;
}

void pbl_bt_hrm_service_handle_measurement(const struct pbl_bt_hrm_service_measurement *measurement,
                                           const struct pbl_bt_device_internal *permitted_devices,
                                           size_t num_permitted_devices) {
}
