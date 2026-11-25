/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/can.h>

#include <clar.h>

#define MAX_SEMS 4

#define CLOCK 48000000U

static struct {
  const struct pbl_sem *sem;
  uint32_t count;
  uint32_t limit;
} s_sems[MAX_SEMS];

static struct pbl_can_timing s_nominal;
static struct pbl_can_timing s_data;
static bool s_has_data;
static uint32_t s_mode;
static uint16_t s_filters_set;
static int s_starts;
static int s_stops;
static int s_aborts;
static bool s_complete_now;
static pbl_can_tx_callback_t s_tx_cb;
static void *s_tx_arg;
static struct pbl_can_frame s_sent;
static int s_status;
static int s_rx_count;
static int s_states;
static int s_recovers;

static uint32_t *prv_count(struct pbl_sem *s) {
  for (int i = 0; i < MAX_SEMS; i++) {
    if (s_sems[i].sem == s) {
      return &s_sems[i].count;
    }
  }
  for (int i = 0; i < MAX_SEMS; i++) {
    if (s_sems[i].sem == nullptr) {
      s_sems[i].sem = s;
      s_sems[i].count = s->initial;
      s_sems[i].limit = s->limit;
      return &s_sems[i].count;
    }
  }
  cl_fail("too many semaphores");
  return nullptr;
}

void pbl_sem_init(struct pbl_sem *s, uint32_t initial, uint32_t limit) {
  s->initial = initial;
  s->limit = limit;
  for (int i = 0; i < MAX_SEMS; i++) {
    if (s_sems[i].sem == s) {
      s_sems[i].sem = nullptr;
    }
  }
  (void)prv_count(s);
}

void pbl_sem_deinit(struct pbl_sem *s) {
  for (int i = 0; i < MAX_SEMS; i++) {
    if (s_sems[i].sem == s) {
      s_sems[i].sem = nullptr;
    }
  }
}

int pbl_sem_take(struct pbl_sem *s, pbl_timeout_t timeout) {
  uint32_t *count = prv_count(s);

  if (*count == 0U) {
    return -EAGAIN;
  }
  (*count)--;
  return 0;
}

void pbl_sem_give(struct pbl_sem *s) {
  uint32_t *count = prv_count(s);

  if (*count < s->limit) {
    (*count)++;
  }
}

static int prv_init(const struct pbl_can *can) {
  return 0;
}

static int prv_get_core_clock(const struct pbl_can *can, uint32_t *rate) {
  *rate = CLOCK;
  return 0;
}

static int prv_configure(const struct pbl_can *can, const struct pbl_can_timing *nominal,
                         const struct pbl_can_timing *data, uint32_t mode) {
  s_nominal = *nominal;
  s_has_data = data != nullptr;
  if (data != nullptr) {
    s_data = *data;
  }
  s_mode = mode;
  return 0;
}

static int prv_set_filter(const struct pbl_can *can, uint8_t idx,
                          const struct pbl_can_filter *filter) {
  if (filter != nullptr) {
    s_filters_set |= 1U << idx;
  } else {
    s_filters_set &= ~(1U << idx);
  }
  return 0;
}

static int prv_start(const struct pbl_can *can) {
  s_starts++;
  return 0;
}

static void prv_complete(const struct pbl_can *can, int status) {
  pbl_can_tx_callback_t cb = s_tx_cb;

  cl_assert(cb != nullptr);
  s_tx_cb = nullptr;
  pbl_can_tx_done(can, cb, s_tx_arg, status);
}

static void prv_stop(const struct pbl_can *can) {
  s_stops++;
  if (s_tx_cb != nullptr) {
    prv_complete(can, -ENETDOWN);
  }
}

static int prv_send(const struct pbl_can *can, const struct pbl_can_frame *frame,
                    pbl_can_tx_callback_t cb, void *arg) {
  if (s_tx_cb != nullptr) {
    return -EBUSY;
  }
  s_sent = *frame;
  s_tx_cb = cb;
  s_tx_arg = arg;
  if (s_complete_now) {
    prv_complete(can, 0);
  }
  return 0;
}

