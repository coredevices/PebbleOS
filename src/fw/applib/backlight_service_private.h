/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "backlight_service.h"
#include "event_service_client.h"

typedef struct __attribute__((packed)) BacklightServiceState {
  BacklightHandler handler;
  EventServiceInfo bls_info;
} BacklightServiceState;

void backlight_service_state_init(BacklightServiceState *state);
