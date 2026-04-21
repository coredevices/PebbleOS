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
  TouchServiceHandler raw_handler;
  void *raw_context;
  EventServiceInfo raw_event_info;
  bool raw_subscribed;
} TouchServiceState;

//! Initialize the state struct to a quiescent state.
void touch_service_state_init(TouchServiceState *state);
