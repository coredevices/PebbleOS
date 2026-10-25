/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/bluetooth/hci_router.h>

#define EVT_DISCONNECTION_COMPLETE   0x05U
#define EVT_ENCRYPTION_CHANGE        0x08U
#define EVT_READ_REMOTE_VERSION      0x0CU
#define EVT_COMMAND_COMPLETE         0x0EU
#define EVT_COMMAND_STATUS           0x0FU
#define EVT_NUM_COMPLETED_PACKETS    0x13U
#define EVT_CONNECTION_COMPLETE      0x03U
#define EVT_SYNC_CONNECTION_COMPLETE 0x2CU
#define EVT_ENCRYPTION_KEY_REFRESH   0x30U
#define EVT_LE_META                  0x3EU
#define EVT_AUTH_PAYLOAD_TIMEOUT     0x57U
#define EVT_ENCRYPTION_CHANGE_V2     0x59U
#define LE_SUBEV_ADV_REPORT          0x02U

#define OP_SET_EVENT_MASK             0x0C01U
#define OP_RESET                      0x0C03U
#define OP_SET_CTL_TO_HOST_FLOW       0x0C31U
#define OP_HOST_BUFFER_SIZE           0x0C33U
#define OP_HOST_NUM_COMPLETED_PACKETS 0x0C35U
#define OP_SET_EVENT_MASK_PAGE_2      0x0C63U
#define OP_WRITE_LE_HOST_SUPP         0x0C6DU
#define OP_LE_SET_EVENT_MASK          0x2001U
#define OP_LE_READ_BUFFER_SIZE        0x2002U
#define OP_LE_READ_LOCAL_FEATURES     0x2003U
#define OP_LE_SET_RANDOM_ADDRESS      0x2005U
#define OP_LE_READ_ADV_CHAN_TX_POWER  0x2007U
#define OP_LE_READ_FAL_SIZE           0x200FU
#define OP_LE_RAND                    0x2018U
#define OP_LE_READ_SUPP_STATES        0x201CU
#define OP_LE_READ_DEFAULT_DATA_LEN   0x2023U
#define OP_LE_WRITE_DEFAULT_DATA_LEN  0x2024U
#define OP_LE_READ_RL_SIZE            0x202AU
#define OP_LE_READ_MAX_DATA_LEN       0x202FU
#define OP_LE_READ_PER_ADV_LIST_SIZE  0x204AU
#define OP_LE_READ_TX_POWER           0x204BU
#define OP_LE_READ_BUFFER_SIZE_V2     0x2060U
#define OP_LE_SET_HOST_FEATURE        0x2074U
#define OP_LE_READ_ALL_LOCAL_FEATURES 0x2087U
#define OP_SF32LB52_BLE_READY         0xFC11U

#define OGF_LE     0x08U
#define OGF_VENDOR 0x3FU

#define STATUS_UNKNOWN_CMD    0x01U
#define STATUS_CMD_DISALLOWED 0x0CU

#define HANDLE_MASK 0x0FFFU

enum {
  RX_TYPE,
  RX_HDR,
  RX_BODY,
};

static uint16_t prv_le16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

static uint64_t prv_le64(const uint8_t *p) {
  uint64_t v = 0;
  for (int i = 7; i >= 0; i--) {
    v = (v << 8) | p[i];
  }
  return v;
}

static void prv_put_le64(uint8_t *p, uint64_t v) {
  for (int i = 0; i < 8; i++) {
    p[i] = (uint8_t)(v >> (8 * i));
  }
}

