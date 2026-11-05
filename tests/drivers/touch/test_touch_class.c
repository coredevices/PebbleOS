/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/touch.h>
#include <pbl/input/input.h>

#include <clar.h>

#define MAX_EVENTS 4

static int s_init_ret;
static int s_enable_ret;
static int s_enable_count;
static int s_disable_count;
static struct pbl_input_event s_events[MAX_EVENTS];
static int s_num_events;

static void prv_record(const struct pbl_input_event *evt, void *user_data) {
  cl_assert(s_num_events < MAX_EVENTS);
  s_events[s_num_events++] = *evt;
}

PBL_INPUT_CALLBACK_DEFINE(prv_record, nullptr);

static int prv_init(const struct pbl_touch *touch) {
  return s_init_ret;
}

static int prv_enable(const struct pbl_touch *touch) {
  s_enable_count++;
  return s_enable_ret;
}

static int prv_disable(const struct pbl_touch *touch) {
  s_disable_count++;
  return 0;
}

static const struct pbl_touch_ops s_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
};

PBL_TOUCH_STATE_DEFINE(s_plain);
static const struct pbl_touch s_plain =
    PBL_TOUCH_INIT(s_plain, "plain", nullptr, &s_ops, .size_x = 200, .size_y = 228);

PBL_TOUCH_STATE_DEFINE(s_inverted);
static const struct pbl_touch s_inverted =
    PBL_TOUCH_INIT(s_inverted, "inverted", nullptr, &s_ops, .size_x = 200, .size_y = 228,
                   .inverted_x = true, .inverted_y = true);

PBL_TOUCH_STATE_DEFINE(s_swapped);
static const struct pbl_touch s_swapped =
    PBL_TOUCH_INIT(s_swapped, "swapped", nullptr, &s_ops, .size_x = 228, .size_y = 200,
                   .inverted_x = true, .swapped_x_y = true);

PBL_TOUCH_STATE_DEFINE(s_absent);
static const struct pbl_touch s_absent = PBL_TOUCH_INIT(s_absent, "absent", nullptr, &s_ops);

const struct pbl_touch *const TOUCH = &s_plain;

void test_touch_class__initialize(void) {
  s_init_ret = 0;
  s_enable_ret = 0;
  s_enable_count = 0;
  s_disable_count = 0;
  s_num_events = 0;
  cl_assert_equal_i(pbl_device_init(&s_plain.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_inverted.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_swapped.dev), 0);
}

static void prv_assert_event(int idx, enum pbl_input_type type, uint16_t code, int32_t value,
                             bool sync) {
  cl_assert_equal_i(s_events[idx].type, type);
  cl_assert_equal_i(s_events[idx].code, code);
  cl_assert_equal_i(s_events[idx].value, value);
  cl_assert_equal_b(s_events[idx].sync, sync);
}

void test_touch_class__users_power_the_touchscreen(void) {
  cl_assert(!pbl_touch_is_enabled(&s_plain));

  cl_assert_equal_i(pbl_touch_enable(&s_plain), 0);
  cl_assert_equal_i(pbl_touch_enable(&s_plain), 0);
  cl_assert_equal_i(s_enable_count, 1);
  cl_assert(pbl_touch_is_enabled(&s_plain));

  cl_assert_equal_i(pbl_touch_disable(&s_plain), 0);
  cl_assert_equal_i(s_disable_count, 0);
  cl_assert_equal_i(pbl_touch_disable(&s_plain), 0);
  cl_assert_equal_i(s_disable_count, 1);
  cl_assert(!pbl_touch_is_enabled(&s_plain));

  cl_assert_equal_i(pbl_touch_disable(&s_plain), -EALREADY);
  cl_assert_equal_i(s_disable_count, 1);
}

void test_touch_class__failed_enable_takes_no_reference(void) {
  s_enable_ret = -EIO;
  cl_assert_equal_i(pbl_touch_enable(&s_plain), -EIO);
  cl_assert(!pbl_touch_is_enabled(&s_plain));
  cl_assert_equal_i(pbl_touch_disable(&s_plain), -EALREADY);
}

void test_touch_class__not_ready(void) {
  s_init_ret = -ENODEV;
  cl_assert_equal_i(pbl_device_init(&s_absent.dev), -ENODEV);
  cl_assert_equal_i(pbl_touch_enable(&s_absent), -ENODEV);
  cl_assert_equal_i(pbl_touch_disable(&s_absent), -ENODEV);
  cl_assert_equal_i(s_enable_count, 0);
}

void test_touch_class__report(void) {
  struct pbl_touch_sample sample;

  pbl_touch_report(&s_plain, true, 10, 20);

  cl_assert_equal_i(s_num_events, 3);
  prv_assert_event(0, PBL_INPUT_EV_KEY, PBL_INPUT_BTN_TOUCH, 1, false);
  prv_assert_event(1, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_X, 10, false);
  prv_assert_event(2, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_Y, 20, true);

  pbl_touch_get_last(&s_plain, &sample);
  cl_assert_equal_b(sample.down, true);
  cl_assert_equal_i(sample.x, 10);
  cl_assert_equal_i(sample.y, 20);
}

void test_touch_class__report_gesture(void) {
  pbl_touch_report_gesture(&s_plain, PBL_INPUT_GES_DOUBLE_TAP, 5, 6);

  cl_assert_equal_i(s_num_events, 3);
  prv_assert_event(0, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_X, 5, false);
  prv_assert_event(1, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_Y, 6, false);
  prv_assert_event(2, PBL_INPUT_EV_GES, PBL_INPUT_GES_DOUBLE_TAP, 0, true);
}

void test_touch_class__inverted(void) {
  pbl_touch_report(&s_inverted, true, 10, 20);

  prv_assert_event(1, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_X, 189, false);
  prv_assert_event(2, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_Y, 207, true);
}

void test_touch_class__inverted_out_of_range(void) {
  pbl_touch_report(&s_inverted, true, 300, 228);

  prv_assert_event(1, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_X, 0, false);
  prv_assert_event(2, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_Y, 0, true);
}

void test_touch_class__swapped_after_inversion(void) {
  pbl_touch_report(&s_swapped, true, 10, 20);

  prv_assert_event(1, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_X, 20, false);
  prv_assert_event(2, PBL_INPUT_EV_ABS, PBL_INPUT_ABS_Y, 217, true);
}

void test_touch_class__disable_lifts_last_sample(void) {
  struct pbl_touch_sample sample;

  cl_assert_equal_i(pbl_touch_enable(&s_plain), 0);
  pbl_touch_report(&s_plain, true, 1, 2);
  cl_assert_equal_i(pbl_touch_disable(&s_plain), 0);

  pbl_touch_get_last(&s_plain, &sample);
  cl_assert_equal_b(sample.down, false);
}
