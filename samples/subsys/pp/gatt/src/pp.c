/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sample.h"

#include <stdio.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/main.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/pp/transports/gatt.h>
#include <pbl/util/byteorder.h>

#define MAIN_STACK_SIZE 4096
#define MAIN_CALLS      32

enum : uint8_t {
  CMD_PING = 0,
  CMD_PONG = 1,
};

struct PBL_PACKED ping_msg {
  uint8_t cmd;
  uint32_t cookie;
};

/* The main thread of the protocol: a work queue of the sample. */
static PBL_WORKQ_DEFINE(s_main_q);
static PBL_WORK_POOL_DEFINE(s_main_pool, MAIN_CALLS);
static struct pbl_thread s_main_thread;
PBL_THREAD_STACK_DEFINE(s_main_stack, MAIN_STACK_SIZE);

void pbl_pp_main_call(void (*fn)(void *data), void *data) {
  /* A call from the main thread itself does not wait for a slot, and none may be lost. */
  const int rc = pbl_work_call(&s_main_q, &s_main_pool, fn, data, PBL_FOREVER);

  PBL_ASSERTN(rc == 0);
}

bool pbl_pp_main_is_current(void) {
  return pbl_workq_is_current(&s_main_q);
}

const char *pbl_pp_gatt_cb_get_serial_number(void) {
  return "SAMPLE000001";
}

/* The sample leaves the connection as the phone set it. */
void pbl_pp_gatt_cb_set_responsiveness(uint16_t conn_handle, struct pbl_bt_responsiveness_req *req,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs) {
}

static void prv_session_cb(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                           void *user_data) {
  switch (event) {
    case PBL_PP_SESSION_OPENED:
      printf("pp_gatt: session opened\n");
      break;
    case PBL_PP_SESSION_CLOSED:
      printf("pp_gatt: session closed\n");
      break;
    case PBL_PP_SESSION_CAPABILITIES:
      printf("pp_gatt: capabilities 0x%08x\n",
             (unsigned int)pbl_pp_session_get_capabilities(session));
      break;
    case PBL_PP_SESSION_REMOTE_INFO:
      printf("pp_gatt: phone app runs on OS %u\n", pbl_pp_session_get_remote_os(session));
      break;
  }
}

PBL_PP_SESSION_LISTENER_DEFINE(sample, prv_session_cb, nullptr);

/* Runs on the system work queue, where sending may wait for room. */
static void prv_ping_handler(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  const struct ping_msg *ping = (const struct ping_msg *)data;
  struct ping_msg pong;
  int rc;

  if (length < sizeof(*ping) || ping->cmd != CMD_PING) {
    return;
  }

  printf("pp_gatt: ping %u\n", (unsigned int)pbl_be32_to_cpu(ping->cookie));

  pong = (struct ping_msg){.cmd = CMD_PONG, .cookie = ping->cookie};
  rc = pbl_pp_send(session, PBL_PP_ENDPOINT_PING, (const uint8_t *)&pong, sizeof(pong),
                   PBL_PP_DEFAULT_TIMEOUT_MS);
  if (rc != 0) {
    printf("pp_gatt: pong not sent (%d)\n", rc);
  }
}

PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_PING, prv_ping_handler);

static void prv_start_cb(void *data) {
  pbl_pp_gatt_destroy();
  pbl_pp_gatt_create();
  pbl_pp_gatt_discover((uint16_t)(uintptr_t)data);
}

static void prv_stop_cb(void *data) {
  pbl_pp_gatt_destroy();
}

void sample_pp_init(void) {
  const struct pbl_thread_attr attr = {
    .name = "pp",
    .entry = pbl_workq_thread,
    .arg = &s_main_q,
    .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO,
    .privileged = true,
    .stack = s_main_stack,
    .stack_size = sizeof(s_main_stack),
  };

  pbl_thread_create(&s_main_thread, &attr);
}

void sample_pp_start(uint16_t conn_handle) {
  pbl_pp_main_call(prv_start_cb, (void *)(uintptr_t)conn_handle);
}

void sample_pp_stop(void) {
  pbl_pp_main_call(prv_stop_cb, nullptr);
}