// Events only a BR/EDR host has a use for (Core v5.4, Vol 4, Part E, 7.7).
static bool prv_is_classic_event(uint8_t code) {
  switch (code) {
    case 0x01: // Inquiry Complete
    case 0x02: // Inquiry Result
    case 0x03: // Connection Complete
    case 0x04: // Connection Request
    case 0x06: // Authentication Complete
    case 0x07: // Remote Name Request Complete
    case 0x09: // Change Connection Link Key Complete
    case 0x0A: // Link Key Type Changed
    case 0x0B: // Read Remote Supported Features Complete
    case 0x0D: // QoS Setup Complete
    case 0x11: // Flush Occurred
    case 0x12: // Role Change
    case 0x14: // Mode Change
    case 0x15: // Return Link Keys
    case 0x16: // PIN Code Request
    case 0x17: // Link Key Request
    case 0x18: // Link Key Notification
    case 0x1B: // Max Slots Change
    case 0x1C: // Read Clock Offset Complete
    case 0x1D: // Connection Packet Type Changed
    case 0x1E: // QoS Violation
    case 0x20: // Page Scan Repetition Mode Change
    case 0x21: // Flow Specification Complete
    case 0x22: // Inquiry Result with RSSI
    case 0x23: // Read Remote Extended Features Complete
    case 0x2C: // Synchronous Connection Complete
    case 0x2D: // Synchronous Connection Changed
    case 0x2E: // Sniff Subrating
    case 0x2F: // Extended Inquiry Result
    case 0x31: // IO Capability Request
    case 0x32: // IO Capability Response
    case 0x33: // User Confirmation Request
    case 0x34: // User Passkey Request
    case 0x35: // Remote OOB Data Request
    case 0x36: // Simple Pairing Complete
    case 0x38: // Link Supervision Timeout Changed
    case 0x39: // Enhanced Flush Complete
    case 0x3B: // User Passkey Notification
    case 0x3C: // Keypress Notification
    case 0x3D: // Remote Host Supported Features Notification
      return true;
    default:
      return false;
  }
}

// Offset of the connection handle in the parameters of events either host can
// receive, or -1.
static int prv_shared_handle_offset(uint8_t code) {
  switch (code) {
    case EVT_DISCONNECTION_COMPLETE:
    case EVT_ENCRYPTION_CHANGE:
    case EVT_READ_REMOTE_VERSION:
    case EVT_ENCRYPTION_KEY_REFRESH:
    case EVT_ENCRYPTION_CHANGE_V2:
      return 1;
    case EVT_AUTH_PAYLOAD_TIMEOUT:
      return 0;
    default:
      return -1;
  }
}

bool hci_router_handle_is_classic(const struct hci_router *r, uint16_t handle) {
  handle &= HANDLE_MASK;
  for (uint8_t i = 0; i < r->num_classic_handles; i++) {
    if (r->classic_handles[i] == handle) {
      return true;
    }
  }
  return false;
}

static void prv_add_classic_handle(struct hci_router *r, uint16_t handle) {
  handle &= HANDLE_MASK;
  if (hci_router_handle_is_classic(r, handle) ||
      r->num_classic_handles >= HCI_ROUTER_MAX_CLASSIC_HANDLES) {
    return;
  }
  r->classic_handles[r->num_classic_handles++] = handle;
}

static void prv_remove_classic_handle(struct hci_router *r, uint16_t handle) {
  handle &= HANDLE_MASK;
  for (uint8_t i = 0; i < r->num_classic_handles; i++) {
    if (r->classic_handles[i] == handle) {
      r->classic_handles[i] = r->classic_handles[--r->num_classic_handles];
      return;
    }
  }
}

void hci_router_init(struct hci_router *r, const struct hci_router_ops *ops,
                     size_t nimble_evt_max) {
  memset(r, 0, sizeof(*r));
  r->ops = ops;
  r->nimble_evt_max = nimble_evt_max;
  r->rx_state = RX_TYPE;
}

void hci_router_reset(struct hci_router *r) {
  r->rx_state = RX_TYPE;
  r->rx_discard = false;
  r->num_classic_handles = 0;
  r->nimble_evt_mask_valid = false;
  r->nimble_evt_mask2_valid = false;
  if (r->cmd_pending) {
    r->cmd_pending = false;
    r->ops->gate_give(r->ops->ctx);
  }
}

void hci_router_set_classic(struct hci_router *r, hci_router_classic_cb_t evt,
                            hci_router_classic_cb_t acl, void *ctx) {
  r->classic_ctx = ctx;
  r->classic_evt = evt;
  r->classic_acl = acl;
}

/* Classic HCI log --------------------------------------------------------------*/

static void prv_log(struct hci_router *r, uint8_t kind, uint8_t action, uint16_t code,
                    const uint8_t *params, size_t len) {
  // Commands come from the classic host's thread, events from the HCI thread; if one
  // preempts the other here, an entry is lost. No exclusive access: it can hang on the
  // classic host's thread (see the Zephyr port's atomics).
  const uint16_t seq = r->log_seq++;
  struct hci_router_log_entry *e = &r->log[seq % HCI_ROUTER_LOG_LEN];
  *e = (struct hci_router_log_entry){
    .seq = seq,
    .kind = kind,
    .action = action,
    .code = code,
    .len = (uint8_t)(len > UINT8_MAX ? UINT8_MAX : len),
  };
  memcpy(e->params, params, len < sizeof(e->params) ? len : sizeof(e->params));
}

