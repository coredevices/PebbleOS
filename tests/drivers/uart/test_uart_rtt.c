/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/uart/rtt.h>
#include <pbl/kernel/timer.h>
#include <pbl/rtt/rtt.h>

#include <clar.h>

void pbl_rtt_reset_for_test(void);

// Fakes
///////////////////////////////////////////////////////////

static bool s_in_isr;
static uint32_t s_waited_us;
static struct pbl_timer *s_timer;
static bool s_timer_active;

bool pbl_in_isr(void) {
  return s_in_isr;
}

void pbl_busy_wait_us(uint32_t us) {
  s_waited_us += us;
}

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

void pbl_timer_init(struct pbl_timer *timer, pbl_timer_fn_t fn) {
  *timer = (struct pbl_timer)PBL_TIMER_INITIALIZER(fn);
}

void pbl_timer_start(struct pbl_timer *timer, pbl_timeout_t delay) {
  s_timer = timer;
  s_timer_active = true;
}

void pbl_timer_stop(struct pbl_timer *timer) {
  s_timer_active = false;
}

static void prv_fire_timer(void) {
  cl_assert(s_timer_active);
  s_timer_active = false;
  s_timer->fn(s_timer);
}

static char s_rx[16];
static size_t s_rx_len;
static size_t s_rx_stop_after;

static void prv_rx(const struct pbl_uart *uart, uint8_t c, void *arg) {
  s_rx[s_rx_len++] = (char)c;
  if (s_rx_len == s_rx_stop_after) {
    pbl_uart_rx_disable(uart);
  }
}

// What a probe does
static void prv_probe_send(const char *data) {
  struct pbl_rtt_buffer *down = &_SEGGER_RTT.down[0];

  for (size_t i = 0; data[i] != '\0'; i++) {
    down->buf[down->wr] = (uint8_t)data[i];
    down->wr = (down->wr + 1U) % down->size;
  }
}

static size_t prv_probe_read(char *buf) {
  struct pbl_rtt_buffer *up = &_SEGGER_RTT.up[0];
  size_t n = 0U;

  while (up->rd != up->wr) {
    buf[n++] = (char)up->buf[up->rd];
    up->rd = (up->rd + 1U) % up->size;
  }
  return n;
}

// Tests
///////////////////////////////////////////////////////////

void test_uart_rtt__initialize(void) {
  pbl_rtt_reset_for_test();
  s_in_isr = false;
  s_waited_us = 0U;
  s_timer = nullptr;
  s_timer_active = false;
  s_rx_len = 0U;
  s_rx_stop_after = 0U;
  cl_assert_equal_i(pbl_device_init(&RTT_UART->dev), 0);
  pbl_rtt_init();
}

void test_uart_rtt__cleanup(void) {
  pbl_uart_rx_disable(RTT_UART);
}

void test_uart_rtt__sends_on_its_channel_and_drops_what_does_not_fit(void) {
  char out[16];

  for (const char *c = "abcdefghij"; *c != '\0'; c++) {
    pbl_uart_poll_out(RTT_UART, (uint8_t)*c);
  }

  // One slot stays empty
  cl_assert_equal_i(prv_probe_read(out), 7);
  cl_assert(memcmp(out, "abcdefg", 7) == 0);

  pbl_uart_poll_out(RTT_UART, 'z');
  cl_assert_equal_i(prv_probe_read(out), 1);
  cl_assert_equal_i(out[0], 'z');
}

void test_uart_rtt__receives_while_enabled(void) {
  prv_probe_send("hi");
  cl_assert(!s_timer_active);

  cl_assert_equal_i(pbl_uart_rx_enable(RTT_UART, prv_rx, nullptr), 0);
  cl_assert(s_timer_active);
  prv_fire_timer();
  cl_assert_equal_i(s_rx_len, 2);
  cl_assert(memcmp(s_rx, "hi", 2) == 0);
  // Polled on
  cl_assert(s_timer_active);

  pbl_uart_rx_disable(RTT_UART);
  cl_assert(!s_timer_active);
}

void test_uart_rtt__holds_bytes_back_once_disabled(void) {
  s_rx_stop_after = 1U;
  prv_probe_send("ab");
  cl_assert_equal_i(pbl_uart_rx_enable(RTT_UART, prv_rx, nullptr), 0);
  prv_fire_timer();

  cl_assert_equal_i(s_rx_len, 1);
  cl_assert(!s_timer_active);
  cl_assert_equal_i(_SEGGER_RTT.down[0].rd, 1);
}

void test_uart_rtt__flush_waits_only_in_an_interrupt(void) {
  char out[16];

  // A probe that read before is waited for, up to the timeout, from an interrupt
  pbl_uart_poll_out(RTT_UART, 'a');
  (void)prv_probe_read(out);
  pbl_uart_poll_out(RTT_UART, 'b');
  s_in_isr = true;
  pbl_uart_flush(RTT_UART);
  cl_assert_equal_i(s_waited_us, 1000);

  // Never from a thread
  (void)prv_probe_read(out);
  pbl_uart_poll_out(RTT_UART, 'c');
  s_waited_us = 0U;
  s_in_isr = false;
  pbl_uart_flush(RTT_UART);
  cl_assert_equal_i(s_waited_us, 0);
}
