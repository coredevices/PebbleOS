/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
#include <pbl/drivers/uart.h>
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/id_addr.h>
#include <pbl/board.h>
#include <pbl/drivers/mbox.h>
#include <pbl/drivers/remoteproc/sf32lb52.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/barrier.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>

#include <hardware/sf32lb52_memorymap.h>
#include <kernel/pebble_tasks.h>

// NOTE: transport.h needs os_mbuf.h to be included first
// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/hci_common.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <nimble/transport_impl.h>
#include <os/os_mempool.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define HCI_WRITE_TIMEOUT_MS 10U

#define HCI_TRACE_HEADER_LEN 16

#define H4TL_PACKET_HOST 0x61
#define H4TL_PACKET_CTRL 0x62

#define BLE_HCI_EXT_SF32LB52_BLE_READY 0xFC11U

// Controller NVDS, read by the LCPU ROM on boot
#define NVDS_BASE    0x2040fe00UL
#define NVDS_MAGIC   0x4e564453UL
#define NVDS_HDR_LEN 8U

#define NVDS_TAG_BD_ADDR     0x01U
#define NVDS_TAG_WAKEUP_TIME 0x0dU
#define NVDS_BD_ADDR_LEN     6U

#ifdef SF32LB52_USE_LXT
#define NVDS_WAKEUP_TIME_US 3500U
#else
#define NVDS_WAKEUP_TIME_US 4500U
#endif

// Maximum sleep duration (10 s), controller logs off, scheduling
static const uint8_t s_nvds_defaults[] = {0x12U, 0x01U, 0x01U, 0x2fU, 0x04U, 0x20U,
                                          0x00U, 0x00U, 0x00U, 0x15U, 0x01U, 0x01U};

// HCI ring layout, fixed by the LCPU ROM. Indexes: offset in the upper half, a mirror flag
// toggled on each wrap in the lower half (same offset: empty if mirrors match, full otherwise).
struct ring {
  uint32_t rd_buf;
  uint32_t wr_buf;
  uint32_t rd_idx;
  uint32_t wr_idx;
  int16_t size;
};

static_assert(sizeof(struct ring) == 20U);

#define RING_IDX_MASK    PBL_GENMASK(31, 16)
#define RING_MIRROR_MASK PBL_GENMASK(15, 0)

#define RING_POOL_SIZE(region_size) (((region_size) - sizeof(struct ring)) & ~3U)

// LCPU to HCPU ring, in LPSYS RAM
#define RX_RING_BASE      SF32LB52_LPSYS_MBOX_BUF_BASE
#define RX_RING_POOL_SIZE RING_POOL_SIZE(SF32LB52_MBOX_BUF_SIZE)

static struct pbl_thread s_hci_thread;
PBL_THREAD_STACK_DEFINE(s_hci_task_stack, CONFIG_BT_HCI_THREAD_STACK_SIZE);
static PBL_SEM_DEFINE(s_hci_data_ready, 0, 1);
/* Given by prv_acl_put_signal() whenever an LL-direction ACL mbuf is returned
 * to the transport pool; taken by the HCI RX task when an alloc fails so we
 * unblock as soon as the host has freed something instead of polling.
 */
static PBL_SEM_DEFINE(s_acl_pool_avail, 0, 1);
static struct hci_h4_sm s_hci_h4sm;

static bool s_open;
// Written data the LCPU has yet to take; sleep is blocked meanwhile
static bool s_busy;
// Data from the LCPU yet to read; it may sleep once there is none, so its RAM stays untouched
static volatile uint32_t s_rx_pending;

static struct os_mbuf *prv_alloc_acl_from_ll(void) {
  struct os_mbuf *om = ble_transport_alloc_acl_from_ll();
  if (om != NULL) {
    return om;
  }

  /* ACL pool exhausted. Block instead of returning NULL (which the H4 state
   * machine would treat as a fatal, unrecoverable framing error). While we
   * wait we stop draining the LCPU->HCPU HCI ring; it fills and backpressures
   * the controller, i.e. transport-level flow control. prv_acl_put_signal()
   * wakes us when the host returns a buffer to the pool; the timeout guards
   * the give-before-wait race and the fact that the from_hs TX path shares
   * mpool_acl, so a freed buffer may be taken before we retry.
   */
  PBL_LOG_DBG("ACL pool empty, waiting for buffer");
  do {
    (void)pbl_sem_take(&s_acl_pool_avail, PBL_MSEC(100));
    om = ble_transport_alloc_acl_from_ll();
  } while (om == NULL);

