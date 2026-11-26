/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "od.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pbl/canopen/canopen.h>
#include <pbl/drivers/can.h>
#include <pbl/kernel/kernel.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>

#include <clar.h>
#include <kernel_test.h>

// A node on a fake controller that completes every frame at once, driven by
// a test thread below the node's priority: time only moves while both wait.

void pbl_canopen_reset_for_test(void);

#define NODE 5U

#define MAX_SENT 64

static struct pbl_can_frame s_sent[MAX_SENT];
static int s_num_sent;
static int s_starts;
static int s_stops;

static struct pbl_canopen s_co;
static void (*s_scenario)(void);

static int s_nmt_calls;
static enum pbl_canopen_nmt_state s_nmt_state;
static int s_reset_calls;
static enum pbl_canopen_reset s_reset;
static int s_writes;
static uint16_t s_write_index;
static uint8_t s_write_sub;
static int s_write_ret;
static int s_syncs;
static uint8_t s_sync_counter;
static int s_hb_calls;
static uint8_t s_hb_node;
static enum pbl_canopen_hb_event s_hb_event;
static enum pbl_canopen_nmt_state s_hb_state;

#define STACK 16384
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
  s_starts++;
  return 0;
}

static void prv_stop(const struct pbl_can *can) {
  s_stops++;
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

PBL_CAN_STATE_DEFINE(s_can, 1U);
static const struct pbl_can s_can =
    PBL_CAN_INIT(s_can, "can", nullptr, &s_ops, &s_limits, nullptr, 0U, 2U);

// ---- callbacks --------------------------------------------------------------

static void prv_nmt_cb(struct pbl_canopen *co, enum pbl_canopen_nmt_state state, void *arg) {
  s_nmt_calls++;
  s_nmt_state = state;
}

static void prv_reset_cb(struct pbl_canopen *co, enum pbl_canopen_reset reset, void *arg) {
  s_reset_calls++;
  s_reset = reset;
}

static int prv_write_cb(struct pbl_canopen *co, uint16_t index, uint8_t subindex, const void *data,
                        size_t len, void *arg) {
  s_writes++;
  s_write_index = index;
  s_write_sub = subindex;
  return s_write_ret;
}

static void prv_sync_cb(struct pbl_canopen *co, uint8_t counter, uint32_t cycles, void *arg) {
  uint32_t value = 0x01020304U + (uint32_t)s_syncs;

  // Stamped in the interrupt, before the callback
  cl_assert(pbl_cycle_get_32() - cycles < 0x80000000U);
  s_syncs++;
  s_sync_counter = counter;
  // Called with the node locked: the API must still work.
  cl_assert_equal_i(pbl_canopen_od_write(co, OD_VALUE, 0U, &value, sizeof(value)), 0);
}

static void prv_hb_cb(struct pbl_canopen *co, uint8_t node_id, enum pbl_canopen_hb_event event,
                      enum pbl_canopen_nmt_state state, void *arg) {
  s_hb_calls++;
  s_hb_node = node_id;
  s_hb_event = event;
  s_hb_state = state;
}

static const struct pbl_canopen_callbacks s_cb = {
  .nmt = prv_nmt_cb,
  .reset = prv_reset_cb,
  .write = prv_write_cb,
  .sync = prv_sync_cb,
  .heartbeat = prv_hb_cb,
};

// ---- helpers ----------------------------------------------------------------

static void prv_sleep(uint32_t ms) {
  pbl_thread_sleep(PBL_MSEC(ms));
}

static void prv_rx(uint32_t id, int len, ...) {
  struct pbl_can_frame frame = {.id = id, .len = (uint8_t)len};
  va_list ap;

  va_start(ap, len);
  for (int i = 0; i < len; i++) {
    frame.data[i] = (uint8_t)va_arg(ap, int);
  }
  va_end(ap);

  pbl_test_isr_enter();
  pbl_can_receive(&s_can, &frame);
  pbl_test_isr_exit();
  prv_sleep(1U);
}

static void prv_clear(void) {
  s_num_sent = 0;
}

static int prv_count(uint32_t id) {
  int n = 0;

  for (int i = 0; i < s_num_sent; i++) {
    n += s_sent[i].id == id;
  }

  return n;
}

// The last frame sent with @p id, which must exist.
static const struct pbl_can_frame *prv_last(uint32_t id) {
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

static void prv_assert_data(const struct pbl_can_frame *f, int len, ...) {
  va_list ap;

  cl_assert_equal_i(f->len, len);
  va_start(ap, len);
  for (int i = 0; i < len; i++) {
    cl_assert_equal_i(f->data[i], va_arg(ap, int));
  }
  va_end(ap);
}

static uint32_t prv_get_le32(const uint8_t *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Send an SDO request and return the response.
static const struct pbl_can_frame *prv_sdo(uint8_t b0, uint16_t index, uint8_t sub, uint32_t val) {
  prv_clear();
  prv_rx(0x600U + NODE, 8U, b0, index & 0xff, index >> 8, sub, val & 0xff, (val >> 8) & 0xff,
         (val >> 16) & 0xff, val >> 24);
  cl_assert_equal_i(prv_count(0x580U + NODE), 1);
  return prv_last(0x580U + NODE);
}

static void prv_sdo_segment(uint8_t b0, const uint8_t data[7]) {
  prv_clear();
  prv_rx(0x600U + NODE, 8U, b0, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
}

static uint32_t prv_sdo_read(uint16_t index, uint8_t sub) {
  const struct pbl_can_frame *f = prv_sdo(0x40U, index, sub, 0U);

  cl_assert_equal_i(f->data[0] & 0xe3U, 0x43U);
  return prv_get_le32(&f->data[4]);
}

static void prv_sdo_write(uint16_t index, uint8_t sub, uint32_t val, uint8_t len) {
  const struct pbl_can_frame *f = prv_sdo((uint8_t)(0x23U | ((4U - len) << 2)), index, sub, val);

  cl_assert_equal_i(f->data[0], 0x60U);
}

static uint32_t prv_sdo_abort(uint8_t b0, uint16_t index, uint8_t sub, uint32_t val) {
  const struct pbl_can_frame *f = prv_sdo(b0, index, sub, val);

  cl_assert_equal_i(f->data[0], 0x80U);
  return prv_get_le32(&f->data[4]);
}

static void prv_nmt(uint8_t cs) {
  prv_rx(0x000U, 2U, cs, NODE);
}

static void prv_entry(void *arg) {
  const struct pbl_canopen_config cfg = {
    .can = &s_can,
    .od = &od,
    .node_id = NODE,
    .bitrate = 500000U,
    .cb = &s_cb,
  };

  cl_assert_equal_i(pbl_device_init(&s_can.dev), 0);
  cl_assert_equal_i(pbl_canopen_init(&s_co, &cfg), 0);
  cl_assert_equal_i(pbl_canopen_start(&s_co), 0);
  prv_sleep(1U);
  s_scenario();
  cl_assert_equal_i(pbl_canopen_stop(&s_co), 0);
  pbl_test_kernel_stop();
}

static void prv_run(void (*scenario)(void)) {
  const struct pbl_thread_attr attr = {
    .name = "test",
    .entry = prv_entry,
    .prio = 2,
    .privileged = true,
    .stack = s_stack,
    .stack_size = STACK,
  };

  s_scenario = scenario;
  cl_assert_equal_i(pbl_thread_create(&s_thread, &attr), 0);
  pbl_test_kernel_run();
}

void test_canopen__initialize(void) {
  memset(&s_thread, 0, sizeof(s_thread));
  pbl_canopen_reset_for_test();
  s_num_sent = 0;
  s_starts = 0;
  s_stops = 0;
  s_nmt_calls = 0;
  s_reset_calls = 0;
  s_writes = 0;
  s_write_ret = 0;
  s_syncs = 0;
  s_hb_calls = 0;
}

void test_canopen__cleanup(void) {
}

// ---- tests ------------------------------------------------------------------

static void prv_boot(void) {
  cl_assert_equal_i(s_starts, 1);
  cl_assert_equal_i(prv_count(0x700U + NODE), 1);
  prv_assert_data(prv_last(0x700U + NODE), 1U, 0x00);
  cl_assert_equal_i(pbl_canopen_nmt_state(&s_co), PBL_CANOPEN_NMT_PRE_OPERATIONAL);
  cl_assert_equal_i(s_nmt_state, PBL_CANOPEN_NMT_PRE_OPERATIONAL);
  cl_assert_equal_i(s_reset_calls, 1);
  cl_assert_equal_i(s_reset, PBL_CANOPEN_RESET_NODE);

  // Heartbeat every 100 ms
  prv_clear();
  prv_sleep(350U);
  cl_assert_equal_i(prv_count(0x700U + NODE), 3);
  prv_assert_data(prv_last(0x700U + NODE), 1U, 0x7f);
  // No PDOs while pre-operational
  cl_assert_equal_i(prv_count(0x180U + NODE), 0);
}

void test_canopen__boot_and_heartbeat(void) {
  prv_run(prv_boot);
  cl_assert_equal_i(s_stops, 1);
}

static void prv_nmt_states(void) {
  prv_clear();
  prv_nmt(0x01);
  cl_assert_equal_i(pbl_canopen_nmt_state(&s_co), PBL_CANOPEN_NMT_OPERATIONAL);
  // The event-driven TPDO goes once on entering operational.
  prv_assert_data(prv_last(0x180U + NODE), 5U, 0x11, 0xdd, 0xcc, 0xbb, 0xaa);

  // Commands for other nodes are ignored, broadcast ones are not.
  prv_rx(0x000U, 2U, 0x02, NODE + 1U);
  cl_assert_equal_i(pbl_canopen_nmt_state(&s_co), PBL_CANOPEN_NMT_OPERATIONAL);
  prv_rx(0x000U, 2U, 0x02, 0);
  cl_assert_equal_i(pbl_canopen_nmt_state(&s_co), PBL_CANOPEN_NMT_STOPPED);

  // No SDO while stopped
  prv_clear();
  prv_rx(0x600U + NODE, 8U, 0x40, 0x00, 0x10, 0, 0, 0, 0, 0);
  cl_assert_equal_i(prv_count(0x580U + NODE), 0);
  prv_sleep(100U);
  prv_assert_data(prv_last(0x700U + NODE), 1U, 0x04);

  prv_nmt(0x80);
  cl_assert_equal_i(pbl_canopen_nmt_state(&s_co), PBL_CANOPEN_NMT_PRE_OPERATIONAL);

  // Reset communication restores the communication defaults only.
  prv_sdo_write(0x1017, 0, 500U, 2U);
  prv_sdo_write(OD_OUTPUT, 0, 0x55U, 1U);
  prv_clear();
  prv_nmt(0x82);
  cl_assert_equal_i(s_reset, PBL_CANOPEN_RESET_COMM);
  prv_assert_data(prv_last(0x700U + NODE), 1U, 0x00);
  cl_assert_equal_i(prv_sdo_read(0x1017, 0), 100U);
  cl_assert_equal_i(prv_sdo_read(OD_OUTPUT, 0), 0x55U);

  // Reset node restores the application ones too.
  prv_nmt(0x81);
  cl_assert_equal_i(s_reset, PBL_CANOPEN_RESET_NODE);
  cl_assert_equal_i(prv_sdo_read(OD_OUTPUT, 0), 0U);
  cl_assert_equal_i(pbl_canopen_nmt_state(&s_co), PBL_CANOPEN_NMT_PRE_OPERATIONAL);
}

void test_canopen__nmt(void) {
  prv_run(prv_nmt_states);
}

static void prv_sdo_expedited(void) {
  uint8_t output;

  cl_assert_equal_i(prv_sdo_read(0x1000, 0), 0x00020192U);
  cl_assert_equal_i(prv_sdo_read(0x1018, 0), 4U);
  cl_assert_equal_i(prv_sdo_read(0x1018, 1), 0x12345678U);
  cl_assert_equal_i(prv_sdo_read(0x1018, 2), 0x42U);
  cl_assert_equal_i(prv_sdo_read(0x1200, 1), 0x600U + NODE);
  cl_assert_equal_i(prv_sdo_read(0x1200, 2), 0x580U + NODE);
  cl_assert_equal_i(prv_sdo_read(0x1014, 0), 0x80U + NODE);
  cl_assert_equal_i(prv_sdo_read(0x1800, 1), 0x40000180U + NODE);
  cl_assert_equal_i(prv_sdo_read(0x1a00, 0), 2U);
  cl_assert_equal_i(prv_sdo_read(0x1a00, 1), 0x20000008U);
  cl_assert_equal_i(prv_sdo_read(0x1a00, 2), 0x20010020U);
  cl_assert_equal_i(prv_sdo_read(OD_CONFIG, OD_CONFIG_OFFSET), (uint32_t)-5);

  // Upload of a byte says its size.
  cl_assert_equal_i(prv_sdo(0x40U, OD_STATUS, 0, 0U)->data[0], 0x4fU);

  s_writes = 0;
  prv_sdo_write(OD_OUTPUT, 0, 0x7aU, 1U);
  cl_assert_equal_i(s_writes, 1);
  cl_assert_equal_i(s_write_index, OD_OUTPUT);
  cl_assert_equal_i(pbl_canopen_od_read(&s_co, OD_OUTPUT, 0, &output, sizeof(output)), 1);
  cl_assert_equal_i(output, 0x7aU);

  // Expedited without a size takes the entry's.
  cl_assert_equal_i(prv_sdo(0x22U, OD_CONFIG, OD_CONFIG_GAIN, 0x1234U)->data[0], 0x60U);
  cl_assert_equal_i(prv_sdo_read(OD_CONFIG, OD_CONFIG_GAIN), 0x1234U);

  cl_assert_equal_i(prv_sdo_abort(0x2fU, OD_STATUS, 0, 1U), 0x06010002U);
  cl_assert_equal_i(prv_sdo_abort(0x40U, OD_SECRET, 0, 0U), 0x06010001U);
  cl_assert_equal_i(prv_sdo_abort(0x40U, 0x3000, 0, 0U), 0x06020000U);
  cl_assert_equal_i(prv_sdo_abort(0x40U, OD_CONFIG, 9, 0U), 0x06090011U);
  cl_assert_equal_i(prv_sdo_abort(0x2bU, OD_OUTPUT, 0, 1U), 0x06070012U);
  cl_assert_equal_i(prv_sdo_abort(0x2fU, OD_VALUE, 0, 1U), 0x06070013U);
  cl_assert_equal_i(prv_sdo_abort(0xc0U, OD_VALUE, 0, 0U), 0x05040001U);

  // The application refuses a value.
  s_write_ret = -ERANGE;
  cl_assert_equal_i(prv_sdo_abort(0x2fU, OD_OUTPUT, 0, 1U), 0x06090030U);
  cl_assert_equal_i(prv_sdo_read(OD_OUTPUT, 0), 0x7aU);
}

void test_canopen__sdo_expedited(void) {
  prv_run(prv_sdo_expedited);
}

static void prv_sdo_segmented(void) {
  static const uint8_t seg1[7] = {'P', 'e', 'b', 'b', 'l', 'e', 'O'};
  static const uint8_t seg2[7] = {'S', '!', 0, 0, 0, 0, 0};
  const struct pbl_can_frame *f;
  char label[21] = {0};

  // Upload of a string longer than 4 bytes
  f = prv_sdo(0x40U, OD_LABEL, 0, 0U);
  cl_assert_equal_i(f->data[0], 0x41U);
  cl_assert_equal_i(prv_get_le32(&f->data[4]), 13U);
  prv_sdo_segment(0x60U, seg1);
  f = prv_last(0x580U + NODE);
  prv_assert_data(f, 8U, 0x00, 'h', 'e', 'l', 'l', 'o', ' ', 'c');
  prv_sdo_segment(0x70U, seg1);
  f = prv_last(0x580U + NODE);
  // 6 bytes left, 1 unused, last
  prv_assert_data(f, 8U, 0x13, 'a', 'n', 'o', 'p', 'e', 'n', 0);

  // Download of a shorter one, size given
  f = prv_sdo(0x21U, OD_LABEL, 0, 9U);
  cl_assert_equal_i(f->data[0], 0x60U);
  prv_sdo_segment(0x00U, seg1);
  prv_assert_data(prv_last(0x580U + NODE), 8U, 0x20, 0, 0, 0, 0, 0, 0, 0);
  // Toggle set, 2 bytes, 5 unused, last
  prv_sdo_segment(0x1bU, seg2);
  prv_assert_data(prv_last(0x580U + NODE), 8U, 0x30, 0, 0, 0, 0, 0, 0, 0);
  cl_assert_equal_i(pbl_canopen_od_read(&s_co, OD_LABEL, 0, label, sizeof(label)), 9);
  cl_assert_equal_s(label, "PebbleOS!");

  // A repeated toggle aborts.
  prv_sdo(0x21U, OD_LABEL, 0, 14U);
  prv_sdo_segment(0x00U, seg1);
  prv_sdo_segment(0x00U, seg1);
  f = prv_last(0x580U + NODE);
  cl_assert_equal_i(f->data[0], 0x80U);
  cl_assert_equal_i(prv_get_le32(&f->data[4]), 0x05030000U);

  // A size that does not match what came aborts, without a write.
  prv_sdo(0x21U, OD_LABEL, 0, 10U);
  prv_sdo_segment(0x0bU, seg2);
  f = prv_last(0x580U + NODE);
  cl_assert_equal_i(prv_get_le32(&f->data[4]), 0x06070010U);
  cl_assert_equal_i(pbl_canopen_od_read(&s_co, OD_LABEL, 0, label, sizeof(label)), 9);

  // Too long for the entry
  cl_assert_equal_i(prv_sdo_abort(0x21U, OD_LABEL, 0, 21U), 0x06070012U);

  // A transfer left hanging times out.
  prv_sdo(0x21U, OD_LABEL, 0, 9U);
  prv_clear();
  prv_sleep(1100U);
  f = prv_last(0x580U + NODE);
  cl_assert_equal_i(f->data[0], 0x80U);
  cl_assert_equal_i(prv_get_le32(&f->data[4]), 0x05040000U);
}

void test_canopen__sdo_segmented(void) {
  prv_run(prv_sdo_segmented);
}

static void prv_tpdo(void) {
  uint8_t status = 0x22U;
  uint32_t value = 0x12345678U;

  // Past the inhibit time of the TPDO sent on entering operational
  prv_nmt(0x01);
  prv_sleep(15U);
  prv_clear();

  // A change goes at once, then waits for the 10 ms inhibit time.
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, OD_STATUS, 0, &status, 1U), 0);
  prv_sleep(1U);
  cl_assert_equal_i(prv_count(0x180U + NODE), 1);
  prv_assert_data(prv_last(0x180U + NODE), 5U, 0x22, 0xdd, 0xcc, 0xbb, 0xaa);
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, OD_VALUE, 0, &value, 4U), 0);
  status = 0x33U;
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, OD_STATUS, 0, &status, 1U), 0);
  prv_sleep(1U);
  cl_assert_equal_i(prv_count(0x180U + NODE), 1);
  prv_sleep(10U);
  cl_assert_equal_i(prv_count(0x180U + NODE), 2);
  prv_assert_data(prv_last(0x180U + NODE), 5U, 0x33, 0x78, 0x56, 0x34, 0x12);

  // An unchanged value is no event.
  prv_sleep(20U);
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, OD_STATUS, 0, &status, 1U), 0);
  prv_sleep(1U);
  cl_assert_equal_i(prv_count(0x180U + NODE), 2);

  cl_assert_equal_i(pbl_canopen_tpdo_request(&s_co, 1U), 0);
  prv_sleep(1U);
  cl_assert_equal_i(prv_count(0x180U + NODE), 3);
  cl_assert_equal_i(pbl_canopen_tpdo_request(&s_co, 2U), -ENOTSUP);
  cl_assert_equal_i(pbl_canopen_tpdo_request(&s_co, 9U), -ENOENT);

  // TPDO 2 goes every second SYNC, sampled after the SYNC callback; TPDO 3
  // on the first SYNC after a change.
  prv_clear();
  prv_rx(0x080U, 0U);
  cl_assert_equal_i(s_syncs, 1);
  cl_assert_equal_i(prv_count(0x280U + NODE), 0);
  cl_assert_equal_i(prv_count(0x380U + NODE), 1);
  prv_assert_data(prv_last(0x380U + NODE), 1U, 0x33);
  status = 0x44U;
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, OD_STATUS, 0, &status, 1U), 0);
  prv_sleep(20U);
  cl_assert_equal_i(prv_count(0x380U + NODE), 1);
  prv_rx(0x080U, 0U);
  cl_assert_equal_i(s_syncs, 2);
  prv_assert_data(prv_last(0x280U + NODE), 4U, 0x05, 0x03, 0x02, 0x01);
  prv_assert_data(prv_last(0x380U + NODE), 1U, 0x44);
  prv_rx(0x080U, 1U, 7);
  cl_assert_equal_i(s_sync_counter, 7);
  prv_rx(0x080U, 0U);
  cl_assert_equal_i(s_sync_counter, 0);
  cl_assert_equal_i(prv_count(0x280U + NODE), 2);
  cl_assert_equal_i(prv_count(0x380U + NODE), 2);

  // Nothing once stopped
  prv_nmt(0x02);
  prv_clear();
  prv_rx(0x080U, 0U);
  cl_assert_equal_i(pbl_canopen_tpdo_request(&s_co, 1U), 0);
  prv_sleep(5U);
  cl_assert_equal_i(prv_count(0x180U + NODE) + prv_count(0x280U + NODE), 0);
}