static void prv_abort(const struct pbl_can *can) {
  s_aborts++;
  if (s_tx_cb != nullptr) {
    prv_complete(can, -ECANCELED);
  }
}

static int prv_get_state(const struct pbl_can *can, enum pbl_can_bus_state *state,
                         struct pbl_can_err_cnt *cnt) {
  *state = PBL_CAN_STATE_ERROR_PASSIVE;
  cnt->tx = 130U;
  cnt->rx = 2U;
  return 0;
}

static int prv_recover(const struct pbl_can *can) {
  s_recovers++;
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
  .recover = prv_recover,
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

static const struct pbl_can_timing_limits s_data_limits = {
  .tseg1_min = 2U,
  .tseg1_max = 16U,
  .tseg2_min = 2U,
  .tseg2_max = 8U,
  .sjw_max = 4U,
  .prescaler_min = 2U,
  .prescaler_max = 256U,
};

PBL_CAN_STATE_DEFINE(s_can, 1U);
static const struct pbl_can s_can =
    PBL_CAN_INIT(s_can, "can", nullptr, &s_ops, &s_limits, &s_data_limits,
                 PBL_CAN_MODE_LISTEN_ONLY | PBL_CAN_MODE_FD | PBL_CAN_MODE_FD_NON_ISO |
                     PBL_CAN_MODE_ONE_SHOT | PBL_CAN_MODE_MANUAL_RECOVERY,
                 2U);

static void prv_tx_cb(const struct pbl_can *can, int status, void *arg) {
  s_status = status;
}

static void prv_rx_cb(const struct pbl_can *can, const struct pbl_can_frame *frame, void *arg) {
  s_rx_count++;
}

static void prv_state_cb(const struct pbl_can *can, enum pbl_can_bus_state state,
                         struct pbl_can_err_cnt cnt, void *arg) {
  s_states++;
}

static void prv_start_classic(void) {
  const struct pbl_can_config cfg = {.bitrate = 500000U};

  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), 0);
  cl_assert_equal_i(pbl_can_start(&s_can), 0);
}

void test_can_class__initialize(void) {
  memset(s_sems, 0, sizeof(s_sems));
  s_filters_set = 0U;
  s_starts = 0;
  s_stops = 0;
  s_aborts = 0;
  s_complete_now = true;
  s_tx_cb = nullptr;
  s_status = 1;
  s_rx_count = 0;
  s_states = 0;
  s_recovers = 0;
  cl_assert_equal_i(pbl_device_init(&s_can.dev), 0);
}

void test_can_class__cleanup(void) {
  cl_assert_equal_i(pbl_can_stop(&s_can), 0);
  for (int i = 0; i < 2; i++) {
    (void)pbl_can_remove_filter(&s_can, i);
  }
  pbl_can_set_rx_callback(&s_can, nullptr, nullptr);
  pbl_can_set_state_callback(&s_can, nullptr, nullptr);
}

