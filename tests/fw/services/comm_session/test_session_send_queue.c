/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/comm_session/session_internal.h"
#include "services/common/comm_session/session_send_queue.h"
#include "util/math.h"

extern void comm_session_send_queue_cleanup(CommSession *session);

// Fakes & Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "fake_kernel_malloc.h"

#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

static CommSession s_session;
static CommSession *s_valid_session;

void comm_session_analytics_inc_bytes_sent(CommSession *session, uint16_t length) {
}

bool comm_session_is_valid(const CommSession *session) {
  if (!session) {
    return false;
  }
  return (s_valid_session == session);
}

void comm_session_send_next(CommSession *session) {
}


// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
  SessionSendQueueJob job;
  size_t consumed_length;
  size_t length;
  uint8_t data[];
} TestSendJob;

static size_t prv_get_length(const TestSendJob *sb) {
  return (sb->length - sb->consumed_length);
}

static const uint8_t *prv_get_read_pointer(const TestSendJob *sb) {
  return (sb->data + sb->consumed_length);
}

static size_t prv_send_job_impl_get_length(const SessionSendQueueJob *send_job) {
  return prv_get_length((TestSendJob *)send_job);
}

size_t prv_send_job_impl_copy(const SessionSendQueueJob *send_job, int start_offset,
                              size_t length, uint8_t *data_out) {
  TestSendJob *sb = (TestSendJob *)send_job;
  const size_t length_remaining = prv_get_length(sb);
  const size_t length_after_offset = (length_remaining - start_offset);
  const size_t length_to_copy = MIN(length_after_offset, length);
  memcpy(data_out, prv_get_read_pointer(sb) + start_offset, length_to_copy);
  return length_to_copy;
}

size_t prv_send_job_impl_get_read_pointer(const SessionSendQueueJob *send_job,
                                          const uint8_t **data_out) {
  TestSendJob *sb = (TestSendJob *)send_job;
  *data_out = prv_get_read_pointer(sb);
  return prv_get_length(sb);
}

void prv_send_job_impl_consume(const SessionSendQueueJob *send_job, size_t length) {
  TestSendJob *sb = (TestSendJob *)send_job;
  sb->consumed_length += length;
}

static int s_free_count;

void prv_send_job_impl_free(SessionSendQueueJob *send_job) {
  kernel_free(send_job);
  ++s_free_count;
}

static const SessionSendJobImpl s_test_job_impl = {
  .get_length = prv_send_job_impl_get_length,
  .copy = prv_send_job_impl_copy,
  .get_read_pointer = prv_send_job_impl_get_read_pointer,
  .consume = prv_send_job_impl_consume,
  .free = prv_send_job_impl_free,
};

SessionSendQueueJob *prv_create_test_job(const uint8_t *data, size_t length) {
  TestSendJob *job = kernel_malloc(sizeof(TestSendJob) + length);
  cl_assert(job);
  *job = (const TestSendJob) {
    .job = {
      .impl = &s_test_job_impl,
    },
    .length = length,
  };
  if (length && data) {
    memcpy(job->data, data, length);
  }
  return (SessionSendQueueJob *)job;
}

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

