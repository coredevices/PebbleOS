/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>
#include <pbl/usb/cdc_acm.h>

#define RUN_S     120
#define TICK_S    10
#define QUEUE_LEN 2048

/* The receive callback runs in the controller's interrupt: it queues, the main thread echoes. */
static PBL_MSGQ_DEFINE(s_rx, sizeof(uint8_t), QUEUE_LEN);

static void prv_rx(const struct pbl_uart *uart, uint8_t c, void *arg) {
  (void)pbl_msgq_put(&s_rx, &c, PBL_NO_WAIT);
}

int main(void) {
  uint64_t start = pbl_uptime_ms();
  uint64_t tick = start;
  unsigned int count = 0U;
  uint8_t c;
  int ret;

  ret = pbl_uart_rx_enable(USB_CDC_ACM_UART(1), prv_rx, nullptr);
  if (ret != 0) {
    printf("usb: serial port 1 failed (%d)\n", ret);
    return ret;
  }

  printf("usb: console on port 0, echo on port 1 for %d s\n", RUN_S);

  while (pbl_uptime_ms() - start < RUN_S * 1000U) {
    if (pbl_uptime_ms() >= tick) {
      printf("usb: %u s, port 1 %s, %u bytes echoed\n", (unsigned int)((tick - start) / 1000U),
             pbl_usb_cdc_acm_dtr(USB_CDC_ACM_UART(1)) ? "open" : "closed", count);
      tick += TICK_S * 1000U;
    }
    if (pbl_msgq_get(&s_rx, &c, PBL_MSEC(100)) != 0) {
      continue;
    }
    pbl_uart_poll_out(USB_CDC_ACM_UART(1), c);
    count++;
  }

  printf("usb: echoed %u bytes\n", count);
  printf("Sample done\n");

  return 0;
}
