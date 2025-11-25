/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/rtc.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

void fake_rtc_init(RtcTicks initial_ticks, time_t initial_time);

void fake_rtc_increment_time(time_t inc);
void fake_rtc_increment_time_ms(uint32_t inc);
void fake_rtc_set_ticks(RtcTicks new_ticks);
void fake_rtc_increment_ticks(RtcTicks tick_increment);
void fake_rtc_auto_increment_ticks(RtcTicks tick_increment);

// TODO: there is a lot of stuff missing.