size_t hci_router_log_copy(const struct hci_router *r, struct hci_router_log_entry *out) {
  const uint16_t n = r->log_seq < HCI_ROUTER_LOG_LEN ? r->log_seq : HCI_ROUTER_LOG_LEN;
  for (uint16_t i = 0; i < n; i++) {
    out[i] = r->log[(uint16_t)(r->log_seq - n + i) % HCI_ROUTER_LOG_LEN];
  }
  return n;
}

/* LE pairing: SMP Pairing Request / Response on the LE fixed channel */

#define L2CAP_CID_SMP       0x0006U
#define SMP_PAIRING_REQUEST 0x01U
#define SMP_PAIRING_RSP     0x02U

// @p acl starts with the ACL header; only the start of the packet is needed.
static void prv_smp_sniff(struct hci_router *r, const uint8_t *acl, size_t len, bool rx) {
  // ACL header (4), L2CAP header (4), SMP code and 6 parameters.
  // A continuation fragment (packet boundary 0b01) has no L2CAP header.
  if (len < 4U + 4U + 7U || ((acl[1] >> 4) & 0x3U) == 0x1U || prv_le16(&acl[6]) != L2CAP_CID_SMP) {
    return;
  }
  const uint8_t *smp = &acl[8];
  if (smp[0] != SMP_PAIRING_REQUEST && smp[0] != SMP_PAIRING_RSP) {
    return;
  }
  struct hci_router_smp_pdu *pdu = smp[0] == SMP_PAIRING_REQUEST ? &r->smp_req : &r->smp_rsp;
  *pdu = (struct hci_router_smp_pdu){
    .valid = true,
    .rx = rx,
    .io_cap = smp[1],
    .oob = smp[2],
    .authreq = smp[3],
    .max_key_size = smp[4],
    .init_key_dist = smp[5],
    .resp_key_dist = smp[6],
  };
}

static void prv_to_classic_evt(struct hci_router *r, const uint8_t *evt, size_t len) {
  // Completed packets come with every data packet and would push the rest out of the log.
  if (evt[0] != EVT_NUM_COMPLETED_PACKETS) {
    prv_log(r, HCI_ROUTER_LOG_EVT, 0, evt[0], &evt[2], len - 2U);
  }
  if (evt[0] == EVT_SYNC_CONNECTION_COMPLETE && len >= 2U + HCI_ROUTER_SYNC_CONN_LEN) {
    memcpy(r->sync_conn, &evt[2], HCI_ROUTER_SYNC_CONN_LEN);
    r->sync_conn_valid = true;
  }
  if (r->classic_evt == NULL) {
    r->stats.drop_classic++;
    return;
  }
  r->stats.classic_evt++;
  r->classic_evt(r->classic_ctx, evt, len);
}

static void prv_to_nimble_evt(struct hci_router *r, const uint8_t *evt, size_t len) {
  if (len > r->nimble_evt_max) {
    r->stats.drop_evt_too_long++;
    return;
  }
  const bool discardable = evt[0] == EVT_LE_META && len > 2 && evt[2] == LE_SUBEV_ADV_REPORT;
  if (r->ops->nimble_evt(r->ops->ctx, evt, len, discardable) < 0) {
    r->stats.drop_alloc++;
    return;
  }
  r->stats.nimble_evt++;
}

static void prv_to_owner_evt(struct hci_router *r, enum hci_router_owner owner, const uint8_t *evt,
                             size_t len) {
  switch (owner) {
    case HCI_ROUTER_OWNER_CLASSIC:
      prv_to_classic_evt(r, evt, len);
      break;
    case HCI_ROUTER_OWNER_ROUTER:
      r->stats.router_acks++;
      break;
    default:
      prv_to_nimble_evt(r, evt, len);
      break;
  }
}

