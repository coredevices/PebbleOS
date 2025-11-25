/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JMEM_ALLOCATOR_INTERNAL_H
#define JMEM_ALLOCATOR_INTERNAL_H

#ifndef JMEM_ALLOCATOR_INTERNAL
# error "The header is for internal routines of memory allocator component. Please, don't use the routines directly."
#endif /* !JMEM_ALLOCATOR_INTERNAL */

#include <stdbool.h>
#include <stddef.h>

/** \addtogroup mem Memory allocation
 * @{
 */

extern void jmem_run_free_unused_memory_callbacks (jmem_free_unused_memory_severity_t, size_t, bool);

/**
 * @}
 */

#endif /* !JMEM_ALLOCATOR_INTERNAL_H */
