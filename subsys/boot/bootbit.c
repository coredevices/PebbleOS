/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/boot/bootbit.h>
#include <pbl/drivers/retained.h>
#include <pbl/util/bits.h>

#define FW_FAIL_CNT_MASK \
  PBL_GENMASK(PBL_BOOTBIT_FW_START_FAIL_STRIKE_TWO, PBL_BOOTBIT_FW_START_FAIL_STRIKE_ONE)
#define RESET_LOOP_CNT_MASK \
  PBL_GENMASK(PBL_BOOTBIT_RESET_LOOP_DETECT_THREE, PBL_BOOTBIT_RESET_LOOP_DETECT_ONE)

static void prv_update(uint32_t clear, uint32_t set) {
  uint32_t bits = pbl_retained_read(CONFIG_BOOT_BOOTBITS_RETAINED_REG);

  pbl_retained_write(CONFIG_BOOT_BOOTBITS_RETAINED_REG, (bits & ~clear) | set);
}

void pbl_bootbit_set(enum pbl_bootbit bit) {
  prv_update(0U, PBL_BIT(bit));
}

void pbl_bootbit_clear(enum pbl_bootbit bit) {
  prv_update(PBL_BIT(bit), 0U);
}

bool pbl_bootbit_test(enum pbl_bootbit bit) {
  return (pbl_retained_read(CONFIG_BOOT_BOOTBITS_RETAINED_REG) & PBL_BIT(bit)) != 0U;
}

uint8_t pbl_bootbit_fw_fail_cnt_get(void) {
  return PBL_FIELD_GET(FW_FAIL_CNT_MASK, pbl_retained_read(CONFIG_BOOT_BOOTBITS_RETAINED_REG));
}

void pbl_bootbit_reset_loop_cnt_set(uint8_t cnt) {
  prv_update(RESET_LOOP_CNT_MASK, PBL_FIELD_PREP(RESET_LOOP_CNT_MASK, cnt));
}
