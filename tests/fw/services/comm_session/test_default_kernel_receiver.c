/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/comm_session/session_receive_router.h"
#include "pbl/services/system_task.h"

#include "clar.h"

#include <string.h>

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_logging.h"
#include "stubs_passert.h"

typedef void (*CallbackEventCallback)(void *data);

static int s_kernel_main_schedule_count;
void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  ++s_kernel_main_schedule_count;
  system_task_add_callback(callback, data);
}

// Fakes
///////////////////////////////////////////////////////////

#include "../../fakes/fake_pbl_malloc.h"
#include "../../fakes/fake_system_task.h"

#define FAKE_COMM_SESSION (CommSession *)1

// Helpers
///////////////////////////////////////////////////////////

extern const ReceiverImplementation g_default_kernel_receiver_implementation;
extern const PebbleTask g_default_kernel_receiver_opt_bg;
extern const PebbleTask g_default_kernel_receiver_opt_main;

typedef enum {
  HandlerA = 0,
  HandlerB,
  HandlerC,
  NumHandlers
} FakeProtocolHandlers;

static int s_handler_call_count[NumHandlers] = { 0 };

typedef struct {
  size_t len;
  uint8_t *buf;
} ExpectedData;

static ExpectedData s_expected_data;
static bool s_skip_data_assert;

//! Record of data received by handlers, for verifying batched delivery order.
#define MAX_RECORDED_CALLS 16
static uint8_t s_recorded_data[MAX_RECORDED_CALLS];
static size_t s_recorded_lens[MAX_RECORDED_CALLS];
static int s_recorded_count;

static void prv_set_expected_data(void *data, size_t len) {
  s_expected_data.buf = data;
  s_expected_data.len = len;
}

static void prv_assert_data_matches_expected(const void *data, size_t len) {
  if (s_skip_data_assert) {
    return;
  }
  cl_assert_equal_i(len, s_expected_data.len);
  cl_assert(memcmp(s_expected_data.buf, data, len) == 0);
}

static void prv_assert_no_handler_calls(void) {
  for (int i = 0; i < NumHandlers; i++) {
    cl_assert_equal_i(s_handler_call_count[i], 0);
  }
}

static void prv_endpoint_handler_a(
    CommSession *session, const uint8_t *data, size_t length) {
  s_handler_call_count[HandlerA]++;
  if (s_recorded_count < MAX_RECORDED_CALLS) {
    s_recorded_data[s_recorded_count] = data[0];
    s_recorded_lens[s_recorded_count] = length;
    s_recorded_count++;
  }
  prv_assert_data_matches_expected(data, length);
}

static void prv_endpoint_handler_b(
    CommSession *session, const uint8_t *data, size_t length) {
  s_handler_call_count[HandlerB]++;
  prv_assert_data_matches_expected(data, length);
}

static void prv_endpoint_handler_c(
    CommSession *session, const uint8_t *data, size_t length) {
  s_handler_call_count[HandlerC]++;
  prv_assert_data_matches_expected(data, length);
}

static const PebbleProtocolEndpoint s_endpoints[NumHandlers] = {
  [0] = {
    .handler = prv_endpoint_handler_a,
    .receiver_opt = &g_default_kernel_receiver_opt_bg,
  },
  [1] = {
    .handler = prv_endpoint_handler_b,
    .receiver_opt = &g_default_kernel_receiver_opt_bg,
  },
  [2] = {
    .handler = prv_endpoint_handler_c,
    .receiver_opt = &g_default_kernel_receiver_opt_main,
  },
};

// Tests
///////////////////////////////////////////////////////////

void test_default_kernel_receiver__cleanup(void) {
  memset(s_handler_call_count, 0x0, sizeof(s_handler_call_count));
  s_kernel_main_schedule_count = 0;
  s_skip_data_assert = false;
  s_recorded_count = 0;
}


