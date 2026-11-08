/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/transport.h>

#include <clar.h>
#include <kernel/event_loop.h>
#include <kernel/events.h>

// Fakes
///////////////////////////////////////////////////////////

#include <fake_kernel_malloc.h>
#include <fake_sys_work.h>

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  pbl_sys_work_call(callback, data);
}

bool launcher_task_is_current_task(void) {
  return false;
}

void event_put(PebbleEvent *event) {
}

void pbl_pp__version_request(struct pbl_pp_session *session) {
}

static int s_meta_error_count;
static enum pbl_pp__meta_error s_last_meta_error;
static uint16_t s_last_meta_endpoint_id;

void pbl_pp__meta_send_error(struct pbl_pp_session *session, enum pbl_pp__meta_error error,
                             uint16_t endpoint_id) {
  ++s_meta_error_count;
  s_last_meta_error = error;
  s_last_meta_endpoint_id = endpoint_id;
}

#define assert_meta_error_sent(e, id)               \
  do {                                              \
    cl_assert_equal_i(s_last_meta_error, e);        \
    cl_assert_equal_i(s_last_meta_endpoint_id, id); \
  } while (0)

static void prv_send_next(struct pbl_pp_session *session) {
}

static const struct pbl_pp_transport s_transport = {
  .name = "test",
  .send_next = prv_send_next,
};

// Endpoints
///////////////////////////////////////////////////////////

#define NON_EXISTENT_ENDPOINT_ID       0x0000
#define OTHER_NON_EXISTENT_ENDPOINT_ID 0xffff
#define PRIVATE_TEST_ENDPOINT_ID       0x0001
#define PUBLIC_TEST_ENDPOINT_ID        0x0002
#define ANY_TEST_ENDPOINT_ID           0x0003

static struct {
  int foo;
} s_test_receiver_ctx;

static int s_prepare_count;
static int s_finish_count;
static int s_cleanup_count;
static size_t s_prepare_length;
static uint8_t s_write_buffer[1024];
static uint16_t s_write_length;
static bool s_prepare_return_null;

static struct pbl_pp_receiver *prv_prepare(struct pbl_pp_session *session,
                                           const struct pbl_pp_endpoint *endpoint, size_t length) {
  ++s_prepare_count;
  s_prepare_length = length;
  if (s_prepare_return_null) {
    return nullptr;
  }
  return (struct pbl_pp_receiver *)&s_test_receiver_ctx;
}

static void prv_write(struct pbl_pp_receiver *receiver, const uint8_t *data, size_t length) {
  cl_assert(s_write_length + length < sizeof(s_write_buffer));
  memcpy(s_write_buffer + s_write_length, data, length);
  s_write_length += length;
}

static void prv_finish(struct pbl_pp_receiver *receiver) {
  ++s_finish_count;
}

static void prv_cleanup(struct pbl_pp_receiver *receiver) {
  ++s_cleanup_count;
}

static const struct pbl_pp_receiver_api s_test_receiver = {
  .prepare = prv_prepare,
  .write = prv_write,
  .finish = prv_finish,
  .cleanup = prv_cleanup,
};

PBL_PP_ENDPOINT_DEFINE_RECEIVER(PRIVATE_TEST_ENDPOINT_ID, PBL_PP_ACCESS_PRIVATE, s_test_receiver);
PBL_PP_ENDPOINT_DEFINE_RECEIVER(PUBLIC_TEST_ENDPOINT_ID, PBL_PP_ACCESS_PUBLIC, s_test_receiver);
PBL_PP_ENDPOINT_DEFINE_RECEIVER(ANY_TEST_ENDPOINT_ID, PBL_PP_ACCESS_ANY, s_test_receiver);

// Kernel receiver, used by endpoints without a receiver of their own.
static int s_kernel_prepare_count;
static const struct pbl_pp_endpoint *s_kernel_prepare_endpoint;

static struct pbl_pp_receiver *prv_kernel_prepare(struct pbl_pp_session *session,
                                                  const struct pbl_pp_endpoint *endpoint,
                                                  size_t length) {
  ++s_kernel_prepare_count;
  s_kernel_prepare_endpoint = endpoint;
  return (struct pbl_pp_receiver *)&s_test_receiver_ctx;
}

const struct pbl_pp_receiver_api pbl_pp_kernel_receiver = {
  .prepare = prv_kernel_prepare,
  .write = prv_write,
  .finish = prv_finish,
  .cleanup = prv_cleanup,
};

#define HANDLER_TEST_ENDPOINT_ID 0x0004

static void prv_handler(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
}

