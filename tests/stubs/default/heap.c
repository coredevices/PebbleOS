/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/util/heap.h>

PBL_WEAK void heap_dump_malloc_instrumentation_to_dbgserial(Heap *heap) {
}

PBL_WEAK size_t heap_size(const Heap *heap) {
  return 0;
}

PBL_WEAK bool heap_is_allocated(Heap *heap, void *ptr) {
  return true;
}
