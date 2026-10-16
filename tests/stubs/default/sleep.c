/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/thread.h>

PBL_WEAK void pbl_sleep_ms(uint32_t ms) {
}

PBL_WEAK void pbl_sleep_us(uint32_t us) {
}

PBL_WEAK void pbl_busy_wait_us(uint32_t us) {
}

PBL_WEAK void pbl_thread_sleep(pbl_timeout_t timeout) {
}
