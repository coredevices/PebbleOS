/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct pebble_mutex_t {
  void *foo;
};

struct pebble_recursive_mutex_t {
  int foo;
};

typedef struct pebble_mutex_t PebbleMutex;
typedef struct pebble_recursive_mutex_t PebbleRecursiveMutex;

PebbleMutex *mutex_create(void) {
  PebbleMutex *mutex = (void *)1;
  return (mutex);
}

void mutex_destroy(PebbleMutex *handle) {
  return;
}

void mutex_lock(PebbleMutex *handle) {
  return;
}

bool mutex_lock_with_timeout(PebbleMutex *handle, uint32_t timeout_ms) {
  return true;
}

void mutex_lock_with_lr(PebbleMutex * handle, uint32_t myLR) {
}

void mutex_unlock(PebbleMutex *handle) {
  return;
}

PebbleRecursiveMutex *mutex_create_recursive(void) {
  PebbleRecursiveMutex *mutex = (void *)1;
  return (mutex);
}

void mutex_lock_recursive(PebbleRecursiveMutex *handle) {
  return;
}

bool mutex_lock_recursive_with_timeout(PebbleRecursiveMutex *handle, uint32_t timeout_ms) {
  return true;
}

bool mutex_is_owned_recursive(PebbleRecursiveMutex *handle) {
  return true;
}

void mutex_unlock_recursive(PebbleRecursiveMutex *handle) {
  return;
}

void mutex_assert_held_by_curr_task(PebbleMutex * handle, bool is_held) {
}

void mutex_assert_recursive_held_by_curr_task(PebbleRecursiveMutex * handle, bool is_held) {
}
