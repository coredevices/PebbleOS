/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/btutil/bt_device.h"
#include <pbl/bluetooth/types.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct pbl_bt_device bt_device_init_with_address(struct pbl_bt_addr address, bool is_random) {
  struct pbl_bt_device_internal device = {
    .address = address,
    .is_classic = false,
    .is_random_address = is_random,
  };
  return device.opaque;
}

struct pbl_bt_addr bt_device_get_address(struct pbl_bt_device device) {
  return ((struct pbl_bt_device_internal *)&device)->address;
}

bool bt_device_address_equal(const struct pbl_bt_addr *addr1, const struct pbl_bt_addr *addr2) {
  if (addr1 == NULL || addr2 == NULL) {
    return false;
  }
  return memcmp(addr1, addr2, sizeof(struct pbl_bt_addr)) == 0;
}

bool bt_device_address_is_invalid(const struct pbl_bt_addr *addr) {
  if (!addr) {
    return true;
  }
  struct pbl_bt_addr invalid = {};
  return bt_device_address_equal(addr, &invalid);
}

bool bt_device_internal_equal(const struct pbl_bt_device_internal *device1_int,
                              const struct pbl_bt_device_internal *device2_int) {
  if (device1_int == NULL || device2_int == NULL) {
    return false;
  }
  return (device1_int->is_classic == device2_int->is_classic &&
          device1_int->is_random_address == device2_int->is_random_address &&
          bt_device_address_equal(&device1_int->address, &device2_int->address));
}

bool bt_device_equal(const struct pbl_bt_device *device1, const struct pbl_bt_device *device2) {
  const struct pbl_bt_device_internal *device1_int = (const struct pbl_bt_device_internal *)device1;
  const struct pbl_bt_device_internal *device2_int = (const struct pbl_bt_device_internal *)device2;
  return bt_device_internal_equal(device1_int, device2_int);
}

bool bt_device_is_invalid(const struct pbl_bt_device *device) {
  const struct pbl_bt_device invalid_device = PBL_BT_DEVICE_INVALID;
  return bt_device_equal(device, &invalid_device);
}