  return om;
}

static os_error_t prv_acl_put_signal(struct os_mempool_ext *mpe, void *data, void *arg) {
  os_error_t err = os_memblock_put_from_cb(&mpe->mpe_mp, data);
  pbl_sem_give(&s_acl_pool_avail);
  return err;
}

static void *prv_alloc_evt(int discardable) {
  void *buf = ble_transport_alloc_evt(discardable);
  if (!buf) {
    PBL_LOG_ERR("EVT alloc failed (discardable=%d)", discardable);
  }

  return buf;
}

static const struct hci_h4_allocators s_hci_h4_allocs_from_ll = {
  .acl = prv_alloc_acl_from_ll,
  .evt = prv_alloc_evt,
};

#if defined(NIMBLE_HCI_SF32LB52_TRACE_BINARY)
static uint16_t s_hci_trace_seq;
#endif

#if defined(NIMBLE_HCI_SF32LB52_TRACE_BINARY) || defined(NIMBLE_HCI_SF32LB52_TRACE_LOG)
#define MAX_HCI_PKT_SIZE 1024
static uint8_t s_hci_buf[MAX_HCI_PKT_SIZE];
#endif

#if defined(NIMBLE_HCI_SF32LB52_TRACE_LOG)
void prv_hci_trace(uint8_t type, const uint8_t *data, uint16_t len, uint8_t h4tl_packet) {
  const char *type_str;

  switch (type) {
    case HCI_H4_CMD:
      type_str = "CMD";
      break;
    case HCI_H4_ACL:
      type_str = "ACL";
      break;
    case HCI_H4_EVT:
      type_str = "EVT";
      break;
    case HCI_H4_ISO:
      type_str = "ISO";
      break;
    default:
      type_str = "UKN";
      break;
  }

  PBL_LOG_DBG("%s, %s %" PRIu16, type_str, (h4tl_packet == H4TL_PACKET_HOST) ? "TX" : "RX", len);
  PBL_LOG_HEXDUMP_DBG(data, len);
}
#elif defined(NIMBLE_HCI_SF32LB52_TRACE_BINARY)
void prv_hci_trace(uint8_t type, const uint8_t *data, uint16_t len, uint8_t h4tl_packet) {
  uint8_t trace_hdr[HCI_TRACE_HEADER_LEN];

  // Magic for Pebble HCI, 'PBTS'
  trace_hdr[0] = 0x50U;
  trace_hdr[1] = 0x42U;
  trace_hdr[2] = 0x54U;
  trace_hdr[3] = 0x53U;
  trace_hdr[4] = 0x06U;
  trace_hdr[5] = 0x01U;
  trace_hdr[6] = (len + 8U) & 0xFFU;
  trace_hdr[7] = (len + 8U) >> 8U;
  trace_hdr[8] = s_hci_trace_seq & 0xFFU;
  trace_hdr[9] = s_hci_trace_seq >> 8U;
  trace_hdr[10] = 0U;
  trace_hdr[11] = 0U;
  trace_hdr[12] = 0U;
  trace_hdr[13] = 0U;
  trace_hdr[14] = h4tl_packet;
  trace_hdr[15] = type;

  s_hci_trace_seq++;

  for (uint8_t i = 0U; i < HCI_TRACE_HEADER_LEN; i++) {
    pbl_uart_poll_out(HCI_TRACE_UART, trace_hdr[i]);
  }

  for (uint16_t i = 0U; i < len; i++) {
    pbl_uart_poll_out(HCI_TRACE_UART, data[i]);
  }
}
#else
#define prv_hci_trace(type, data, len, h4tl_packet)
#endif

#if defined(NIMBLE_HCI_SF32LB52_TRACE_BINARY) || defined(NIMBLE_HCI_SF32LB52_TRACE_LOG)
void prv_hci_trace_mbuf(uint8_t type, struct os_mbuf *om, uint8_t h4tl_packet) {
  PBL_ASSERTN(os_mbuf_len(om) < MAX_HCI_PKT_SIZE);
  os_mbuf_copydata(om, 0, os_mbuf_len(om), s_hci_buf);
  prv_hci_trace(type, s_hci_buf, os_mbuf_len(om), h4tl_packet);
}
#else
#define prv_hci_trace_mbuf(type, om, h4tl_packet)
#endif

static inline volatile struct ring *prv_tx(void) {
  return (volatile struct ring *)LCPU->mem.base;
}

