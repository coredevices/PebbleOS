/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct Heap Heap;

//! Logs an analytics event about the native heap OOM fault.
void app_heap_analytics_log_native_heap_oom_fault(size_t requested_size, Heap *heap);

//! Logs an analytics event about the JerryScript heap OOM fault.
void app_heap_analytics_log_rocky_heap_oom_fault(void);

//! Captures native heap and given JerryScript heap stats to the app heartbeat.
//! @param jerry_mem_stats JerryScript heap stats on NULL if not available.
void app_heap_analytics_log_stats_to_app_heartbeat(bool is_rocky_app);
