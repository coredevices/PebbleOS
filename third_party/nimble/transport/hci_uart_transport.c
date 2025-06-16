/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// TODO: transport.h needs os_mbuf.h to be included first
// clang-format off
#include <os/os_mbuf.h>
// clang-format on

#include <board/board.h>
#include <drivers/uart.h>
#include <kernel/pebble_tasks.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <nimble/transport_impl.h>
#include <os/os_mempool.h>
#include <queue.h>
#include <system/passert.h>
#include <util/circular_buffer.h>
#include <util/math.h>

#define TX_Q_SIZE                                                                      \
  (MYNEWT_VAL(BLE_TRANSPORT_ACL_FROM_LL_COUNT) + MYNEWT_VAL(BLE_TRANSPORT_EVT_COUNT) + \
   MYNEWT_VAL(BLE_TRANSPORT_EVT_DISCARDABLE_COUNT))

extern void ble_chipset_init(void);
extern bool ble_chipset_start(void);

struct uart_tx {
  uint8_t type;
  uint8_t sent_type;
  uint16_t len;
  uint16_t idx;

  struct os_mbuf *om;
  uint8_t *buf;
  bool buf_needs_free;
};

static TaskHandle_t s_rx_task_handle;
static CircularBuffer s_rx_buffer;
static uint8_t s_rx_storage[1024];
static SemaphoreHandle_t s_rx_data_ready;
static SemaphoreHandle_t s_cmd_done;

static QueueHandle_t s_tx_queue;
static struct hci_h4_sm hci_uart_h4sm;
static bool chipset_start_done = false;

static void prv_lock(void) { portENTER_CRITICAL(); }

static void prv_unlock(void) { portEXIT_CRITICAL(); }

static int hci_uart_frame_cb(uint8_t pkt_type, void *data) {
  xSemaphoreGive(s_cmd_done);

  // HACK: passing responses to commands Nimble didn't generate causes issues
  if (!chipset_start_done) {
    ble_transport_free(data);
    return 0;
  }

  switch (pkt_type) {
    case HCI_H4_ACL:
      return ble_transport_to_hs_acl(data);
    case HCI_H4_EVT:
      return ble_transport_to_hs_evt(data);
    case HCI_H4_ISO:
      return ble_transport_to_hs_iso(data);
    default:
      WTF;
  }

  return -1;
}

static int hci_uart_tx_char(BaseType_t *should_context_switch) {
  struct uart_tx *tx = NULL;
  uint8_t ch;

  if (xQueuePeekFromISR(s_tx_queue, &tx) == pdFALSE) return -1;

  if (!tx->sent_type) {
    tx->sent_type = 1;
    return tx->type;
  }

  switch (tx->type) {
    case HCI_H4_CMD:
      ch = tx->buf[tx->idx];
      tx->idx++;
      if (tx->idx == tx->len) {
        if (tx->buf_needs_free) ble_transport_free(tx->buf);
        xQueueReceiveFromISR(s_tx_queue, &tx, should_context_switch);
        kernel_free(tx);
      }
      break;
    case HCI_H4_ACL:
    case HCI_H4_ISO:
      os_mbuf_copydata(tx->om, 0, 1, &ch);
      os_mbuf_adj(tx->om, 1);
      tx->len--;
      if (tx->len == 0) {
        os_mbuf_free_chain(tx->om);
        xQueueReceiveFromISR(s_tx_queue, &tx, should_context_switch);
        kernel_free(tx);
      }
      break;
    default:
      WTF;
  }

  return ch;
}

static void ble_hci_tx_byte(BaseType_t *should_context_switch) {
  int c = hci_uart_tx_char(should_context_switch);
  if (c == -1) {
    uart_set_tx_interrupt_enabled(BLUETOOTH_UART, false);
  } else {
    uart_write_byte(BLUETOOTH_UART, c);
  }
}

static bool prv_uart_tx_irq_handler(UARTDevice *dev) {
  BaseType_t should_context_switch = false;
  ble_hci_tx_byte(&should_context_switch);
  return should_context_switch;
}

static bool prv_uart_rx_irq_handler(UARTDevice *dev, uint8_t data,
                                    const UARTRXErrorFlags *err_flags) {
  BaseType_t should_context_switch = false;

  if (err_flags->framing_error || err_flags->overrun_error) {
    PBL_LOG_D(LOG_DOMAIN_BT_STACK, LOG_LEVEL_ERROR, "Bluetooth UART overrun:%d framing:%d",
              err_flags->overrun_error, err_flags->framing_error);
  }

  prv_lock();
  PBL_ASSERTN(circular_buffer_get_write_space_remaining(&s_rx_buffer) > 0);
  circular_buffer_write(&s_rx_buffer, &data, 1);
  xSemaphoreGiveFromISR(s_rx_data_ready, &should_context_switch);
  prv_unlock();

  return should_context_switch;
}

