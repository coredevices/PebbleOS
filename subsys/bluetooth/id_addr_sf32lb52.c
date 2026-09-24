/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <bf0_hal_efuse.h>
#include <pbl/bluetooth/id_addr.h>

#define UID_SIZE  8U
#define UID_MAGIC 0xA5U

// The first six bytes of the eFUSE unique ID are the address, followed by
// their checksum and a magic byte.
int pbl_bt_id_addr_get(struct pbl_bt_addr *addr, enum pbl_bt_id_addr_type *type) {
  uint8_t uid[UID_SIZE];
  uint8_t chksum = 0U;

  if (HAL_EFUSE_Read(0, uid, UID_SIZE) != (int32_t)UID_SIZE || uid[7] != UID_MAGIC) {
    return -ENODEV;
  }

  for (size_t i = 0U; i < sizeof(addr->octets); i++) {
    chksum += uid[i];
  }
  if (chksum != uid[6]) {
    return -EIO;
  }

  memcpy(addr->octets, uid, sizeof(addr->octets));
  *type = PBL_BT_ID_ADDR_PUBLIC;

  return 0;
}
