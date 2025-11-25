/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void advanced_logging_init(void);

void pbl_log_advanced(const char* buffer, int length, bool async);