PBL_PP_ENDPOINT_DEFINE(HANDLER_TEST_ENDPOINT_ID, PBL_PP_ACCESS_ANY, prv_handler);

// Helpers
///////////////////////////////////////////////////////////

static struct pbl_pp_session *s_session;

#define RECEIVE(...)                                                       \
  do {                                                                     \
    const uint8_t partial_data[] = {__VA_ARGS__};                          \
    pbl_pp_session_receive(s_session, partial_data, sizeof(partial_data)); \
  } while (0)

// Tests
///////////////////////////////////////////////////////////

void test_receive__initialize(void) {
  fake_kernel_malloc_init();
  fake_kernel_malloc_mark();
  s_session = pbl_pp_session_open(&s_transport, PBL_PP_DESTINATION_SYSTEM, nullptr);
  s_prepare_count = 0;
  s_finish_count = 0;
  s_cleanup_count = 0;
  s_prepare_length = 0;
  s_write_length = 0;
  s_prepare_return_null = false;
  s_meta_error_count = 0;
  s_last_meta_error = 0;
  s_last_meta_endpoint_id = 0;
  s_kernel_prepare_count = 0;
  s_kernel_prepare_endpoint = nullptr;
}

void test_receive__cleanup(void) {
  if (s_session) {
    pbl_pp_session_close(s_session);
    s_session = nullptr;
  }
  fake_sys_work_callbacks_cleanup();
  fake_kernel_malloc_mark_assert_equal();
  fake_kernel_malloc_deinit();
}

void test_receive__header_byte_by_byte(void) {
  // Length, big endian
  RECEIVE(0x00);
  cl_assert_equal_i(s_prepare_count, 0);
  RECEIVE(0x01);
  cl_assert_equal_i(s_prepare_count, 0);

  // Endpoint ID, big endian
  RECEIVE(PRIVATE_TEST_ENDPOINT_ID >> 8);
  cl_assert_equal_i(s_prepare_count, 0);
  RECEIVE(PRIVATE_TEST_ENDPOINT_ID & 0xff);
  cl_assert_equal_i(s_prepare_count, 1);
  cl_assert_equal_i(s_prepare_length, 1);
}

