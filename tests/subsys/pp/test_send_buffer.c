/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <errno.h>

#include <pbl/kernel/sem.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/transport.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/size.h>

#include <clar.h>

// Fakes
///////////////////////////////////////////////////////////

#include <fake_kernel_malloc.h>
#include <fake_rtc.h>
#include <fake_sem.h>

static struct pbl_pp_session s_session;
static struct pbl_pp_session *s_valid_session;
static int s_lock_depth;
static int s_send_next_count;
static bool s_in_send_task;

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
  ++s_send_next_count;
}

bool pbl_pp__in_send_task(void) {
  return s_in_send_task;
}

void pbl_pp__send_next_now(struct pbl_pp_session *session) {
  // Pretend to send out all the data:
  pbl_pp_session_send_queue_consume(session, pbl_pp_session_send_queue_length(session));
}

// Helpers
///////////////////////////////////////////////////////////

static const uint16_t ENDPOINT_ID = 1234;
static const uint32_t TIMEOUT_MS = 500;

static void prv_drop(void) {
  pbl_pp__lock();
  pbl_pp__send_queue_drop(&s_session);
  pbl_pp__unlock();
}

static pbl_tick_t prv_session_closed_yield_cb(struct pbl_sem *handle) {
  if (s_valid_session) {
    prv_drop();
    s_valid_session = nullptr;
  }
  return 10;
}

static pbl_tick_t prv_receive_but_no_bytes_freed_yield_cb(struct pbl_sem *handle) {
  fake_rtc_increment_ticks(100);
  pbl_sem_give(handle);
  return 100;
}

static struct pbl_pp_send_buffer *prv_fill(bool bulk, size_t length) {
  struct pbl_pp_send_buffer *sb =
      bulk ? pbl_pp_send_buffer_begin_bulk(&s_session, ENDPOINT_ID, length, TIMEOUT_MS)
           : pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, length, TIMEOUT_MS);
  cl_assert(sb);
  uint8_t fake_data[length];
  memset(fake_data, 0, length);
  cl_assert_equal_b(pbl_pp_send_buffer_write(sb, fake_data, length), true);
  pbl_pp_send_buffer_end(sb);
  return sb;
}

// Tests
///////////////////////////////////////////////////////////

void test_send_buffer__initialize(void) {
  s_session = (struct pbl_pp_session){};
  s_valid_session = &s_session;
  s_lock_depth = 0;
  s_send_next_count = 0;
  s_in_send_task = false;
  fake_kernel_malloc_init();
  fake_kernel_malloc_enable_stats(true);
  fake_kernel_malloc_mark();
}

void test_send_buffer__cleanup(void) {
  prv_drop();
  pbl_pp__send_buffer_reset();
  cl_assert_equal_i(s_lock_depth, 0);

  fake_kernel_malloc_mark_assert_equal();
  fake_kernel_malloc_deinit();
}

void test_send_buffer__null_session(void) {
  cl_assert_equal_p(nullptr, pbl_pp_send_buffer_begin(nullptr, ENDPOINT_ID, 1, TIMEOUT_MS));
  cl_assert_equal_i(pbl_pp_send_buffer_max_payload(nullptr), 0);
}

void test_send_buffer__closed_session(void) {
  s_valid_session = nullptr;
  cl_assert_equal_p(nullptr, pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 1, TIMEOUT_MS));
  cl_assert_equal_i(pbl_pp_send_buffer_max_payload(&s_session), 0);
}

void test_send_buffer__begin_with_more_than_max_payload(void) {
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  cl_assert(max_length > 0);
  cl_assert_equal_p(pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, max_length + 1, TIMEOUT_MS),
                    nullptr);
}

void test_send_buffer__send(void) {
  const uint8_t data[] = {1, 2, 3};

  cl_assert_equal_i(pbl_pp_send(&s_session, ENDPOINT_ID, data, sizeof(data), TIMEOUT_MS), 0);
  cl_assert_equal_i(s_send_next_count, 1);

  uint8_t out[sizeof(struct pbl_pp_header) + sizeof(data)];
  cl_assert_equal_i(pbl_pp_session_send_queue_length(&s_session), sizeof(out));
  cl_assert_equal_i(pbl_pp_session_send_queue_copy(&s_session, 0, sizeof(out), out), sizeof(out));
  const struct pbl_pp_header *header = (const struct pbl_pp_header *)out;
  cl_assert_equal_i(header->length, pbl_cpu_to_be16(sizeof(data)));
  cl_assert_equal_i(header->endpoint_id, pbl_cpu_to_be16(ENDPOINT_ID));
  cl_assert_equal_m(out + sizeof(*header), data, sizeof(data));
}

