/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pressure_service.h"
#include "app_timer.h"

typedef struct PressureServiceState {
  PressureDataHandler handler;
  AppTimer *poll_timer;
  int32_t ref_pressure_pa;   //!< Reference pressure for altitude (0 = use standard 101325 Pa)
  PressureODR odr;
  bool use_full_formula;
} PressureServiceState;

//! Initialize the pressure service state for a new app. Called during app_state_init.
void pressure_service_state_init(PressureServiceState *state);

//! Clean up any active subscriptions. Called during app teardown.
void pressure_service_state_deinit(PressureServiceState *state);
