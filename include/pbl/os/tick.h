/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "portmacro.h"

#include <stdint.h>

TickType_t milliseconds_to_ticks(uint32_t milliseconds);

uint32_t ticks_to_milliseconds(TickType_t ticks);
