/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_permissions.h"
#include "applib/event_service_client.h"

typedef struct AppPermissionServiceState {
  EventServiceInfo event_info;
  AppPermissionChangedHandler handler;
  void *context;
} AppPermissionServiceState;

void app_permission_service_state_init(AppPermissionServiceState *state);
