/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event_service_client.h"
#include "touch_service.h"

#include <stdbool.h>

//! Per-task state for the applib touch service. Must live in task-accessible
//! memory (app/kernel state) so syscalls that validate buffers see it
//! as userspace-local.
typedef struct TouchServiceState {
  // System slot: an internal handler that observes touch alongside (and
  // before) any app subscriber. Never cleared by touch_service_unsubscribe().
  TouchServiceHandler system_handler;
  void *system_context;
  // Raw slot: the app-facing subscription managed via touch_service_subscribe.
  TouchServiceHandler raw_handler;
  void *raw_context;
  // A single event-service subscription is shared by both slots.
  EventServiceInfo event_info;
  bool subscribed;
} TouchServiceState;

//! Initialize the state struct to a quiescent state.
void touch_service_state_init(TouchServiceState *state);

//! Set the internal system-level touch handler slot. Not exposed to apps: it
//! is used by system components that must observe touch alongside any app
//! subscriber. The system handler runs before the app's raw handler. Pass a
//! NULL handler to clear the slot. Independent of touch_service_unsubscribe().
void touch_service_set_system_handler(TouchServiceHandler handler, void *context);
