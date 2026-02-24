/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

typedef struct Heap Heap;

//! Logs an analytics event about the native heap OOM fault.
void app_heap_analytics_log_native_heap_oom_fault(size_t requested_size, Heap *heap);

//! Captures native heap stats to the app heartbeat.
void app_heap_analytics_log_stats_to_app_heartbeat(void);