static inline volatile struct ring *prv_rx(void) {
  return (volatile struct ring *)RX_RING_BASE;
}

static uint32_t prv_ring_used(uint32_t rd, uint32_t wr, uint32_t size) {
  uint32_t rd_idx = PBL_FIELD_GET(RING_IDX_MASK, rd);
  uint32_t wr_idx = PBL_FIELD_GET(RING_IDX_MASK, wr);

  if (rd_idx == wr_idx) {
    return ((rd ^ wr) & RING_MIRROR_MASK) == 0U ? 0U : size;
  }

  return (wr_idx > rd_idx) ? (wr_idx - rd_idx) : (size - (rd_idx - wr_idx));
}

static uint32_t prv_ring_advance(uint32_t ptr, uint32_t len, uint32_t size) {
  uint32_t idx = PBL_FIELD_GET(RING_IDX_MASK, ptr) + len;
  uint32_t mirror = PBL_FIELD_GET(RING_MIRROR_MASK, ptr);

  if (idx >= size) {
    idx -= size;
    mirror ^= RING_MIRROR_MASK;
  }

  return PBL_FIELD_PREP(RING_IDX_MASK, idx) | mirror;
}

static bool prv_ring_empty(volatile struct ring *ring) {
  return prv_ring_used(ring->rd_idx, ring->wr_idx, (uint32_t)ring->size) == 0U;
}

static void prv_ring_reset_tx(void) {
  volatile struct ring *tx = prv_tx();

  tx->rd_idx = 0U;
  tx->wr_idx = 0U;
  tx->wr_buf = LCPU->mem.base + sizeof(struct ring);
  tx->rd_buf = tx->wr_buf + SF32LB52_HPSYS_SRAM_LCPU_OFFSET;
  tx->size = (int16_t)RING_POOL_SIZE(LCPU->mem.size);
  pbl_wmb();
}

static size_t prv_ring_put(volatile struct ring *ring, const uint8_t *data, size_t len) {
  uint32_t size = (uint32_t)ring->size;
  uint32_t wr = ring->wr_idx;
  uint32_t idx = PBL_FIELD_GET(RING_IDX_MASK, wr);
  uint8_t *pool = (uint8_t *)ring->wr_buf;
  uint32_t space = size - prv_ring_used(ring->rd_idx, wr, size);
  size_t first;

  len = MIN(len, space);
  if (len == 0U) {
    return 0U;
  }

  pbl_mb();

  first = MIN(len, size - idx);
  memcpy(&pool[idx], data, first);
  memcpy(pool, &data[first], len - first);

  pbl_wmb();
  ring->wr_idx = prv_ring_advance(wr, len, size);

  return len;
}

// Contiguous data from the LCPU, in place
static size_t prv_ring_peek(const uint8_t **data) {
  volatile struct ring *rx = prv_rx();
  uint32_t wr, rd, size, idx, used;

  if (s_rx_pending == 0U) {
    return 0U;
  }

  wr = rx->wr_idx;
  rd = rx->rd_idx;
  size = (uint32_t)rx->size;
  idx = PBL_FIELD_GET(RING_IDX_MASK, rd);

  if (size == 0U || size > RX_RING_POOL_SIZE || idx >= size) {
    return 0U;
  }

  used = prv_ring_used(rd, wr, size);
  if (used == 0U) {
    return 0U;
  }

  pbl_rmb();
  *data = (const uint8_t *)rx->rd_buf + idx;

  return MIN(used, size - idx);
}

static void prv_ring_consume(size_t len) {
  volatile struct ring *rx = prv_rx();
  uint32_t size = (uint32_t)rx->size;

  pbl_mb();

  pbl_irq_lock();
  s_rx_pending = prv_ring_used(rx->rd_idx, rx->wr_idx, size) - len;
  rx->rd_idx = prv_ring_advance(rx->rd_idx, len, size);
  pbl_irq_unlock();
}

static void prv_set_busy(bool busy) {
  if (s_busy == busy) {
    return;
  }

  s_busy = busy;
  if (busy) {
    soc_sf32lb_sleep_block(SOC_SF32LB_WFI);
  } else {
    soc_sf32lb_sleep_release(SOC_SF32LB_WFI);
  }
}

static void prv_mbox_rx(const struct pbl_mbox_client *cl, const void *msg) {
  volatile struct ring *rx = prv_rx();

  s_rx_pending = prv_ring_used(rx->rd_idx, rx->wr_idx, (uint32_t)rx->size);
  pbl_sem_give(&s_hci_data_ready);
}

