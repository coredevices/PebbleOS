/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/id_addr.h>
#include <pbl/drivers/nvmem/sf32lb.h>

#define UID_SIZE  8U
#define UID_MAGIC 0xA5U

#ifdef CONFIG_SOC_SF32LB57
#define EFUSE SF32LB57_EFUSE
#else
#define EFUSE SF32LB52_EFUSE
#endif

static const struct pbl_nvmem_cell s_uid_cell = PBL_NVMEM_CELL(EFUSE, 0U, UID_SIZE);

// The first six bytes of the eFUSE unique ID are the address, followed by
// their checksum and a magic byte. Parts not programmed at the factory have none.
int pbl_bt_id_addr_get(struct pbl_bt_addr *addr, enum pbl_bt_id_addr_type *type) {
  uint8_t uid[UID_SIZE];
  uint8_t chksum = 0U;
  int ret;

  ret = pbl_nvmem_cell_read(&s_uid_cell, uid, sizeof(uid));
  if (ret != 0) {
    return ret;
  }

  if (uid[7] != UID_MAGIC) {
    return -ENOENT;
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
