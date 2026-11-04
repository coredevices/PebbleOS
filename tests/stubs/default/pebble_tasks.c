/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <kernel/pebble_tasks.h>

PBL_WEAK PebbleTask pebble_task_get_current(void) {
  return 0;
}

PBL_WEAK struct pbl_thread *pebble_task_get_thread(PebbleTask task) {
  return nullptr;
}

PBL_WEAK const char *pebble_task_get_name(PebbleTask task) {
  return nullptr;
}

PBL_WEAK void pebble_task_unregister(PebbleTask task) {
}

PBL_WEAK void pebble_task_register(PebbleTask task, struct pbl_thread *thread) {
}
