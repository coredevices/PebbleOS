/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <pbl/bluetooth/gatt.h>

void pbl_bt_gatt_respond_read_subscription(uint32_t transaction_id, uint16_t response_code) {
}

void pbl_bt_gatt_send_changed_indication(const struct pbl_bt_device_internal *device,
                                         const struct pbl_bt_att_handle_range *data) {
}
