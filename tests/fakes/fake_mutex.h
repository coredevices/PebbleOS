/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/passert.h"
#include "util/list.h"

#include <os/mutex.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct FakePebbleMutex {
  ListNode node;
  uint8_t lock_count;
  bool recursive;
} FakePebbleMutex;

static FakePebbleMutex *s_mutex_list;
static bool s_asserts_disabled;
static bool s_assert_triggered;

//
// Helpers
//

static void *prv_mutex_create(bool is_recursive) {
  FakePebbleMutex *mutex = malloc(sizeof(FakePebbleMutex));
  *mutex = (FakePebbleMutex) {
    .recursive = is_recursive,
  };
  s_mutex_list = (FakePebbleMutex *)list_prepend((ListNode *)s_mutex_list, (ListNode *)mutex);
  return mutex;
}

static bool prv_list_foreach_assert_unlocked(ListNode *node, void *context) {
  FakePebbleMutex *mutex = (FakePebbleMutex *)node;
  bool *failed = context;
  if (mutex->lock_count != 0) {
    // If this is failing, set your breakpoint here to find out which mutex
    printf("Mutex (0x%lx) was not unlocked when fake_mutex_assert_all_unlocked called\n",
           (unsigned long)mutex);
    s_assert_triggered = true;
    cl_assert(s_asserts_disabled);
    *failed = true;
  }
  return true;
}

//
// Fake Mutex API
//

void fake_mutex_reset(bool assert_all_unlocked) {
  ListNode *iter = (ListNode *)s_mutex_list;
  while (iter) {
    ListNode *next = iter->next;
    if (assert_all_unlocked) {
      FakePebbleMutex *mutex = (FakePebbleMutex *)iter;
      cl_assert_equal_i(0, mutex->lock_count);
    }
    free(iter);
    iter = next;
  }
  s_mutex_list = NULL;

  s_asserts_disabled = false;
  s_assert_triggered = false;
}

void fake_mutex_assert_all_unlocked(void) {
  bool failed;
  list_foreach((ListNode *)s_mutex_list, prv_list_foreach_assert_unlocked, &failed);
}

//
// Mutex API
//

PebbleMutex *mutex_create(void) {
  const bool is_recursive = false;
  return prv_mutex_create(is_recursive);
}

void mutex_destroy(PebbleMutex *handle) {
  FakePebbleMutex *mutex = (FakePebbleMutex *)handle;
  list_remove((ListNode *)mutex, (ListNode **)&s_mutex_list, NULL);
}

void mutex_lock(PebbleMutex *handle) {
  FakePebbleMutex *mutex = (FakePebbleMutex *)handle;
  if (mutex->lock_count != 0) {
    s_assert_triggered = true;
    cl_assert_(s_asserts_disabled, "mutex_lock called with mutex that was already locked");
  }
  mutex->lock_count++;
}

bool mutex_lock_with_timeout(PebbleMutex *handle, uint32_t timeout_ms) {
  mutex_lock(handle);
  return true;
}

void mutex_lock_with_lr(PebbleMutex *handle, uint32_t myLR) {
  mutex_lock(handle);
}

void mutex_unlock(PebbleMutex *handle) {
  FakePebbleMutex *mutex = (FakePebbleMutex *)handle;
  if (mutex->lock_count != 1) {
    s_assert_triggered = true;
    cl_assert_(s_asserts_disabled, "mutex_unlock called with mutex that was not locked");
  }
  mutex->lock_count--;
}

PebbleRecursiveMutex * mutex_create_recursive(void) {
  const bool is_recursive = true;
  return prv_mutex_create(is_recursive);
}

void mutex_lock_recursive(PebbleRecursiveMutex *handle) {
  FakePebbleMutex *mutex = (FakePebbleMutex *)handle;
  mutex->lock_count++;
}

bool mutex_lock_recursive_with_timeout(PebbleRecursiveMutex *handle, uint32_t timeout_ms) {
  mutex_lock_recursive(handle);
  return true;
}

bool mutex_lock_recursive_with_timeout_and_lr(PebbleRecursiveMutex *handle,
                                              uint32_t timeout_ms,
                                              uint32_t LR) {
  mutex_lock_recursive(handle);
  return true;
}

//! Tests if a given mutex is owned by the current task. Useful for
//! ensuring locks are held when they should be.
bool mutex_is_owned_recursive(PebbleRecursiveMutex *handle) {
  return true;
}

void mutex_unlock_recursive(PebbleRecursiveMutex *handle) {
  FakePebbleMutex *mutex = (FakePebbleMutex *)handle;
  if (mutex->lock_count == 0) {
    s_assert_triggered = true;
    cl_assert_(s_asserts_disabled,
               "mutex_unlock_recursive called when lock count not greater than 0");
  }
  mutex->lock_count--;
}

//! @return true if the calling task owns the mutex
void mutex_assert_held_by_curr_task(PebbleMutex *handle, bool is_held) {
  return;
}

//! @return true if the calling task owns the mutex
void mutex_assert_recursive_held_by_curr_task(PebbleRecursiveMutex *handle, bool is_held) {
  return;
}

// PRIVATE: Only used for testing this module
void fake_mutex_set_should_assert(bool should) {
  s_asserts_disabled = !should;
}

bool fake_mutex_get_assert_triggered(void) {
  return s_assert_triggered;
}

bool fake_mutex_all_unlocked(void) {
  bool failed;
  s_asserts_disabled = true;
  list_foreach((ListNode *)s_mutex_list, prv_list_foreach_assert_unlocked, &failed);
  s_asserts_disabled = false;
  return !failed;
}
