/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_test.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pbl/drivers/can.h>
#include <pbl/kernel/kernel.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>

#include <clar_asserts.h>
#include <kernel_test.h>

// A node on a fake controller that completes every frame at once, driven by
// a test thread below the node's priority: time only moves while both wait.

void pbl_canopen_reset_for_test(void);

#define MAX_SENT 64

#define STACK 16384

struct pbl_canopen ct_co;
int ct_starts;
int ct_stops;

static struct pbl_can_frame s_sent[MAX_SENT];
static int s_num_sent;
static const struct pbl_canopen_od *s_od;
static const struct pbl_canopen_callbacks *s_cb;
static void (*s_attach)(struct pbl_canopen *co);
static void (*s_scenario)(void);
static uint8_t s_stack[STACK] __attribute__((aligned(8)));
static struct pbl_thread s_thread;

// ---- stubs ------------------------------------------------------------------

void pbl_log_sync(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
                  ...) {
}

void pbl_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
             ...) {
}

// ---- fake controller --------------------------------------------------------

static int prv_init(const struct pbl_can *can) {
  return 0;
}

static int prv_get_core_clock(const struct pbl_can *can, uint32_t *rate) {
  *rate = 48000000U;
  return 0;
}

static int prv_configure(const struct pbl_can *can, const struct pbl_can_timing *nominal,
                         const struct pbl_can_timing *data, uint32_t mode) {
  return 0;
}

static int prv_set_filter(const struct pbl_can *can, uint8_t idx,
                          const struct pbl_can_filter *filter) {
  return 0;
}

static int prv_start(const struct pbl_can *can) {
  ct_starts++;
  return 0;
}

static void prv_stop(const struct pbl_can *can) {
  ct_stops++;
}

static int prv_send(const struct pbl_can *can, const struct pbl_can_frame *frame,
                    pbl_can_tx_callback_t cb, void *arg) {
  cl_assert(s_num_sent < MAX_SENT);
  s_sent[s_num_sent++] = *frame;
  pbl_can_tx_done(can, cb, arg, 0);
  return 0;
}

static void prv_abort(const struct pbl_can *can) {
}

static int prv_get_state(const struct pbl_can *can, enum pbl_can_bus_state *state,
                         struct pbl_can_err_cnt *cnt) {
  *state = PBL_CAN_STATE_ERROR_ACTIVE;
  return 0;
}

static const struct pbl_can_ops s_ops = {
  .init = prv_init,
  .get_core_clock = prv_get_core_clock,
  .configure = prv_configure,
  .set_filter = prv_set_filter,
  .start = prv_start,
  .stop = prv_stop,
  .send = prv_send,
  .abort = prv_abort,
  .get_state = prv_get_state,
};

static const struct pbl_can_timing_limits s_limits = {
  .tseg1_min = 3U,
  .tseg1_max = 64U,
  .tseg2_min = 2U,
  .tseg2_max = 32U,
  .sjw_max = 16U,
  .prescaler_min = 2U,
  .prescaler_max = 256U,
};

PBL_CAN_STATE_DEFINE(ct_can, 1U);
const struct pbl_can ct_can =
    PBL_CAN_INIT(ct_can, "can", nullptr, &s_ops, &s_limits, nullptr, 0U, 2U);

// ---- helpers ----------------------------------------------------------------

void ct_sleep(uint32_t ms) {
  pbl_thread_sleep(PBL_MSEC(ms));
}

void ct_rx(uint32_t id, int len, ...) {
  struct pbl_can_frame frame = {.id = id, .len = (uint8_t)len};
  va_list ap;

  va_start(ap, len);
  for (int i = 0; i < len; i++) {
    frame.data[i] = (uint8_t)va_arg(ap, int);
  }
  va_end(ap);

  pbl_test_isr_enter();
  pbl_can_receive(&ct_can, &frame);
  pbl_test_isr_exit();
  ct_sleep(1U);
}

void ct_clear(void) {
  s_num_sent = 0;
}

int ct_count(uint32_t id) {
  int n = 0;

  for (int i = 0; i < s_num_sent; i++) {
    n += s_sent[i].id == id;
  }

  return n;
}

