/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

//! Carefully timed spinloop that allows one to delay at a microsecond
//! granularity.
void delay_us(uint32_t us);

//! Waits for a certain amount of milliseconds by busy-waiting.
//!
//! @param millis The number of milliseconds to wait for
void delay_ms(uint32_t millis);
