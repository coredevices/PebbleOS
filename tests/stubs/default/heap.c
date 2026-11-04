/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/heap.h>

PBL_WEAK void pbl_heap_dump(struct pbl_heap *heap, pbl_heap_dump_cb_t cb, void *arg) {
}

PBL_WEAK size_t pbl_heap_size(const struct pbl_heap *heap) {
  return 0;
}

PBL_WEAK bool pbl_heap_is_allocated(struct pbl_heap *heap, void *ptr) {
  return true;
}

PBL_WEAK [[noreturn]] void pbl_heap_oom_handler(struct pbl_heap *heap, size_t size,
                                                uintptr_t caller) {
  abort();
}