void test_can_class__timing(void) {
  struct pbl_can_timing t;

  // 500 kbit/s from 48 MHz: 2 * 48 time quanta, sampled at 87.5%
  cl_assert_equal_i(pbl_can_calc_timing(&s_limits, CLOCK, 500000U, 875U, &t), 0);
  cl_assert_equal_i(t.prescaler, 2);
  cl_assert_equal_i(t.tseg1, 41);
  cl_assert_equal_i(t.tseg2, 6);
  cl_assert_equal_i(t.sjw, 6);

  // 1 Mbit/s at 75%: 2 * 24 time quanta
  cl_assert_equal_i(pbl_can_calc_timing(&s_limits, CLOCK, 1000000U, 750U, &t), 0);
  cl_assert_equal_i(t.prescaler, 2);
  cl_assert_equal_i(t.tseg1, 17);
  cl_assert_equal_i(t.tseg2, 6);

  // 1 Mbit/s at 50%: the synchronization jump width fits phase segment 1 too
  cl_assert_equal_i(pbl_can_calc_timing(&s_limits, CLOCK, 1000000U, 500U, &t), 0);
  cl_assert_equal_i(t.tseg1, 11);
  cl_assert_equal_i(t.tseg2, 12);
  cl_assert_equal_i(t.sjw, 11);

  // 100 kbit/s: phase segment 1 at its longest moves the sample point earlier
  cl_assert_equal_i(pbl_can_calc_timing(&s_limits, CLOCK, 100000U, 875U, &t), 0);
  cl_assert_equal_i(t.prescaler * (1U + t.tseg1 + t.tseg2), 480);
  cl_assert(t.tseg1 <= 64U && t.tseg2 <= 32U);

  // 2 Mbit/s data phase: 2 * 12 time quanta
  cl_assert_equal_i(pbl_can_calc_timing(&s_data_limits, CLOCK, 2000000U, 750U, &t), 0);
  cl_assert_equal_i(t.prescaler, 2);
  cl_assert_equal_i(t.tseg1, 8);
  cl_assert_equal_i(t.tseg2, 3);
  cl_assert_equal_i(t.sjw, 3);

  // Not divided exactly, or too fast for the shortest bit
  cl_assert_equal_i(pbl_can_calc_timing(&s_data_limits, CLOCK, 5000000U, 750U, &t), -EINVAL);
  cl_assert_equal_i(pbl_can_calc_timing(&s_data_limits, CLOCK, 8000000U, 750U, &t), -EINVAL);
  cl_assert_equal_i(pbl_can_calc_timing(&s_limits, CLOCK, 0U, 875U, &t), -EINVAL);
}

void test_can_class__dlc(void) {
  cl_assert_equal_i(pbl_can_len_to_dlc(8U), 8);
  cl_assert_equal_i(pbl_can_len_to_dlc(12U), 9);
  cl_assert_equal_i(pbl_can_len_to_dlc(64U), 15);
  cl_assert_equal_i(pbl_can_dlc_to_len(13U), 32);
  cl_assert_equal_i(pbl_can_dlc_to_len(15U), 64);
}

void test_can_class__configure(void) {
  struct pbl_can_config cfg = {.bitrate = 500000U, .data_bitrate = 2000000U};
  struct pbl_can_config got;

  cl_assert_equal_i(pbl_can_start(&s_can), -EINVAL);

  // The default sample point of the nominal bit rate is CiA 301's
  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), 0);
  cl_assert(!s_has_data);
  cl_assert_equal_i(s_nominal.tseg1, 41);
  cl_assert_equal_i(pbl_can_config_get(&s_can, &got), 0);
  cl_assert_equal_i(got.data_bitrate, 0);

  cfg.mode = PBL_CAN_MODE_FD;
  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), 0);
  cl_assert(s_has_data);
  cl_assert_equal_i(s_data.prescaler, 2);
  cl_assert_equal_i(s_mode, PBL_CAN_MODE_FD);

  cfg.mode = PBL_CAN_MODE_LOOPBACK;
  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), -ENOTSUP);

  cfg.mode = PBL_CAN_MODE_FD_NON_ISO;
  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), -EINVAL);

  cfg.mode = 0U;
  cfg.bitrate = 333333U;
  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), -EINVAL);

  prv_start_classic();
  cfg.bitrate = 250000U;
  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), -EBUSY);
}

