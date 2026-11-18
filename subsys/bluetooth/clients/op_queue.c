/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "op_queue.h"

#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/list.h>

#include <nimble/nimble_port.h>

typedef struct {
  ListNode node;
  NimbleGattClientOpStartFn start;
  void *ctx;
} GattClientOp;

//! Guards all state. The head of s_ops is the running op when s_op_running is set.
static PBL_MUTEX_DEFINE(s_lock);
static ListNode *s_ops;
static bool s_op_running;
static bool s_kick_scheduled;
//! Ops start on the host task, where the host also handles disconnections:
//! NimBLE sends a procedure's request before it tracks the procedure, so a
//! disconnection handled in between would leave it waiting for its timeout.
static struct ble_npl_event s_run_event;
static bool s_run_event_initialized;

static void prv_run_cb(struct ble_npl_event *ev);

static void prv_kick_locked(void) {
  if (s_op_running || s_kick_scheduled || (s_ops == nullptr)) {
    return;
  }
  if (!s_run_event_initialized) {
    ble_npl_event_init(&s_run_event, prv_run_cb, nullptr);
    s_run_event_initialized = true;
  }
  s_kick_scheduled = true;
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_run_event);
}

static void prv_pop_locked(void) {
  GattClientOp *op = (GattClientOp *)s_ops;
  list_remove(&op->node, &s_ops, nullptr);
  pbl_free(op->ctx);
  pbl_free(op);
}

static void prv_run_cb(struct ble_npl_event *ev) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_kick_scheduled = false;
  while (!s_op_running && (s_ops != nullptr)) {
    GattClientOp *op = (GattClientOp *)s_ops;
    s_op_running = true;
    pbl_mutex_unlock(&s_lock);
    // The op may complete (even fail) before start returns, so op must not be
    // touched after a successful start.
    const int rc = op->start(op->ctx);
    pbl_mutex_lock(&s_lock, PBL_FOREVER);
    if (rc == 0) {
      break;
    }
    // Failed to start: drop it and try the next one
    s_op_running = false;
    prv_pop_locked();
  }
  pbl_mutex_unlock(&s_lock);
}

void nimble_gattc_op_queue_push(NimbleGattClientOpStartFn start, void *ctx) {
  GattClientOp *op = pbl_zalloc_check(sizeof(*op));
  list_init(&op->node);
  op->start = start;
  op->ctx = ctx;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_ops == nullptr) {
    s_ops = &op->node;
  } else {
    list_append(s_ops, &op->node);
  }
  prv_kick_locked();
  pbl_mutex_unlock(&s_lock);
}

void nimble_gattc_op_queue_complete(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_op_running) {
    s_op_running = false;
    prv_pop_locked();
    prv_kick_locked();
  }
  pbl_mutex_unlock(&s_lock);
}

void nimble_gattc_op_queue_init(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  while (s_ops != nullptr) {
    prv_pop_locked();
  }
  s_op_running = false;
  s_kick_scheduled = false;
  s_run_event_initialized = false;
  pbl_mutex_unlock(&s_lock);
}
