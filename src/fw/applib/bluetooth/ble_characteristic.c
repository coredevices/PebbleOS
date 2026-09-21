/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_characteristic.h"

#include "syscall/syscall.h"

bool ble_characteristic_is_readable(pbl_bt_characteristic_t characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) & PBL_BT_ATTRIBUTE_PROPERTY_READ);
}

bool ble_characteristic_is_writable(pbl_bt_characteristic_t characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) & PBL_BT_ATTRIBUTE_PROPERTY_WRITE);
}

bool ble_characteristic_is_writable_without_response(pbl_bt_characteristic_t characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          PBL_BT_ATTRIBUTE_PROPERTY_WRITE_WITHOUT_RESPONSE);
}

bool ble_characteristic_is_subscribable(pbl_bt_characteristic_t characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          (PBL_BT_ATTRIBUTE_PROPERTY_NOTIFY | PBL_BT_ATTRIBUTE_PROPERTY_INDICATE));
}

bool ble_characteristic_is_notifiable(pbl_bt_characteristic_t characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) & PBL_BT_ATTRIBUTE_PROPERTY_NOTIFY);
}

bool ble_characteristic_is_indicatable(pbl_bt_characteristic_t characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          PBL_BT_ATTRIBUTE_PROPERTY_INDICATE);
}
