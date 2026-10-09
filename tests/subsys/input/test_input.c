/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/input/input.h>

#include <clar.h>

#define MAX_EVENTS 8

struct listener {
  struct pbl_input_event events[MAX_EVENTS];
  int count;
};

static struct listener s_first;
static struct listener s_second;

static void prv_record(const struct pbl_input_event *evt, void *user_data) {
  struct listener *l = user_data;

  cl_assert(l->count < MAX_EVENTS);
  l->events[l->count++] = *evt;
}

static void prv_record_second(const struct pbl_input_event *evt, void *user_data) {
  prv_record(evt, user_data);
}

PBL_INPUT_CALLBACK_DEFINE(prv_record, &s_first);
PBL_INPUT_CALLBACK_DEFINE(prv_record_second, &s_second);

void test_input__initialize(void) {
  s_first = (struct listener){0};
  s_second = (struct listener){0};
}

static void prv_assert_event(const struct pbl_input_event *evt, enum pbl_input_type type,
                             uint16_t code, int32_t value, bool sync) {
  cl_assert_equal_i(evt->type, type);
  cl_assert_equal_i(evt->code, code);
  cl_assert_equal_i(evt->value, value);
  cl_assert_equal_b(evt->sync, sync);
}

void test_input__key_reaches_every_listener(void) {
  pbl_input_report_key(PBL_INPUT_KEY_SELECT, true, true);

  cl_assert_equal_i(s_first.count, 1);
  cl_assert_equal_i(s_second.count, 1);
  prv_assert_event(&s_first.events[0], PBL_INPUT_EV_KEY, PBL_INPUT_KEY_SELECT, 1, true);
  prv_assert_event(&s_second.events[0], PBL_INPUT_EV_KEY, PBL_INPUT_KEY_SELECT, 1, true);
}

void test_input__report_keeps_order(void) {
  pbl_input_report_key(PBL_INPUT_BTN_TOUCH, true, false);
  pbl_input_report_abs(PBL_INPUT_ABS_X, 12, false);
  pbl_input_report_abs(PBL_INPUT_ABS_Y, -3, false);
  pbl_input_report_ges(PBL_INPUT_GES_PALM, true);

  cl_assert_equal_i(s_first.count, 4);
  prv_assert_event(&s_first.events[0], PBL_INPUT_EV_KEY, PBL_INPUT_BTN_TOUCH, 1, false);
  prv_assert_event(&s_first.events[1], PBL_INPUT_EV_ABS, PBL_INPUT_ABS_X, 12, false);
  prv_assert_event(&s_first.events[2], PBL_INPUT_EV_ABS, PBL_INPUT_ABS_Y, -3, false);
  prv_assert_event(&s_first.events[3], PBL_INPUT_EV_GES, PBL_INPUT_GES_PALM, 0, true);
}
