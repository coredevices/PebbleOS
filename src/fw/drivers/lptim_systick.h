/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

void lptim_systick_init(void);

void lptim_calibrate_init(void);

bool lptim_systick_is_initialized(void);

void lptim_systick_enable(void);

void lptim_systick_pause(void);

void lptim_systick_resume(void);

void lptim_systick_tickless_idle(uint32_t ticks_from_now);

void lptim_systick_tickless_exit(void);

uint32_t lptim_systick_get_elapsed_ticks(void);

void lptim_systick_sync_after_wfi(void);

//! Returns the calibrated RC10K frequency in Hz (measured against HXT48).
//! This value is updated periodically by the calibration timer.
uint32_t lptim_systick_get_rc10k_freq(void);
