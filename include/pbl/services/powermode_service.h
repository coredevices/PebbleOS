/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Initialize the power mode service.
void powermode_service_init(void);

//! Called once the launcher has finished booting. Low-power CPU mode is not
//! entered until this runs so early init stays at high performance.
void powermode_service_boot_complete(void);

//! Enable or disable the power mode service. When disabled, request and
//! release calls are no-ops.
void powermode_service_set_enabled(bool enabled);

//! Request high-performance CPU mode. Must be paired with a
//! powermode_service_release_hp() call.
void powermode_service_request_hp(void);

//! Release a previously requested high-performance mode. The CPU will
//! return to low-power mode only when all clients have released.
void powermode_service_release_hp(void);

//! Keep the CPU at least at the light tier (48 MHz) for \a duration_ms longer.
//! Used for short bursts of UI work (compositor, animations) without a persistent hold.
void powermode_service_boost_ms(uint32_t duration_ms);

//! Report how long a CPU-bound work slice took. When it exceeds the budget for
//! the current frequency tier, the governor steps up to the next tier.
void powermode_service_report_work_ms(uint32_t duration_ms);