static const uint8_t TEST_DATA[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

void test_session_send_queue__initialize(void) {
  s_valid_session = &s_session;
  s_free_count = 0;
  fake_kernel_malloc_init();
  fake_kernel_malloc_enable_stats(true);
  fake_kernel_malloc_mark();
  s_session = (const CommSession) {};
}

void test_session_send_queue__cleanup(void) {
  if (s_valid_session) {
    comm_session_send_queue_cleanup(s_valid_session);
  }

  // Check for leaks:
  fake_kernel_malloc_mark_assert_equal();
  fake_kernel_malloc_deinit();
}

void test_session_send_queue__get_length_returns_summed_length_of_all_jobs(void) {
  cl_assert_equal_i(0, comm_session_send_queue_get_length(s_valid_session));

  int num_jobs = 3;
  for (int i = 0; i < num_jobs; ++i) {
    SessionSendQueueJob *job = prv_create_test_job(TEST_DATA, sizeof(TEST_DATA));
    comm_session_send_queue_add_job(s_valid_session, &job);
    cl_assert(job);
    cl_assert_equal_i((i + 1) * sizeof(TEST_DATA),
                      comm_session_send_queue_get_length(s_valid_session));
  }
}

static void prv_add_jobs(int num_jobs) {
  for (int i = 0; i < num_jobs; ++i) {
    SessionSendQueueJob *job = prv_create_test_job(TEST_DATA, sizeof(TEST_DATA));
    comm_session_send_queue_add_job(s_valid_session, &job);
    cl_assert(job);
  }
}

void test_session_send_queue__copy_empty_queue(void) {
  uint8_t data_out[2];
  cl_assert_equal_i(0,
                    comm_session_send_queue_copy(s_valid_session, 0, sizeof(data_out), data_out));
}

void test_session_send_queue__copy_less_than_head_job_zero_offset(void) {
  int num_jobs = 3;
  prv_add_jobs(num_jobs);

  uint8_t data_out[2];
  memset(data_out, 0, sizeof(data_out));
  cl_assert_equal_i(sizeof(data_out),
                    comm_session_send_queue_copy(s_valid_session, 0, sizeof(data_out), data_out));
  cl_assert_equal_m(data_out, TEST_DATA, sizeof(data_out));
}

void test_session_send_queue__copy_less_than_head_job_with_offset_shorter_than_job(void) {
  int num_jobs = 3;
  prv_add_jobs(num_jobs);

  uint8_t data_out[1];
  memset(data_out, 0, sizeof(data_out));
  int offset = 1;
  cl_assert_equal_i(sizeof(data_out),
                    comm_session_send_queue_copy(s_valid_session, offset,
                                                 sizeof(data_out), data_out));
  cl_assert_equal_m(data_out, TEST_DATA + offset, sizeof(data_out));
}

void test_session_send_queue__copy_less_than_head_job_with_offset_longer_than_job(void) {
  int num_jobs = 3;
  prv_add_jobs(num_jobs);

  uint8_t data_out[sizeof(TEST_DATA) - 1];
  memset(data_out, 0, sizeof(data_out));
  int offset = sizeof(TEST_DATA) + 1;
  cl_assert_equal_i(sizeof(data_out),
                    comm_session_send_queue_copy(s_valid_session, offset,
                                                 sizeof(data_out), data_out));
  cl_assert_equal_m(data_out, TEST_DATA + (offset % sizeof(TEST_DATA)), sizeof(data_out));
}

void test_session_send_queue__copy_overlapping_multiple_jobs_with_offset(void) {
  int num_jobs = 3;
  prv_add_jobs(num_jobs);

  uint8_t data_out[2 * sizeof(TEST_DATA)];
  memset(data_out, 0, sizeof(data_out));
  int offset = 1;
  cl_assert_equal_i(sizeof(data_out),
                    comm_session_send_queue_copy(s_valid_session, offset,
                                                 sizeof(data_out), data_out));
  for (int i = 0; i < 2; ++i) {
    cl_assert_equal_m(data_out + (i * sizeof(TEST_DATA)),
                      TEST_DATA + offset, sizeof(TEST_DATA) - offset);
    cl_assert_equal_m(data_out + ((i + 1) * sizeof(TEST_DATA)) - offset,
                      TEST_DATA, offset);
  }
}

void test_session_send_queue__get_read_pointer(void) {
  cl_assert_equal_i(s_free_count, 0);

  int num_jobs = 3;
  prv_add_jobs(num_jobs);
  const uint8_t *data_out = NULL;

  for (int consumed = 0; consumed < sizeof(TEST_DATA); ++consumed) {
    cl_assert_equal_i(comm_session_send_queue_get_read_pointer(s_valid_session, &data_out),
                      sizeof(TEST_DATA) - consumed);
    cl_assert_equal_m(data_out, TEST_DATA + consumed, sizeof(TEST_DATA) - consumed);

    comm_session_send_queue_consume(s_valid_session, 1);
  }

  // Expect head to be free'd:
  cl_assert_equal_i(s_free_count, 1);

  // Next job can be read:
  cl_assert_equal_i(comm_session_send_queue_get_read_pointer(s_valid_session, &data_out),
                    sizeof(TEST_DATA));
  cl_assert_equal_m(data_out, TEST_DATA, sizeof(TEST_DATA));
}

void test_session_send_queue__get_read_pointer_no_jobs(void) {
  const uint8_t *data_out = NULL;
  cl_assert_equal_i(0, comm_session_send_queue_get_read_pointer(s_valid_session, &data_out));
}

void test_session_send_queue__consume_more_than_one_job(void) {
  int num_jobs = 3;
  prv_add_jobs(num_jobs);

  size_t consumed = sizeof(TEST_DATA) + 1;
  comm_session_send_queue_consume(s_valid_session, consumed);

  // Expect head to be free'd:
  cl_assert_equal_i(s_free_count, 1);

  cl_assert_equal_i((num_jobs * sizeof(TEST_DATA)) - consumed,
                    comm_session_send_queue_get_length(s_valid_session));
}

void test_session_send_queue__consume_all(void) {
  int num_jobs = 3;
  prv_add_jobs(num_jobs);

  comm_session_send_queue_consume(s_valid_session, UINT32_MAX);

  cl_assert_equal_i(s_free_count, num_jobs);
}

void test_session_send_queue__cleanup_calls_free_on_all_jobs(void) {
  cl_assert_equal_i(s_free_count, 0);

  // Add a couple jobs:
  int num_jobs = 3;
  prv_add_jobs(num_jobs);

  // When the session is disconnected, comm_session_send_queue_cleanup() is called:
  comm_session_send_queue_cleanup(s_valid_session);

  cl_assert_equal_i(s_free_count, num_jobs);
}

void test_session_send_queue__session_closed_when_add_is_called(void) {
  s_valid_session = NULL;

  SessionSendQueueJob *job = prv_create_test_job(TEST_DATA, sizeof(TEST_DATA));
  comm_session_send_queue_add_job(s_valid_session, &job);
  cl_assert(!job);
  cl_assert_equal_i(s_free_count, 1);
}
