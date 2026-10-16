/* SPDX-FileCopyrightText: 2026 Eyal Leifenberg */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "event_service_client.h"
#include "notification_service.h"

typedef struct NotificationServiceState {
  NotificationCountHandler handler;
  EventServiceInfo event_info;
} NotificationServiceState;

void notification_service_state_init(NotificationServiceState *state);