void test_send_buffer__send_errors(void) {
  const uint8_t data[] = {1, 2, 3};
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  uint8_t big[max_length + 1];

  cl_assert_equal_i(pbl_pp_send(&s_session, ENDPOINT_ID, big, sizeof(big), TIMEOUT_MS), -EMSGSIZE);

  struct pbl_pp_send_buffer *full_sb = prv_fill(false, max_length);
  (void)full_sb;
  fake_sem_set_yield_callback(pbl_pp__send_buffer_semaphore(),
                              prv_receive_but_no_bytes_freed_yield_cb);
  cl_assert_equal_i(pbl_pp_send(&s_session, ENDPOINT_ID, data, sizeof(data), TIMEOUT_MS), -EAGAIN);

  s_valid_session = nullptr;
  cl_assert_equal_i(pbl_pp_send(&s_session, ENDPOINT_ID, data, sizeof(data), TIMEOUT_MS),
                    -ENOTCONN);
  cl_assert_equal_i(pbl_pp_send(nullptr, ENDPOINT_ID, data, sizeof(data), TIMEOUT_MS), -ENOTCONN);
  s_valid_session = &s_session;
}

void test_send_buffer__not_enough_space_in_time(void) {
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  prv_fill(false, max_length);

  fake_sem_set_yield_callback(pbl_pp__send_buffer_semaphore(),
                              prv_receive_but_no_bytes_freed_yield_cb);

  cl_assert_equal_p(pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 1, TIMEOUT_MS), nullptr);
}

void test_send_buffer__multiple_smaller_messages(void) {
  size_t bytes_free = pbl_pp_send_buffer_max_payload(&s_session) + sizeof(struct pbl_pp_header);
  int count = 0;

  while (bytes_free > 0) {
    size_t payload_length = 1;

    bytes_free -= sizeof(struct pbl_pp_header) + payload_length;
    // If another message cannot fit after this one, use up the space:
    if (bytes_free <= (sizeof(struct pbl_pp_header) + payload_length)) {
      payload_length += bytes_free;
      bytes_free = 0;
    }

    prv_fill(false, payload_length);
    ++count;
  }
  cl_assert(count > 1);

  cl_assert_equal_p(pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 1, 0), nullptr);
}

void test_send_buffer__not_enough_space_in_send_task(void) {
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  prv_fill(false, max_length);

  // The send_next callback is queued behind the caller, so the data is sent at once.
  s_in_send_task = true;
  fake_sem_set_yield_callback(pbl_pp__send_buffer_semaphore(),
                              prv_receive_but_no_bytes_freed_yield_cb);

  struct pbl_pp_send_buffer *sb = pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 1, TIMEOUT_MS);
  cl_assert(sb);
  pbl_pp_send_buffer_end(sb);
  cl_assert_equal_i(pbl_pp_session_send_queue_length(&s_session), sizeof(struct pbl_pp_header));
}

void test_send_buffer__writing_but_then_session_closed(void) {
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  prv_fill(false, max_length);

  fake_sem_set_yield_callback(pbl_pp__send_buffer_semaphore(), prv_session_closed_yield_cb);

  cl_assert_equal_p(pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 1, TIMEOUT_MS), nullptr);
}

void test_send_buffer__end_after_session_closed_frees(void) {
  struct pbl_pp_send_buffer *sb = pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 1, TIMEOUT_MS);
  cl_assert(sb);
  s_valid_session = nullptr;
  pbl_pp_send_buffer_end(sb);
  cl_assert_equal_i(s_send_next_count, 0);
  s_valid_session = &s_session;
  cl_assert_equal_i(pbl_pp_session_send_queue_length(&s_session), 0);
}

void test_send_buffer__write_beyond_available_space(void) {
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  struct pbl_pp_send_buffer *sb =
      pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, max_length, TIMEOUT_MS);
  uint8_t fake_data[max_length];
  memset(fake_data, 0, max_length);
  cl_assert_equal_b(pbl_pp_send_buffer_write(sb, fake_data, max_length), true);
  cl_assert_equal_b(pbl_pp_send_buffer_write(sb, fake_data, 1), false);
  pbl_pp_send_buffer_end(sb);
}

