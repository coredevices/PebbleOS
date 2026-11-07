/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void pbl_coredump__log(const char *msg);
void pbl_coredump__log_int(const char *msg, uint32_t value, int base);

#ifdef UNITTEST
void pbl_coredump__test_reset(void);
#endif
