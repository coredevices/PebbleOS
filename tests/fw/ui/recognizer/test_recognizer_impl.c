/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_recognizer_impl.h"

#include "clar_asserts.h"

#include "applib/ui/recognizer/recognizer.h"
#include "applib/ui/recognizer/recognizer_impl.h"

static const int TEST_PATTERN = 0xA5A5A5A5;

static RecognizerImpl s_test_impl;

static void prv_handle_touch_event(Recognizer *recognizer, const TouchEvent *touch_event) {
  TestImplData *data = recognizer_get_impl_data(recognizer, &s_test_impl);
  cl_assert_equal_i(data->test, TEST_PATTERN);
  if (data->last_touch_event) {
    *data->last_touch_event = *touch_event;
  }
  if (data->handled) {
    *data->handled = true;
  }
  if(data->new_state && (*data->new_state != RecognizerState_Possible)) {
    cl_assert(data->updated);
    cl_assert_equal_p(*data->updated, false);
    *data->updated = true;
    recognizer_transition_state(recognizer, *data->new_state);
  }
}

static bool prv_cancel(Recognizer *recognizer) {
  TestImplData *data = recognizer_get_impl_data(recognizer, &s_test_impl);
  cl_assert_equal_i(data->test, TEST_PATTERN);
  if (data->cancelled) {
    cl_assert_equal_b(*data->cancelled, false);
    *data->cancelled = true;
  }
  return true;
}

static void prv_reset(Recognizer *recognizer) {
  TestImplData *data = recognizer_get_impl_data(recognizer, &s_test_impl);
  cl_assert_equal_i(data->test, TEST_PATTERN);
  if (data->reset) {
    cl_assert_equal_b(*data->reset, false);
    *data->reset = true;
  }
}

static void prv_on_destroy(Recognizer *recognizer) {
  TestImplData *data = recognizer_get_impl_data(recognizer, &s_test_impl);
  cl_assert_equal_i(data->test, TEST_PATTERN);
  if (data->destroyed) {
    cl_assert_equal_b(*data->destroyed, false);
    *data->destroyed = true;
  }
}

static void prv_on_fail(Recognizer *recognizer) {
  TestImplData *data = recognizer_get_impl_data(recognizer, &s_test_impl);
  cl_assert_equal_i(data->test, TEST_PATTERN);
  if (data->failed) {
    cl_assert_equal_b(*data->failed, false);
    *data->failed = true;
  }
}

static void prv_sub_event_handler(const Recognizer *recognizer, RecognizerEvent event) {
  RecognizerEvent *event_type = recognizer_get_user_data(recognizer);
  if (event_type) {
    *event_type = event;
  }
}

Recognizer *test_recognizer_create(TestImplData *test_impl_data, void *user_data) {
  s_test_impl = (RecognizerImpl) {
    .handle_touch_event = prv_handle_touch_event,
    .cancel = prv_cancel,
    .reset = prv_reset,
    .on_fail = prv_on_fail
  };
  test_impl_data->test = TEST_PATTERN;
  return recognizer_create_with_data(&s_test_impl, test_impl_data,
                                     sizeof(*test_impl_data), prv_sub_event_handler,
                                     user_data);
}

void test_recognizer_enable_on_destroy(void) {
  s_test_impl.on_destroy = prv_on_destroy;
}

void test_recognizer_destroy(Recognizer **recognizer) {
  recognizer_destroy(*recognizer);
}

void *test_recognizer_get_data(Recognizer *recognizer) {
  return recognizer_get_impl_data(recognizer, &s_test_impl);
}
