/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Initialize the power mode service.
void powermode_service_init(void);

//! Enable or disable the power mode service. When disabled, request and
//! release calls are no-ops.
void powermode_service_set_enabled(bool enabled);

//! Request high-performance CPU mode. Must be paired with a
//! powermode_service_release_hp() call.
void powermode_service_request_hp(void);

//! Release a previously requested high-performance mode. The CPU will
//! return to low-power mode only when all clients have released.
void powermode_service_release_hp(void);
