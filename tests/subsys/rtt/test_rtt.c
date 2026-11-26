/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/rtt/rtt.h>

#include <clar.h>

void pbl_rtt_reset_for_test(void);

// Fakes
///////////////////////////////////////////////////////////

static uint32_t s_waited_us;
// The probe reads this many bytes of channel 0 each time the target waits
static size_t s_read_while_waiting;

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

// What a probe does
static size_t prv_probe_read(unsigned int ch, char *buf, size_t max) {
  struct pbl_rtt_buffer *up = &_SEGGER_RTT.up[ch];
  size_t n = 0U;

  while (n < max && up->rd != up->wr) {
    buf[n++] = (char)up->buf[up->rd];
    up->rd = (up->rd + 1U) % up->size;
  }
  return n;
}

static void prv_probe_send(const char *data) {
  struct pbl_rtt_buffer *down = &_SEGGER_RTT.down[0];

  for (size_t i = 0; data[i] != '\0'; i++) {
    down->buf[down->wr] = (uint8_t)data[i];
    down->wr = (down->wr + 1U) % down->size;
  }
}

void pbl_busy_wait_us(uint32_t us) {
  char buf[8];

  s_waited_us += us;
  (void)prv_probe_read(0U, buf, s_read_while_waiting);
}

// Tests
///////////////////////////////////////////////////////////

void test_rtt__initialize(void) {
  pbl_rtt_reset_for_test();
  s_waited_us = 0U;
  s_read_while_waiting = 0U;
}

void test_rtt__sets_up_a_control_block_a_probe_finds(void) {
  cl_assert_equal_s(_SEGGER_RTT.id, "");

  pbl_rtt_init();

  cl_assert_equal_s(_SEGGER_RTT.id, "SEGGER RTT");
  cl_assert_equal_i(_SEGGER_RTT.max_up, 2);
  cl_assert_equal_i(_SEGGER_RTT.max_down, 1);
  cl_assert_equal_s(_SEGGER_RTT.up[0].name, "Terminal");
  cl_assert_equal_s(_SEGGER_RTT.down[0].name, "Terminal");
  cl_assert_equal_i(_SEGGER_RTT.up[1].size, 8);
  cl_assert(_SEGGER_RTT.up[0].buf != _SEGGER_RTT.up[1].buf);
  cl_assert_equal_i(_SEGGER_RTT.down[0].size, 8);
}

void test_rtt__writes_set_the_block_up(void) {
  char out[8];

  cl_assert_equal_i(pbl_rtt_write(1U, "ab", 2U), 2);

  cl_assert_equal_s(_SEGGER_RTT.id, "SEGGER RTT");
  cl_assert_equal_i(prv_probe_read(1U, out, sizeof(out)), 2);
  cl_assert(memcmp(out, "ab", 2) == 0);
  cl_assert_equal_i(prv_probe_read(0U, out, sizeof(out)), 0);
}

void test_rtt__drops_a_write_that_does_not_fit_whole(void) {
  char out[8];

  // One slot stays empty
  cl_assert_equal_i(pbl_rtt_write(0U, "abcde", 5U), 5);
  cl_assert_equal_i(pbl_rtt_write(0U, "fgh", 3U), 0);
  cl_assert_equal_i(pbl_rtt_write(0U, "fg", 2U), 2);
  cl_assert_equal_i(pbl_rtt_write(0U, "h", 1U), 0);

  cl_assert_equal_i(prv_probe_read(0U, out, sizeof(out)), 7);
  cl_assert(memcmp(out, "abcdefg", 7) == 0);

  // Across the end of the buffer
  cl_assert_equal_i(pbl_rtt_write(0U, "xyz", 3U), 3);
  cl_assert_equal_i(prv_probe_read(0U, out, sizeof(out)), 3);
  cl_assert(memcmp(out, "xyz", 3) == 0);
}

void test_rtt__ignores_missing_channels(void) {
  char in[4];

  cl_assert_equal_i(pbl_rtt_write(2U, "a", 1U), 0);
  cl_assert_equal_i(pbl_rtt_read(1U, in, sizeof(in)), 0);
  cl_assert(pbl_rtt_flush(2U, 1000U));
}

void test_rtt__reads_what_the_probe_sent(void) {
  char in[8];

  cl_assert_equal_i(pbl_rtt_read(0U, in, sizeof(in)), 0);

  prv_probe_send("hello");
  cl_assert_equal_i(pbl_rtt_read(0U, in, 2U), 2);
  cl_assert(memcmp(in, "he", 2) == 0);
  cl_assert_equal_i(pbl_rtt_read(0U, in, sizeof(in)), 3);
  cl_assert(memcmp(in, "llo", 3) == 0);
  cl_assert_equal_i(pbl_rtt_read(0U, in, sizeof(in)), 0);
}

void test_rtt__flush_waits_only_for_a_probe_seen_reading(void) {
  char out[8];

  // No probe has read: nothing to wait for
  cl_assert_equal_i(pbl_rtt_write(0U, "a", 1U), 1);
  cl_assert(!pbl_rtt_flush(0U, 1000U));
  cl_assert_equal_i(s_waited_us, 0);

  // One that has read is waited for until it catches up
  (void)prv_probe_read(0U, out, sizeof(out));
  cl_assert_equal_i(pbl_rtt_write(0U, "bcd", 3U), 3);
  s_read_while_waiting = 1U;
  cl_assert(pbl_rtt_flush(0U, 1000U));
  cl_assert_equal_i(s_waited_us, 300);

  // Empty: nothing to wait for
  s_waited_us = 0U;
  cl_assert(pbl_rtt_flush(0U, 1000U));
  cl_assert_equal_i(s_waited_us, 0);
}

void test_rtt__flush_stops_waiting_for_a_probe_gone(void) {
  char out[8];

  cl_assert_equal_i(pbl_rtt_write(0U, "a", 1U), 1);
  (void)prv_probe_read(0U, out, sizeof(out));

  // The probe stops reading: waited for once, up to the timeout
  cl_assert_equal_i(pbl_rtt_write(0U, "b", 1U), 1);
  cl_assert(!pbl_rtt_flush(0U, 1000U));
  cl_assert_equal_i(s_waited_us, 1000);

  // Then gone
  s_waited_us = 0U;
  cl_assert_equal_i(pbl_rtt_write(0U, "c", 1U), 1);
  cl_assert(!pbl_rtt_flush(0U, 1000U));
  cl_assert_equal_i(s_waited_us, 0);

  // Until it reads again
  (void)prv_probe_read(0U, out, sizeof(out));
  cl_assert_equal_i(pbl_rtt_write(0U, "d", 1U), 1);
  s_read_while_waiting = 1U;
  cl_assert(pbl_rtt_flush(0U, 1000U));
  cl_assert_equal_i(s_waited_us, 100);
}
