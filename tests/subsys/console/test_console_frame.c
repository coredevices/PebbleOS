/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <string.h>

#include <pbl/console/console.h>
#include <pbl/crc/crc.h>
#include <pbl/kernel/work.h>
#include <pbl/util/base64.h>

#include <console/console_private.h>

static char s_text[512];
static size_t s_text_len;

static uint8_t s_pp[PBL_CONSOLE_FRAME_MTU];
static size_t s_pp_len;
static int s_pp_count;

static int s_acked = -1;

static char s_tx[2048];
static size_t s_tx_len;

static bool s_task_queue_full;
static pbl_work_call_fn_t s_task_cb;
static pbl_work_call_fn_t s_timer_work_cb;

void pbl_console_lock(void) {
}

void pbl_console_unlock(void) {
}

void pbl_console_putchar(char c) {
  s_tx[s_tx_len++] = c;
}

void pbl_console_flush(void) {
}

bool pbl_sys_work_call_from_isr(pbl_work_call_fn_t cb, void *data) {
  if (s_task_queue_full) {
    return false;
  }
  s_task_cb = cb;
  return true;
}

void pbl_sys_work_call_high_from_isr(pbl_work_call_fn_t cb, void *data) {
  s_timer_work_cb = cb;
}

void pbl_pp_console_receive(const void *data, size_t len) {
  memcpy(s_pp, data, len);
  s_pp_len = len;
  s_pp_count++;
}

void pbl_pp_console_ack(uint8_t seq) {
  s_acked = seq;
}

void pbl_console__imaging_handle_frame(const void *data, size_t len) {
}

static void prv_text_input(char c) {
  if (s_text_len < sizeof(s_text)) {
    s_text[s_text_len++] = c;
  }
}

static void prv_feed(const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    pbl_console__frame_input_from_isr(data[i], prv_text_input);
  }
}

static void prv_feed_str(const char *str) {
  prv_feed(str, strlen(str));
}

static void prv_run_task(void) {
  if (s_timer_work_cb != nullptr) {
    pbl_work_call_fn_t cb = s_timer_work_cb;
    s_timer_work_cb = nullptr;
    cb(nullptr);
  }
  if (s_task_cb != nullptr) {
    pbl_work_call_fn_t cb = s_task_cb;
    s_task_cb = nullptr;
    cb(nullptr);
  }
}

static size_t prv_encode(char *out, char channel, uint8_t seq, const void *payload, size_t len) {
  uint8_t raw[PBL_CONSOLE_FRAME_MTU + 5];
  size_t pos = 0;

  raw[0] = seq;
  memcpy(&raw[1], payload, len);
  const uint32_t crc = pbl_crc32(pbl_crc32(0, &channel, 1), raw, 1 + len);
  memcpy(&raw[1 + len], &crc, sizeof(crc));

  out[pos++] = 0x1b;
  out[pos++] = '_';
  out[pos++] = channel;
  pos += pbl_base64_encode(&out[pos], 1024, raw, 1 + len + sizeof(crc));
  out[pos++] = 0x1b;
  out[pos++] = '\\';
  return pos;
}

static void prv_feed_frame(uint8_t seq, const char *payload) {
  char frame[1024];
  const size_t len = prv_encode(frame, PBL_CONSOLE_CHANNEL_PP, seq, payload, strlen(payload));

  prv_feed(frame, len);
  prv_run_task();
}

static void prv_assert_ack(uint8_t seq) {
  char expected[64];
  const size_t len = prv_encode(expected, CONSOLE_FRAME_CHANNEL_ACK, 0, &seq, sizeof(seq));

  cl_assert_equal_i(s_tx_len, len);
  cl_assert(memcmp(s_tx, expected, len) == 0);
  s_tx_len = 0;
}

void test_console_frame__initialize(void) {
  s_text_len = 0;
  s_pp_len = 0;
  s_pp_count = 0;
  s_tx_len = 0;
  s_task_queue_full = false;
  s_task_cb = nullptr;
  s_acked = -1;
  // Leaves the sequence number state from the previous test behind.
  prv_feed_frame(0, "reset");
  s_pp_count = 0;
  s_tx_len = 0;
}

