/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <time.h>

#include <pbl/kernel/time.h>
#include <pbl/services/time.h>

#include <services/clock/tz_storage.h>

// Fakes the kernel wall clock and uptime, and the clock service timezone storage.

void fake_rtc_init(uint64_t initial_ticks, time_t initial_time);

void fake_rtc_increment_time(time_t inc);
void fake_rtc_increment_time_ms(uint32_t inc);
void fake_rtc_set_ticks(uint64_t new_ticks);
void fake_rtc_increment_ticks(uint64_t tick_increment);
void fake_rtc_auto_increment_ticks(uint64_t tick_increment);
