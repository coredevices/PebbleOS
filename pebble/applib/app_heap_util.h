/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup MemoryManagement Memory Management
//!   \brief Utility functions for managing an application's memory.
//!
//!   @{

//! Calculates the number of bytes of heap memory currently being used by the application.
//! @return The number of bytes on the heap currently being used.
size_t heap_bytes_used(void);

//! Calculates the number of bytes of heap memory \a not currently being used by the application.
//! @return The number of bytes on the heap not currently being used.
size_t heap_bytes_free(void);

//!   @} // end addtogroup MemoryManagement
//! @} // end addtogroup Foundation
