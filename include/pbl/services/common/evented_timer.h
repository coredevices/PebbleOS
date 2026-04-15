/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "kernel/pebble_tasks.h"

//! @file evented_timer.h
//! A timer service that runs callbacks on the calling tasks event loop. This makes timers easy to use without having
//! to deal with concurrency issues.

// uintptr_t to avoid casting to pointers in unit tests (previously: uint32_t)
typedef uintptr_t EventedTimerID;
#define EVENTED_TIMER_INVALID_ID 0


typedef void (*EventedTimerCallback)(void* data);

//! Call once at startup to initialize the evented timer service.
void evented_timer_init(void);

//! Called by the kernel to clean up any timers that still may be pending for an app. These timers are cancelled
//! without notifying the original client.
void evented_timer_clear_process_timers(PebbleTask task);

EventedTimerID evented_timer_register(uint32_t timeout_ms, bool repeating,
                                      EventedTimerCallback callback, void* callback_data);

bool evented_timer_reschedule(EventedTimerID timer, uint32_t new_timeout_ms);

//! Reschedules a given timer if possible or creates new one
//! Returns passed timer id or new id if succeeded,  EVENTED_TIMER_INVALID_ID in any other case
EventedTimerID evented_timer_register_or_reschedule(EventedTimerID timer_id, uint32_t timeout_ms,
    EventedTimerCallback callback, void *data);

//! Cancel a currently running timer. No-op if timer is EVENTED_TIMER_INVALID_ID.
void evented_timer_cancel(EventedTimerID timer);

//! Checks that the given timer exists.
bool evented_timer_exists(EventedTimerID timer);

//! Checks that the given timer targets the current task.
bool evented_timer_is_current_task(EventedTimerID timer);

//! Reset the evented_timer system. Only useful in unit tests.
void evented_timer_reset(void);

//! Get the data passed to the timer
void *evented_timer_get_data(EventedTimerID timer);
