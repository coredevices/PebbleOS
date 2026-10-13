/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/task_wdt/task_wdt.h>

PBL_WEAK int pbl_task_wdt_add(struct pbl_thread *thread, uint32_t timeout_ms,
                              pbl_task_wdt_callback_t callback, void *user_data) {
  return 0;
}

PBL_WEAK int pbl_task_wdt_delete(int channel_id) {
  return 0;
}

PBL_WEAK int pbl_task_wdt_feed(int channel_id) {
  return 0;
}

PBL_WEAK void pbl_task_wdt_feed_self(void) {
}

PBL_WEAK void pbl_task_wdt_feed_thread(struct pbl_thread *thread) {
}

PBL_WEAK void pbl_task_wdt_feed_all(void) {
}

PBL_WEAK void pbl_task_wdt_suspend(uint32_t timeout_ms) {
}

PBL_WEAK void pbl_task_wdt_resume(void) {
}