void test_receive__unhandled_endpoint(void) {
  RECEIVE(0x00, 0x01, NON_EXISTENT_ENDPOINT_ID >> 8, NON_EXISTENT_ENDPOINT_ID & 0xff, 0x55);
  cl_assert_equal_i(s_prepare_count, 0);
  assert_meta_error_sent(PBL_PP__META_UNHANDLED, NON_EXISTENT_ENDPOINT_ID);

  RECEIVE(0x00, 0x01, OTHER_NON_EXISTENT_ENDPOINT_ID >> 8, OTHER_NON_EXISTENT_ENDPOINT_ID & 0xff,
          0x55);
  cl_assert_equal_i(s_prepare_count, 0);
  assert_meta_error_sent(PBL_PP__META_UNHANDLED, OTHER_NON_EXISTENT_ENDPOINT_ID);

  RECEIVE(0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  cl_assert_equal_i(s_prepare_count, 1);
  cl_assert_equal_i(s_meta_error_count, 2);
}

void test_receive__unhandled_and_supported_concat(void) {
  RECEIVE(0x00, 0x01, NON_EXISTENT_ENDPOINT_ID >> 8, NON_EXISTENT_ENDPOINT_ID & 0xff, 0x55, 0x00,
          0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  assert_meta_error_sent(PBL_PP__META_UNHANDLED, NON_EXISTENT_ENDPOINT_ID);
  cl_assert_equal_i(s_prepare_count, 1);
  cl_assert_equal_i(s_write_length, 1);
  cl_assert_equal_i(s_write_buffer[0], 0xaa);
}

void test_receive__skipped_payload_split_across_writes(void) {
  RECEIVE(0x00, 0x03, NON_EXISTENT_ENDPOINT_ID >> 8, NON_EXISTENT_ENDPOINT_ID & 0xff, 0x55);
  RECEIVE(0x55);
  RECEIVE(0x55, 0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  cl_assert_equal_i(s_meta_error_count, 1);
  cl_assert_equal_i(s_prepare_count, 1);
  cl_assert_equal_i(s_finish_count, 1);
  cl_assert_equal_i(s_write_buffer[0], 0xaa);
}

void test_receive__system_disallowed_endpoint(void) {
  RECEIVE(0x00, 0x01, PUBLIC_TEST_ENDPOINT_ID >> 8, PUBLIC_TEST_ENDPOINT_ID & 0xff, 0x55);
  cl_assert_equal_i(s_prepare_count, 0);
  assert_meta_error_sent(PBL_PP__META_DISALLOWED, PUBLIC_TEST_ENDPOINT_ID);

  RECEIVE(0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  cl_assert_equal_i(s_prepare_count, 1);
}

void test_receive__app_disallowed_endpoint(void) {
  pbl_pp_session_close(s_session);
  s_session = pbl_pp_session_open(&s_transport, PBL_PP_DESTINATION_APP, nullptr);

  RECEIVE(0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  cl_assert_equal_i(s_prepare_count, 0);
  assert_meta_error_sent(PBL_PP__META_DISALLOWED, PRIVATE_TEST_ENDPOINT_ID);

  RECEIVE(0x00, 0x01, PUBLIC_TEST_ENDPOINT_ID >> 8, PUBLIC_TEST_ENDPOINT_ID & 0xff, 0x55);
  cl_assert_equal_i(s_prepare_count, 1);
}

void test_receive__any_endpoint_allowed_on_both(void) {
  RECEIVE(0x00, 0x01, ANY_TEST_ENDPOINT_ID >> 8, ANY_TEST_ENDPOINT_ID & 0xff, 0x55);
  cl_assert_equal_i(s_prepare_count, 1);

  pbl_pp_session_close(s_session);
  s_session = pbl_pp_session_open(&s_transport, PBL_PP_DESTINATION_APP, nullptr);

  RECEIVE(0x00, 0x01, ANY_TEST_ENDPOINT_ID >> 8, ANY_TEST_ENDPOINT_ID & 0xff, 0x55);
  cl_assert_equal_i(s_prepare_count, 2);
  cl_assert_equal_i(s_meta_error_count, 0);
}

void test_receive__default_receiver_for_handler_endpoints(void) {
  RECEIVE(0x00, 0x01, HANDLER_TEST_ENDPOINT_ID >> 8, HANDLER_TEST_ENDPOINT_ID & 0xff, 0x55);
  cl_assert_equal_i(s_kernel_prepare_count, 1);
  cl_assert_equal_i(s_prepare_count, 0);
  cl_assert(s_kernel_prepare_endpoint);
  cl_assert_equal_i(s_kernel_prepare_endpoint->id, HANDLER_TEST_ENDPOINT_ID);
  cl_assert_equal_p(s_kernel_prepare_endpoint->handler, prv_handler);
  cl_assert_equal_i(s_kernel_prepare_endpoint->task, PBL_PP_HANDLER_BG);
  cl_assert_equal_i(s_finish_count, 1);
}

void test_receive__ignore_message_if_no_receiver_could_be_prepared(void) {
  s_prepare_return_null = true;
  RECEIVE(0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  cl_assert_equal_i(s_prepare_count, 1);
  cl_assert_equal_i(s_finish_count, 0);
  s_prepare_return_null = false;

  RECEIVE(0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  cl_assert_equal_i(s_prepare_count, 2);
  cl_assert_equal_i(s_finish_count, 1);
}

void test_receive__cleanup_receiver_if_session_is_closed(void) {
  RECEIVE(0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8, PRIVATE_TEST_ENDPOINT_ID & 0xff);
  cl_assert_equal_i(s_prepare_count, 1);

  pbl_pp_session_close(s_session);
  s_session = nullptr;

  cl_assert_equal_i(s_cleanup_count, 1);
}

void test_receive__payload_in_pieces(void) {
  const uint8_t expected_payload[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee};

  RECEIVE(0x00, sizeof(expected_payload), PRIVATE_TEST_ENDPOINT_ID >> 8,
          PRIVATE_TEST_ENDPOINT_ID & 0xff, 0xaa);
  cl_assert_equal_i(s_write_length, 1);
  cl_assert_equal_i(s_finish_count, 0);

  RECEIVE(0xbb, 0xcc, 0xdd);
  cl_assert_equal_i(s_write_length, 4);
  cl_assert_equal_i(s_finish_count, 0);

  // Last payload byte, then the start of the next header
  RECEIVE(0xee, 0x00, 0x01, PRIVATE_TEST_ENDPOINT_ID >> 8);
  cl_assert_equal_i(s_finish_count, 1);
  cl_assert_equal_i(s_write_length, sizeof(expected_payload));
  cl_assert_equal_m(s_write_buffer, expected_payload, sizeof(expected_payload));

  RECEIVE(PRIVATE_TEST_ENDPOINT_ID & 0xff, 0x11);
  cl_assert_equal_i(s_prepare_count, 2);
  cl_assert_equal_i(s_finish_count, 2);
}
