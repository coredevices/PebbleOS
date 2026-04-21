/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch_service.h"
#include "touch_service_private.h"

#include <string.h>

void touch_service_state_init(TouchServiceState *state) {
  memset(state, 0, sizeof(*state));
}

void touch_service_subscribe(TouchServiceHandler handler, void *context) {
  (void)handler;
  (void)context;
}

void touch_service_unsubscribe(void) {
}

bool touch_service_is_enabled(void) {
  return false;
}