static void prv_rx_cmd_ack(struct hci_router *r, uint16_t opcode, const uint8_t *evt, size_t len) {
  if (r->cmd_pending && opcode == r->cmd_opcode) {
    const enum hci_router_owner owner = (enum hci_router_owner)r->cmd_owner;
    r->cmd_pending = false;
    // Free the gate before delivering, so the host can send its next command.
    r->ops->gate_give(r->ops->ctx);
    prv_to_owner_evt(r, owner, evt, len);
    return;
  }

  if (evt[0] == EVT_COMMAND_COMPLETE && opcode == OP_SF32LB52_BLE_READY) {
    r->stats.drop_vendor_ready++;
    return;
  }

  // A no-op (opcode 0) only returns command credits.
  if (opcode != 0) {
    r->stats.cc_unmatched++;
  }
  prv_to_nimble_evt(r, evt, len);
}

static void prv_rx_nocp(struct hci_router *r, const uint8_t *evt, size_t len) {
  if (len < 3U) {
    r->stats.drop_malformed++;
    return;
  }
  const uint8_t *params = &evt[2];
  const uint8_t n = params[0];
  if (len < 3U + 4U * n) {
    r->stats.drop_malformed++;
    return;
  }

  uint8_t n_classic = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (hci_router_handle_is_classic(r, prv_le16(&params[1 + 4 * i]))) {
      n_classic++;
    }
  }

  if (n_classic == 0) {
    prv_to_nimble_evt(r, evt, len);
    return;
  }
  if (n_classic == n) {
    prv_to_classic_evt(r, evt, len);
    return;
  }

  r->stats.nocp_split++;
  for (int classic = 0; classic <= 1; classic++) {
    uint8_t *out = r->scratch;
    uint8_t count = 0;
    for (uint8_t i = 0; i < n; i++) {
      const uint8_t *entry = &params[1 + 4 * i];
      if (hci_router_handle_is_classic(r, prv_le16(entry)) == (classic != 0)) {
        memcpy(&out[3 + 4 * count], entry, 4);
        count++;
      }
    }
    out[0] = EVT_NUM_COMPLETED_PACKETS;
    out[1] = (uint8_t)(1 + 4 * count);
    out[2] = count;
    if (classic) {
      prv_to_classic_evt(r, out, 3U + 4U * count);
    } else {
      prv_to_nimble_evt(r, out, 3U + 4U * count);
    }
  }
}

static void prv_rx_evt(struct hci_router *r, const uint8_t *evt, size_t len) {
  const uint8_t code = evt[0];
  const uint8_t *params = &evt[2];
  const size_t plen = len - 2U;

  if (code == EVT_COMMAND_COMPLETE && plen >= 3U) {
    prv_rx_cmd_ack(r, prv_le16(&params[1]), evt, len);
    return;
  }
  if (code == EVT_COMMAND_STATUS && plen >= 4U) {
    prv_rx_cmd_ack(r, prv_le16(&params[2]), evt, len);
    return;
  }
  if (code == EVT_NUM_COMPLETED_PACKETS) {
    prv_rx_nocp(r, evt, len);
    return;
  }

  if (prv_is_classic_event(code)) {
    if ((code == EVT_CONNECTION_COMPLETE || code == EVT_SYNC_CONNECTION_COMPLETE) && plen >= 3U &&
        params[0] == 0) {
      prv_add_classic_handle(r, prv_le16(&params[1]));
    }
    prv_to_classic_evt(r, evt, len);
    return;
  }

  const int off = prv_shared_handle_offset(code);
  if (off >= 0 && plen >= (size_t)off + 2U) {
    const uint16_t handle = prv_le16(&params[off]);
    if (hci_router_handle_is_classic(r, handle)) {
      if (code == EVT_DISCONNECTION_COMPLETE && params[0] == 0) {
        prv_remove_classic_handle(r, handle);
      }
      prv_to_classic_evt(r, evt, len);
      return;
    }
  }

  prv_to_nimble_evt(r, evt, len);
}

static void prv_rx_acl(struct hci_router *r, const uint8_t *acl, size_t len) {
  if (hci_router_handle_is_classic(r, prv_le16(acl))) {
    if (r->classic_acl == NULL) {
      r->stats.drop_classic++;
      return;
    }
    r->stats.classic_acl++;
    r->classic_acl(r->classic_ctx, acl, len);
    return;
  }

  prv_smp_sniff(r, acl, len, true);
  if (r->ops->nimble_acl(r->ops->ctx, acl, len) < 0) {
    r->stats.drop_alloc++;
    return;
  }
  r->stats.nimble_acl++;
}