void test_canopen__tpdo(void) {
  prv_run(prv_tpdo);
}

static void prv_rpdo(void) {
  const struct pbl_can_frame *f;
  int16_t setpoint;
  uint8_t output;

  // Ignored while pre-operational
  prv_rx(0x200U + NODE, 3U, 0x01, 0x02, 0x03);
  cl_assert_equal_i(prv_sdo_read(OD_OUTPUT, 0), 0U);

  prv_nmt(0x01);
  s_writes = 0;
  prv_rx(0x200U + NODE, 3U, 0x42, 0xfe, 0xff);
  cl_assert_equal_i(s_writes, 2);
  cl_assert_equal_i(pbl_canopen_od_read(&s_co, OD_OUTPUT, 0, &output, 1U), 1);
  cl_assert_equal_i(output, 0x42U);
  cl_assert_equal_i(pbl_canopen_od_read(&s_co, OD_SETPOINT, 0, &setpoint, 2U), 2);
  cl_assert_equal_i(setpoint, -2);

  // Too short: not processed, emergency 0x8210
  prv_clear();
  prv_rx(0x200U + NODE, 2U, 0x43, 0x00);
  f = prv_last(0x80U + NODE);
  prv_assert_data(f, 8U, 0x10, 0x82, 0x11, 0x01, 0x00, 0, 0, 0);
  cl_assert_equal_i(prv_sdo_read(OD_OUTPUT, 0), 0x42U);
  cl_assert_equal_i(prv_sdo_read(0x1001, 0), 0x11U);

  // The next good one clears it.
  prv_clear();
  prv_rx(0x200U + NODE, 3U, 0x44, 0x00, 0x00);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0);
  cl_assert_equal_i(prv_sdo_read(0x1001, 0), 0U);
  cl_assert_equal_i(prv_sdo_read(0x1003, 0), 1U);
  cl_assert_equal_i(prv_sdo_read(0x1003, 1), 0x8210U);

  // RPDO 2 is synchronous: applied on the next SYNC.
  prv_rx(0x300U + NODE, 2U, 0x10, 0x00);
  cl_assert_equal_i(prv_sdo_read(OD_SETPOINT, 0), 0U);
  prv_rx(0x080U, 0U);
  cl_assert_equal_i(prv_sdo_read(OD_SETPOINT, 0), 0x10U);

  // Its 30 ms deadline is missed: emergency 0x8250, cleared by the next one.
  prv_clear();
  prv_sleep(40U);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x50, 0x82, 0x11, 0x02, 0x00, 0, 0, 0);
  prv_clear();
  prv_rx(0x300U + NODE, 2U, 0x20, 0x00);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0);
  cl_assert_equal_i(prv_sdo_read(0x1003, 0), 2U);
  cl_assert_equal_i(prv_sdo_read(0x1003, 1), 0x8250U);
  cl_assert_equal_i(prv_sdo_read(0x1003, 2), 0x8210U);

  // Clearing the history
  cl_assert_equal_i(prv_sdo_abort(0x2fU, 0x1003, 0, 1U), 0x06090030U);
  prv_sdo_write(0x1003, 0, 0U, 1U);
  cl_assert_equal_i(prv_sdo_read(0x1003, 0), 0U);
}

