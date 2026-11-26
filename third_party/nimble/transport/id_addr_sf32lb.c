/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/id_addr.h>
#include <pbl/drivers/nvmem/sf32lb.h>
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

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
int pbl_bt_id_addr_get(ble_addr_t *addr) {
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

  for (size_t i = 0U; i < sizeof(addr->val); i++) {
    chksum += uid[i];
  }
  // Not worth refusing to boot over: as good as none.
  if (chksum != uid[6]) {
    PBL_LOG_ERR("The identity address in the eFUSE is corrupt");
    return -ENOENT;
  }

  memcpy(addr->val, uid, sizeof(addr->val));
  addr->type = BLE_ADDR_PUBLIC;

  return 0;
}