void test_can_class__filters(void) {
  struct pbl_can_filter filter = {.id = 0x123, .mask = 0x7ff};

  cl_assert_equal_i(pbl_can_add_filter(&s_can, &filter), 0);
  filter.flags = PBL_CAN_FRAME_IDE;
  filter.id = 0x1abcdef0;
  filter.mask = 0x1fffffff;
  cl_assert_equal_i(pbl_can_add_filter(&s_can, &filter), 1);
  cl_assert_equal_i(pbl_can_add_filter(&s_can, &filter), -ENOSPC);
  cl_assert_equal_i(s_filters_set, 0x3);

  cl_assert_equal_i(pbl_can_remove_filter(&s_can, 0), 0);
  cl_assert_equal_i(pbl_can_remove_filter(&s_can, 0), -EINVAL);
  cl_assert_equal_i(s_filters_set, 0x2);

  filter.flags = 0U;
  cl_assert_equal_i(pbl_can_add_filter(&s_can, &filter), -EINVAL);

  prv_start_classic();
  filter.id = 0x7ff;
  filter.mask = 0x7ff;
  cl_assert_equal_i(pbl_can_add_filter(&s_can, &filter), -EBUSY);
  cl_assert_equal_i(pbl_can_remove_filter(&s_can, 1), -EBUSY);
}

void test_can_class__send_validates(void) {
  struct pbl_can_frame frame = {.id = 0x123, .len = 2};

  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), -ENETDOWN);

  prv_start_classic();

  frame.id = 0x800;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), -EINVAL);
  frame.id = 0x123;
  frame.len = 9;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), -EINVAL);
  frame.len = 8;
  frame.flags = PBL_CAN_FRAME_FDF;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), -EINVAL);
  frame.flags = PBL_CAN_FRAME_BRS;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), -EINVAL);
  frame.flags = PBL_CAN_FRAME_IDE;
  frame.id = 0x1fffffff;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), 0);
  cl_assert_equal_i(s_status, 0);
}

void test_can_class__send_listen_only(void) {
  const struct pbl_can_config cfg = {.bitrate = 500000U, .mode = PBL_CAN_MODE_LISTEN_ONLY};
  struct pbl_can_frame frame = {.id = 0x123, .len = 2};

  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), 0);
  cl_assert_equal_i(pbl_can_start(&s_can), 0);
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), -EPERM);
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_MSEC(10), nullptr, nullptr), -EPERM);
}

void test_can_class__send_fd(void) {
  const struct pbl_can_config cfg = {
    .bitrate = 500000U,
    .data_bitrate = 2000000U,
    .mode = PBL_CAN_MODE_FD,
  };
  struct pbl_can_frame frame = {.id = 0x123, .flags = PBL_CAN_FRAME_FDF | PBL_CAN_FRAME_BRS};

  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), 0);
  cl_assert_equal_i(pbl_can_start(&s_can), 0);

  frame.len = 10;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, nullptr, nullptr), -EINVAL);
  frame.len = 64;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, nullptr, nullptr), 0);
  cl_assert_equal_i(s_sent.len, 64);
  frame.flags = PBL_CAN_FRAME_FDF | PBL_CAN_FRAME_RTR;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, nullptr, nullptr), -EINVAL);
}

void test_can_class__send_waits_for_room(void) {
  struct pbl_can_frame frame = {.id = 0x123, .len = 1};

  prv_start_classic();
  s_complete_now = false;

  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), 0);
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), -EAGAIN);

  prv_complete(&s_can, 0);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), 0);

  // Stopping completes the frame in flight and gives its room back
  cl_assert_equal_i(pbl_can_stop(&s_can), 0);
  cl_assert_equal_i(s_status, -ENETDOWN);
  cl_assert_equal_i(pbl_can_start(&s_can), 0);
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), 0);
}

void test_can_class__send_blocking(void) {
  struct pbl_can_frame frame = {.id = 0x123, .len = 1};

  prv_start_classic();

  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_MSEC(10), nullptr, nullptr), 0);
  cl_assert_equal_i(s_aborts, 0);

  // A frame that does not go in time is aborted
  s_complete_now = false;
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_MSEC(10), nullptr, nullptr), -ETIMEDOUT);
  cl_assert_equal_i(s_aborts, 1);
  cl_assert_equal_i(pbl_can_send(&s_can, &frame, PBL_NO_WAIT, prv_tx_cb, nullptr), 0);
}

