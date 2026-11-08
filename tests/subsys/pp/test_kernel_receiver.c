/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/work.h>
#include <pbl/pp/endpoint.h>

#include <clar.h>

// Fakes
///////////////////////////////////////////////////////////

#include <fakes/fake_pbl_malloc.h>
#include <fakes/fake_sys_work.h>

typedef void (*CallbackEventCallback)(void *data);

static int s_kernel_main_schedule_count;
void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  ++s_kernel_main_schedule_count;
  pbl_sys_work_call(callback, data);
}

#define FAKE_SESSION ((struct pbl_pp_session *)1)

// Helpers
///////////////////////////////////////////////////////////

enum {
  HandlerA = 0,
  HandlerB,
  HandlerC,
  NumHandlers
};

static int s_handler_call_count[NumHandlers];
static struct pbl_pp_session *s_handler_session[NumHandlers];

static const void *s_expected_data;
static size_t s_expected_len;
static bool s_skip_data_assert;

#define MAX_RECORDED_CALLS 16
static uint8_t s_recorded_data[MAX_RECORDED_CALLS];
static size_t s_recorded_lens[MAX_RECORDED_CALLS];
static int s_recorded_count;

static void prv_set_expected_data(const void *data, size_t len) {
  s_expected_data = data;
  s_expected_len = len;
}

static void prv_assert_data_matches_expected(const void *data, size_t len) {
  if (s_skip_data_assert) {
    return;
  }
  cl_assert_equal_i(len, s_expected_len);
  cl_assert(memcmp(s_expected_data, data, len) == 0);
}

static void prv_assert_no_handler_calls(void) {
  for (int i = 0; i < NumHandlers; i++) {
    cl_assert_equal_i(s_handler_call_count[i], 0);
  }
}

static void prv_handler(int idx, struct pbl_pp_session *session, const uint8_t *data,
                        size_t length) {
  s_handler_call_count[idx]++;
  s_handler_session[idx] = session;
  if (idx == HandlerA && s_recorded_count < MAX_RECORDED_CALLS) {
    s_recorded_data[s_recorded_count] = data[0];
    s_recorded_lens[s_recorded_count] = length;
    s_recorded_count++;
  }
  prv_assert_data_matches_expected(data, length);
}

static void prv_handler_a(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  prv_handler(HandlerA, session, data, length);
}

static void prv_handler_b(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  prv_handler(HandlerB, session, data, length);
}

static void prv_handler_c(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  prv_handler(HandlerC, session, data, length);
}

static const struct pbl_pp_endpoint s_endpoints[NumHandlers] = {
  [HandlerA] = {.handler = prv_handler_a, .id = 0x0001, .task = PBL_PP_HANDLER_BG},
  [HandlerB] = {.handler = prv_handler_b, .id = 0x0002, .task = PBL_PP_HANDLER_BG},
  [HandlerC] = {.handler = prv_handler_c, .id = 0x0003, .task = PBL_PP_HANDLER_MAIN},
};

static struct pbl_pp_receiver *prv_prepare_write(struct pbl_pp_session *session, int idx,
                                                 const void *data, size_t length) {
  struct pbl_pp_receiver *receiver =
      pbl_pp_kernel_receiver.prepare(session, &s_endpoints[idx], length);
  cl_assert(receiver != nullptr);
  pbl_pp_kernel_receiver.write(receiver, data, length);
  return receiver;
}

// Tests
///////////////////////////////////////////////////////////

void test_kernel_receiver__initialize(void) {
  memset(s_handler_call_count, 0, sizeof(s_handler_call_count));
  memset(s_handler_session, 0, sizeof(s_handler_session));
  s_kernel_main_schedule_count = 0;
  s_skip_data_assert = false;
  s_recorded_count = 0;
}

void test_kernel_receiver__cleanup(void) {
  fake_sys_work_callbacks_cleanup();
}

