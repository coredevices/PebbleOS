/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>
#include <pbl/usb/cdc_acm.h>

#define ECHO_S    60
#define QUEUE_LEN 2048

/* The receive callback runs in the controller's interrupt: it queues, the main thread echoes. */
static PBL_MSGQ_DEFINE(s_rx, sizeof(uint8_t), QUEUE_LEN);

static void prv_rx(const struct pbl_uart *uart, uint8_t c, void *arg) {
  (void)pbl_msgq_put(&s_rx, &c, PBL_NO_WAIT);
}

int main(void) {
  uint64_t end = pbl_uptime_ms() + ECHO_S * 1000U;
  unsigned int count = 0U;
  uint8_t c;
  int ret;

  /* The device connects to the bus as the port comes up, with the other devices. */
  ret = pbl_uart_rx_enable(USB_CDC_ACM_UART(0), prv_rx, nullptr);
  if (ret != 0) {
    fprintf(stderr, "usb: serial port failed (%d)\n", ret);
    return ret;
  }

  printf("usb: serial port up, echoing for %d s\n", ECHO_S);

  while (pbl_uptime_ms() < end) {
    if (pbl_msgq_get(&s_rx, &c, PBL_MSEC(100)) != 0) {
      continue;
    }
    pbl_uart_poll_out(USB_CDC_ACM_UART(0), c);
    count++;
  }

  printf("usb: echoed %u bytes\n", count);
  printf("Sample done\n");

  return 0;
}
