/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sf32lb_sco.h"

#include "sf32lb_ring.h"

#include <string.h>

#include <pbl/board.h>
#include <pbl/drivers/mbox.h>
#include <pbl/drivers/remoteproc/sf32lb.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/barrier.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

// NOTE: transport.h needs os_mbuf.h to be included first
// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/hci_common.h>
#include <nimble/transport.h>

PBL_LOG_MODULE_DECLARE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

// Audio memory the LCPU ROM shares, in its SRAM region: a ring each way, then their pools
#define AUDIO_MEM_OFFSET    0xe000U
#define AUDIO_DOWNLINK_RING 0x00U
#define AUDIO_UPLINK_RING   0x20U
#define AUDIO_DOWNLINK_POOL 0x50U
#define AUDIO_POOL_SIZE     460U
#define AUDIO_UPLINK_POOL   (AUDIO_DOWNLINK_POOL + AUDIO_POOL_SIZE)

// Each received packet comes behind a header: length, HCI packet status, two unused bytes
#define FRAME_HDR_LEN    4U
#define FRAME_STATUS_MAX 3U

#define SCO_HDR_LEN      3U
#define SCO_MAX_DATA_LEN 255U
#define SCO_STATUS_SHIFT 12U

#define AIR_MODE_CVSD 0x02U

// Synchronous Connection Complete, past the event header
#define SYNC_CONN_STATUS   0U
#define SYNC_CONN_HANDLE   1U
#define SYNC_CONN_RX_LEN   12U
#define SYNC_CONN_TX_LEN   14U
#define SYNC_CONN_AIR_MODE 16U
#define SYNC_CONN_LEN      17U

// Command Complete, past the event header
#define CMD_COMPLETE_OPCODE 1U
#define CMD_COMPLETE_LEN    4U
#define HCI_RESET_OPCODE    BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND, BLE_HCI_OCF_CB_RESET)

static PBL_MUTEX_DEFINE(s_lock);

static bool s_active;
static uint16_t s_handle;
// Samples of a packet, in bytes: twice what it takes on air
static uint16_t s_frame_len;

static uintptr_t prv_mem(void) {
  return pbl_rproc_sf32lb_get_sram(BT_CONTROLLER)->base + AUDIO_MEM_OFFSET;
}

static volatile struct ring *prv_downlink(void) {
  return (volatile struct ring *)(prv_mem() + AUDIO_DOWNLINK_RING);
}

static volatile struct ring *prv_uplink(void) {
  return (volatile struct ring *)(prv_mem() + AUDIO_UPLINK_RING);
}

static void prv_ring_reset(volatile struct ring *ring, uintptr_t pool) {
  ring->rd_buf = pool;
  ring->wr_buf = pool;
  ring->rd_idx = 0U;
  ring->wr_idx = 0U;
  ring->size = (int16_t)AUDIO_POOL_SIZE;
}

static void prv_rings_reset(void) {
  prv_ring_reset(prv_downlink(), prv_mem() + AUDIO_DOWNLINK_POOL);
  prv_ring_reset(prv_uplink(), prv_mem() + AUDIO_UPLINK_POOL);
  pbl_wmb();
}

static void prv_ring_peek(volatile struct ring *ring, uint8_t *data, uint32_t len) {
  const uint8_t *pool = (const uint8_t *)ring->rd_buf;
  uint32_t idx = PBL_FIELD_GET(RING_IDX_MASK, ring->rd_idx);
  uint32_t first = PBL_MIN(len, AUDIO_POOL_SIZE - idx);

  memcpy(data, &pool[idx], first);
  memcpy(&data[first], pool, len - first);
}

static void prv_mbox_rx(const struct pbl_mbox_client *cl, const void *msg) {
  nimble_hci__sf32lb_wake();
}

static const struct pbl_mbox_client s_mbox_cl = {
  .rx_callback = prv_mbox_rx,
};

static void prv_stop(void) {
  if (!s_active) {
    return;
  }

  s_active = false;
  prv_rings_reset();
  pbl_rproc_sf32lb_sram_put(BT_CONTROLLER);
}

void nimble_hci__sf32lb_sco_event(const struct ble_hci_ev *ev) {
  const uint8_t *data = ev->data;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  switch (ev->opcode) {
    case BLE_HCI_EVCODE_SYNCH_CONN_COMP: {
      uint16_t rx_len, tx_len;

      if (ev->length < SYNC_CONN_LEN || data[SYNC_CONN_STATUS] != 0U || s_active) {
        break;
      }

      rx_len = pbl_get_le16(&data[SYNC_CONN_RX_LEN]);
      tx_len = pbl_get_le16(&data[SYNC_CONN_TX_LEN]);
      if (data[SYNC_CONN_AIR_MODE] != AIR_MODE_CVSD || rx_len != tx_len || rx_len == 0U ||
          2U * rx_len > SCO_MAX_DATA_LEN) {
        PBL_LOG_WRN("Synchronous connection without audio: air mode %u, lengths %u/%u",
                    data[SYNC_CONN_AIR_MODE], rx_len, tx_len);
        break;
      }

      // LPSYS stays awake for the call: the rings are read and written all along
      pbl_rproc_sf32lb_sram_get(BT_CONTROLLER);
      s_handle = pbl_get_le16(&data[SYNC_CONN_HANDLE]);
      s_frame_len = 2U * rx_len;
      s_active = true;
      PBL_LOG_DBG("Audio of connection %u: %u bytes a packet", s_handle, s_frame_len);
      break;
    }
    case BLE_HCI_EVCODE_COMMAND_COMPLETE:
      // The controller forgets the rings when it resets
      if (ev->length >= CMD_COMPLETE_LEN &&
          pbl_get_le16(&data[CMD_COMPLETE_OPCODE]) == HCI_RESET_OPCODE) {
        prv_stop();
        pbl_rproc_sf32lb_sram_get(BT_CONTROLLER);
        prv_rings_reset();
        pbl_rproc_sf32lb_sram_put(BT_CONTROLLER);
      }
      break;
    case BLE_HCI_EVCODE_DISCONN_CMP:
      if (ev->length >= 3U && data[0] == 0U && s_active && pbl_get_le16(&data[1]) == s_handle) {
        prv_stop();
      }
      break;
    case BLE_HCI_EVCODE_HW_ERROR:
      prv_stop();
      break;
    default:
      break;
  }

  pbl_mutex_unlock(&s_lock);
}