void test_can_class__reports(void) {
  struct pbl_can_frame frame = {.id = 0x123};
  enum pbl_can_bus_state state;
  struct pbl_can_err_cnt cnt;

  cl_assert_equal_i(pbl_can_get_state(&s_can, &state, &cnt), 0);
  cl_assert_equal_i(state, PBL_CAN_STATE_STOPPED);
  cl_assert_equal_i(cnt.tx, 0);

  prv_start_classic();
  cl_assert_equal_i(pbl_can_get_state(&s_can, &state, &cnt), 0);
  cl_assert_equal_i(state, PBL_CAN_STATE_ERROR_PASSIVE);
  cl_assert_equal_i(cnt.tx, 130);

  pbl_can_receive(&s_can, &frame);
  pbl_can_set_rx_callback(&s_can, prv_rx_cb, nullptr);
  pbl_can_receive(&s_can, &frame);
  cl_assert_equal_i(s_rx_count, 1);

  pbl_can_set_state_callback(&s_can, prv_state_cb, nullptr);
  pbl_can_report_state(&s_can, PBL_CAN_STATE_BUS_OFF, cnt);
  cl_assert_equal_i(s_states, 1);
}

void test_can_class__recover(void) {
  const struct pbl_can_config cfg = {.bitrate = 500000U, .mode = PBL_CAN_MODE_MANUAL_RECOVERY};
  const struct pbl_can_err_cnt cnt = {.tx = 255U};

  cl_assert_equal_i(pbl_can_recover(&s_can), -ENETDOWN);

  prv_start_classic();
  cl_assert_equal_i(pbl_can_recover(&s_can), -ENOTSUP);
  cl_assert_equal_i(pbl_can_stop(&s_can), 0);

  cl_assert_equal_i(pbl_can_configure(&s_can, &cfg), 0);
  cl_assert_equal_i(s_mode, PBL_CAN_MODE_MANUAL_RECOVERY);
  cl_assert_equal_i(pbl_can_start(&s_can), 0);
  cl_assert_equal_i(pbl_can_recover(&s_can), 0);
  cl_assert_equal_i(s_recovers, 0);

  pbl_can_report_state(&s_can, PBL_CAN_STATE_BUS_OFF, cnt);
  cl_assert_equal_i(pbl_can_recover(&s_can), 0);
  cl_assert_equal_i(s_recovers, 1);
}

void test_can_class__stats(void) {
  const struct pbl_can_err_cnt cnt = {.tx = 255U};
  struct pbl_can_stats before, after;

  cl_assert_equal_i(pbl_can_get_stats(&s_can, &before), 0);

  prv_start_classic();
  pbl_can_report_error(&s_can, PBL_CAN_ERROR_ACK);
  pbl_can_report_error(&s_can, PBL_CAN_ERROR_ACK);
  pbl_can_report_error(&s_can, PBL_CAN_ERROR_ARBITRATION_LOST);
  pbl_can_report_error(&s_can, PBL_CAN_ERROR_COUNT);
  pbl_can_report_state(&s_can, PBL_CAN_STATE_BUS_OFF, cnt);
  pbl_can_report_state(&s_can, PBL_CAN_STATE_BUS_OFF, cnt);
  pbl_can_report_state(&s_can, PBL_CAN_STATE_ERROR_ACTIVE, cnt);
  pbl_can_report_state(&s_can, PBL_CAN_STATE_BUS_OFF, cnt);

  cl_assert_equal_i(pbl_can_get_stats(&s_can, &after), 0);
  cl_assert_equal_i(after.errors[PBL_CAN_ERROR_ACK] - before.errors[PBL_CAN_ERROR_ACK], 2);
  cl_assert_equal_i(
      after.errors[PBL_CAN_ERROR_ARBITRATION_LOST] - before.errors[PBL_CAN_ERROR_ARBITRATION_LOST],
      1);
  cl_assert_equal_i(after.errors[PBL_CAN_ERROR_BIT] - before.errors[PBL_CAN_ERROR_BIT], 0);
  cl_assert_equal_i(after.bus_off - before.bus_off, 2);
}
