/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "rocky_api.h"
#include "jerry-api.h"

// array of evented APIs, last entry must be NULL
void rocky_global_init(const RockyGlobalAPI *const *evented_apis);

void rocky_global_deinit(void);

bool rocky_global_has_event_handlers(const char *event_name);

void rocky_global_call_event_handlers(jerry_value_t event);

//! Schedules the event to be processed on a later event loop iteration.
void rocky_global_call_event_handlers_async(jerry_value_t event);

// Create a BaseEvent, filling in the type field with the given type string.
// The returned event must be released by the caller.
jerry_value_t rocky_global_create_event(const char *type_str);
