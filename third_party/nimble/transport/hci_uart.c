/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include <board/board.h>
#include <kernel/pebble_tasks.h>
#include <pbl/drivers/uart.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>
#include <system/passert.h>

// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/hci_common.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <nimble/transport_impl.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define RX_RING_SIZE 1024U

static struct pbl_thread *s_hci_task_handle;
PBL_THREAD_STACK_DEFINE(s_hci_task_stack, 1024);
static PBL_SEM_DEFINE(s_rx_ready, 0, 1);
static PBL_SEM_DEFINE(s_acl_pool_avail, 0, 1);
static PBL_MUTEX_DEFINE(s_tx_mutex);
static struct hci_h4_sm s_hci_h4sm;

// Filled by the UART ISR, drained by the HCI task.
static uint8_t s_rx_ring[RX_RING_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;
static volatile bool s_rx_paused;

static struct os_mbuf *prv_alloc_acl_from_ll(void) {
  struct os_mbuf *om;

  // A NULL return is a fatal framing error for the H4 state machine, so wait
  // for the host to free a buffer. The UART backpressures meanwhile.
  while ((om = ble_transport_alloc_acl_from_ll()) == NULL) {
    (void)pbl_sem_take(&s_acl_pool_avail, PBL_MSEC(100));
  }

  return om;
}

static os_error_t prv_acl_put_signal(struct os_mempool_ext *mpe, void *data, void *arg) {
  os_error_t err = os_memblock_put_from_cb(&mpe->mpe_mp, data);
  pbl_sem_give(&s_acl_pool_avail);
  return err;
}

static void *prv_alloc_evt(int discardable) {
  void *buf = ble_transport_alloc_evt(discardable);
  if (buf == NULL) {
    PBL_LOG_ERR("EVT alloc failed (discardable=%d)", discardable);
  }

  return buf;
}

static const struct hci_h4_allocators s_hci_h4_allocs_from_ll = {
  .acl = prv_alloc_acl_from_ll,
  .evt = prv_alloc_evt,
};

static int prv_hci_frame_cb(uint8_t pkt_type, void *data) {
  switch (pkt_type) {
    case HCI_H4_EVT:
      return ble_transport_to_hs_evt(data);
    case HCI_H4_ACL:
      return ble_transport_to_hs_acl(data);
    default:
      WTF;
  }

  return -1;
}

static bool prv_rx_irq_handler(UARTDevice *dev, uint8_t data, const UARTRXErrorFlags *err_flags) {
  uint32_t head = s_rx_head;
  uint32_t next = (head + 1U) % RX_RING_SIZE;

  // The byte has already left the UART, so pause while there is still room
  // for it; the rest waits in the UART.
  s_rx_ring[head] = data;
  s_rx_head = next;

  if ((next + 1U) % RX_RING_SIZE == s_rx_tail) {
    s_rx_paused = true;
    uart_set_rx_interrupt_enabled(dev, false);
  }

  pbl_sem_give(&s_rx_ready);

  return false;
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
        uart_set_rx_interrupt_enabled(BT_HCI_UART, true);
      }
    }
  }
}

static void prv_write(const uint8_t *data, size_t len) {
  for (size_t i = 0U; i < len; i++) {
    uart_write_byte(BT_HCI_UART, data[i]);
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
  hci_h4_sm_init(&s_hci_h4sm, &s_hci_h4_allocs_from_ll, prv_hci_frame_cb);
  ble_transport_register_put_acl_from_ll_cb(prv_acl_put_signal);

  struct pbl_thread_attr attr = {
    .name = "NimbleHCI",
    .entry = prv_hci_task_main,
    .prio = PBL_PRIO_IDLE + 3,
    .privileged = true,
    .stack = s_hci_task_stack,
    .stack_size = sizeof(s_hci_task_stack),
  };

  s_hci_task_handle = pebble_task_create(PebbleTask_BTHCI, &attr);
  PBL_ASSERTN(s_hci_task_handle);

  uart_init(BT_HCI_UART);
  uart_set_rx_interrupt_handler(BT_HCI_UART, prv_rx_irq_handler);
  uart_set_rx_interrupt_enabled(BT_HCI_UART, true);
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
