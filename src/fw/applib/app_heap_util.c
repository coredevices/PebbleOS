/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_heap_util.h"
#include "process_state/app_state/app_state.h"
#include "process_state/worker_state/worker_state.h"
#include "kernel/pebble_tasks.h"
#include "system/passert.h"
#include "util/heap.h"

static Heap* get_task_heap(void) {
  PebbleTask task = pebble_task_get_current();
  Heap *heap = NULL;

  if (task == PebbleTask_App) {
    heap = app_state_get_heap();
  } else if (task == PebbleTask_Worker) {
    heap = worker_state_get_heap();
  } else {
    WTF;
  }

  return heap;
}

size_t heap_bytes_used(void) {
  Heap *heap = get_task_heap();
  return heap->current_size;
}

size_t heap_bytes_free(void) {
  Heap *heap = get_task_heap();
  return heap_size(heap) - heap->current_size;
}
