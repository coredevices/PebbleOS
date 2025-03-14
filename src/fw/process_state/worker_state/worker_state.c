/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>

#include "kernel/util/segment.h"
#include "process_management/process_manager.h"
#include "process_state/worker_state/worker_state.h"
#include "syscall/syscall.h"
#include "tinymt32.h"
#include "util/attributes.h"

typedef struct {
  Heap heap;

  struct tm gmtime_tm;
  struct tm localtime_tm;
  char localtime_zone[TZ_LEN];

  tinymt32_t rand_seed;

  AccelServiceState accel_state;

  CompassServiceConfig *compass_config;

  EventServiceInfo event_service_state;

  PluginServiceState plugin_service_state;

  LogState log_state;

  BatteryStateServiceState battery_state_service_state;

  TickTimerServiceState tick_timer_service_state;

  ConnectionServiceState connection_service_state;

#if CAPABILITY_HAS_HEALTH_TRACKING
  HealthServiceState health_service_state;
#endif
} WorkerState;

KERNEL_READONLY_DATA static WorkerState *s_worker_state_ptr;

bool worker_state_configure(MemorySegment *worker_state_ram) {
  s_worker_state_ptr = memory_segment_split(worker_state_ram, NULL,
                                            sizeof(WorkerState));
  return s_worker_state_ptr != NULL;
}

void worker_state_init(void) {
  s_worker_state_ptr->rand_seed.mat1 = 0; // Uninitialized

  accel_service_state_init(worker_state_get_accel_state());

  plugin_service_state_init(worker_state_get_plugin_service());

  battery_state_service_state_init(worker_state_get_battery_state_service_state());

  connection_service_state_init(worker_state_get_connection_service_state());

  tick_timer_service_state_init(worker_state_get_tick_timer_service_state());

#if CAPABILITY_HAS_HEALTH_TRACKING
  health_service_state_init(worker_state_get_health_service_state());
#endif
}

void worker_state_deinit(void) {
#if CAPABILITY_HAS_HEALTH_TRACKING
  health_service_state_deinit(worker_state_get_health_service_state());
#endif
}

Heap *worker_state_get_heap(void) {
  return &s_worker_state_ptr->heap;
}

struct tm *worker_state_get_gmtime_tm(void) {
  return &s_worker_state_ptr->gmtime_tm;
}
struct tm *worker_state_get_localtime_tm(void) {
  return &s_worker_state_ptr->localtime_tm;
}
char *worker_state_get_localtime_zone(void) {
  return s_worker_state_ptr->localtime_zone;
}
void *worker_state_get_rand_ptr(void) {
  return &s_worker_state_ptr->rand_seed;
}

AccelServiceState *worker_state_get_accel_state(void) {
  return &s_worker_state_ptr->accel_state;
}

CompassServiceConfig **worker_state_get_compass_config(void) {
  return &s_worker_state_ptr->compass_config;
}

EventServiceInfo *worker_state_get_event_service_state(void) {
  return &s_worker_state_ptr->event_service_state;
}

PluginServiceState *worker_state_get_plugin_service(void) {
  return &s_worker_state_ptr->plugin_service_state;
}

LogState *worker_state_get_log_state(void) {
  return &s_worker_state_ptr->log_state;
}

BatteryStateServiceState *worker_state_get_battery_state_service_state(void) {
  return &s_worker_state_ptr->battery_state_service_state;
}

TickTimerServiceState *worker_state_get_tick_timer_service_state(void) {
  return &s_worker_state_ptr->tick_timer_service_state;
}

ConnectionServiceState *worker_state_get_connection_service_state(void) {
  return &s_worker_state_ptr->connection_service_state;
}

#if CAPABILITY_HAS_HEALTH_TRACKING
HealthServiceState *worker_state_get_health_service_state(void) {
  return &s_worker_state_ptr->health_service_state;
}
#endif



// ===================================================================================================
// Serial Commands
#ifdef MALLOC_INSTRUMENTATION
void command_dump_malloc_worker(void) {
  heap_dump_malloc_instrumentation_to_dbgserial(worker_state_get_heap());
}
#endif