void test_kernel_receiver__zero_length_is_dropped(void) {
  cl_assert_equal_p(pbl_pp_kernel_receiver.prepare(FAKE_SESSION, &s_endpoints[HandlerA], 0),
                    nullptr);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__prepare_write_finish_single(void) {
  const char *data = "helloworld";

  struct pbl_pp_receiver *receiver = prv_prepare_write(FAKE_SESSION, HandlerA, data, strlen(data));
  prv_assert_no_handler_calls();

  pbl_pp_kernel_receiver.finish(receiver);
  // Pended on SysWorkq, not run right away:
  prv_assert_no_handler_calls();
  cl_assert_equal_i(s_kernel_main_schedule_count, 0);

  prv_set_expected_data(data, strlen(data));
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerA], 1);
  cl_assert_equal_p(s_handler_session[HandlerA], FAKE_SESSION);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__main_task_endpoint(void) {
  const char *data = "main";

  struct pbl_pp_receiver *receiver = prv_prepare_write(FAKE_SESSION, HandlerC, data, strlen(data));
  pbl_pp_kernel_receiver.finish(receiver);
  cl_assert_equal_i(s_kernel_main_schedule_count, 1);
  prv_assert_no_handler_calls();

  prv_set_expected_data(data, strlen(data));
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerC], 1);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__multiple_sessions(void) {
  struct pbl_pp_receiver *receiver[NumHandlers];
  struct pbl_pp_session *session[NumHandlers] = {
    (struct pbl_pp_session *)1,
    (struct pbl_pp_session *)2,
    (struct pbl_pp_session *)3,
  };
  const char *data[NumHandlers] = {"Session 1 Data!!", "This is Session 2 Data!", "Session 3"};

  for (int i = 0; i < NumHandlers; i++) {
    receiver[i] = pbl_pp_kernel_receiver.prepare(session[i], &s_endpoints[i], strlen(data[i]));
    cl_assert(receiver[i] != nullptr);
    for (size_t j = 0; j < strlen(data[i]); j++) {
      pbl_pp_kernel_receiver.write(receiver[i], (const uint8_t *)&data[i][j], 1);
    }
  }
  prv_assert_no_handler_calls();

  for (int i = NumHandlers - 1; i >= 0; i--) {
    pbl_pp_kernel_receiver.finish(receiver[i]);
    cl_assert_equal_i(s_handler_call_count[i], 0);

    prv_set_expected_data(data[i], strlen(data[i]));
    fake_sys_work_callbacks_invoke_pending();
    cl_assert_equal_i(s_handler_call_count[i], 1);
    cl_assert_equal_p(s_handler_session[i], session[i]);
  }

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__same_session_batched(void) {
  const int batch_num = 10;
  char data = 'a';

  for (int i = 0; i < batch_num; i++) {
    pbl_pp_kernel_receiver.finish(prv_prepare_write(FAKE_SESSION, HandlerA, &data, 1));
    data += 1;
  }
  prv_assert_no_handler_calls();

  s_skip_data_assert = true;
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerA], batch_num);

  cl_assert_equal_i(s_recorded_count, batch_num);
  for (int i = 0; i < batch_num; i++) {
    cl_assert_equal_i(s_recorded_data[i], 'a' + i);
    cl_assert_equal_i(s_recorded_lens[i], 1);
  }

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__batch_drains_one_per_callback(void) {
  const int batch_num = 5;
  char data = 'a';

  for (int i = 0; i < batch_num; i++) {
    pbl_pp_kernel_receiver.finish(prv_prepare_write(FAKE_SESSION, HandlerA, &data, 1));
    data += 1;
  }
  prv_assert_no_handler_calls();
  s_skip_data_assert = true;

  // The flood coalesces into a single pending callback.
  cl_assert_equal_i(fake_sys_work_count_callbacks(), 1);

  for (int i = 0; i < batch_num; i++) {
    cl_assert_equal_i(s_handler_call_count[HandlerA], i);
    fake_sys_work_callbacks_invoke(1);
    cl_assert_equal_i(s_handler_call_count[HandlerA], i + 1);
  }

  cl_assert_equal_i(fake_sys_work_count_callbacks(), 0);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__bg_and_main_drain_independently(void) {
  char data = 'x';

  pbl_pp_kernel_receiver.finish(prv_prepare_write(FAKE_SESSION, HandlerA, &data, 1));
  pbl_pp_kernel_receiver.finish(prv_prepare_write(FAKE_SESSION, HandlerC, &data, 1));
  cl_assert_equal_i(fake_sys_work_count_callbacks(), 2);
  cl_assert_equal_i(s_kernel_main_schedule_count, 1);

  s_skip_data_assert = true;
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerA], 1);
  cl_assert_equal_i(s_handler_call_count[HandlerC], 1);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__receiver_cleanup(void) {
  const char *data = "cleanup test!";

  struct pbl_pp_receiver *receiver = prv_prepare_write(FAKE_SESSION, HandlerA, data, strlen(data));
  pbl_pp_kernel_receiver.cleanup(receiver);

  fake_sys_work_callbacks_invoke_pending();
  prv_assert_no_handler_calls();
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_kernel_receiver__cleanup_after_finish(void) {
  const char *data = "cleanup but finish ran first!";

  struct pbl_pp_receiver *receiver = prv_prepare_write(FAKE_SESSION, HandlerA, data, strlen(data));
  pbl_pp_kernel_receiver.finish(receiver);
  pbl_pp_kernel_receiver.cleanup(receiver);

  // The message is owned by the pending callback now.
  cl_assert(fake_pbl_malloc_num_net_allocs() != 0);

  prv_set_expected_data(data, strlen(data));
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerA], 1);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}
