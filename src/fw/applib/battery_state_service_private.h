/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event_service_client.h"
#include "battery_state_service.h"
#include "pbl/kernel/compiler.h"

typedef struct PBL_PACKED BatteryStateServiceState {
  BatteryStateHandler handler;

  EventServiceInfo bss_info;
} BatteryStateServiceState;

void battery_state_service_state_init(BatteryStateServiceState *state);
