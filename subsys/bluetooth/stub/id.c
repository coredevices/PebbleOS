/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/id.h>
#include <pbl/bluetooth/types.h>

#include <string.h>

void pbl_bt_id_set_local_device_name(const char device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]) {
}

void pbl_bt_id_copy_local_identity_address(struct pbl_bt_addr *addr_out) {
  memset(addr_out, 0xAA, sizeof(*addr_out));
}

void pbl_bt_set_local_address(bool allow_cycling, const struct pbl_bt_addr *pinned_address) {
}

void pbl_bt_id_copy_chip_info_string(char *dest, size_t dest_size) {
  strncpy(dest, "QEMU", dest_size);
}

bool pbl_bt_id_generate_private_resolvable_address(struct pbl_bt_addr *address_out) {
  *address_out = (struct pbl_bt_addr){};
  return true;
}
