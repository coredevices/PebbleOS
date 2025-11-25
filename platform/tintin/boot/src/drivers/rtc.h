/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Initialize the RTC with LSE as the clocksource
//! @return false if LSE init failed
bool rtc_init(void);

//! Set the RTC to run in fast mode
void rtc_initialize_fast_mode(void);

//! Slow down the RTC so we can keep time in standby mode
void rtc_slow_down(void);

//! Speed up the RTC for the firmware
void rtc_speed_up(void);
