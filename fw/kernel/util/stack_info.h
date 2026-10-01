/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Return the number of free bytes left in the current stack. Returns 0 if stack space could
//! not be determined.
uint32_t stack_free_bytes(void);
