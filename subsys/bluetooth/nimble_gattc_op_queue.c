/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "nimble_gattc_op_queue.h"

#include <comm/bt_lock.h>
#include <kernel/pbl_malloc.h>
#include <nimble/nimble_port.h>
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
//! Ops start on the host task, where the host also handles disconnections:
//! NimBLE sends a procedure's request before it tracks the procedure, so a
//! disconnection handled in between would leave it waiting for its timeout.
static struct ble_npl_event s_run_event;

static void prv_kick_locked(void) {
  if (s_op_running || s_kick_scheduled || (s_ops == NULL)) {
    return;
  }
  s_kick_scheduled = true;
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_run_event);
}

static void prv_pop_locked(void) {
  GattClientOp *op = (GattClientOp *)s_ops;
  list_remove(&op->node, &s_ops, NULL);
  kernel_free(op->ctx);
  kernel_free(op);
}

static void prv_run_cb(struct ble_npl_event *ev) {
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
  ble_npl_event_init(&s_run_event, prv_run_cb, NULL);

  bt_lock();
  while (s_ops != NULL) {
    prv_pop_locked();
  }
  s_op_running = false;
  s_kick_scheduled = false;
  bt_unlock();
}