// The last frame sent with @p id, which must exist.
const struct pbl_can_frame *ct_last(uint32_t id) {
  for (int i = s_num_sent - 1; i >= 0; i--) {
    if (s_sent[i].id == id) {
      return &s_sent[i];
    }
  }

  static char msg[48];

  for (int i = 0; i < s_num_sent; i++) {
    printf("sent %03x len %u\n", (unsigned int)s_sent[i].id, s_sent[i].len);
  }
  snprintf(msg, sizeof(msg), "frame %03x not sent", (unsigned int)id);
  cl_fail(msg);
  return nullptr;
}

void ct_assert_data(const struct pbl_can_frame *f, int len, ...) {
  va_list ap;

  cl_assert_equal_i(f->len, len);
  va_start(ap, len);
  for (int i = 0; i < len; i++) {
    cl_assert_equal_i(f->data[i], va_arg(ap, int));
  }
  va_end(ap);
}

uint32_t ct_get_le32(const uint8_t *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Send an SDO request and return the response.
const struct pbl_can_frame *ct_sdo(uint8_t b0, uint16_t index, uint8_t sub, uint32_t val) {
  ct_clear();
  ct_rx(0x600U + CT_NODE, 8U, b0, index & 0xff, index >> 8, sub, val & 0xff, (val >> 8) & 0xff,
        (val >> 16) & 0xff, val >> 24);
  cl_assert_equal_i(ct_count(0x580U + CT_NODE), 1);
  return ct_last(0x580U + CT_NODE);
}

void ct_sdo_segment(uint8_t b0, const uint8_t data[7]) {
  ct_clear();
  ct_rx(0x600U + CT_NODE, 8U, b0, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
}

uint32_t ct_sdo_read(uint16_t index, uint8_t sub) {
  const struct pbl_can_frame *f = ct_sdo(0x40U, index, sub, 0U);

  cl_assert_equal_i(f->data[0] & 0xe3U, 0x43U);
  return ct_get_le32(&f->data[4]);
}

void ct_sdo_write(uint16_t index, uint8_t sub, uint32_t val, uint8_t len) {
  const struct pbl_can_frame *f = ct_sdo((uint8_t)(0x23U | ((4U - len) << 2)), index, sub, val);

  cl_assert_equal_i(f->data[0], 0x60U);
}

uint32_t ct_sdo_abort(uint8_t b0, uint16_t index, uint8_t sub, uint32_t val) {
  const struct pbl_can_frame *f = ct_sdo(b0, index, sub, val);

  cl_assert_equal_i(f->data[0], 0x80U);
  return ct_get_le32(&f->data[4]);
}

void ct_nmt(uint8_t cs) {
  ct_rx(0x000U, 2U, cs, CT_NODE);
}

static void prv_entry(void *arg) {
  const struct pbl_canopen_config cfg = {
    .can = &ct_can,
    .od = s_od,
    .node_id = CT_NODE,
    .bitrate = 500000U,
    .cb = s_cb,
  };

  cl_assert_equal_i(pbl_device_init(&ct_can.dev), 0);
  cl_assert_equal_i(pbl_canopen_init(&ct_co, &cfg), 0);
  if (s_attach != nullptr) {
    s_attach(&ct_co);
  }
  cl_assert_equal_i(pbl_canopen_start(&ct_co), 0);
  ct_sleep(1U);
  s_scenario();
  cl_assert_equal_i(pbl_canopen_stop(&ct_co), 0);
  pbl_test_kernel_stop();
}

void ct_run(const struct pbl_canopen_od *od, const struct pbl_canopen_callbacks *cb,
            void (*attach)(struct pbl_canopen *co), void (*scenario)(void)) {
  const struct pbl_thread_attr attr = {
    .name = "test",
    .entry = prv_entry,
    .prio = 2,
    .privileged = true,
    .stack = s_stack,
    .stack_size = STACK,
  };

  s_od = od;
  s_cb = cb;
  s_attach = attach;
  s_scenario = scenario;
  cl_assert_equal_i(pbl_thread_create(&s_thread, &attr), 0);
  pbl_test_kernel_run();
}

void ct_init(void) {
  memset(&s_thread, 0, sizeof(s_thread));
  pbl_canopen_reset_for_test();
  // The cycle counter of the POSIX arch counts nanoseconds.
  pbl_cycles_per_sec_set(1000000000U);
  s_num_sent = 0;
  ct_starts = 0;
  ct_stops = 0;
}
