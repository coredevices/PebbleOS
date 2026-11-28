/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hci_alloc.h"
#include "sf32lb_ring.h"
#include "sf32lb_sco.h"

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include <pbl/bluetooth/id_addr.h>
#include <pbl/board.h>
#include <pbl/drivers/mbox.h>
#include <pbl/drivers/remoteproc/sf32lb.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/barrier.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>

// NOTE: transport.h needs os_mbuf.h to be included first
// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/hci_common.h>
#include <nimble/nimble_port_pebble.h>
#include <nimble/transport.h>
#include <nimble/transport/hci_h4.h>
#include <nimble/transport_impl.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

#define HCI_WRITE_TIMEOUT_MS 100U

#define BLE_HCI_EXT_SF32LB52_BLE_READY 0xFC11U

// Controller NVDS, read by the LCPU ROM on boot, in its SRAM region
#ifdef CONFIG_SOC_SF32LB57
#define NVDS_OFFSET 0x13600U
#else
#define NVDS_OFFSET 0xfe00U
#endif
#define NVDS_MAGIC   0x4e564453UL
#define NVDS_HDR_LEN 8U

#define NVDS_TAG_BD_ADDR     0x01U
#define NVDS_TAG_WAKEUP_TIME 0x0dU
#define NVDS_BD_ADDR_LEN     6U

#define NVDS_WAKEUP_TIME_US 4500U

// Maximum sleep duration (10 s), controller logs off, scheduling
static const uint8_t s_nvds_defaults[] = {0x12U, 0x01U, 0x01U, 0x2fU, 0x04U, 0x20U,
                                          0x00U, 0x00U, 0x00U, 0x15U, 0x01U, 0x01U};

#define RING_POOL_SIZE(region_size) (((region_size) - sizeof(struct ring)) & ~3U)

// LCPU to HCPU ring, in the LCPU SRAM region
#ifdef CONFIG_SOC_SF32LB57
#define RX_RING_OFFSET 0x13a00U
#else
#define RX_RING_OFFSET 0x2800U
#endif
#define RX_RING_SIZE      512U
#define RX_RING_POOL_SIZE RING_POOL_SIZE(RX_RING_SIZE)

static struct pbl_thread s_hci_thread;
PBL_THREAD_STACK_DEFINE(s_hci_task_stack, CONFIG_NIMBLE_HCI_THREAD_STACK_SIZE);
static PBL_SEM_DEFINE(s_hci_data_ready, 0, 1);
// Given once the LCPU runs, and given back by each waiter
static PBL_SEM_DEFINE(s_lcpu_ready, 0, 1);
static atomic_bool s_lcpu_up;
// Held while a packet is written, so that packets of two threads do not mix.
static PBL_MUTEX_DEFINE(s_tx_lock);
static struct hci_h4_sm s_hci_h4sm;

// Written data the LCPU has yet to take; sleep is blocked meanwhile
static bool s_busy;
// Data from the LCPU yet to read; it may sleep once there is none, so its RAM stays untouched
static volatile uint32_t s_rx_pending;

static inline volatile struct ring *prv_tx(void) {
  return (volatile struct ring *)pbl_rproc_sf32lb_get_ipc_mem(BT_CONTROLLER)->base;
}

static inline volatile struct ring *prv_rx(void) {
  return (volatile struct ring *)(pbl_rproc_sf32lb_get_sram(BT_CONTROLLER)->base + RX_RING_OFFSET);
}

static bool prv_ring_empty(volatile struct ring *ring) {
  return nimble_hci__ring_used(ring->rd_idx, ring->wr_idx, (uint32_t)ring->size) == 0U;
}

static void prv_ring_reset_tx(void) {
  const struct pbl_mem_region *mem = pbl_rproc_sf32lb_get_ipc_mem(BT_CONTROLLER);
  volatile struct ring *tx = prv_tx();

  tx->rd_idx = 0U;
  tx->wr_idx = 0U;
  tx->wr_buf = mem->base + sizeof(struct ring);
  tx->rd_buf = pbl_rproc_sf32lb_da(BT_CONTROLLER, tx->wr_buf);
  tx->size = (int16_t)RING_POOL_SIZE(mem->size);
  pbl_wmb();
}

static size_t prv_ring_put(volatile struct ring *ring, const uint8_t *data, size_t len) {
  uint32_t size = (uint32_t)ring->size;
  uint32_t wr = ring->wr_idx;
  uint32_t idx = PBL_FIELD_GET(RING_IDX_MASK, wr);
  uint8_t *pool = (uint8_t *)ring->wr_buf;
  uint32_t space = size - nimble_hci__ring_used(ring->rd_idx, wr, size);
  size_t first;

  len = PBL_MIN(len, space);
  if (len == 0U) {
    return 0U;
  }

  pbl_mb();

  first = PBL_MIN(len, size - idx);
  memcpy(&pool[idx], data, first);
  memcpy(pool, &data[first], len - first);

  pbl_wmb();
  ring->wr_idx = nimble_hci__ring_advance(wr, len, size);

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

  used = nimble_hci__ring_used(rd, wr, size);
  if (used == 0U) {
    return 0U;
  }

  pbl_rmb();
  *data = (const uint8_t *)rx->rd_buf + idx;

  return PBL_MIN(used, size - idx);
}

