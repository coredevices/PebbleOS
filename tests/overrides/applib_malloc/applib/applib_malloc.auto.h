/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// applib allocations go through the pbl_malloc fakes, so tests can track them.
#include <pbl/kernel/malloc.h>

#define applib_type_zalloc(Type)  pbl_zalloc(sizeof(Type))
#define applib_type_malloc(Type)  pbl_malloc(sizeof(Type))
#define applib_type_size(Type)    sizeof(Type)
#define applib_malloc(size)       pbl_malloc(size)
#define applib_zalloc(size)       pbl_zalloc(size)
#define applib_malloc_try(size)   pbl_malloc(size)
#define applib_zalloc_try(size)   pbl_zalloc(size)
#define applib_calloc(n, size)    pbl_calloc(n, size)
#define applib_realloc(ptr, size) pbl_realloc(ptr, size)
#define applib_free(ptr)          pbl_free(ptr)