//! With one event in flight, walk through the prepare, write, finish happy
//! path.  Ensure that the endpoint handler CB is run from kernel BG and that
//! we don't leak memory
void test_default_kernel_receiver__prepare_write_finish_single(void) {
  char *data = "helloworld";

  Receiver *receiver = g_default_kernel_receiver_implementation.prepare(
      FAKE_COMM_SESSION, &s_endpoints[0], strlen(data));
  cl_assert(receiver != NULL);

  g_default_kernel_receiver_implementation.write(receiver, (uint8_t *)data, strlen(data));
  prv_assert_no_handler_calls();

  g_default_kernel_receiver_implementation.finish(receiver);

  // CBs shouldn't immediately execute since they are pended on KernelBG
  prv_assert_no_handler_calls();

  prv_set_expected_data(data, strlen(data));
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerA], 1);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! Multiple sessions should be able to be transmitting messages concurrently
void test_default_kernel_receiver__prepare_write_finish_multiple_sessions(void) {
  Receiver *receiver[NumHandlers];
  CommSession *session[NumHandlers] = {
    (CommSession *)1,
    (CommSession *)2,
    (CommSession *)3,
  };

  char *data[NumHandlers] = {
    "Session 1 Data!!",
    "This is Session 2 Data!",
    "Session 3"
  };

  for (int i = 0; i < NumHandlers; i++) {
    receiver[i] = g_default_kernel_receiver_implementation.prepare(
      session[i], &s_endpoints[i], strlen(data[i]));
    cl_assert(receiver[i] != NULL);

    for (int j = 0; j < strlen(data[i]); j++) {
      g_default_kernel_receiver_implementation.write(
          receiver[i], (uint8_t *)&data[i][j], 1);
    }
  }

  prv_assert_no_handler_calls();

  for (int i = NumHandlers - 1; i >= 0; i--) {
    int kernel_main_schedule_count_before = s_kernel_main_schedule_count;

    g_default_kernel_receiver_implementation.finish(receiver[i]);
    cl_assert_equal_i(s_handler_call_count[i], 0);

    bool should_execute_on_kernel_main =
        (s_endpoints[i].receiver_opt == &g_default_kernel_receiver_opt_main);
    if (should_execute_on_kernel_main) {
      cl_assert_equal_i(kernel_main_schedule_count_before + 1, s_kernel_main_schedule_count);
    }

    prv_set_expected_data(data[i], strlen(data[i]));
    fake_system_task_callbacks_invoke_pending();
    cl_assert_equal_i(s_handler_call_count[i], 1);
  }

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! It's possible the same session can receiver multiple messages before any
//! are processed on kernel BG. Make sure they do not interfere with one
//! another. With coalesced callbacks, a single system_task callback drains
//! all pending messages in order.
void test_default_kernel_receiver__same_session_batched(void) {
  const int batch_num = 10;
  char data = 'a';
  for (int i = 0; i < batch_num; i++) {
    Receiver *receiver = g_default_kernel_receiver_implementation.prepare(
        FAKE_COMM_SESSION, &s_endpoints[0], 1);
    cl_assert(receiver != NULL);

    g_default_kernel_receiver_implementation.write(
        receiver, (uint8_t *)&data, 1);

    g_default_kernel_receiver_implementation.finish(receiver);

    data += 1;
  }

  prv_assert_no_handler_calls();

  // All 10 messages are coalesced into a single system_task callback.
  // Skip per-handler expected_data assertion; verify order via recording.
  s_skip_data_assert = true;
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerA], batch_num);

  // Verify that handlers were called in the correct order with correct data
  cl_assert_equal_i(s_recorded_count, batch_num);
  for (int i = 0; i < batch_num; i++) {
    cl_assert_equal_i(s_recorded_data[i], 'a' + i);
    cl_assert_equal_i(s_recorded_lens[i], 1);
  }

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! Make sure that if cleanup runs and we are partially through updating a
//! buffer the callback does not get called
void test_default_kernel_receiver__receiver_cleanup(void) {
  char *data = "cleanup test!";

  Receiver *receiver = g_default_kernel_receiver_implementation.prepare(
      FAKE_COMM_SESSION, &s_endpoints[0], strlen(data));
  cl_assert(receiver != NULL);

  g_default_kernel_receiver_implementation.write(receiver, (uint8_t *)data, strlen(data));
  prv_assert_no_handler_calls();


  g_default_kernel_receiver_implementation.cleanup(receiver);

  fake_system_task_callbacks_invoke_pending();
  prv_assert_no_handler_calls();
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! Test the case where a callback has been pended to kernelBG and we get a cleanup
void test_default_kernel_receiver__race_condition(void) {
  char *data = "cleanup but finish ran first!";

  Receiver *receiver = g_default_kernel_receiver_implementation.prepare(
      FAKE_COMM_SESSION, &s_endpoints[0], strlen(data));
  cl_assert(receiver != NULL);

  g_default_kernel_receiver_implementation.write(receiver, (uint8_t *)data, strlen(data));
  prv_assert_no_handler_calls();

  g_default_kernel_receiver_implementation.finish(receiver);
  g_default_kernel_receiver_implementation.cleanup(receiver);

  // our msg should not have been freed since it was offloaded to kernelBG
  cl_assert(fake_pbl_malloc_num_net_allocs() != 0);

  prv_set_expected_data(data, strlen(data));
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(s_handler_call_count[HandlerA], 1);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}