void test_canopen__rpdo(void) {
  prv_run(prv_rpdo);
}

static void prv_remap(void) {
  // The mapping cannot change while the TPDO is valid.
  cl_assert_equal_i(prv_sdo_abort(0x2fU, 0x1a00, 0, 0U), 0x08000022U);
  // Nor can the COB-ID of a valid PDO.
  cl_assert_equal_i(prv_sdo_abort(0x23U, 0x1800, 1, 0x190U), 0x06090030U);

  prv_sdo_write(0x1800, 1, 0xc0000180U + NODE, 4U);
  // Entries need sub-index 0 cleared first.
  cl_assert_equal_i(prv_sdo_abort(0x23U, 0x1a00, 1, 0x20020008U), 0x08000022U);
  prv_sdo_write(0x1a00, 0, 0U, 1U);
  prv_sdo_write(0x1a00, 1, 0x20020008U, 4U);
  prv_sdo_write(0x1a00, 2, 0x20030010U, 4U);
  prv_sdo_write(0x1a00, 3, 0x20050020U, 4U);
  // Not mappable, missing, too long
  cl_assert_equal_i(prv_sdo_abort(0x2fU, 0x1a00, 0, 3U), 0x06040041U);
  prv_sdo_write(0x1a00, 3, 0x30000008U, 4U);
  cl_assert_equal_i(prv_sdo_abort(0x2fU, 0x1a00, 0, 3U), 0x06020000U);
  prv_sdo_write(0x1a00, 3, 0x20010020U, 4U);
  prv_sdo_write(0x1a00, 4, 0x20010020U, 4U);
  cl_assert_equal_i(prv_sdo_abort(0x2fU, 0x1a00, 0, 4U), 0x06040042U);
  prv_sdo_write(0x1a00, 0, 2U, 1U);
  // A restricted COB-ID is refused.
  cl_assert_equal_i(prv_sdo_abort(0x23U, 0x1800, 1, 0x705U), 0x06090030U);
  prv_sdo_write(0x1800, 1, 0x40000190U, 4U);
  cl_assert_equal_i(prv_sdo_abort(0x2fU, 0x1800, 2, 250U), 0x06090030U);

  prv_nmt(0x01);
  prv_clear();
  // Sent on entering operational, then again past the inhibit time.
  prv_rx(0x200U + NODE, 3U, 0x99, 0x34, 0x12);
  cl_assert_equal_i(prv_count(0x190U), 0);
  prv_sleep(15U);
  cl_assert_equal_i(prv_count(0x190U), 1);
  prv_assert_data(prv_last(0x190U), 3U, 0x99, 0x34, 0x12);
  cl_assert_equal_i(prv_count(0x180U + NODE), 0);

  // Back to the defaults
  prv_nmt(0x82);
  cl_assert_equal_i(prv_sdo_read(0x1800, 1), 0x40000180U + NODE);
  cl_assert_equal_i(prv_sdo_read(0x1a00, 1), 0x20000008U);
}

