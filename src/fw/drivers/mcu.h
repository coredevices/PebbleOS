/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "system/status_codes.h"

StatusCode mcu_get_serial(void *buf, size_t *buf_sz);

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks);
