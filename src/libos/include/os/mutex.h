/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>


#include "util/list.h"

struct pebble_mutex_t;
typedef struct pebble_mutex_t PebbleMutex;
struct pebble_recursive_mutex_t;
typedef struct pebble_recursive_mutex_t PebbleRecursiveMutex;

#define INVALID_MUTEX_HANDLE 0

//! @return INVALID_MUTEX_HANDLE if failed, success otherwise.
PebbleMutex *mutex_create(void);

void mutex_destroy(PebbleMutex *handle);

void mutex_lock(PebbleMutex *handle);

bool mutex_lock_with_timeout(PebbleMutex *handle, uint32_t timeout_ms);

void mutex_lock_with_lr(PebbleMutex *handle, uint32_t myLR);

void mutex_unlock(PebbleMutex *handle);

PebbleRecursiveMutex * mutex_create_recursive(void);

void mutex_lock_recursive(PebbleRecursiveMutex *handle);

bool mutex_lock_recursive_with_timeout(PebbleRecursiveMutex *handle, uint32_t timeout_ms);

bool mutex_lock_recursive_with_timeout_and_lr(PebbleRecursiveMutex *handle,
                                              uint32_t timeout_ms,
                                              uint32_t LR);

//! Tests if a given mutex is owned by the current task. Useful for
//! ensuring locks are held when they should be.
bool mutex_is_owned_recursive(PebbleRecursiveMutex *handle);

void mutex_unlock_recursive(PebbleRecursiveMutex *handle);

//! @return true if the calling task owns the mutex
void mutex_assert_held_by_curr_task(PebbleMutex *handle, bool is_held);

//! @return true if the calling task owns the mutex
void mutex_assert_recursive_held_by_curr_task(PebbleRecursiveMutex *handle, bool is_held);
