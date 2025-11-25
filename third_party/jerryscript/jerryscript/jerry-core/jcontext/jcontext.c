/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "jcontext.h"

/** \addtogroup context Context
 * @{
 */

/**
 * Global context.
 */
jerry_context_t jerry_global_context;

/**
 * Jerry global heap section attribute.
 */
#ifndef JERRY_HEAP_SECTION_ATTR
#define JERRY_GLOBAL_HEAP_SECTION
#else /* JERRY_HEAP_SECTION_ATTR */
#define JERRY_GLOBAL_HEAP_SECTION __attribute__ ((section (JERRY_HEAP_SECTION_ATTR)))
#endif /* !JERRY_HEAP_SECTION_ATTR */

/**
 * Global heap.
 */
jmem_heap_t jerry_global_heap __attribute__ ((aligned (JMEM_ALIGNMENT))) JERRY_GLOBAL_HEAP_SECTION;

#ifndef CONFIG_ECMA_LCACHE_DISABLE

/**
 * Global hash table.
 */
jerry_hash_table_t jerry_global_hash_table;

#endif /* !CONFIG_ECMA_LCACHE_DISABLE */

/**
 * @}
 */
