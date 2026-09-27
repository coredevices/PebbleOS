/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// A controller that only exists to let the host synchronise: every command
// completes with success and no event is ever raised, so no link can form.

#include <stdint.h>
#include <string.h>

#include <pbl/util/rand32.h>

// clang-format off
#include <os/os_mbuf.h>
// clang-format on
#include <nimble/hci_common.h>
#include <nimble/transport.h>
#include <nimble/transport_impl.h>
#include <os/endian.h>

// Any value works: nothing is on the air to clash with it.
static const uint8_t s_bd_addr[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

// Return parameters the host parses; the length must match what it expects.
// Commands not listed here complete with status only.
struct fake_rp {
  uint16_t opcode;
  uint8_t len;
  void (*fill)(uint8_t *rp);
};

static void prv_fill_local_ver(uint8_t *rp) {
  struct ble_hci_ip_rd_local_ver_rp *v = (void *)rp;
  v->hci_ver = BLE_HCI_VER_BCS_4_2;
  v->lmp_ver = BLE_HCI_VER_BCS_4_2;
}

static void prv_fill_loc_supp_feat(uint8_t *rp) {
  struct ble_hci_ip_rd_loc_supp_feat_rp *f = (void *)rp;
  // BR/EDR not supported, LE supported (controller)
  f->features = htole64(0x0000006000000000ULL);
}

static void prv_fill_le_buf_size(uint8_t *rp) {
  struct ble_hci_le_rd_buf_size_rp *b = (void *)rp;
  b->data_len = htole16(27);
  b->data_packets = 4;
}

static void prv_fill_bd_addr(uint8_t *rp) {
  memcpy(rp, s_bd_addr, sizeof(s_bd_addr));
}

static void prv_fill_rand(uint8_t *rp) {
  struct ble_hci_le_rand_rp *r = (void *)rp;
  r->random_number = ((uint64_t)rand32() << 32) | rand32();
}

static void prv_fill_sugg_def_data_len(uint8_t *rp) {
  struct ble_hci_le_rd_sugg_def_data_len_rp *d = (void *)rp;
  d->max_tx_octets = htole16(27);
  d->max_tx_time = htole16(328);
}

static void prv_fill_list_size(uint8_t *rp) {
  rp[0] = 8;
}

#define RP(ogf, ocf, type, fn) \
  {BLE_HCI_OP(BLE_HCI_OGF_##ogf, BLE_HCI_OCF_##ocf), sizeof(struct ble_hci_##type##_rp), fn}

static const struct fake_rp s_rps[] = {
  RP(INFO_PARAMS, IP_RD_LOCAL_VER, ip_rd_local_ver, prv_fill_local_ver),
  RP(INFO_PARAMS, IP_RD_LOC_SUPP_CMD, ip_rd_loc_supp_cmd, NULL),
  RP(INFO_PARAMS, IP_RD_LOC_SUPP_FEAT, ip_rd_loc_supp_feat, prv_fill_loc_supp_feat),
  RP(INFO_PARAMS, IP_RD_BUF_SIZE, ip_rd_buf_size, NULL),
  RP(INFO_PARAMS, IP_RD_BD_ADDR, ip_rd_bd_addr, prv_fill_bd_addr),
  RP(STATUS_PARAMS, RD_RSSI, rd_rssi, NULL),
  RP(LE, LE_RD_BUF_SIZE, le_rd_buf_size, prv_fill_le_buf_size),
  RP(LE, LE_RD_LOC_SUPP_FEAT, le_rd_loc_supp_feat, NULL),
  RP(LE, LE_RD_ADV_CHAN_TXPWR, le_rd_adv_chan_txpwr, NULL),
  RP(LE, LE_RD_WHITE_LIST_SIZE, le_rd_white_list, prv_fill_list_size),
  RP(LE, LE_RD_CHAN_MAP, le_rd_chan_map, NULL),
  RP(LE, LE_RAND, le_rand, prv_fill_rand),
  RP(LE, LE_LT_KEY_REQ_REPLY, le_lt_key_req_reply, NULL),
  RP(LE, LE_LT_KEY_REQ_NEG_REPLY, le_lt_key_req_neg_reply, NULL),
  RP(LE, LE_SET_DATA_LEN, le_set_data_len, NULL),
  RP(LE, LE_RD_SUGG_DEF_DATA_LEN, le_rd_sugg_def_data_len, prv_fill_sugg_def_data_len),
  RP(LE, LE_RD_RESOLV_LIST_SIZE, le_rd_resolv_list_size, prv_fill_list_size),
  RP(LE, LE_RD_PHY, le_rd_phy, NULL),
};

static const struct fake_rp *prv_find_rp(uint16_t opcode) {
  for (size_t i = 0; i < sizeof(s_rps) / sizeof(s_rps[0]); i++) {
    if (s_rps[i].opcode == opcode) {
      return &s_rps[i];
    }
  }
  return NULL;
}

void ble_transport_ll_init(void) {
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  struct ble_hci_cmd *cmd = buf;
  const struct fake_rp *rp = prv_find_rp(le16toh(cmd->opcode));
  uint8_t rp_len = rp ? rp->len : 0;
  struct ble_hci_ev *ev;
  struct ble_hci_ev_command_complete *cc;
  int rc;

  ev = ble_transport_alloc_evt(0);
  if (ev == NULL) {
    rc = BLE_ERR_MEM_CAPACITY;
    goto done;
  }

  cc = (void *)ev->data;
  ev->opcode = BLE_HCI_EVCODE_COMMAND_COMPLETE;
  ev->length = sizeof(*cc) + rp_len;
  cc->num_packets = 1;
  cc->opcode = cmd->opcode;
  cc->status = BLE_ERR_SUCCESS;
  memset(cc->return_params, 0, rp_len);
  if (rp != NULL && rp->fill != NULL) {
    rp->fill(cc->return_params);
  }

  // Delivered synchronously: the host stores the ack and releases the
  // semaphore it is about to wait on.
  rc = ble_transport_to_hs_evt(ev);

done:
  ble_transport_free(buf);
  return rc;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  os_mbuf_free_chain(om);
  return 0;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  os_mbuf_free_chain(om);
  return 0;
}
