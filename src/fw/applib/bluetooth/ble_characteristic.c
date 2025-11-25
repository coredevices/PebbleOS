/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ble_characteristic.h"

#include "syscall/syscall.h"

bool ble_characteristic_is_readable(BLECharacteristic characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          BLEAttributePropertyRead);
}

bool ble_characteristic_is_writable(BLECharacteristic characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          BLEAttributePropertyWrite);
}

bool ble_characteristic_is_writable_without_response(BLECharacteristic characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          BLEAttributePropertyWriteWithoutResponse);
}

bool ble_characteristic_is_subscribable(BLECharacteristic characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          (BLEAttributePropertyNotify | BLEAttributePropertyIndicate));
}

bool ble_characteristic_is_notifiable(BLECharacteristic characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          BLEAttributePropertyNotify);
}

bool ble_characteristic_is_indicatable(BLECharacteristic characteristic) {
  return (sys_ble_characteristic_get_properties(characteristic) &
          BLEAttributePropertyIndicate);
}
