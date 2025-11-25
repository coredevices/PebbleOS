/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/event_service_client.h"
#include "connection_service.h"

typedef struct ConnectionServiceState {
  ConnectionHandlers handlers;
  EventServiceInfo bcs_info;
} ConnectionServiceState;

void connection_service_state_init(ConnectionServiceState *state);