void test_console_frame__text_passes_through(void) {
  prv_feed_str("help\r");

  cl_assert_equal_i(s_text_len, 5);
  cl_assert(memcmp(s_text, "help\r", 5) == 0);
  cl_assert_equal_i(s_pp_count, 0);
}

void test_console_frame__escape_sequences_reach_the_shell(void) {
  prv_feed_str("\x1b[A\x1b\x1b[B");

  cl_assert_equal_i(s_text_len, 7);
  cl_assert(memcmp(s_text, "\x1b[A\x1b\x1b[B", 7) == 0);
}

void test_console_frame__frame_is_delivered_and_acknowledged(void) {
  prv_feed_frame(7, "payload");

  cl_assert_equal_i(s_pp_count, 1);
  cl_assert_equal_i(s_pp_len, 7);
  cl_assert(memcmp(s_pp, "payload", 7) == 0);
  cl_assert_equal_i(s_text_len, 0);
  prv_assert_ack(7);
}

void test_console_frame__largest_frame(void) {
  char frame[1024];
  char payload[PBL_CONSOLE_FRAME_MTU];

  memset(payload, 'x', sizeof(payload));
  prv_feed(frame, prv_encode(frame, PBL_CONSOLE_CHANNEL_PP, 1, payload, sizeof(payload)));
  prv_run_task();

  cl_assert_equal_i(s_pp_count, 1);
  cl_assert_equal_i(s_pp_len, PBL_CONSOLE_FRAME_MTU);
  prv_assert_ack(1);
}

void test_console_frame__repeat_is_acknowledged_but_not_handled(void) {
  prv_feed_frame(9, "one");
  prv_assert_ack(9);
  prv_feed_frame(9, "one");
  prv_assert_ack(9);
  cl_assert_equal_i(s_pp_count, 1);

  prv_feed_frame(10, "two");
  prv_assert_ack(10);
  cl_assert_equal_i(s_pp_count, 2);
}

void test_console_frame__seq_0_is_always_handled(void) {
  prv_feed_frame(0, "open");
  prv_feed_frame(0, "open");

  cl_assert_equal_i(s_pp_count, 2);
}

void test_console_frame__bad_crc_is_dropped_silently(void) {
  char frame[64];
  const size_t len = prv_encode(frame, PBL_CONSOLE_CHANNEL_PP, 3, "abc", 3);

  frame[4] = (frame[4] == 'A') ? 'B' : 'A';
  prv_feed(frame, len);
  prv_run_task();

  cl_assert_equal_i(s_pp_count, 0);
  cl_assert_equal_i(s_tx_len, 0);

  // The repeat gets through.
  prv_feed_frame(3, "abc");
  prv_assert_ack(3);
  cl_assert_equal_i(s_pp_count, 1);
}

void test_console_frame__not_a_frame_goes_back_to_text(void) {
  prv_feed_str("\x1b_Pabc\rhelp\r");

  // What was taken for a frame is gone, what follows is text.
  cl_assert_equal_i(s_text_len, 5);
  cl_assert(memcmp(s_text, "help\r", 5) == 0);
  cl_assert_equal_i(s_pp_count, 0);
}

void test_console_frame__bad_terminator_goes_back_to_text(void) {
  prv_feed_str("\x1b_PAAAA\x1bxhelp\r");

  cl_assert_equal_i(s_text_len, 5);
  cl_assert(memcmp(s_text, "help\r", 5) == 0);
  cl_assert_equal_i(s_pp_count, 0);
}

void test_console_frame__oversized_frame_goes_back_to_text(void) {
  prv_feed_str("\x1b_P");
  for (int i = 0; i < 2000; i++) {
    prv_feed_str("A");
  }
  // Past the buffer, so these count as text.
  cl_assert(s_text_len > 0);
  s_text_len = 0;

  prv_feed_str("help\r");
  cl_assert_equal_i(s_text_len, 5);
  cl_assert_equal_i(s_pp_count, 0);
}

