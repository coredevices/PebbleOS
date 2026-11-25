/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hci_alloc.h"

#include <stdbool.h>
#include <stdint.h>

#include <pbl/board.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/hci_common.h>
#include <nimble/nimble_port_pebble.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <nimble/transport_impl.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define RX_RING_SIZE 1024U

static struct pbl_thread s_hci_thread;
PBL_THREAD_STACK_DEFINE(s_hci_task_stack, CONFIG_NIMBLE_HCI_THREAD_STACK_SIZE);
static PBL_SEM_DEFINE(s_rx_ready, 0, 1);
static PBL_MUTEX_DEFINE(s_tx_mutex);
static struct hci_h4_sm s_hci_h4sm;

// Filled by the UART ISR, drained by the HCI task.
static uint8_t s_rx_ring[RX_RING_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;
static volatile bool s_rx_paused;

static int prv_hci_frame_cb(uint8_t pkt_type, void *data) {
  switch (pkt_type) {
    case HCI_H4_EVT:
      return ble_transport_to_hs_evt(data);
    case HCI_H4_ACL:
      return ble_transport_to_hs_acl(data);
    default:
      pbl_panic();
  }

  return -1;
}

static void prv_rx(const struct pbl_uart *uart, uint8_t data, void *arg) {
  uint32_t head = s_rx_head;
  uint32_t next = (head + 1U) % RX_RING_SIZE;

  // The byte has already left the UART, so pause while there is still room
  // for it; the rest waits in the UART.
  s_rx_ring[head] = data;
  s_rx_head = next;

  if ((next + 1U) % RX_RING_SIZE == s_rx_tail) {
    s_rx_paused = true;
    (void)pbl_uart_rx_disable(uart);
  }

  pbl_sem_give(&s_rx_ready);
}

static void prv_hci_task_main(void *unused) {
  while (true) {
    pbl_sem_take(&s_rx_ready, PBL_FOREVER);

    while (s_rx_tail != s_rx_head) {
      uint32_t tail = s_rx_tail;
      uint32_t head = s_rx_head;
      uint32_t len = (head > tail) ? (head - tail) : (RX_RING_SIZE - tail);

      int consumed = hci_h4_sm_rx(&s_hci_h4sm, &s_rx_ring[tail], len);
      if (consumed <= 0) {
        PBL_LOG_ERR("hci_h4_sm_rx returned %d", consumed);
        consumed = len;
      }
      s_rx_tail = (tail + consumed) % RX_RING_SIZE;

      if (s_rx_paused) {
        s_rx_paused = false;
        (void)pbl_uart_rx_enable(BT_HCI_UART, prv_rx, nullptr);
      }
    }
  }
}

static void prv_write(const uint8_t *data, size_t len) {
  for (size_t i = 0U; i < len; i++) {
    pbl_uart_poll_out(BT_HCI_UART, data[i]);
  }
}

static void prv_write_mbuf(uint8_t type, struct os_mbuf *om) {
  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
  prv_write(&type, 1U);
  for (struct os_mbuf *x = om; x != NULL; x = SLIST_NEXT(x, om_next)) {
    prv_write(x->om_data, x->om_len);
  }
  pbl_mutex_unlock(&s_tx_mutex);

  os_mbuf_free_chain(om);
}

void ble_transport_ll_init(void) {
  nimble_hci__alloc_init();
  hci_h4_sm_init(&s_hci_h4sm, &nimble_hci__allocs_from_ll, prv_hci_frame_cb);

  struct pbl_thread_attr attr = {
    .name = "NimbleHCI",
    .entry = prv_hci_task_main,
    .prio = CONFIG_NIMBLE_HCI_THREAD_PRIO,
    .privileged = true,
    .stack = s_hci_task_stack,
    .stack_size = sizeof(s_hci_task_stack),
  };

  PBL_ASSERTN(pbl_thread_create(&s_hci_thread, &attr) == 0);

  PBL_ASSERTN(pbl_uart_rx_enable(BT_HCI_UART, prv_rx, nullptr) == 0);
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  struct ble_hci_cmd *cmd = buf;
  uint8_t type = HCI_H4_CMD;

  pbl_mutex_lock(&s_tx_mutex, PBL_FOREVER);
  prv_write(&type, 1U);
  prv_write(buf, sizeof(*cmd) + cmd->length);
  pbl_mutex_unlock(&s_tx_mutex);

  ble_transport_free(buf);

  return 0;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  prv_write_mbuf(HCI_H4_ACL, om);
  return 0;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  prv_write_mbuf(HCI_H4_ISO, om);
  return 0;
}

struct pbl_thread *nimble_port_pebble_hci_thread(void) {
  return &s_hci_thread;
}