static uint8_t read_buf[64];
static void prv_rx_task_main(void *unused) {
  int consumed_bytes;
  uint16_t bytes_remaining;

  while (true) {
    xSemaphoreTake(s_rx_data_ready, portMAX_DELAY);

    while (true) {
      prv_lock();

      bytes_remaining = circular_buffer_get_read_space_remaining(&s_rx_buffer);
      if (bytes_remaining == 0) {
        prv_unlock();
        break;
      }

      bytes_remaining = MIN(sizeof(read_buf), bytes_remaining);
      circular_buffer_copy(&s_rx_buffer, read_buf, bytes_remaining);
      prv_unlock();

      consumed_bytes = hci_h4_sm_rx(&hci_uart_h4sm, read_buf, bytes_remaining);
      if (consumed_bytes <= 0) {
        PBL_LOG_D(LOG_DOMAIN_BT_STACK, LOG_LEVEL_ERROR, "hci_h4_sm_rx rc=%d", consumed_bytes);
        break;
      }

      prv_lock();
      circular_buffer_consume(&s_rx_buffer, consumed_bytes);
      prv_unlock();
    }
  }
}

void ble_transport_ll_init(void) {
  hci_h4_sm_init(&hci_uart_h4sm, &hci_h4_allocs_from_ll, hci_uart_frame_cb);

  s_tx_queue = xQueueCreate(TX_Q_SIZE, sizeof(struct uart_tx *));
  PBL_ASSERTN(s_tx_queue);

  s_rx_data_ready = xSemaphoreCreateBinary();
  s_cmd_done = xSemaphoreCreateBinary();
  circular_buffer_init(&s_rx_buffer, s_rx_storage, sizeof(s_rx_storage));

  ble_chipset_init();

  uart_init(BLUETOOTH_UART);
  uart_set_baud_rate(BLUETOOTH_UART, 115200);
  uart_set_rx_interrupt_handler(BLUETOOTH_UART, prv_uart_rx_irq_handler);
  uart_set_tx_interrupt_handler(BLUETOOTH_UART, prv_uart_tx_irq_handler);
  uart_set_rx_interrupt_enabled(BLUETOOTH_UART, true);

  TaskParameters_t task_params = {
      .pvTaskCode = prv_rx_task_main,
      .pcName = "NimbleRX",
      .usStackDepth = 4000 / sizeof(StackType_t), // TODO: can probably be reduced
      .uxPriority = (tskIDLE_PRIORITY + 3) | portPRIVILEGE_BIT,
      .puxStackBuffer = NULL,
  };

  pebble_task_create(PebbleTask_BTHCI, &task_params, &s_rx_task_handle);
  PBL_ASSERTN(s_rx_task_handle);

  if (ble_chipset_start()) {
    chipset_start_done = true;
  }
}

static void ble_transport_tx_item(struct uart_tx *tx_item) {
  xQueueSendToBack(s_tx_queue, &tx_item, portMAX_DELAY);
  uart_set_tx_interrupt_enabled(BLUETOOTH_UART, true);
}

void ble_queue_cmd(void *buf, bool needs_free, bool wait) {
  struct uart_tx *tx_item = kernel_malloc(sizeof(struct uart_tx));
  PBL_ASSERTN(tx_item);
  tx_item->type = HCI_H4_CMD;
  tx_item->sent_type = 0;
  tx_item->len = 3 + ((uint8_t *)buf)[2];
  tx_item->buf = buf;
  tx_item->idx = 0;
  tx_item->om = NULL;
  tx_item->buf_needs_free = needs_free;

  ble_transport_tx_item(tx_item);

  if (wait) xSemaphoreTake(s_cmd_done, portMAX_DELAY);
}

/* APIs to be implemented by HS/LL side of transports */
int ble_transport_to_ll_cmd_impl(void *buf) {
  ble_queue_cmd(buf, true, false);
  return 0;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  struct uart_tx *tx_item = kernel_malloc(sizeof(struct uart_tx));
  PBL_ASSERTN(tx_item);
  tx_item->type = HCI_H4_ACL;
  tx_item->sent_type = 0;
  tx_item->len = OS_MBUF_PKTLEN(om);
  tx_item->buf = NULL;
  tx_item->idx = 0;
  tx_item->om = om;

  ble_transport_tx_item(tx_item);

  return 0;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  struct uart_tx *tx_item = kernel_malloc(sizeof(struct uart_tx));
  PBL_ASSERTN(tx_item);
  tx_item->type = HCI_H4_ISO;
  tx_item->sent_type = 0;
  tx_item->len = OS_MBUF_PKTLEN(om);
  tx_item->buf = NULL;
  tx_item->idx = 0;
  tx_item->om = om;

  ble_transport_tx_item(tx_item);

  return 0;
}