static void prv_ring_consume(size_t len) {
  volatile struct ring *rx = prv_rx();
  uint32_t size = (uint32_t)rx->size;

  pbl_mb();

  pbl_irq_lock();
  s_rx_pending = nimble_hci__ring_used(rx->rd_idx, rx->wr_idx, size) - len;
  rx->rd_idx = nimble_hci__ring_advance(rx->rd_idx, len, size);
  pbl_irq_unlock();
}

static void prv_set_busy(bool busy) {
  if (s_busy == busy) {
    return;
  }

  s_busy = busy;
  if (busy) {
    pbl_idle_state_block(SOC_SF32LB_IDLE_WFI);
  } else {
    pbl_idle_state_release(SOC_SF32LB_IDLE_WFI);
  }
}

static void prv_mbox_rx(const struct pbl_mbox_client *cl, const void *msg) {
  volatile struct ring *rx = prv_rx();

  s_rx_pending = nimble_hci__ring_used(rx->rd_idx, rx->wr_idx, (uint32_t)rx->size);
  pbl_sem_give(&s_hci_data_ready);
}

static void prv_mbox_tx_done(const struct pbl_mbox_client *cl, const void *msg, int ret) {
  pbl_irq_lock();
  if (prv_ring_empty(prv_tx())) {
    prv_set_busy(false);
  } else {
    // the LCPU took the doorbell before draining the ring
    (void)pbl_mbox_send(pbl_rproc_sf32lb_get_tx_chan(BT_CONTROLLER), nullptr);
  }
  pbl_irq_unlock();
}

static const struct pbl_mbox_client s_mbox_cl = {
  .rx_callback = prv_mbox_rx,
  .tx_done = prv_mbox_tx_done,
};

// Nothing of a packet is written until all of it fits: one that times out half written would
// leave the controller out of frame for good.
static int prv_tx_begin(size_t len) {
  volatile struct ring *tx = prv_tx();
  uint32_t size = (uint32_t)tx->size;
  uint64_t start = pbl_uptime_ms();

  if (len > size) {
    return -EMSGSIZE;
  }

  if (!atomic_load_explicit(&s_lcpu_up, memory_order_acquire)) {
    pbl_sem_take(&s_lcpu_ready, PBL_FOREVER);
    pbl_sem_give(&s_lcpu_ready);
  }

  pbl_mutex_lock(&s_tx_lock, PBL_FOREVER);

  while (size - nimble_hci__ring_used(tx->rd_idx, tx->wr_idx, size) < len) {
    if (pbl_uptime_ms() - start >= HCI_WRITE_TIMEOUT_MS) {
      pbl_mutex_unlock(&s_tx_lock);
      return -ETIMEDOUT;
    }

    pbl_sleep_ms(1U);
  }

  return 0;
}

static void prv_tx_put(const void *data, size_t len) {
  size_t written = prv_ring_put(prv_tx(), data, len);

  PBL_ASSERTN(written == len);
}

static void prv_tx_end(void) {
  pbl_irq_lock();
  prv_set_busy(true);
  pbl_irq_unlock();

  (void)pbl_mbox_send(pbl_rproc_sf32lb_get_tx_chan(BT_CONTROLLER), nullptr);

  pbl_mutex_unlock(&s_tx_lock);
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

#ifdef CONFIG_NIMBLE_BLE_BREDR_SCO
      nimble_hci__sf32lb_sco_event(ev);
#endif

      return ble_transport_to_hs_evt(data);
    case HCI_H4_ACL:
      return ble_transport_to_hs_acl(data);
    default:
      pbl_panic();
      break;
  }

  return -1;
}

static void prv_nvds_config(const uint8_t *bd_addr) {
  uint8_t *nvds = (uint8_t *)(pbl_rproc_sf32lb_get_sram(BT_CONTROLLER)->base + NVDS_OFFSET);
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
#ifdef CONFIG_NIMBLE_ID_ADDR
  ble_addr_t addr;
  int rc;

  rc = pbl_bt_id_addr_get(&addr);
  if (rc == -ENOENT) {
    // The ROM default is shared by every part: leave the controller without
    // one, so the host takes a random static address.
    addr = (ble_addr_t){.type = BLE_ADDR_PUBLIC};
  } else {
    PBL_ASSERT(rc == 0, "No identity address (%d)", rc);
  }

  prv_nvds_config(addr.type == BLE_ADDR_PUBLIC ? addr.val : nullptr);
#else
  prv_nvds_config(nullptr);
#endif
}

