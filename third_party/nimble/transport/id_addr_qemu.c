/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/bluetooth/id_addr.h>

#include <devicetree/regs.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// Address given at launch, in the system controller. All zeros if none was.
#define SYSCTRL_BT_ADDR_LO (PBL_DT_REG_SYSCTRL + 0x18)
#define SYSCTRL_BT_ADDR_HI (PBL_DT_REG_SYSCTRL + 0x1C)

int pbl_bt_id_addr_get(ble_addr_t *addr) {
  uint32_t lo = REG32(SYSCTRL_BT_ADDR_LO);
  uint32_t hi = REG32(SYSCTRL_BT_ADDR_HI) & 0xFFFFU;

  if (lo == 0U && hi == 0U) {
    return -ENOENT;
  }

  addr->val[0] = lo;
  addr->val[1] = lo >> 8;
  addr->val[2] = lo >> 16;
  addr->val[3] = lo >> 24;
  addr->val[4] = hi;
  addr->val[5] = hi >> 8;
  addr->type = BLE_ADDR_RANDOM;

  return 0;
}
