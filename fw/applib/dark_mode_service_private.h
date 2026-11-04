/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "dark_mode_service.h"
#include "event_service_client.h"

typedef struct DarkModeServiceState {
  DarkModeHandler handler;
  EventServiceInfo dark_mode_info;
} DarkModeServiceState;

void dark_mode_service_state_init(DarkModeServiceState *state);
