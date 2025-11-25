/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event_service_client.h"
#include "tick_timer_service.h"

typedef struct __attribute__((packed)) TickTimerServiceState {
  TickHandler handler;
  TimeUnits tick_units;
  struct tm last_time;
  bool first_tick;

  EventServiceInfo tick_service_info;
} TickTimerServiceState;

void tick_timer_service_state_init(TickTimerServiceState *state);

//! @internal
//! initializes an event service that responds to PEBBLE_TICK_EVENT
void tick_timer_service_init(void);

//! @internal
//! de-register the tick timer handler
void tick_timer_service_reset(void);
