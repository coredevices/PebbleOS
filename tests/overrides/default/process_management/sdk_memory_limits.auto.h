/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Memory limits for SDK applications
//!
//! These macros describe the memory limits imposed on SDK applications
//! based on the major SDK version they were compiled against. The
//! limits vary depending on both the SDK version and the platform.
//!
//! The APP_RAM_nX_SIZE macros describe the total amount of memory that
//! an SDK app can use directly for stack, heap, text, data and bss.

#pragma once

#include <stddef.h>

#define APP_RAM_SYSTEM_SIZE ((size_t)65536)
#define APP_RAM_2X_SIZE ((size_t)23900)
#define APP_RAM_3X_SIZE ((size_t)65536)
#define APP_RAM_4X_SIZE ((size_t)65536)
