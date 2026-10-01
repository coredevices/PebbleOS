/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

//! Carefully timed spinloop that allows one to delay at a microsecond
//! granularity.
void delay_us(uint32_t us);

void delay_init(void);