static void prv_rx_complete(struct hci_router *r) {
  if (r->rx_discard) {
    switch (r->rx_type) {
      case HCI_ROUTER_H4_SCO:
        r->stats.drop_sco++;
        break;
      case HCI_ROUTER_H4_ISO:
        r->stats.drop_iso++;
        break;
      default:
        r->stats.drop_oversize++;
        break;
    }
  } else if (r->rx_type == HCI_ROUTER_H4_EVT) {
    prv_rx_evt(r, r->rx_buf, r->rx_total);
  } else {
    prv_rx_acl(r, r->rx_buf, r->rx_total);
  }
  r->rx_state = RX_TYPE;
}

static uint8_t prv_hdr_len(uint8_t type) {
  switch (type) {
    case HCI_ROUTER_H4_ACL:
    case HCI_ROUTER_H4_ISO:
      return 4;
    case HCI_ROUTER_H4_SCO:
      return 3;
    case HCI_ROUTER_H4_EVT:
      return 2;
    default:
      return 0;
  }
}

static uint16_t prv_payload_len(uint8_t type, const uint8_t *hdr) {
  switch (type) {
    case HCI_ROUTER_H4_ACL:
      return prv_le16(&hdr[2]);
    case HCI_ROUTER_H4_ISO:
      return prv_le16(&hdr[2]) & 0x3FFFU;
    case HCI_ROUTER_H4_SCO:
      return hdr[2];
    default:
      return hdr[1];
  }
}

void hci_router_rx(struct hci_router *r, const uint8_t *data, size_t len) {
  while (len > 0) {
    switch (r->rx_state) {
      case RX_TYPE:
        r->rx_hdr_len = prv_hdr_len(*data);
        if (r->rx_hdr_len == 0) {
          // H4 has no framing to resync on; skip until something looks like a type.
          r->stats.drop_unknown_type++;
        } else {
          r->rx_type = *data;
          r->rx_pos = 0;
          r->rx_state = RX_HDR;
        }
        data++;
        len--;
        break;

      case RX_HDR: {
        r->rx_buf[r->rx_pos++] = *data++;
        len--;
        if (r->rx_pos < r->rx_hdr_len) {
          break;
        }
        const uint32_t total = r->rx_hdr_len + prv_payload_len(r->rx_type, r->rx_buf);
        r->rx_discard = r->rx_type == HCI_ROUTER_H4_SCO || r->rx_type == HCI_ROUTER_H4_ISO ||
                        total > sizeof(r->rx_buf);
        r->rx_total = total;
        if (r->rx_pos == total) {
          prv_rx_complete(r);
        } else {
          r->rx_state = RX_BODY;
        }
        break;
      }

      case RX_BODY: {
        size_t n = r->rx_total - r->rx_pos;
        if (n > len) {
          n = len;
        }
        if (!r->rx_discard) {
          memcpy(&r->rx_buf[r->rx_pos], data, n);
        }
        r->rx_pos += n;
        data += n;
        len -= n;
        if (r->rx_pos == r->rx_total) {
          prv_rx_complete(r);
        }
        break;
      }

      default:
        r->rx_state = RX_TYPE;
        break;
    }
  }
}

static void prv_tx_lock(struct hci_router *r) {
  if (!r->ops->tx_lock(r->ops->ctx, false)) {
    r->stats.tx_lock_waits++;
    (void)r->ops->tx_lock(r->ops->ctx, true);
  }
}

static int prv_tx_packet(struct hci_router *r, uint8_t type, const uint8_t *data, size_t len) {
  prv_tx_lock(r);
  int rc = r->ops->tx_write(r->ops->ctx, &type, 1);
  if (rc == 0) {
    rc = r->ops->tx_write(r->ops->ctx, data, len);
  }
  r->ops->tx_unlock(r->ops->ctx);
  return rc;
}

