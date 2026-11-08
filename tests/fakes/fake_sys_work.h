/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "fake_pebble_tasks.h"

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/work.h>
#include <pbl/task_wdt/task_wdt.h>
#include <pbl/util/list.h>

#include <clar_asserts.h>

typedef struct {
  ListNode node;
  pbl_work_call_fn_t callback;
  void *data;
} SysWorkCallbackNode;

static ListNode *s_sys_work_callback_head = nullptr;
static bool s_invoke_as_current = false;
static uint32_t sys_work_available_space = ~(uint32_t)0;

bool pbl_sys_work_call(pbl_work_call_fn_t cb, void *data) {
  SysWorkCallbackNode *node = (SysWorkCallbackNode *)malloc(sizeof(SysWorkCallbackNode));
  cl_assert(node != nullptr);
  list_init(&node->node);

  cl_assert(cb);
  node->callback = cb;
  node->data = data;

  s_sys_work_callback_head = list_prepend(s_sys_work_callback_head, &node->node);
  cl_assert(s_sys_work_callback_head);
  sys_work_available_space--;
  return true;
}

bool pbl_sys_work_call_from_isr(pbl_work_call_fn_t cb, void *data) {
  return pbl_sys_work_call(cb, data);
}

bool pbl_sys_work_try_call(pbl_work_call_fn_t cb, void *data) {
  return pbl_sys_work_call(cb, data);
}

size_t pbl_sys_work_num_free(void) {
  return sys_work_available_space;
}

void fake_sys_work_set_num_free(uint32_t space) {
  sys_work_available_space = space;
}

////////////////////////////////////
// Stub:
//
void stub_invoke_sys_work_as_current(void) {
  s_invoke_as_current = !s_invoke_as_current;
}

////////////////////////////////////
// Fake manipulation:
//
void *s_fake_sys_work_current_cb;

void fake_sys_work_callbacks_invoke(int num_to_invoke) {
  PebbleTask current_task = pebble_task_get_current();
  if (!s_invoke_as_current) {
    stub_pebble_tasks_set_current(PebbleTask_SysWorkq);
  }

  // Start at tail ("oldest" callback):
  SysWorkCallbackNode *node = (SysWorkCallbackNode *)list_get_tail(s_sys_work_callback_head);
  while (node && num_to_invoke) {
    // do callback first, in case callback enqueues more callbacks
    if (node->callback) {
      s_fake_sys_work_current_cb = node->callback;
      node->callback(node->data);
      s_fake_sys_work_current_cb = nullptr;
    }
    SysWorkCallbackNode *prev = (SysWorkCallbackNode *)list_get_prev(&node->node);
    list_remove(&node->node, &s_sys_work_callback_head, nullptr);
    free(node);
    node = prev;

    sys_work_available_space++;
    num_to_invoke--;
  }

  stub_pebble_tasks_set_current(current_task);
}

void fake_sys_work_callbacks_invoke_pending(void) {
  // sometimes the cb's may add new jobs so we need to keep looping until no
  // more are left
  while (s_sys_work_callback_head) {
    fake_sys_work_callbacks_invoke(list_count(s_sys_work_callback_head));
  }
}

void fake_sys_work_callbacks_cleanup(void) {
  SysWorkCallbackNode *node = (SysWorkCallbackNode *)s_sys_work_callback_head;
  while (node) {
    SysWorkCallbackNode *next = (SysWorkCallbackNode *)list_get_next(&node->node);
    list_remove(&node->node, &s_sys_work_callback_head, nullptr);
    free(node);
    node = next;
  }
  cl_assert(s_sys_work_callback_head == nullptr);
}

uint32_t fake_sys_work_count_callbacks(void) {
  return list_count(s_sys_work_callback_head);
}

void *pbl_sys_work_current_fn(void) {
  return s_fake_sys_work_current_cb;
}
