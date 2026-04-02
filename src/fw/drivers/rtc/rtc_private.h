/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @file rtc_private.h
//!
//! Functions private to implementations of RTC drivers for stm32 platforms

//! Called by rtc_init to initialize the clock source.
//!
//! Warning! In some cases this function may detect an edge case and reset the system to address
//! it! See the implementation for details.
//!
//! @return True if we managed to correctly get onto the LSE, false otherwise
bool rtc_init_config_clock_source(void);

//! Configure the LSE oscillator component of the RCC
void rtc_init_config_lse_clock_source(void);

//! Verify that the clock source is set up correctly.
//!
//! Warning! In some cases this function may detect an edge case and reset the system to address
//! it! See the implementation for details.
void rtc_init_verify_clock_source_config(void);

//! Enable access to the RTC's backup registers
void rtc_enable_backup_regs(void);
