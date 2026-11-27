/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include <pbl/bluetooth/addr.h>
#include <pbl/bluetooth/gap.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/local_id.h>

#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

// Kept: the address is asked for while the stack is stopped too.
static uint8_t s_address[BLE_DEV_ADDR_LEN];
static char s_device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

static void prv_apply_device_name(void) {
  int rc = ble_svc_gap_device_name_set(s_device_name);

  if (rc != 0) {
    PBL_LOG_ERR("Could not set the device name (%d)", rc);
  }
}

void bt_local_id_handle_start(void) {
  uint8_t own_addr_type;

  if (ble_hs_id_infer_auto(0, &own_addr_type) != 0 ||
      ble_hs_id_copy_addr(own_addr_type, s_address, nullptr) != 0) {
    PBL_LOG_ERR("No identity address");
  }

  // "Pebble" and the last two bytes of the address.
  if (s_device_name[0] == '\0') {
    snprintf(s_device_name, sizeof(s_device_name), "Pebble %02X%02X", s_address[1], s_address[0]);
  }

  prv_apply_device_name();
}

void bt_local_id_set_device_name(const char *device_name) {
  strncpy(s_device_name, device_name, sizeof(s_device_name));
  s_device_name[sizeof(s_device_name) - 1] = '\0';
  prv_apply_device_name();
}

void bt_local_id_copy_device_name(char name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE]) {
  strncpy(name_out, s_device_name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
}

void bt_local_id_copy_address(uint8_t addr_out[6]) {
  memcpy(addr_out, s_address, sizeof(s_address));
}

void bt_local_id_copy_address_string(char addr_str_out[PBL_BT_ADDR_FMT_BUFFER_SIZE]) {
  snprintf(addr_str_out, PBL_BT_ADDR_FMT_BUFFER_SIZE, PBL_BT_ADDR_FMT,
           PBL_BT_ADDR_XPLODE(s_address));
}