// Booting the LCPU calibrates the radio for about a second, so it is not done by the caller
static void prv_lcpu_boot(void) {
  int ret;

  pbl_rproc_sf32lb_sram_get(BT_CONTROLLER);
  prv_controller_config();
  pbl_rproc_sf32lb_sram_put(BT_CONTROLLER);

  ret = pbl_rproc_boot(BT_CONTROLLER);
  PBL_ASSERT(ret == 0, "LCPU boot failed (%d)", ret);

  atomic_store_explicit(&s_lcpu_up, true, memory_order_release);
  pbl_sem_give(&s_lcpu_ready);
}

#ifdef CONFIG_NIMBLE_BLE_BREDR_SCO
void nimble_hci__sf32lb_wake(void) {
  pbl_sem_give(&s_hci_data_ready);
}
#endif

static void prv_hci_task_main(void *unused) {
  prv_lcpu_boot();

#ifdef CONFIG_NIMBLE_BLE_BREDR_SCO
  nimble_hci__sf32lb_sco_init();
#endif

  while (true) {
    const uint8_t *data;
    size_t len;

    pbl_sem_take(&s_hci_data_ready, PBL_FOREVER);

    while ((len = prv_ring_peek(&data)) > 0U) {
      int consumed;

      // The allocators wait for a buffer, so this is not expected to fail.
      consumed = hci_h4_sm_rx(&s_hci_h4sm, data, len);
      if (consumed <= 0) {
        PBL_LOG_ERR("hci_h4_sm_rx returned %d", consumed);
        consumed = (int)len;
      }

      prv_ring_consume((size_t)consumed);
    }

#ifdef CONFIG_NIMBLE_BLE_BREDR_SCO
    nimble_hci__sf32lb_sco_poll();
#endif
  }
}

void ble_transport_ll_init(void) {
  int ret;

  // The role may name any remote processor; the getters below take the SF32LB driver's
  PBL_ASSERTN(BT_CONTROLLER->ops == &pbl_rproc_sf32lb_ops);

  nimble_hci__alloc_init();
  hci_h4_sm_init(&s_hci_h4sm, &nimble_hci__allocs_from_ll, prv_hci_frame_cb);

  prv_ring_reset_tx();

  ret = pbl_mbox_request(pbl_rproc_sf32lb_get_rx_chan(BT_CONTROLLER), &s_mbox_cl);
  PBL_ASSERT(ret == 0, "LCPU RX doorbell request failed (%d)", ret);

  ret = pbl_mbox_request(pbl_rproc_sf32lb_get_tx_chan(BT_CONTROLLER), &s_mbox_cl);
  PBL_ASSERT(ret == 0, "LCPU TX doorbell request failed (%d)", ret);

  struct pbl_thread_attr attr = {
    .name = "NimbleHCI",
    .entry = prv_hci_task_main,
    .prio = CONFIG_NIMBLE_HCI_THREAD_PRIO,
    .privileged = true,
    .stack = s_hci_task_stack,
    .stack_size = sizeof(s_hci_task_stack),
  };

  PBL_ASSERTN(pbl_thread_create(&s_hci_thread, &attr) == 0);
}

static int prv_write_mbuf(uint8_t h4_type, struct os_mbuf *om) {
  int ret;

  ret = prv_tx_begin(1U + OS_MBUF_PKTLEN(om));
  if (ret == 0) {
    prv_tx_put(&h4_type, 1U);
    for (struct os_mbuf *x = om; x != NULL; x = SLIST_NEXT(x, om_next)) {
      prv_tx_put(x->om_data, x->om_len);
    }
    prv_tx_end();
  }

  os_mbuf_free_chain(om);

  return ret;
}

/* APIs to be implemented by HS/LL side of transports */
int ble_transport_to_ll_cmd_impl(void *buf) {
  struct ble_hci_cmd *cmd = buf;
  uint8_t h4_cmd = HCI_H4_CMD;
  int ret;

  ret = prv_tx_begin(1U + sizeof(*cmd) + cmd->length);
  if (ret == 0) {
    prv_tx_put(&h4_cmd, 1U);
    prv_tx_put(cmd, sizeof(*cmd) + cmd->length);
    prv_tx_end();
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

  ret = prv_write_mbuf(HCI_H4_ACL, om);

  if (ret != 0) {
    PBL_LOG_ERR("Failed to write HCI ACL (%d)", ret);
    return BLE_ERR_MEM_CAPACITY;
  }

  return 0;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  int ret;

  ret = prv_write_mbuf(HCI_H4_ISO, om);

  if (ret != 0) {
    PBL_LOG_ERR("Failed to write HCI ISO (%d)", ret);
    return BLE_ERR_MEM_CAPACITY;
  }

  return 0;
}

struct pbl_thread *nimble_port_pebble_hci_thread(void) {
  return &s_hci_thread;
}