int hci_router_tx_cmd(struct hci_router *r, enum hci_router_owner owner, const uint8_t *cmd,
                      size_t len) {
  if (len < 3U || len != 3U + cmd[2]) {
    r->stats.tx_errors++;
    return -1;
  }

  const uint16_t opcode = prv_le16(cmd);
  // Host Number Of Completed Packets is not acknowledged on success.
  const bool acked = opcode != OP_HOST_NUM_COMPLETED_PACKETS;

  if (acked) {
    if (!r->ops->gate_take(r->ops->ctx, false)) {
      r->stats.gate_waits++;
      if (!r->ops->gate_take(r->ops->ctx, true)) {
        // The command in flight was never answered; do not stall the sender.
        r->stats.gate_timeouts++;
      }
    }
    r->cmd_owner = (uint8_t)owner;
    r->cmd_opcode = opcode;
    r->cmd_pending = true;
  }

  const uint8_t *out = cmd;
  uint8_t merged[3 + 8];
  if ((opcode == OP_SET_EVENT_MASK || opcode == OP_SET_EVENT_MASK_PAGE_2) &&
      len == sizeof(merged)) {
    const bool page2 = opcode == OP_SET_EVENT_MASK_PAGE_2;
    uint64_t *nimble = page2 ? &r->nimble_evt_mask2 : &r->nimble_evt_mask;
    uint64_t *classic = page2 ? &r->classic_evt_mask2 : &r->classic_evt_mask;
    uint64_t mask = prv_le64(&cmd[3]);
    if (owner == HCI_ROUTER_OWNER_NIMBLE) {
      *nimble = mask;
      if (page2) {
        r->nimble_evt_mask2_valid = true;
      } else {
        r->nimble_evt_mask_valid = true;
      }
    } else if (owner == HCI_ROUTER_OWNER_CLASSIC) {
      *classic = mask;
    }
    memcpy(merged, cmd, 3);
    prv_put_le64(&merged[3], *nimble | *classic);
    out = merged;
  }

  const int rc = prv_tx_packet(r, HCI_ROUTER_H4_CMD, out, len);
  if (rc != 0) {
    r->stats.tx_errors++;
    if (acked && r->cmd_pending && r->cmd_opcode == opcode) {
      r->cmd_pending = false;
      r->ops->gate_give(r->ops->ctx);
    }
    return -1;
  }
  r->stats.tx_cmd++;
  return 0;
}

int hci_router_tx_data(struct hci_router *r, uint8_t h4_type, hci_router_frag_next_t next,
                       void *it) {
  const uint8_t *data;
  size_t len;
  int rc;

  // The start of the packet, to spot LE pairing PDUs.
  uint8_t head[15];
  size_t head_len = 0;

  prv_tx_lock(r);
  rc = r->ops->tx_write(r->ops->ctx, &h4_type, 1);
  while (rc == 0 && next(it, &data, &len)) {
    rc = r->ops->tx_write(r->ops->ctx, data, len);
    const size_t n = len < sizeof(head) - head_len ? len : sizeof(head) - head_len;
    memcpy(&head[head_len], data, n);
    head_len += n;
  }
  r->ops->tx_unlock(r->ops->ctx);

  if (rc == 0 && h4_type == HCI_ROUTER_H4_ACL) {
    prv_smp_sniff(r, head, head_len, false);
  }

  if (rc != 0) {
    r->stats.tx_errors++;
    return -1;
  }
  r->stats.tx_data++;
  return 0;
}

enum {
  CLASSIC_PASS,
  CLASSIC_MERGE,
  CLASSIC_FAKE,
  CLASSIC_REJECT,
};

static int prv_classic_class(uint16_t opcode, uint8_t *status) {
  switch (opcode) {
    // Would change state NimBLE owns: answer success locally.
    case OP_RESET:
    case OP_SET_CTL_TO_HOST_FLOW:
    case OP_HOST_BUFFER_SIZE:
    case OP_HOST_NUM_COMPLETED_PACKETS:
    case OP_WRITE_LE_HOST_SUPP:
    case OP_LE_SET_EVENT_MASK:
    case OP_LE_SET_RANDOM_ADDRESS:
    case OP_LE_WRITE_DEFAULT_DATA_LEN:
    case OP_LE_SET_HOST_FEATURE:
      *status = 0;
      return CLASSIC_FAKE;
    case OP_SET_EVENT_MASK:
    case OP_SET_EVENT_MASK_PAGE_2:
      return CLASSIC_MERGE;
    // LE commands that only read.
    case OP_LE_READ_BUFFER_SIZE:
    case OP_LE_READ_LOCAL_FEATURES:
    case OP_LE_READ_ADV_CHAN_TX_POWER:
    case OP_LE_READ_FAL_SIZE:
    case OP_LE_RAND:
    case OP_LE_READ_SUPP_STATES:
    case OP_LE_READ_DEFAULT_DATA_LEN:
    case OP_LE_READ_RL_SIZE:
    case OP_LE_READ_MAX_DATA_LEN:
    case OP_LE_READ_PER_ADV_LIST_SIZE:
    case OP_LE_READ_TX_POWER:
    case OP_LE_READ_BUFFER_SIZE_V2:
    case OP_LE_READ_ALL_LOCAL_FEATURES:
      return CLASSIC_PASS;
    default:
      break;
  }

  switch (opcode >> 10) {
    case OGF_LE:
      *status = STATUS_CMD_DISALLOWED;
      return CLASSIC_REJECT;
    case OGF_VENDOR:
      *status = STATUS_UNKNOWN_CMD;
      return CLASSIC_REJECT;
    default:
      return CLASSIC_PASS;
  }
}

