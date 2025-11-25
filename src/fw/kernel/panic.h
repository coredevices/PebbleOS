/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void launcher_panic(uint32_t error_code);

uint32_t launcher_panic_get_current_error(void);

