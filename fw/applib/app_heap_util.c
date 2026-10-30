/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_heap_util.h"

#include <pbl/kernel/fatal.h>
#include <pbl/kernel/heap.h>

#include <kernel/pebble_tasks.h>
#include <process_state/app_state/app_state.h>
#include <process_state/worker_state/worker_state.h>

static struct pbl_heap *get_task_heap(void) {
  PebbleTask task = pebble_task_get_current();
  struct pbl_heap *heap = nullptr;

  if (task == PebbleTask_App) {
    heap = app_state_get_heap();
  } else if (task == PebbleTask_Worker) {
    heap = worker_state_get_heap();
  } else {
    pbl_panic();
  }

  return heap;
}

size_t heap_bytes_used(void) {
  struct pbl_heap *heap = get_task_heap();
  return pbl_heap_used(heap);
}

size_t heap_bytes_free(void) {
  struct pbl_heap *heap = get_task_heap();
  return pbl_heap_size(heap) - pbl_heap_used(heap);
}