static void prv_classic_local_cc(struct hci_router *r, uint16_t opcode, uint8_t status) {
  const uint8_t cc[] = {EVT_COMMAND_COMPLETE, 4, 1, opcode & 0xFF, opcode >> 8, status};
  prv_to_classic_evt(r, cc, sizeof(cc));
}

int hci_router_classic_cmd(struct hci_router *r, const uint8_t *cmd, size_t len) {
  if (len < 3U || len != 3U + cmd[2]) {
    r->stats.tx_errors++;
    return -1;
  }

  const uint16_t opcode = prv_le16(cmd);
  uint8_t status = 0;
  switch (prv_classic_class(opcode, &status)) {
    case CLASSIC_FAKE:
      prv_log(r, HCI_ROUTER_LOG_CMD, HCI_ROUTER_LOG_FAKE, opcode, &cmd[3], cmd[2]);
      r->stats.classic_cmd_faked++;
      // Host Number Of Completed Packets is not acknowledged on success.
      if (opcode != OP_HOST_NUM_COMPLETED_PACKETS) {
        prv_classic_local_cc(r, opcode, status);
      }
      return 0;
    case CLASSIC_REJECT:
      prv_log(r, HCI_ROUTER_LOG_CMD, HCI_ROUTER_LOG_REJECT, opcode, &cmd[3], cmd[2]);
      r->stats.classic_cmd_rejected++;
      prv_classic_local_cc(r, opcode, status);
      return 0;
    case CLASSIC_MERGE: {
      const bool page2 = opcode == OP_SET_EVENT_MASK_PAGE_2;
      const bool nimble_valid = page2 ? r->nimble_evt_mask2_valid : r->nimble_evt_mask_valid;
      if (len != 3U + 8U) {
        r->stats.tx_errors++;
        return -1;
      }
      if (!nimble_valid) {
        // Merged when NimBLE sets its own mask.
        if (page2) {
          r->classic_evt_mask2 = prv_le64(&cmd[3]);
        } else {
          r->classic_evt_mask = prv_le64(&cmd[3]);
        }
        prv_log(r, HCI_ROUTER_LOG_CMD, HCI_ROUTER_LOG_FAKE, opcode, &cmd[3], cmd[2]);
        r->stats.classic_cmd_faked++;
        prv_classic_local_cc(r, opcode, 0);
        return 0;
      }
      prv_log(r, HCI_ROUTER_LOG_CMD, HCI_ROUTER_LOG_MERGE, opcode, &cmd[3], cmd[2]);
      r->stats.classic_cmd_merged++;
      return hci_router_tx_cmd(r, HCI_ROUTER_OWNER_CLASSIC, cmd, len);
    }
    default:
      prv_log(r, HCI_ROUTER_LOG_CMD, HCI_ROUTER_LOG_PASS, opcode, &cmd[3], cmd[2]);
      r->stats.classic_cmd_passed++;
      return hci_router_tx_cmd(r, HCI_ROUTER_OWNER_CLASSIC, cmd, len);
  }
}

int hci_router_classic_set_event_mask(struct hci_router *r, uint64_t mask) {
  r->classic_evt_mask = mask;
  if (!r->nimble_evt_mask_valid) {
    // Merged when NimBLE sets its own mask during start-up.
    return 0;
  }
  uint8_t cmd[3 + 8] = {OP_SET_EVENT_MASK & 0xFF, OP_SET_EVENT_MASK >> 8, 8};
  prv_put_le64(&cmd[3], r->nimble_evt_mask | mask);
  return hci_router_tx_cmd(r, HCI_ROUTER_OWNER_ROUTER, cmd, sizeof(cmd));
}
