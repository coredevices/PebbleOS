/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef ECMA_GC_H
#define ECMA_GC_H

#include "ecma-globals.h"
#include "jmem-allocator.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmagc Garbage collector
 * @{
 */

extern void ecma_init_gc_info (ecma_object_t *);
extern void ecma_ref_object (ecma_object_t *);
extern void ecma_deref_object (ecma_object_t *);
extern void ecma_gc_run (jmem_free_unused_memory_severity_t);
extern void ecma_free_unused_memory (jmem_free_unused_memory_severity_t, size_t, bool);

/**
 * @}
 * @}
 */

#endif /* !ECMA_GC_H */
