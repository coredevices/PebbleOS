/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/ppg.h>
#include <pbl/util/size.h>

#include <clar.h>

static const struct pbl_ppg_slot s_slots[] = {
  {.led_mask = 0x1, .leds = {PBL_PPG_LED_GREEN}, .rx_mask = 0x3},
  {.led_mask = 0x2, .leds = {[1] = PBL_PPG_LED_RED}, .rx_mask = 0x1},
  {.rx_mask = 0x1},
};

static const uint32_t s_gains_ohm[] = {10000U, 100000U};

static const struct pbl_ppg_info s_info = {
  .slots = s_slots,
  .num_slots = ARRAY_LENGTH(s_slots),
  .num_rx = 2U,
  .max_led_current_ua = 100000U,
  .gains_ohm = s_gains_ohm,
  .num_gains = ARRAY_LENGTH(s_gains_ohm),
  .full_scale = 0x800000,
  .full_scale_uv = 900000U,
  .max_sample_rate_hz = 1000U,
  .fifo_depth = 64U,
};

static struct {
  struct pbl_ppg_config config;
  int configured;
  int started;
  int stopped;
  int start_ret;
  uint8_t slot;
  uint8_t led_or_rx;
  uint32_t value;
  int reports;
  int16_t last_samples;
} s_drv;

static int prv_init(const struct pbl_ppg *ppg) {
  return 0;
}

static const struct pbl_ppg_info *prv_get_info(const struct pbl_ppg *ppg) {
  return &s_info;
}

static int prv_configure(const struct pbl_ppg *ppg, const struct pbl_ppg_config *config) {
  s_drv.config = *config;
  s_drv.configured++;
  return 0;
}

static int prv_start(const struct pbl_ppg *ppg) {
  s_drv.started++;
  return s_drv.start_ret;
}

static int prv_stop(const struct pbl_ppg *ppg) {
  s_drv.stopped++;
  return 0;
}

static int prv_set_led_current(const struct pbl_ppg *ppg, uint8_t slot, uint8_t led, uint32_t ua) {
  s_drv.slot = slot;
  s_drv.led_or_rx = led;
  s_drv.value = ua;
  return 0;
}

static int prv_set_gain(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx, uint32_t ohm) {
  s_drv.slot = slot;
  s_drv.led_or_rx = rx;
  s_drv.value = ohm;
  return 0;
}

static int prv_get_channel(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx,
                           struct pbl_ppg_channel *channel) {
  channel->gain_ohm = s_gains_ohm[rx];
  channel->calibrated_gain_ohm = s_gains_ohm[rx];
  return 0;
}

static const struct pbl_ppg_ops s_ops = {
  .init = prv_init,
  .get_info = prv_get_info,
  .configure = prv_configure,
  .start = prv_start,
  .stop = prv_stop,
  .set_led_current = prv_set_led_current,
  .set_gain = prv_set_gain,
  .get_channel = prv_get_channel,
};

PBL_PPG_STATE_DEFINE(s_ppg);
static const struct pbl_ppg s_ppg = PBL_PPG_INIT(s_ppg, "ppg", nullptr, nullptr, &s_ops);

static void prv_samples(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch,
                        void *user_data) {
  cl_assert_equal_p(ppg, &s_ppg);
  cl_assert_equal_p(user_data, &s_drv);
  s_drv.reports++;
  s_drv.last_samples = (int16_t)batch->num_samples;
}

static const struct pbl_ppg_listener s_listener = {.samples = prv_samples, .user_data = &s_drv};

void test_ppg__initialize(void) {
  memset(&s_drv, 0, sizeof(s_drv));
  s_ppg_device_state = (struct pbl_device_state){};
  s_ppg_ppg_state.listener = nullptr;
  s_ppg_ppg_state.running = false;
}

void test_ppg__not_ready(void) {
  const struct pbl_ppg_config config = {.sample_rate_hz = 25U, .rx_mask = {0x1}, .batch = 1U};

  cl_assert_equal_p(pbl_ppg_get_info(&s_ppg), nullptr);
  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), -ENODEV);
  cl_assert_equal_i(pbl_ppg_start(&s_ppg), -ENODEV);
  cl_assert_equal_i(s_drv.configured, 0);
}

void test_ppg__configure_validates(void) {
  struct pbl_ppg_config config = {.sample_rate_hz = 25U, .rx_mask = {0x3, 0x1}, .batch = 25U};

  cl_assert_equal_i(pbl_device_init(&s_ppg.dev), 0);
  cl_assert_equal_p(pbl_ppg_get_info(&s_ppg), &s_info);

  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), 0);
  cl_assert_equal_i(s_drv.configured, 1);
  cl_assert_equal_i(s_drv.config.rx_mask[0], 0x3);

  config.rx_mask[1] = 0x2;
  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), -EINVAL);

  config.rx_mask[1] = 0U;
  config.rx_mask[3] = 0x1;
  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), -EINVAL);

  config = (struct pbl_ppg_config){.sample_rate_hz = 25U, .batch = 1U};
  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), -EINVAL);

  config.rx_mask[0] = 0x1;
  config.sample_rate_hz = 2000U;
  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), -EINVAL);

  config.sample_rate_hz = 25U;
  config.batch = 0U;
  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), -EINVAL);
  cl_assert_equal_i(s_drv.configured, 1);
}

