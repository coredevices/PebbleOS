/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event_service_client.h"
#include "tick_timer_service.h"

typedef struct TickTimerServiceState {
  TickHandler handler;
  TimeUnits tick_units;
  struct pbl_tm last_time;
  bool first_tick;
  bool last_is_24h;

  EventServiceInfo tick_service_info;
} TickTimerServiceState;

void tick_timer_service_state_init(TickTimerServiceState *state);

//! @internal
//! initializes an event service that responds to PEBBLE_TICK_EVENT
void tick_timer_service_init(void);

//! @internal
//! de-register the tick timer handler
void tick_timer_service_reset(void);