static void prv_mbox_tx_done(const struct pbl_mbox_client *cl, const void *msg, int ret) {
  pbl_irq_lock();
  if (s_open && prv_ring_empty(prv_tx())) {
    prv_set_busy(false);
  } else if (s_open) {
    // the LCPU took the doorbell before draining the ring
    (void)pbl_mbox_send(&LCPU->tx_chan, nullptr);
  }
  pbl_irq_unlock();
}

static const struct pbl_mbox_client s_mbox_cl = {
  .rx_callback = prv_mbox_rx,
  .tx_done = prv_mbox_tx_done,
};

static int prv_write(const void *data, size_t len) {
  const uint8_t *buf = data;
  uint64_t start;

  if (!s_open) {
    return -EINVAL;
  }

  start = pbl_uptime_ms();
  while (len > 0U) {
    size_t written = prv_ring_put(prv_tx(), buf, len);

    if (written > 0U) {
      buf += written;
      len -= written;

      pbl_irq_lock();
      prv_set_busy(true);
      pbl_irq_unlock();

      (void)pbl_mbox_send(&LCPU->tx_chan, nullptr);
      continue;
    }

    if (pbl_uptime_ms() - start >= HCI_WRITE_TIMEOUT_MS) {
      return -ETIMEDOUT;
    }
  }

  return 0;
}

static int prv_hci_frame_cb(uint8_t pkt_type, void *data) {
  struct ble_hci_ev *ev;
  struct ble_hci_ev_command_complete *cmd_complete;

  switch (pkt_type) {
    case HCI_H4_EVT:
      ev = data;
      cmd_complete = (void *)ev->data;

      if (ev->opcode == BLE_HCI_EVCODE_COMMAND_COMPLETE) {
        PBL_LOG_DBG("CMD complete %x", cmd_complete->opcode);
        // NOTE: do not confuse NimBLE with SF32LB52 vendor specific command
        if (cmd_complete->opcode == BLE_HCI_EXT_SF32LB52_BLE_READY) {
          break;
        }
      }

      prv_hci_trace(pkt_type, data, ev->length + sizeof(*ev), H4TL_PACKET_CTRL);

      return ble_transport_to_hs_evt(data);
    case HCI_H4_ACL:
      prv_hci_trace(pkt_type, OS_MBUF_DATA((struct os_mbuf *)data, uint8_t *),
                    OS_MBUF_PKTLEN((struct os_mbuf *)data), H4TL_PACKET_CTRL);

      return ble_transport_to_hs_acl(data);
    default:
      pbl_panic();
      break;
  }

  return -1;
}

static void prv_hci_task_main(void *unused) {
  while (true) {
    const uint8_t *data;
    size_t len;

    pbl_sem_take(&s_hci_data_ready, PBL_FOREVER);

    while ((len = prv_ring_peek(&data)) > 0U) {
      int consumed;

      /* prv_alloc_acl_from_ll() blocks until a buffer is available, so the
       * H4 state machine never reports an OOM on the ACL path; any
       * non-positive return here is unexpected.
       */
      consumed = hci_h4_sm_rx(&s_hci_h4sm, data, len);
      if (consumed <= 0) {
        PBL_LOG_ERR("hci_h4_sm_rx returned %d", consumed);
        consumed = (int)len;
      }

      prv_ring_consume((size_t)consumed);
    }
  }
}

static void prv_nvds_config(const uint8_t *bd_addr) {
  uint8_t *nvds = (uint8_t *)NVDS_BASE;
  uint8_t *p = nvds + NVDS_HDR_LEN;

  if (bd_addr != nullptr) {
    *p++ = NVDS_TAG_BD_ADDR;
    *p++ = NVDS_BD_ADDR_LEN;
    memcpy(p, bd_addr, NVDS_BD_ADDR_LEN);
    p += NVDS_BD_ADDR_LEN;
  }

  *p++ = NVDS_TAG_WAKEUP_TIME;
  *p++ = 2U;
  *p++ = (uint8_t)NVDS_WAKEUP_TIME_US;
  *p++ = (uint8_t)(NVDS_WAKEUP_TIME_US >> 8);

  memcpy(p, s_nvds_defaults, sizeof(s_nvds_defaults));
  p += sizeof(s_nvds_defaults);

  *(volatile uint32_t *)nvds = NVDS_MAGIC;
  *(volatile uint16_t *)(nvds + 4) = (uint16_t)(p - (nvds + NVDS_HDR_LEN));
  *(volatile uint16_t *)(nvds + 6) = 0U;
}

