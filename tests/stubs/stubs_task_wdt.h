/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/task_wdt/task_wdt.h>

int pbl_task_wdt_add(struct pbl_thread *thread, uint32_t timeout_ms,
                     pbl_task_wdt_callback_t callback, void *user_data) {
  return 0;
}

int pbl_task_wdt_delete(int channel_id) {
  return 0;
}

int pbl_task_wdt_feed(int channel_id) {
  return 0;
}

void pbl_task_wdt_feed_self(void) {
}

void pbl_task_wdt_feed_thread(struct pbl_thread *thread) {
}

void pbl_task_wdt_feed_all(void) {
}

void pbl_task_wdt_suspend(uint32_t timeout_ms) {
}

void pbl_task_wdt_resume(void) {
}