void test_ppg__start_stop(void) {
  const struct pbl_ppg_config config = {.sample_rate_hz = 25U, .rx_mask = {0x1}, .batch = 1U};

  cl_assert_equal_i(pbl_device_init(&s_ppg.dev), 0);

  s_drv.start_ret = -EIO;
  cl_assert_equal_i(pbl_ppg_start(&s_ppg), -EIO);
  cl_assert(!pbl_ppg_is_running(&s_ppg));

  s_drv.start_ret = 0;
  cl_assert_equal_i(pbl_ppg_start(&s_ppg), 0);
  cl_assert(pbl_ppg_is_running(&s_ppg));
  cl_assert_equal_i(pbl_ppg_start(&s_ppg), -EALREADY);
  cl_assert_equal_i(pbl_ppg_configure(&s_ppg, &config), -EBUSY);

  cl_assert_equal_i(pbl_ppg_stop(&s_ppg), 0);
  cl_assert_equal_i(pbl_ppg_stop(&s_ppg), 0);
  cl_assert_equal_i(s_drv.stopped, 1);
  cl_assert(!pbl_ppg_is_running(&s_ppg));
}

void test_ppg__led_current(void) {
  cl_assert_equal_i(pbl_device_init(&s_ppg.dev), 0);

  cl_assert_equal_i(pbl_ppg_set_led_current(&s_ppg, 1U, 1U, 20000U), 0);
  cl_assert_equal_i(s_drv.slot, 1);
  cl_assert_equal_i(s_drv.led_or_rx, 1);
  cl_assert_equal_i(s_drv.value, 20000);

  cl_assert_equal_i(pbl_ppg_set_led_current(&s_ppg, 1U, 0U, 20000U), -EINVAL);
  cl_assert_equal_i(pbl_ppg_set_led_current(&s_ppg, 2U, 0U, 20000U), -EINVAL);
  cl_assert_equal_i(pbl_ppg_set_led_current(&s_ppg, 3U, 0U, 20000U), -EINVAL);
  cl_assert_equal_i(pbl_ppg_set_led_current(&s_ppg, 0U, 0U, 100001U), -EINVAL);
}

void test_ppg__gain(void) {
  struct pbl_ppg_channel channel;

  cl_assert_equal_i(pbl_device_init(&s_ppg.dev), 0);

  cl_assert_equal_i(pbl_ppg_set_gain(&s_ppg, 0U, 1U, 100000U), 0);
  cl_assert_equal_i(s_drv.led_or_rx, 1);
  cl_assert_equal_i(s_drv.value, 100000);

  cl_assert_equal_i(pbl_ppg_set_gain(&s_ppg, 0U, 1U, 50000U), -EINVAL);
  cl_assert_equal_i(pbl_ppg_set_gain(&s_ppg, 1U, 1U, 100000U), -EINVAL);
  cl_assert_equal_i(pbl_ppg_get_channel(&s_ppg, 1U, 1U, &channel), -EINVAL);

  cl_assert_equal_i(pbl_ppg_get_channel(&s_ppg, 0U, 1U, &channel), 0);
  cl_assert_equal_i(channel.gain_ohm, 100000);
}

void test_ppg__report(void) {
  const struct pbl_ppg_sample samples[3] = {};
  const struct pbl_ppg_batch batch = {.samples = samples, .num_samples = 3U};

  pbl_ppg_report_samples(&s_ppg, &batch);
  cl_assert_equal_i(s_drv.reports, 0);

  cl_assert_equal_i(pbl_ppg_set_listener(&s_ppg, &s_listener), 0);
  pbl_ppg_report_samples(&s_ppg, &batch);
  cl_assert_equal_i(s_drv.reports, 1);
  cl_assert_equal_i(s_drv.last_samples, 3);
}

void test_ppg__value_to_pa(void) {
  const struct pbl_ppg_channel channel = {.calibrated_gain_ohm = 10000U};

  cl_assert_equal_i(pbl_ppg_value_to_pa(&s_info, &channel, 0x800000), 90000000);
  cl_assert_equal_i(pbl_ppg_value_to_pa(&s_info, &channel, -0x400000), -45000000);
  cl_assert_equal_i(pbl_ppg_value_to_pa(&s_info, &(struct pbl_ppg_channel){}, 1000), 0);
}
