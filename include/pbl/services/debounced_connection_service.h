/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include <stdbool.h>

void debounced_connection_service_init(void);

bool debounced_connection_service_is_connected(void);

void debounced_connection_service_handle_event(PebbleCommSessionEvent *e);