void test_canopen__remap(void) {
  prv_run(prv_remap);
}

static void prv_emcy(void) {
  static const uint8_t msef[5] = {1, 2, 3, 4, 5};

  prv_clear();
  cl_assert_equal_i(pbl_canopen_emcy_raise(&s_co, 0x5000U, 0x80U, msef), 0);
  cl_assert_equal_i(pbl_canopen_emcy_raise(&s_co, 0x5000U, 0x80U, msef), -EALREADY);
  prv_sleep(1U);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x00, 0x50, 0x81, 1, 2, 3, 4, 5);

  // Held back by the inhibit time
  prv_sdo_write(0x1015, 0, 1000U, 2U);
  prv_clear();
  cl_assert_equal_i(pbl_canopen_emcy_raise(&s_co, 0x4200U, 0x08U, nullptr), 0);
  cl_assert_equal_i(pbl_canopen_emcy_clear(&s_co, 0x5000U), 0);
  prv_sleep(1U);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x00, 0x42, 0x89, 0, 0, 0, 0, 0);
  cl_assert_equal_i(prv_count(0x80U + NODE), 1);
  prv_sleep(100U);
  cl_assert_equal_i(prv_count(0x80U + NODE), 2);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x00, 0x00, 0x09, 0, 0, 0, 0, 0);

  cl_assert_equal_i(pbl_canopen_emcy_clear(&s_co, 0x5000U), -ENOENT);
  cl_assert_equal_i(prv_sdo_read(0x1003, 0), 2U);
  cl_assert_equal_i(prv_sdo_read(0x1003, 1), 0x4200U);
  cl_assert_equal_i(prv_sdo_read(0x1003, 2), 0x5000U);

  // Disabled by its COB-ID
  prv_sdo_write(0x1014, 0, 0x80000085U, 4U);
  prv_clear();
  cl_assert_equal_i(pbl_canopen_emcy_clear(&s_co, 0x4200U), 0);
  prv_sleep(200U);
  cl_assert_equal_i(prv_count(0x80U + NODE), 0);
  cl_assert_equal_i(prv_sdo_read(0x1001, 0), 0U);
}