static void prv_controller_config(void) {
#ifdef CONFIG_BT_ID_ADDR
  struct pbl_bt_addr addr;
  enum pbl_bt_id_addr_type type;
  int rc;

  rc = pbl_bt_id_addr_get(&addr, &type);
  PBL_ASSERT(rc == 0, "No identity address (%d)", rc);

  prv_nvds_config(type == PBL_BT_ID_ADDR_PUBLIC ? addr.octets : nullptr);
#else
  prv_nvds_config(nullptr);
#endif
}

void ble_transport_ll_reinit(void) {
  int ret;

  hci_h4_sm_init(&s_hci_h4sm, &s_hci_h4_allocs_from_ll, prv_hci_frame_cb);

  prv_ring_reset_tx();

  ret = pbl_mbox_request(&LCPU->rx_chan, &s_mbox_cl);
  PBL_ASSERT(ret == 0, "LCPU RX doorbell request failed (%d)", ret);

  ret = pbl_mbox_request(&LCPU->tx_chan, &s_mbox_cl);
  PBL_ASSERT(ret == 0, "LCPU TX doorbell request failed (%d)", ret);

  s_open = true;

  prv_controller_config();

  ret = pbl_rproc_boot(&LCPU->rproc);
  PBL_ASSERT(ret == 0, "LCPU boot failed (%d)", ret);
}

void ble_transport_ll_init(void) {
  ble_transport_ll_reinit();

  ble_transport_register_put_acl_from_ll_cb(prv_acl_put_signal);

  struct pbl_thread_attr attr = {
    .name = "NimbleHCI",
    .entry = prv_hci_task_main,
    .prio = CONFIG_BT_HCI_THREAD_PRIO,
    .privileged = true,
    .stack = s_hci_task_stack,
    .stack_size = sizeof(s_hci_task_stack),
  };

  PBL_ASSERTN(pbl_thread_create(&s_hci_thread, &attr) == 0);
  pebble_task_register(PebbleTask_BTHCI, &s_hci_thread);
}

void ble_transport_ll_deinit(void) {
  pbl_mbox_free(&LCPU->tx_chan);
  pbl_mbox_free(&LCPU->rx_chan);

  pbl_irq_lock();
  s_open = false;
  prv_set_busy(false);
  pbl_irq_unlock();

  pbl_rproc_shutdown(&LCPU->rproc);
}

static int prv_write_mbuf(uint8_t h4_type, struct os_mbuf *om) {
  int ret;

  ret = prv_write(&h4_type, 1U);
  if (ret != 0) {
    return ret;
  }

  for (struct os_mbuf *x = om; x != NULL; x = SLIST_NEXT(x, om_next)) {
    ret = prv_write(x->om_data, x->om_len);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

/* APIs to be implemented by HS/LL side of transports */
int ble_transport_to_ll_cmd_impl(void *buf) {
  struct ble_hci_cmd *cmd = buf;
  uint8_t h4_cmd = HCI_H4_CMD;
  int ret;

  prv_hci_trace(HCI_H4_CMD, (uint8_t *)cmd, sizeof(*cmd) + cmd->length, H4TL_PACKET_HOST);

  ret = prv_write(&h4_cmd, 1U);
  if (ret == 0) {
    ret = prv_write(cmd, sizeof(*cmd) + cmd->length);
  }

  ble_transport_free(buf);

  if (ret != 0) {
    PBL_LOG_ERR("Failed to write HCI CMD (%d)", ret);
    return BLE_ERR_MEM_CAPACITY;
  }

  return 0;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  int ret;

  prv_hci_trace_mbuf(HCI_H4_ACL, om, H4TL_PACKET_HOST);

  ret = prv_write_mbuf(HCI_H4_ACL, om);
  os_mbuf_free_chain(om);

  if (ret != 0) {
    PBL_LOG_ERR("Failed to write HCI ACL (%d)", ret);
    return BLE_ERR_MEM_CAPACITY;
  }

  return 0;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  int ret;

  prv_hci_trace_mbuf(HCI_H4_ISO, om, H4TL_PACKET_HOST);

  ret = prv_write_mbuf(HCI_H4_ISO, om);
  os_mbuf_free_chain(om);

  if (ret != 0) {
    PBL_LOG_ERR("Failed to write HCI ISO (%d)", ret);
    return BLE_ERR_MEM_CAPACITY;
  }

  return 0;
}