void test_send_buffer__send_queue_interface(void) {
  const size_t max_payload_length = pbl_pp_send_buffer_max_payload(&s_session);
  struct pbl_pp_send_buffer *sb =
      pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, max_payload_length, TIMEOUT_MS);
  uint8_t payload[max_payload_length];
  for (size_t i = 0; i < max_payload_length; ++i) {
    payload[i] = i % 0xff;
  }

  // Write in two parts:
  const size_t first = max_payload_length / 2;
  cl_assert_equal_b(pbl_pp_send_buffer_write(sb, payload, first), true);
  cl_assert_equal_b(pbl_pp_send_buffer_write(sb, payload + first, max_payload_length - first),
                    true);

  cl_assert_equal_i(s_send_next_count, 0);
  pbl_pp_send_buffer_end(sb);
  cl_assert_equal_i(s_send_next_count, 1);

  const size_t total = max_payload_length + sizeof(struct pbl_pp_header);
  cl_assert_equal_i(pbl_pp_session_send_queue_length(&s_session), total);

  uint8_t out[total];
  cl_assert_equal_i(pbl_pp_session_send_queue_copy(&s_session, 0, total, out), total);
  const struct pbl_pp_header *header = (const struct pbl_pp_header *)out;
  cl_assert_equal_i(header->length, pbl_cpu_to_be16(max_payload_length));
  cl_assert_equal_i(header->endpoint_id, pbl_cpu_to_be16(ENDPOINT_ID));
  cl_assert_equal_m(out + sizeof(*header), payload, max_payload_length);

  // With an offset:
  const size_t offset = 2;
  cl_assert_equal_i(pbl_pp_session_send_queue_copy(&s_session, offset, total, out), total - offset);
  cl_assert_equal_m(out + sizeof(*header) - offset, payload, max_payload_length);

  // Peek and consume:
  size_t bytes_read = 0;
  size_t read_space;
  const uint8_t *data_out;
  while ((read_space = pbl_pp_session_send_queue_peek(&s_session, &data_out)) > 0) {
    if (bytes_read == 0) {
      cl_assert(read_space >= sizeof(struct pbl_pp_header));
      cl_assert_equal_m(data_out + sizeof(struct pbl_pp_header), payload,
                        read_space - sizeof(struct pbl_pp_header));
    } else {
      cl_assert_equal_m(data_out, payload + bytes_read - sizeof(struct pbl_pp_header), read_space);
    }
    pbl_pp_session_send_queue_consume(&s_session, read_space);
    bytes_read += read_space;
  }
  cl_assert_equal_i(bytes_read, total);
  cl_assert_equal_i(pbl_pp_session_send_queue_length(&s_session), 0);
}

void test_send_buffer__bulk_leaves_room_for_interactive(void) {
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  prv_fill(true, max_length);

  fake_sem_set_yield_callback(pbl_pp__send_buffer_bulk_semaphore(),
                              prv_receive_but_no_bytes_freed_yield_cb);

  // A small interactive message gets a buffer right away...
  struct pbl_pp_send_buffer *sb = pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 19, 0);
  cl_assert(sb);
  pbl_pp_send_buffer_end(sb);

  // ...while more bulk data has to wait for the transfer to drain.
  cl_assert_equal_p(pbl_pp_send_buffer_begin_bulk(&s_session, ENDPOINT_ID, 1, TIMEOUT_MS), nullptr);
}

void test_send_buffer__interactive_limit_is_unchanged(void) {
  const size_t max_length = pbl_pp_send_buffer_max_payload(&s_session);
  prv_fill(false, max_length);

  fake_sem_set_yield_callback(pbl_pp__send_buffer_semaphore(),
                              prv_receive_but_no_bytes_freed_yield_cb);
  cl_assert_equal_p(pbl_pp_send_buffer_begin(&s_session, ENDPOINT_ID, 1, TIMEOUT_MS), nullptr);
}

void test_send_buffer__freeing_wakes_both_classes(void) {
  prv_fill(true, 10);
  prv_drop();

  // A bulk sender retrying must not take the only wakeup an interactive sender waits for.
  cl_assert_equal_i(pbl_sem_take(pbl_pp__send_buffer_semaphore(), PBL_NO_WAIT), 0);
  cl_assert_equal_i(pbl_sem_take(pbl_pp__send_buffer_bulk_semaphore(), PBL_NO_WAIT), 0);
}