// Takes a packet out of the downlink ring, as HCI synchronous data
static size_t prv_receive(uint8_t *pkt) {
  volatile struct ring *ring = prv_downlink();
  uint8_t hdr[FRAME_HDR_LEN];
  uint32_t used;
  size_t len = 0U;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (!s_active) {
    goto out;
  }

  pbl_rmb();
  used = nimble_hci__ring_used(ring->rd_idx, ring->wr_idx, AUDIO_POOL_SIZE);
  if (used < FRAME_HDR_LEN) {
    goto out;
  }

  prv_ring_peek(ring, hdr, sizeof(hdr));
  if (hdr[0] != s_frame_len || hdr[1] > FRAME_STATUS_MAX) {
    // Out of step with the controller: start over from what it writes next
    PBL_LOG_WRN("Bad audio frame: %u %u", hdr[0], hdr[1]);
    ring->rd_idx = ring->wr_idx;
    goto out;
  }

  if (used < FRAME_HDR_LEN + s_frame_len) {
    goto out;
  }

  ring->rd_idx = nimble_hci__ring_advance(ring->rd_idx, FRAME_HDR_LEN, AUDIO_POOL_SIZE);
  prv_ring_peek(ring, &pkt[SCO_HDR_LEN], s_frame_len);
  pbl_mb();
  ring->rd_idx = nimble_hci__ring_advance(ring->rd_idx, s_frame_len, AUDIO_POOL_SIZE);

  pbl_put_le16(s_handle | ((uint16_t)hdr[1] << SCO_STATUS_SHIFT), pkt);
  pkt[2] = (uint8_t)s_frame_len;
  len = SCO_HDR_LEN + s_frame_len;

out:
  pbl_mutex_unlock(&s_lock);

  return len;
}

void nimble_hci__sf32lb_sco_poll(void) {
  uint8_t pkt[SCO_HDR_LEN + SCO_MAX_DATA_LEN];
  size_t len;

  while ((len = prv_receive(pkt)) > 0U) {
    (void)ble_transport_to_hs_sco(pkt, (uint16_t)len);
  }
}

int ble_transport_to_ll_sco_impl(const uint8_t *pkt, uint16_t len) {
  volatile struct ring *ring = prv_uplink();
  uint32_t wr, idx, first;
  uint8_t *pool;
  int ret = 0;

  if (len < SCO_HDR_LEN || len != SCO_HDR_LEN + pkt[2]) {
    return BLE_ERR_INV_HCI_CMD_PARMS;
  }

  pkt += SCO_HDR_LEN;
  len -= SCO_HDR_LEN;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (!s_active) {
    ret = BLE_ERR_UNK_CONN_ID;
    goto out;
  }

  wr = ring->wr_idx;
  if (AUDIO_POOL_SIZE - nimble_hci__ring_used(ring->rd_idx, wr, AUDIO_POOL_SIZE) < len) {
    ret = BLE_ERR_MEM_CAPACITY;
    goto out;
  }

  pool = (uint8_t *)ring->wr_buf;
  idx = PBL_FIELD_GET(RING_IDX_MASK, wr);
  first = PBL_MIN((uint32_t)len, AUDIO_POOL_SIZE - idx);
  memcpy(&pool[idx], pkt, first);
  memcpy(pool, &pkt[first], len - first);

  pbl_wmb();
  ring->wr_idx = nimble_hci__ring_advance(wr, len, AUDIO_POOL_SIZE);

out:
  pbl_mutex_unlock(&s_lock);

  return ret;
}

void nimble_hci__sf32lb_sco_init(void) {
  int ret;

  pbl_rproc_sf32lb_sram_get(BT_CONTROLLER);
  prv_rings_reset();
  pbl_rproc_sf32lb_sram_put(BT_CONTROLLER);

  // The LCPU rings for each packet it received
  ret = pbl_mbox_request(pbl_rproc_sf32lb_get_audio_rx_chan(BT_CONTROLLER), &s_mbox_cl);
  PBL_ASSERT(ret == 0, "Audio RX doorbell request failed (%d)", ret);

  ret = pbl_mbox_request(pbl_rproc_sf32lb_get_audio_tx_chan(BT_CONTROLLER), &s_mbox_cl);
  PBL_ASSERT(ret == 0, "Audio TX doorbell request failed (%d)", ret);
}
