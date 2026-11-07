/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/kernel/malloc.h>
#include <pbl/pp/send.h>
#include <pbl/pp/transport.h>
#include <pbl/util/math.h>

#include <clar.h>

// Fakes
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <fake_kernel_malloc.h>

static struct pbl_pp_session s_session;
static struct pbl_pp_session *s_valid_session;
static int s_lock_depth;
static int s_send_next_count;

void pbl_pp__lock(void) {
  ++s_lock_depth;
}

void pbl_pp__unlock(void) {
  cl_assert(s_lock_depth > 0);
  --s_lock_depth;
}

void pbl_pp__assert_locked(void) {
  cl_assert(s_lock_depth > 0);
}

bool pbl_pp__session_is_open(const struct pbl_pp_session *session) {
  return (session != nullptr) && (session == s_valid_session);
}

void pbl_pp_session_send_next(struct pbl_pp_session *session) {
  cl_assert_equal_i(s_lock_depth, 0);
  ++s_send_next_count;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

struct test_job {
  struct pbl_pp_send_job job;
  size_t consumed_length;
  size_t length;
  uint8_t data[];
};

static size_t prv_get_length(const struct test_job *job) {
  return job->length - job->consumed_length;
}

static const uint8_t *prv_get_read_pointer(const struct test_job *job) {
  return job->data + job->consumed_length;
}

static size_t prv_job_get_length(const struct pbl_pp_send_job *send_job) {
  cl_assert(s_lock_depth > 0);
  return prv_get_length((const struct test_job *)send_job);
}

static size_t prv_job_copy(const struct pbl_pp_send_job *send_job, size_t offset, size_t length,
                           uint8_t *data_out) {
  const struct test_job *job = (const struct test_job *)send_job;
  const size_t length_to_copy = MIN(prv_get_length(job) - offset, length);

  memcpy(data_out, prv_get_read_pointer(job) + offset, length_to_copy);
  return length_to_copy;
}

static size_t prv_job_get_read_pointer(const struct pbl_pp_send_job *send_job,
                                       const uint8_t **data_out) {
  const struct test_job *job = (const struct test_job *)send_job;

  *data_out = prv_get_read_pointer(job);
  return prv_get_length(job);
}

static void prv_job_consume(const struct pbl_pp_send_job *send_job, size_t length) {
  ((struct test_job *)send_job)->consumed_length += length;
}

static int s_free_count;

static void prv_job_free(struct pbl_pp_send_job *send_job) {
  cl_assert(s_lock_depth > 0);
  pbl_free(send_job);
  ++s_free_count;
}

static const struct pbl_pp_send_job_api s_test_job_api = {
  .get_length = prv_job_get_length,
  .copy = prv_job_copy,
  .get_read_pointer = prv_job_get_read_pointer,
  .consume = prv_job_consume,
  .free = prv_job_free,
};

static struct pbl_pp_send_job *prv_create_test_job(const uint8_t *data, size_t length) {
  struct test_job *job = pbl_malloc(sizeof(*job) + length);
  cl_assert(job);
  *job = (struct test_job){
    .job = {.api = &s_test_job_api},
    .length = length,
  };
  memcpy(job->data, data, length);
  return &job->job;
}

static const uint8_t TEST_DATA[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

static void prv_add_jobs(int num_jobs) {
  for (int i = 0; i < num_jobs; ++i) {
    struct pbl_pp_send_job *job = prv_create_test_job(TEST_DATA, sizeof(TEST_DATA));
    pbl_pp_send_job_submit(s_valid_session, &job);
    cl_assert(job);
  }
}

static void prv_drop(void) {
  pbl_pp__lock();
  pbl_pp__send_queue_drop(&s_session);
  pbl_pp__unlock();
}

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

void test_send_queue__initialize(void) {
  s_session = (struct pbl_pp_session){};
  s_valid_session = &s_session;
  s_free_count = 0;
  s_lock_depth = 0;
  s_send_next_count = 0;
  fake_kernel_malloc_init();
  fake_kernel_malloc_enable_stats(true);
  fake_kernel_malloc_mark();
}

void test_send_queue__cleanup(void) {
  prv_drop();
  cl_assert_equal_i(s_lock_depth, 0);

  fake_kernel_malloc_mark_assert_equal();
  fake_kernel_malloc_deinit();
}

void test_send_queue__submit_kicks_transport(void) {
  prv_add_jobs(2);
  cl_assert_equal_i(s_send_next_count, 2);
}

void test_send_queue__length_is_sum_of_all_jobs(void) {
  cl_assert_equal_i(0, pbl_pp_session_send_queue_length(&s_session));

  for (int i = 0; i < 3; ++i) {
    prv_add_jobs(1);
    cl_assert_equal_i((i + 1) * sizeof(TEST_DATA), pbl_pp_session_send_queue_length(&s_session));
  }
}

void test_send_queue__copy_empty_queue(void) {
  uint8_t data_out[2];
  cl_assert_equal_i(0, pbl_pp_session_send_queue_copy(&s_session, 0, sizeof(data_out), data_out));
}

void test_send_queue__copy_less_than_head_job_zero_offset(void) {
  prv_add_jobs(3);

  uint8_t data_out[2] = {};
  cl_assert_equal_i(sizeof(data_out),
                    pbl_pp_session_send_queue_copy(&s_session, 0, sizeof(data_out), data_out));
  cl_assert_equal_m(data_out, TEST_DATA, sizeof(data_out));
}

void test_send_queue__copy_less_than_head_job_with_offset_shorter_than_job(void) {
  prv_add_jobs(3);

  uint8_t data_out[1] = {};
  const size_t offset = 1;
  cl_assert_equal_i(sizeof(data_out),
                    pbl_pp_session_send_queue_copy(&s_session, offset, sizeof(data_out), data_out));
  cl_assert_equal_m(data_out, TEST_DATA + offset, sizeof(data_out));
}

void test_send_queue__copy_with_offset_longer_than_job(void) {
  prv_add_jobs(3);

  uint8_t data_out[sizeof(TEST_DATA) - 1] = {};
  const size_t offset = sizeof(TEST_DATA) + 1;
  cl_assert_equal_i(sizeof(data_out),
                    pbl_pp_session_send_queue_copy(&s_session, offset, sizeof(data_out), data_out));
  cl_assert_equal_m(data_out, TEST_DATA + (offset % sizeof(TEST_DATA)), sizeof(data_out));
}

void test_send_queue__copy_overlapping_multiple_jobs_with_offset(void) {
  prv_add_jobs(3);

  uint8_t data_out[2 * sizeof(TEST_DATA)] = {};
  const size_t offset = 1;
  cl_assert_equal_i(sizeof(data_out),
                    pbl_pp_session_send_queue_copy(&s_session, offset, sizeof(data_out), data_out));
  for (int i = 0; i < 2; ++i) {
    cl_assert_equal_m(data_out + (i * sizeof(TEST_DATA)), TEST_DATA + offset,
                      sizeof(TEST_DATA) - offset);
    cl_assert_equal_m(data_out + ((i + 1) * sizeof(TEST_DATA)) - offset, TEST_DATA, offset);
  }
}

void test_send_queue__peek(void) {
  prv_add_jobs(3);
  const uint8_t *data_out = nullptr;

  for (size_t consumed = 0; consumed < sizeof(TEST_DATA); ++consumed) {
    cl_assert_equal_i(pbl_pp_session_send_queue_peek(&s_session, &data_out),
                      sizeof(TEST_DATA) - consumed);
    cl_assert_equal_m(data_out, TEST_DATA + consumed, sizeof(TEST_DATA) - consumed);
    pbl_pp_session_send_queue_consume(&s_session, 1);
  }

  cl_assert_equal_i(s_free_count, 1);

  cl_assert_equal_i(pbl_pp_session_send_queue_peek(&s_session, &data_out), sizeof(TEST_DATA));
  cl_assert_equal_m(data_out, TEST_DATA, sizeof(TEST_DATA));
}

void test_send_queue__peek_no_jobs(void) {
  const uint8_t *data_out = nullptr;
  cl_assert_equal_i(0, pbl_pp_session_send_queue_peek(&s_session, &data_out));
}

void test_send_queue__consume_more_than_one_job(void) {
  prv_add_jobs(3);

  const size_t consumed = sizeof(TEST_DATA) + 1;
  pbl_pp_session_send_queue_consume(&s_session, consumed);

  cl_assert_equal_i(s_free_count, 1);
  cl_assert_equal_i((3 * sizeof(TEST_DATA)) - consumed,
                    pbl_pp_session_send_queue_length(&s_session));
}

void test_send_queue__consume_all(void) {
  prv_add_jobs(3);

  pbl_pp_session_send_queue_consume(&s_session, SIZE_MAX);

  cl_assert_equal_i(s_free_count, 3);
  cl_assert_equal_i(0, pbl_pp_session_send_queue_length(&s_session));
}

void test_send_queue__drop_frees_all_jobs(void) {
  prv_add_jobs(3);

  prv_drop();

  cl_assert_equal_i(s_free_count, 3);
}

void test_send_queue__session_closed_when_submit_is_called(void) {
  s_valid_session = nullptr;

  struct pbl_pp_send_job *job = prv_create_test_job(TEST_DATA, sizeof(TEST_DATA));
  pbl_pp_send_job_submit(&s_session, &job);
  cl_assert_equal_p(job, nullptr);
  cl_assert_equal_i(s_free_count, 1);
  cl_assert_equal_i(s_send_next_count, 0);
}
