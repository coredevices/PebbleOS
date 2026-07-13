/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "nimble_gattc_op_queue.h"

#include <comm/bt_lock.h>
#include <kernel/pbl_malloc.h>
#include <pbl/services/system_task.h>
#include <pbl/util/list.h>

typedef struct {
  ListNode node;
  NimbleGattClientOpStartFn start;
  void *ctx;
} GattClientOp;

//! All state is guarded by bt_lock. The head of s_ops is the running op when
//! s_op_running is set.
static ListNode *s_ops;
static bool s_op_running;
static bool s_kick_scheduled;

static void prv_run_cb(void *unused);

static void prv_kick_locked(void) {
  if (s_op_running || s_kick_scheduled || (s_ops == NULL)) {
    return;
  }
  s_kick_scheduled = true;
  system_task_add_callback(prv_run_cb, NULL);
}

static void prv_pop_locked(void) {
  GattClientOp *op = (GattClientOp *)s_ops;
  list_remove(&op->node, &s_ops, NULL);
  kernel_free(op->ctx);
  kernel_free(op);
}

static void prv_run_cb(void *unused) {
  bt_lock();
  s_kick_scheduled = false;
  while (!s_op_running && (s_ops != NULL)) {
    GattClientOp *op = (GattClientOp *)s_ops;
    s_op_running = true;
    bt_unlock();
    // The op may complete (even fail) before start returns, so op must not be
    // touched after a successful start.
    const int rc = op->start(op->ctx);
    bt_lock();
    if (rc == 0) {
      break;
    }
    // Failed to start: drop it and try the next one
    s_op_running = false;
    prv_pop_locked();
  }
  bt_unlock();
}

void nimble_gattc_op_queue_push(NimbleGattClientOpStartFn start, void *ctx) {
  GattClientOp *op = kernel_zalloc_check(sizeof(*op));
  list_init(&op->node);
  op->start = start;
  op->ctx = ctx;

  bt_lock();
  if (s_ops == NULL) {
    s_ops = &op->node;
  } else {
    list_append(s_ops, &op->node);
  }
  prv_kick_locked();
  bt_unlock();
}

void nimble_gattc_op_queue_complete(void) {
  bt_lock();
  if (s_op_running) {
    s_op_running = false;
    prv_pop_locked();
    prv_kick_locked();
  }
  bt_unlock();
}

void nimble_gattc_op_queue_init(void) {
  bt_lock();
  while (s_ops != NULL) {
    prv_pop_locked();
  }
  s_op_running = false;
  bt_unlock();
}
