/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include <pbl/bluetooth/id.h>
#include <pbl/services/bluetooth/local_id.h>

// Cached: some clients, such as the Settings app, ask often.
static struct pbl_bt_addr s_local_address;
static char s_local_device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

// "Pebble" and the last two bytes of the address.
static void prv_set_default_device_name(void) {
  snprintf(s_local_device_name, sizeof(s_local_device_name), "Pebble %02X%02X",
           s_local_address.octets[1], s_local_address.octets[0]);
}

static bool prv_has_device_name(void) {
  return (s_local_device_name[0] != '\0');
}

static void prv_configure_device_name(void) {
  pbl_bt_id_set_local_device_name(s_local_device_name);
}

void bt_local_id_configure_driver(void) {
  // Request the local address from the BT driver and cache it:
  pbl_bt_id_copy_local_identity_address(&s_local_address);

  if (!prv_has_device_name()) {
    prv_set_default_device_name();
  }

  prv_configure_device_name();
}

void bt_local_id_set_device_name(const char *device_name) {
  strncpy(s_local_device_name, device_name, sizeof(s_local_device_name));
  s_local_device_name[sizeof(s_local_device_name) - 1] = '\0';
  prv_configure_device_name();
}

void bt_local_id_copy_device_name(char name_out[PBL_BT_DEVICE_NAME_BUFFER_SIZE]) {
  strncpy(name_out, s_local_device_name, PBL_BT_DEVICE_NAME_BUFFER_SIZE);
}

void bt_local_id_copy_address(struct pbl_bt_addr *addr_out) {
  *addr_out = s_local_address;
}

void bt_local_id_copy_address_hex_string(char addr_hex_str_out[PBL_BT_BD_ADDR_FMT_BUFFER_SIZE]) {
  static const struct pbl_bt_addr null_addr = {};
  if (0 != memcmp(&null_addr, &s_local_address, sizeof(s_local_address))) {
    snprintf(addr_hex_str_out, PBL_BT_BD_ADDR_FMT_BUFFER_SIZE, PBL_BT_BD_ADDR_FMT,
             PBL_BT_ADDR_XPLODE(s_local_address));
  } else {
    snprintf(addr_hex_str_out, PBL_BT_BD_ADDR_FMT_BUFFER_SIZE, "Unknown");
  }
}

void bt_local_id_copy_address_mac_string(char addr_mac_str_out[PBL_BT_ADDR_FMT_BUFFER_SIZE]) {
  snprintf(addr_mac_str_out, PBL_BT_ADDR_FMT_BUFFER_SIZE, PBL_BT_ADDR_FMT,
           PBL_BT_ADDR_XPLODE(s_local_address));
}