void test_console_frame__busy_frame_is_dropped(void) {
  char frame[64];
  const size_t len = prv_encode(frame, PBL_CONSOLE_CHANNEL_PP, 4, "abc", 3);

  // A frame that arrives while the previous one waits to be handled.
  prv_feed(frame, len);
  char other[64];
  const size_t other_len = prv_encode(other, PBL_CONSOLE_CHANNEL_PP, 5, "xyz", 3);
  prv_feed(other, other_len);
  prv_run_task();

  cl_assert_equal_i(s_pp_count, 1);
  cl_assert(memcmp(s_pp, "abc", 3) == 0);
  prv_assert_ack(4);

  // The host repeats it, as it was not acknowledged.
  prv_feed(other, other_len);
  prv_run_task();
  cl_assert_equal_i(s_pp_count, 2);
  prv_assert_ack(5);
}

void test_console_frame__largest_frame_survives_a_busy_frame(void) {
  char frame[1024];
  char payload[PBL_CONSOLE_FRAME_MTU];
  char other[64];

  memset(payload, 'x', sizeof(payload));
  prv_feed(frame, prv_encode(frame, PBL_CONSOLE_CHANNEL_PP, 1, payload, sizeof(payload)));
  prv_feed(other, prv_encode(other, PBL_CONSOLE_CHANNEL_PP, 2, "xyz", 3));
  prv_run_task();

  cl_assert_equal_i(s_text_len, 0);
  cl_assert_equal_i(s_pp_count, 1);
  cl_assert_equal_i(s_pp_len, PBL_CONSOLE_FRAME_MTU);
  prv_assert_ack(1);
}

void test_console_frame__full_task_queue_drops_the_frame(void) {
  s_task_queue_full = true;
  prv_feed_frame(6, "abc");
  cl_assert_equal_i(s_pp_count, 0);

  s_task_queue_full = false;
  prv_feed_frame(6, "abc");
  cl_assert_equal_i(s_pp_count, 1);
  prv_assert_ack(6);
}

void test_console_frame__sends_decodable_frame(void) {
  char expected[64];

  pbl_console_frame_send(PBL_CONSOLE_CHANNEL_PP, 9, "abc", 3);

  const size_t len = prv_encode(expected, PBL_CONSOLE_CHANNEL_PP, 9, "abc", 3);
  cl_assert_equal_i(s_tx_len, len);
  cl_assert(memcmp(s_tx, expected, len) == 0);
}

static size_t prv_encode_ack(char *out, uint8_t seq) {
  return prv_encode(out, CONSOLE_FRAME_CHANNEL_ACK, 0, &seq, sizeof(seq));
}

void test_console_frame__ack_reaches_the_transport_unacknowledged(void) {
  char ack[64];
  const size_t len = prv_encode_ack(ack, 42);

  s_tx_len = 0;
  prv_feed(ack, len);
  prv_run_task();

  cl_assert_equal_i(s_acked, 42);
  cl_assert_equal_i(s_tx_len, 0);
}

void test_console_frame__ack_gets_through_while_a_frame_waits(void) {
  char frame[64];
  char ack[64];
  const size_t frame_len = prv_encode(frame, PBL_CONSOLE_CHANNEL_PP, 3, "abc", 3);
  const size_t ack_len = prv_encode_ack(ack, 42);

  prv_feed(frame, frame_len);
  pbl_work_call_fn_t frame_cb = s_task_cb;
  prv_feed(ack, ack_len);
  prv_run_task();
  cl_assert_equal_i(s_acked, 42);

  frame_cb(nullptr);
  cl_assert_equal_i(s_pp_count, 1);
}

void test_console_frame__damaged_ack_is_ignored(void) {
  char ack[64];
  const size_t len = prv_encode_ack(ack, 42);

  // Flip a base64 character.
  ack[3] = (ack[3] == 'A') ? 'B' : 'A';
  prv_feed(ack, len);
  prv_run_task();

  cl_assert_equal_i(s_acked, -1);
}

void test_console_frame__short_ack_is_ignored(void) {
  char ack[64];
  const size_t len = prv_encode_ack(ack, 42);

  memmove(&ack[3], &ack[7], len - 7);
  prv_feed(ack, len - 4);
  prv_run_task();

  cl_assert_equal_i(s_acked, -1);
}
