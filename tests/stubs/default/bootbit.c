/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/boot/bootbit.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK void pbl_bootbit_set(enum pbl_bootbit bit) {
}

PBL_WEAK void pbl_bootbit_clear(enum pbl_bootbit bit) {
}

PBL_WEAK bool pbl_bootbit_test(enum pbl_bootbit bit) {
  return false;
}

PBL_WEAK uint8_t pbl_bootbit_fw_fail_cnt_get(void) {
  return 0U;
}

PBL_WEAK void pbl_bootbit_reset_loop_cnt_set(uint8_t cnt) {
}
