/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/heap.h"

void heap_dump_malloc_instrumentation_to_dbgserial(Heap *heap) {}

size_t heap_size(const Heap *heap) {
  return 0;
}

bool heap_is_allocated(Heap *heap, void *ptr) {
  return true;
}

