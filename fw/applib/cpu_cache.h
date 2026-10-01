/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! @file cpu_cache.h
//! @addtogroup Foundation
//! @{
//!   @addtogroup MemoryManagement Memory Management
//!   \brief Utility functions for managing an application's memory.
//!
//!   @{

//! Flushes the data cache and invalidates the instruction cache for the given region of memory,
//! if necessary. This is only required when your app is loading or modifying code in memory and
//! intends to execute it. On some platforms, code executed may be cached internally to improve
//! performance. After writing to memory, but before executing, this function must be called in
//! order to avoid undefined behavior. On platforms without caching, this performs no operation.
//! @param start The beginning of the buffer to flush
//! @param size How many bytes to flush
void memory_cache_flush(void *start, size_t size);

//!   @} // end addtogroup MemoryManagement
//! @} // end addtogroup Foundation