void test_canopen__emcy(void) {
  prv_run(prv_emcy);
}

static void prv_hb_consumer(void) {
  // Not monitored until the first heartbeat
  prv_sleep(100U);
  cl_assert_equal_i(s_hb_calls, 0);

  prv_rx(0x709U, 1U, 0x00);
  cl_assert_equal_i(s_hb_calls, 1);
  cl_assert_equal_i(s_hb_event, PBL_CANOPEN_HB_BOOT);
  cl_assert_equal_i(s_hb_node, 9U);
  prv_rx(0x709U, 1U, 0x7f);
  cl_assert_equal_i(s_hb_calls, 2);
  cl_assert_equal_i(s_hb_event, PBL_CANOPEN_HB_STATE);
  cl_assert_equal_i(s_hb_state, PBL_CANOPEN_NMT_PRE_OPERATIONAL);
  prv_sleep(30U);
  prv_rx(0x709U, 1U, 0x7f);
  cl_assert_equal_i(s_hb_calls, 2);

  prv_clear();
  prv_sleep(60U);
  cl_assert_equal_i(s_hb_calls, 3);
  cl_assert_equal_i(s_hb_event, PBL_CANOPEN_HB_TIMEOUT);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x30, 0x81, 0x11, 0x09, 0, 0, 0, 0);

  prv_clear();
  prv_rx(0x709U, 1U, 0x05);
  cl_assert_equal_i(s_hb_event, PBL_CANOPEN_HB_STATE);
  prv_assert_data(prv_last(0x80U + NODE), 8U, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0);

  // The same node twice is refused; another one takes over the second entry.
  cl_assert_equal_i(prv_sdo_abort(0x23U, 0x1016, 2, 0x00090064U), 0x06040043U);
  prv_sdo_write(0x1016, 2, 0x000a0064U, 4U);
  prv_sdo_write(0x1016, 1, 0U, 4U);
  prv_rx(0x70aU, 1U, 0x05);
  cl_assert_equal_i(s_hb_node, 10U);
  s_hb_calls = 0;
  prv_rx(0x709U, 1U, 0x7f);
  cl_assert_equal_i(s_hb_calls, 0);
}

