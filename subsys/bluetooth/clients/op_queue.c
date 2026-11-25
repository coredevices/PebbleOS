/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "op_queue.h"

#include <errno.h>

#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>

#include <nimble/nimble_port.h>

struct op {
  struct op *next;
  pbl_bt__gattc_op_start_fn_t start;
  void *ctx;
};

/* Guards what follows. The head is the running operation while s_running is set. */
static PBL_MUTEX_DEFINE(s_lock);
static struct op *s_head;
static struct op *s_tail;
static bool s_running;
static bool s_kicked;
static struct ble_npl_event s_run_event;
static bool s_run_event_initialized;

static void prv_run_cb(struct ble_npl_event *ev);

static void prv_kick_locked(void) {
  if (s_running || s_kicked || s_head == nullptr) {
    return;
  }

  if (!s_run_event_initialized) {
    ble_npl_event_init(&s_run_event, prv_run_cb, nullptr);
    s_run_event_initialized = true;
  }

  s_kicked = true;
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_run_event);
}

static void prv_pop_locked(void) {
  struct op *op = s_head;

  s_head = op->next;
  if (s_head == nullptr) {
    s_tail = nullptr;
  }

  pbl_free(op->ctx);
  pbl_free(op);
}

static void prv_run_cb(struct ble_npl_event *ev) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_kicked = false;
  while (!s_running && s_head != nullptr) {
    struct op *op = s_head;
    int rc;

    s_running = true;
    pbl_mutex_unlock(&s_lock);
    /* It may be done, and gone, by the time this returns 0. */
    rc = op->start(op->ctx);
    pbl_mutex_lock(&s_lock, PBL_FOREVER);
    if (rc == 0) {
      break;
    }

    s_running = false;
    prv_pop_locked();
  }
  pbl_mutex_unlock(&s_lock);
}

int pbl_bt__gattc_op_queue_push(pbl_bt__gattc_op_start_fn_t start, void *ctx) {
  struct op *op = pbl_malloc(sizeof(*op));

  if (op == nullptr) {
    return -ENOMEM;
  }

  *op = (struct op){.start = start, .ctx = ctx};

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_tail == nullptr) {
    s_head = op;
  } else {
    s_tail->next = op;
  }
  s_tail = op;
  prv_kick_locked();
  pbl_mutex_unlock(&s_lock);

  return 0;
}

void pbl_bt__gattc_op_queue_complete(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_running) {
    s_running = false;
    prv_pop_locked();
    prv_kick_locked();
  }
  pbl_mutex_unlock(&s_lock);
}

void pbl_bt__gattc_op_queue_init(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  while (s_head != nullptr) {
    prv_pop_locked();
  }
  s_running = false;
  s_kicked = false;
  s_run_event_initialized = false;
  pbl_mutex_unlock(&s_lock);
}
