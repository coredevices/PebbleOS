/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// NOTE: This file is uses as an -include file when compiling each jerry-core source file, so
// be careful what you add here!

#define JERRY_CONTEXT(field) (rocky_runtime_context_get()->jerry_global_context.field)
#define JERRY_HEAP_CONTEXT(field) (rocky_runtime_context_get()->jerry_global_heap.field)
#define JERRY_HASH_TABLE_CONTEXT(field) (rocky_runtime_context_get()->jerry_global_hash_table.field)

#include "jcontext.h"

typedef struct RockyRuntimeContext {
  jerry_context_t jerry_global_context;
  jmem_heap_t jerry_global_heap;
#ifndef CONFIG_ECMA_LCACHE_DISABLE
  jerry_hash_table_t jerry_global_hash_table;
#endif
} RockyRuntimeContext;

_Static_assert(
    ((offsetof(RockyRuntimeContext, jerry_global_heap) +
      offsetof(jmem_heap_t, area)) % JMEM_ALIGNMENT) == 0,
    "jerry_global_heap.area must be aligned to JMEM_ALIGNMENT!");

extern RockyRuntimeContext * rocky_runtime_context_get(void);