void test_canopen__heartbeat_consumer(void) {
  prv_run(prv_hb_consumer);
}

static void prv_api(void) {
  uint8_t small[2];
  char label[4] = "abc";
  uint32_t value;

  cl_assert_equal_i(pbl_canopen_od_read(&s_co, 0x3000, 0, &value, 4U), -ENOENT);
  cl_assert_equal_i(pbl_canopen_od_read(&s_co, OD_VALUE, 0, small, sizeof(small)), -ENOSPC);
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, OD_VALUE, 0, small, sizeof(small)), -EINVAL);
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, OD_LABEL, 0, label, 3U), 0);
  cl_assert_equal_i(prv_sdo(0x40U, OD_LABEL, 0, 0U)->data[0], 0x47U);
  // Read-only over the bus, writable by the application
  value = 7U;
  cl_assert_equal_i(pbl_canopen_od_write(&s_co, 0x1018, 4, &value, 4U), 0);
  cl_assert_equal_i(prv_sdo_read(0x1018, 4), 7U);
  cl_assert_equal_i(pbl_canopen_start(&s_co), -EALREADY);
}

void test_canopen__api(void) {
  prv_run(prv_api);
}

void test_canopen__init_errors(void) {
  struct pbl_canopen_config cfg = {
    .can = &s_can,
    .od = &od,
    .node_id = 0U,
    .bitrate = 500000U,
  };

  cl_assert_equal_i(pbl_canopen_init(&s_co, &cfg), -EINVAL);
  cfg.node_id = 128U;
  cl_assert_equal_i(pbl_canopen_init(&s_co, &cfg), -EINVAL);
  cfg.node_id = 1U;
  cfg.od = nullptr;
  cl_assert_equal_i(pbl_canopen_init(&s_co, &cfg), -EINVAL);
}
