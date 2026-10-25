/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/bluetooth/hci_router.h>
#include <pbl/util/size.h>

#include <clar.h>

#define MAX_PKTS 16
#define MAX_PKT  1100

struct pkt {
  size_t len;
  uint8_t data[MAX_PKT];
};

struct sink {
  int n;
  struct pkt pkts[MAX_PKTS];
};

static struct hci_router s_router;
static struct sink s_nimble_evt;
static struct sink s_nimble_acl;
static struct sink s_classic_evt;
static struct sink s_classic_acl;
static bool s_evt_alloc_fails;
static bool s_last_discardable;

// TX side
static bool s_locked;
static bool s_lock_busy;
static int s_lock_count;
static bool s_write_outside_lock;
static uint8_t s_tx[4096];
static size_t s_tx_len;
static size_t s_tx_packet_starts[32];
static int s_tx_packets;
static bool s_gate_free;
static bool s_gate_wait_succeeds;
static int s_gate_gives;

static void prv_push(struct sink *s, const uint8_t *data, size_t len) {
  cl_assert(s->n < MAX_PKTS);
  cl_assert(len <= MAX_PKT);
  s->pkts[s->n].len = len;
  memcpy(s->pkts[s->n].data, data, len);
  s->n++;
}

static int prv_nimble_evt(void *ctx, const uint8_t *evt, size_t len, bool discardable) {
  s_last_discardable = discardable;
  if (s_evt_alloc_fails) {
    return -1;
  }
  prv_push(&s_nimble_evt, evt, len);
  return 0;
}

static int prv_nimble_acl(void *ctx, const uint8_t *acl, size_t len) {
  prv_push(&s_nimble_acl, acl, len);
  return 0;
}

static void prv_classic_evt(void *ctx, const uint8_t *pkt, size_t len) {
  prv_push(&s_classic_evt, pkt, len);
}

static void prv_classic_acl(void *ctx, const uint8_t *pkt, size_t len) {
  prv_push(&s_classic_acl, pkt, len);
}

static bool prv_tx_lock(void *ctx, bool wait) {
  if (s_lock_busy && !wait) {
    return false;
  }
  cl_assert(!s_locked);
  s_locked = true;
  s_lock_count++;
  s_tx_packet_starts[s_tx_packets++] = s_tx_len;
  return true;
}

static void prv_tx_unlock(void *ctx) {
  cl_assert(s_locked);
  s_locked = false;
}

static int prv_tx_write(void *ctx, const uint8_t *data, size_t len) {
  if (!s_locked) {
    s_write_outside_lock = true;
  }
  cl_assert(s_tx_len + len <= sizeof(s_tx));
  memcpy(&s_tx[s_tx_len], data, len);
  s_tx_len += len;
  return 0;
}

static bool prv_gate_take(void *ctx, bool wait) {
  if (s_gate_free) {
    s_gate_free = false;
    return true;
  }
  return wait && s_gate_wait_succeeds;
}

static void prv_gate_give(void *ctx) {
  s_gate_free = true;
  s_gate_gives++;
}

static const struct hci_router_ops s_ops = {
  .nimble_evt = prv_nimble_evt,
  .nimble_acl = prv_nimble_acl,
  .tx_lock = prv_tx_lock,
  .tx_unlock = prv_tx_unlock,
  .tx_write = prv_tx_write,
  .gate_take = prv_gate_take,
  .gate_give = prv_gate_give,
};

void test_hci_router__initialize(void) {
  memset(&s_nimble_evt, 0, sizeof(s_nimble_evt));
  memset(&s_nimble_acl, 0, sizeof(s_nimble_acl));
  memset(&s_classic_evt, 0, sizeof(s_classic_evt));
  memset(&s_classic_acl, 0, sizeof(s_classic_acl));
  s_evt_alloc_fails = false;
  s_last_discardable = false;
  s_locked = false;
  s_lock_busy = false;
  s_lock_count = 0;
  s_write_outside_lock = false;
  s_tx_len = 0;
  s_tx_packets = 0;
  s_gate_free = true;
  s_gate_wait_succeeds = true;
  s_gate_gives = 0;
  hci_router_init(&s_router, &s_ops, 70);
}

static void prv_rx(const uint8_t *data, size_t len) {
  hci_router_rx(&s_router, data, len);
}

// Connect a classic ACL link on @p handle through a Connection Complete.
static void prv_classic_connect(uint16_t handle) {
  const uint8_t cc[] = {0x04, 0x03, 11, 0x00, handle & 0xFF, handle >> 8, 1, 2,
                        3,    4,    5,  6,    0x01,          0x00};
  prv_rx(cc, sizeof(cc));
}

// --- H4 reassembly --------------------------------------------------------

static const uint8_t s_le_meta[] = {0x04, 0x3E, 0x04, 0x0C, 0x00, 0x40, 0x00};

void test_hci_router__event_in_one_read(void) {
  prv_rx(s_le_meta, sizeof(s_le_meta));
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_nimble_evt.pkts[0].len, sizeof(s_le_meta) - 1);
  cl_assert(memcmp(s_nimble_evt.pkts[0].data, &s_le_meta[1], sizeof(s_le_meta) - 1) == 0);
}

void test_hci_router__byte_by_byte(void) {
  for (size_t i = 0; i < sizeof(s_le_meta); i++) {
    prv_rx(&s_le_meta[i], 1);
  }
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert(memcmp(s_nimble_evt.pkts[0].data, &s_le_meta[1], sizeof(s_le_meta) - 1) == 0);
}

void test_hci_router__packets_across_64_byte_reads(void) {
  uint8_t stream[600];
  size_t len = 0;
  // ACL of 251 bytes, an event, another ACL, back to back.
  for (int k = 0; k < 2; k++) {
    stream[len++] = 0x02;
    stream[len++] = 0x40;
    stream[len++] = 0x00;
    stream[len++] = 251;
    stream[len++] = 0;
    for (int i = 0; i < 251; i++) {
      stream[len++] = (uint8_t)(i + k);
    }
    if (k == 0) {
      memcpy(&stream[len], s_le_meta, sizeof(s_le_meta));
      len += sizeof(s_le_meta);
    }
  }
  for (size_t off = 0; off < len; off += 64) {
    prv_rx(&stream[off], (len - off) < 64 ? (len - off) : 64);
  }
  cl_assert_equal_i(s_nimble_acl.n, 2);
  cl_assert_equal_i(s_nimble_acl.pkts[0].len, 4 + 251);
  cl_assert_equal_i(s_nimble_acl.pkts[1].data[4 + 250], (uint8_t)(250 + 1));
  cl_assert_equal_i(s_nimble_evt.n, 1);
}

void test_hci_router__largest_event_reaches_classic(void) {
  uint8_t evt[1 + 2 + 255];
  evt[0] = 0x04;
  evt[1] = 0x2F; // Extended Inquiry Result, BR/EDR only
  evt[2] = 255;
  memset(&evt[3], 0xAB, 255);
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  prv_rx(evt, sizeof(evt));
  cl_assert_equal_i(s_classic_evt.n, 1);
  cl_assert_equal_i(s_classic_evt.pkts[0].len, 257);
}

void test_hci_router__largest_acl(void) {
  static uint8_t acl[1 + 4 + 1021];
  acl[0] = 0x02;
  acl[1] = 0x01;
  acl[2] = 0x00;
  acl[3] = 1021 & 0xFF;
  acl[4] = 1021 >> 8;
  prv_rx(acl, sizeof(acl));
  cl_assert_equal_i(s_nimble_acl.n, 1);
  cl_assert_equal_i(s_nimble_acl.pkts[0].len, 4 + 1021);
}

void test_hci_router__oversize_acl_is_skipped(void) {
  static uint8_t stream[1 + 4 + 1022 + sizeof(s_le_meta)];
  stream[0] = 0x02;
  stream[3] = 1022 & 0xFF;
  stream[4] = 1022 >> 8;
  memcpy(&stream[1 + 4 + 1022], s_le_meta, sizeof(s_le_meta));
  prv_rx(stream, sizeof(stream));
  cl_assert_equal_i(s_router.stats.drop_oversize, 1);
  cl_assert_equal_i(s_nimble_acl.n, 0);
  cl_assert_equal_i(s_nimble_evt.n, 1);
}

void test_hci_router__oversize_length_field_does_not_wrap(void) {
  // A 0xFFFF length must be skipped in full, not wrapped to a short packet.
  static uint8_t stream[1 + 4 + 0xFFFF];
  stream[0] = 0x02;
  stream[3] = 0xFF;
  stream[4] = 0xFF;
  prv_rx(stream, sizeof(stream));
  cl_assert_equal_i(s_router.stats.drop_oversize, 1);
  prv_rx(s_le_meta, sizeof(s_le_meta));
  cl_assert_equal_i(s_nimble_evt.n, 1);
}

void test_hci_router__unknown_types_skipped(void) {
  const uint8_t junk[] = {0x00, 0x01, 0x07, 0xFF};
  prv_rx(junk, sizeof(junk));
  prv_rx(s_le_meta, sizeof(s_le_meta));
  cl_assert_equal_i(s_router.stats.drop_unknown_type, 4);
  cl_assert_equal_i(s_nimble_evt.n, 1);
}

void test_hci_router__stream_starting_mid_packet(void) {
  // The tail of an LE Meta event, then a whole one. The tail's bytes are not
  // packet types until one happens to be; what follows must not crash and the
  // router must be back in step once the stray "packet" ends.
  const uint8_t tail[] = {0x0C, 0x00, 0x40, 0x00};
  prv_rx(tail, sizeof(tail));
  prv_rx(s_le_meta, sizeof(s_le_meta));
  prv_rx(s_le_meta, sizeof(s_le_meta));
  cl_assert(s_router.stats.drop_unknown_type >= 1);
  cl_assert(s_nimble_evt.n >= 1);
  const struct pkt *last = &s_nimble_evt.pkts[s_nimble_evt.n - 1];
  cl_assert(memcmp(last->data, &s_le_meta[1], sizeof(s_le_meta) - 1) == 0);
}

void test_hci_router__sco_and_iso_dropped(void) {
  const uint8_t sco[] = {0x03, 0x01, 0x00, 0x03, 0xAA, 0xBB, 0xCC};
  const uint8_t iso[] = {0x05, 0x01, 0x00, 0x02, 0x00, 0xAA, 0xBB};
  prv_rx(sco, sizeof(sco));
  prv_rx(iso, sizeof(iso));
  prv_rx(s_le_meta, sizeof(s_le_meta));
  cl_assert_equal_i(s_router.stats.drop_sco, 1);
  cl_assert_equal_i(s_router.stats.drop_iso, 1);
  cl_assert_equal_i(s_nimble_evt.n, 1);
}

// --- Event routing ----------------------------------------------------------

void test_hci_router__event_over_nimble_size_dropped(void) {
  uint8_t evt[1 + 2 + 69];
  evt[0] = 0x04;
  evt[1] = 0x3E;
  evt[2] = 69; // 71 bytes, one more than NimBLE takes
  memset(&evt[3], 0, 69);
  prv_rx(evt, sizeof(evt));
  cl_assert_equal_i(s_router.stats.drop_evt_too_long, 1);
  cl_assert_equal_i(s_nimble_evt.n, 0);

  evt[2] = 68;
  prv_rx(evt, sizeof(evt) - 1);
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_nimble_evt.pkts[0].len, 70);
}

void test_hci_router__adv_report_is_discardable(void) {
  const uint8_t adv[] = {0x04, 0x3E, 0x02, 0x02, 0x00};
  prv_rx(adv, sizeof(adv));
  cl_assert(s_last_discardable);
  prv_rx(s_le_meta, sizeof(s_le_meta));
  cl_assert(!s_last_discardable);
}

void test_hci_router__alloc_failure_counted(void) {
  s_evt_alloc_fails = true;
  prv_rx(s_le_meta, sizeof(s_le_meta));
  cl_assert_equal_i(s_router.stats.drop_alloc, 1);
}

void test_hci_router__classic_events_without_host_dropped(void) {
  const uint8_t conn_req[] = {0x04, 0x04, 0x0A, 1, 2, 3, 4, 5, 6, 0x04, 0x07, 0x24, 0x01};
  prv_rx(conn_req, sizeof(conn_req));
  cl_assert_equal_i(s_router.stats.drop_classic, 1);
  cl_assert_equal_i(s_nimble_evt.n, 0);
}

void test_hci_router__unknown_event_goes_to_nimble(void) {
  const uint8_t hw_err[] = {0x04, 0x10, 0x01, 0x00};
  const uint8_t vendor[] = {0x04, 0xFF, 0x02, 0x01, 0x02};
  prv_rx(hw_err, sizeof(hw_err));
  prv_rx(vendor, sizeof(vendor));
  cl_assert_equal_i(s_nimble_evt.n, 2);
}

void test_hci_router__vendor_ready_dropped(void) {
  const uint8_t ready[] = {0x04, 0x0E, 0x04, 0x01, 0x11, 0xFC, 0x00};
  prv_rx(ready, sizeof(ready));
  cl_assert_equal_i(s_router.stats.drop_vendor_ready, 1);
  cl_assert_equal_i(s_nimble_evt.n, 0);
}

// --- Command Complete / Status routing ---------------------------------------

static const uint8_t s_read_bd_addr[] = {0x09, 0x10, 0x00};
static const uint8_t s_read_bd_addr_cc[] = {0x04, 0x0E, 0x0A, 0x01, 0x09, 0x10, 0x00,
                                            1,    2,    3,    4,    5,    6};

void test_hci_router__cc_follows_recorded_owner(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);

  cl_assert_equal_i(
      hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_read_bd_addr, sizeof(s_read_bd_addr)),
      0);
  cl_assert(!s_gate_free);
  prv_rx(s_read_bd_addr_cc, sizeof(s_read_bd_addr_cc));
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert(s_gate_free);

  cl_assert_equal_i(hci_router_classic_cmd(&s_router, s_read_bd_addr, sizeof(s_read_bd_addr)), 0);
  prv_rx(s_read_bd_addr_cc, sizeof(s_read_bd_addr_cc));
  cl_assert_equal_i(s_classic_evt.n, 1);
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_router.stats.cc_unmatched, 0);
}

void test_hci_router__cc_without_record_goes_to_nimble(void) {
  prv_rx(s_read_bd_addr_cc, sizeof(s_read_bd_addr_cc));
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_router.stats.cc_unmatched, 1);
}

void test_hci_router__cc_opcode_mismatch_keeps_record(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, s_read_bd_addr, sizeof(s_read_bd_addr)), 0);

  // A Command Complete for some other opcode is NimBLE's, as today.
  const uint8_t other_cc[] = {0x04, 0x0E, 0x04, 0x01, 0x03, 0x0C, 0x00};
  prv_rx(other_cc, sizeof(other_cc));
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_router.stats.cc_unmatched, 1);
  cl_assert(!s_gate_free);

  prv_rx(s_read_bd_addr_cc, sizeof(s_read_bd_addr_cc));
  cl_assert_equal_i(s_classic_evt.n, 1);
  cl_assert(s_gate_free);
}

void test_hci_router__command_status_routed(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t create_conn[] = {0x05, 0x04, 0x00};
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, create_conn, sizeof(create_conn)), 0);
  const uint8_t cs[] = {0x04, 0x0F, 0x04, 0x00, 0x01, 0x05, 0x04};
  prv_rx(cs, sizeof(cs));
  cl_assert_equal_i(s_classic_evt.n, 1);
  cl_assert(s_gate_free);
}

void test_hci_router__nop_credit_not_counted_unmatched(void) {
  const uint8_t nop[] = {0x04, 0x0E, 0x03, 0x01, 0x00, 0x00};
  prv_rx(nop, sizeof(nop));
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_router.stats.cc_unmatched, 0);
}

void test_hci_router__gate_waits_then_times_out(void) {
  cl_assert_equal_i(
      hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_read_bd_addr, sizeof(s_read_bd_addr)),
      0);
  // The first command is never answered.
  s_gate_wait_succeeds = false;
  cl_assert_equal_i(
      hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_read_bd_addr, sizeof(s_read_bd_addr)),
      0);
  cl_assert_equal_i(s_router.stats.gate_waits, 1);
  cl_assert_equal_i(s_router.stats.gate_timeouts, 1);
  cl_assert_equal_i(s_router.stats.tx_cmd, 2);
}

void test_hci_router__host_num_completed_packets_skips_gate(void) {
  const uint8_t hnocp[] = {0x35, 0x0C, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00};
  cl_assert_equal_i(hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, hnocp, sizeof(hnocp)), 0);
  cl_assert(s_gate_free);
  cl_assert(!s_router.cmd_pending);
}

void test_hci_router__reset_releases_gate(void) {
  cl_assert_equal_i(
      hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_read_bd_addr, sizeof(s_read_bd_addr)),
      0);
  hci_router_reset(&s_router);
  cl_assert(s_gate_free);
  cl_assert(!s_router.cmd_pending);
}

void test_hci_router__classic_reset_is_faked(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t reset[] = {0x03, 0x0C, 0x00};
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, reset, sizeof(reset)), 0);
  cl_assert_equal_i(s_tx_len, 0);
  cl_assert_equal_i(s_classic_evt.n, 1);
  const uint8_t expect[] = {0x0E, 0x04, 0x01, 0x03, 0x0C, 0x00};
  cl_assert(memcmp(s_classic_evt.pkts[0].data, expect, sizeof(expect)) == 0);
  cl_assert(s_gate_free);
}

// --- Event mask merge ---------------------------------------------------------

static const uint8_t s_nimble_mask_cmd[] = {0x01, 0x0C, 0x08, 0x90, 0x80, 0x00,
                                            0x02, 0x00, 0x80, 0x00, 0x20};

void test_hci_router__event_mask_untouched_without_classic(void) {
  cl_assert_equal_i(hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_nimble_mask_cmd,
                                      sizeof(s_nimble_mask_cmd)),
                    0);
  cl_assert_equal_i(s_tx_len, 1 + sizeof(s_nimble_mask_cmd));
  cl_assert_equal_i(s_tx[0], 0x01);
  cl_assert(memcmp(&s_tx[1], s_nimble_mask_cmd, sizeof(s_nimble_mask_cmd)) == 0);
}

void test_hci_router__event_mask_merged(void) {
  cl_assert_equal_i(hci_router_classic_set_event_mask(&s_router, 0x18), 0);
  // Nothing sent until NimBLE has set its own mask.
  cl_assert_equal_i(s_tx_len, 0);

  cl_assert_equal_i(hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_nimble_mask_cmd,
                                      sizeof(s_nimble_mask_cmd)),
                    0);
  cl_assert_equal_i(s_tx[1 + 3], 0x90 | 0x18);
  const uint8_t cc[] = {0x04, 0x0E, 0x04, 0x01, 0x01, 0x0C, 0x00};
  prv_rx(cc, sizeof(cc));
  cl_assert_equal_i(s_nimble_evt.n, 1);

  // A later classic change is sent by the router and its ack swallowed.
  s_tx_len = 0;
  cl_assert_equal_i(hci_router_classic_set_event_mask(&s_router, 0x04), 0);
  cl_assert_equal_i(s_tx[1 + 3], 0x90 | 0x04);
  prv_rx(cc, sizeof(cc));
  cl_assert_equal_i(s_router.stats.router_acks, 1);
  cl_assert_equal_i(s_nimble_evt.n, 1);
}

// --- Handle ownership, ACL and Number of Completed Packets --------------------

void test_hci_router__acl_follows_handle_owner(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  prv_classic_connect(0x0081);
  cl_assert(hci_router_handle_is_classic(&s_router, 0x0081));

  const uint8_t classic_acl[] = {0x02, 0x81, 0x20, 0x02, 0x00, 0xAA, 0xBB};
  const uint8_t le_acl[] = {0x02, 0x01, 0x20, 0x02, 0x00, 0xCC, 0xDD};
  prv_rx(classic_acl, sizeof(classic_acl));
  prv_rx(le_acl, sizeof(le_acl));
  cl_assert_equal_i(s_classic_acl.n, 1);
  cl_assert_equal_i(s_nimble_acl.n, 1);
}

void test_hci_router__failed_connection_not_owned(void) {
  const uint8_t cc[] = {0x04, 0x03, 11, 0x04, 0x81, 0x00, 1, 2, 3, 4, 5, 6, 0x01, 0x00};
  prv_rx(cc, sizeof(cc));
  cl_assert(!hci_router_handle_is_classic(&s_router, 0x0081));
}

void test_hci_router__disconnect_follows_handle_owner(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  prv_classic_connect(0x0081);
  const uint8_t disc_classic[] = {0x04, 0x05, 0x04, 0x00, 0x81, 0x00, 0x13};
  const uint8_t disc_le[] = {0x04, 0x05, 0x04, 0x00, 0x01, 0x00, 0x13};
  prv_rx(disc_le, sizeof(disc_le));
  prv_rx(disc_classic, sizeof(disc_classic));
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_classic_evt.n, 2); // Connection Complete + Disconnection Complete
  cl_assert(!hci_router_handle_is_classic(&s_router, 0x0081));
}

void test_hci_router__nocp_all_nimble_passes_unchanged(void) {
  const uint8_t nocp[] = {0x04, 0x13, 0x05, 0x01, 0x01, 0x00, 0x02, 0x00};
  prv_rx(nocp, sizeof(nocp));
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert(memcmp(s_nimble_evt.pkts[0].data, &nocp[1], sizeof(nocp) - 1) == 0);
  cl_assert_equal_i(s_router.stats.nocp_split, 0);
}

void test_hci_router__nocp_split_by_handle(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  prv_classic_connect(0x0081);
  s_classic_evt.n = 0;

  const uint8_t nocp[] = {0x04, 0x13, 0x0D, 0x03,  // 3 handles
                          0x01, 0x00, 0x02, 0x00,  // LE 0x0001: 2
                          0x81, 0x00, 0x01, 0x00,  // classic 0x0081: 1
                          0x02, 0x00, 0x05, 0x00}; // LE 0x0002: 5
  prv_rx(nocp, sizeof(nocp));
  cl_assert_equal_i(s_router.stats.nocp_split, 1);

  const uint8_t expect_le[] = {0x13, 0x09, 0x02, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x05, 0x00};
  const uint8_t expect_classic[] = {0x13, 0x05, 0x01, 0x81, 0x00, 0x01, 0x00};
  cl_assert_equal_i(s_nimble_evt.n, 1);
  cl_assert_equal_i(s_nimble_evt.pkts[0].len, sizeof(expect_le));
  cl_assert(memcmp(s_nimble_evt.pkts[0].data, expect_le, sizeof(expect_le)) == 0);
  cl_assert_equal_i(s_classic_evt.n, 1);
  cl_assert_equal_i(s_classic_evt.pkts[0].len, sizeof(expect_classic));
  cl_assert(memcmp(s_classic_evt.pkts[0].data, expect_classic, sizeof(expect_classic)) == 0);
}

void test_hci_router__nocp_short_is_malformed(void) {
  const uint8_t nocp[] = {0x04, 0x13, 0x05, 0x02, 0x01, 0x00, 0x02, 0x00};
  prv_rx(nocp, sizeof(nocp));
  cl_assert_equal_i(s_router.stats.drop_malformed, 1);
  cl_assert_equal_i(s_nimble_evt.n, 0);
}

// --- TX framing under the lock -------------------------------------------------

struct frags {
  const uint8_t *parts[3];
  size_t lens[3];
  int i;
  int n;
};

static bool prv_next(void *it, const uint8_t **data, size_t *len) {
  struct frags *f = it;
  if (f->i >= f->n) {
    return false;
  }
  *data = f->parts[f->i];
  *len = f->lens[f->i];
  f->i++;
  return true;
}

void test_hci_router__cmd_and_acl_never_interleave(void) {
  const uint8_t hdr[] = {0x01, 0x00, 0x04, 0x00};
  const uint8_t l2cap[] = {0x00, 0x00, 0x04, 0x00};
  struct frags f = {.parts = {hdr, l2cap}, .lens = {sizeof(hdr), sizeof(l2cap)}, .n = 2};

  cl_assert_equal_i(hci_router_tx_data(&s_router, HCI_ROUTER_H4_ACL, prv_next, &f), 0);
  cl_assert_equal_i(
      hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_read_bd_addr, sizeof(s_read_bd_addr)),
      0);
  s_lock_busy = true; // another sender holds the lock at first
  f.i = 0;
  cl_assert_equal_i(hci_router_tx_data(&s_router, HCI_ROUTER_H4_ACL, prv_next, &f), 0);

  cl_assert(!s_write_outside_lock);
  cl_assert(!s_locked);
  cl_assert_equal_i(s_lock_count, 3);
  cl_assert_equal_i(s_router.stats.tx_lock_waits, 1);

  // Each lock covers exactly one whole H4 packet.
  const uint8_t expect[] = {0x02, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x09,
                            0x10, 0x00, 0x02, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00};
  cl_assert_equal_i(s_tx_len, sizeof(expect));
  cl_assert(memcmp(s_tx, expect, sizeof(expect)) == 0);
  cl_assert_equal_i(s_tx_packet_starts[0], 0);
  cl_assert_equal_i(s_tx_packet_starts[1], 9);
  cl_assert_equal_i(s_tx_packet_starts[2], 13);
}

void test_hci_router__malformed_cmd_rejected(void) {
  const uint8_t bad[] = {0x09, 0x10, 0x05};
  cl_assert(hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, bad, sizeof(bad)) < 0);
  cl_assert_equal_i(s_tx_len, 0);
  cl_assert(s_gate_free);
}

// --- Classic host start-up commands -----------------------------------------------

static void prv_expect_local_cc(int idx, uint16_t opcode, uint8_t status) {
  const uint8_t expect[] = {0x0E, 0x04, 0x01, opcode & 0xFF, opcode >> 8, status};
  cl_assert(idx < s_classic_evt.n);
  cl_assert_equal_i(s_classic_evt.pkts[idx].len, sizeof(expect));
  cl_assert(memcmp(s_classic_evt.pkts[idx].data, expect, sizeof(expect)) == 0);
}

void test_hci_router__classic_le_setters_faked(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t le_evt_mask[] = {0x01, 0x20, 0x08, 0xff, 0xff, 0, 0, 0, 0, 0, 0};
  const uint8_t le_rand_addr[] = {0x05, 0x20, 0x06, 1, 2, 3, 4, 5, 0xC6};
  const uint8_t le_def_data_len[] = {0x24, 0x20, 0x04, 0xfb, 0x00, 0x48, 0x08};
  const uint8_t le_host_feat[] = {0x74, 0x20, 0x02, 0x20, 0x01};
  const uint8_t le_host_supp[] = {0x6D, 0x0C, 0x02, 0x01, 0x00};
  const uint8_t ctl_flow[] = {0x31, 0x0C, 0x01, 0x01};
  const uint8_t host_buf[] = {0x33, 0x0C, 0x07, 0, 1, 0, 1, 0, 0, 0};

  const uint8_t *cmds[] = {le_evt_mask,  le_rand_addr, le_def_data_len, le_host_feat,
                           le_host_supp, ctl_flow,     host_buf};
  const size_t lens[] = {sizeof(le_evt_mask),  sizeof(le_rand_addr), sizeof(le_def_data_len),
                         sizeof(le_host_feat), sizeof(le_host_supp), sizeof(ctl_flow),
                         sizeof(host_buf)};
  for (size_t i = 0; i < ARRAY_LENGTH(cmds); i++) {
    cl_assert_equal_i(hci_router_classic_cmd(&s_router, cmds[i], lens[i]), 0);
    prv_expect_local_cc(i, cmds[i][0] | (cmds[i][1] << 8), 0x00);
  }
  // Nothing reached the controller and the gate was never taken.
  cl_assert_equal_i(s_tx_len, 0);
  cl_assert(s_gate_free);
  cl_assert_equal_i(s_router.stats.classic_cmd_faked, ARRAY_LENGTH(cmds));
}

void test_hci_router__classic_host_num_completed_faked_silently(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t hnocp[] = {0x35, 0x0C, 0x05, 0x01, 0x81, 0x00, 0x01, 0x00};
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, hnocp, sizeof(hnocp)), 0);
  cl_assert_equal_i(s_tx_len, 0);
  cl_assert_equal_i(s_classic_evt.n, 0);
}

void test_hci_router__classic_reads_and_bredr_passed(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t le_read_buf[] = {0x02, 0x20, 0x00};
  const uint8_t read_buf[] = {0x05, 0x10, 0x00};
  const uint8_t wr_ssp[] = {0x56, 0x0C, 0x01, 0x01};
  const uint8_t wr_cod[] = {0x24, 0x0C, 0x03, 0x04, 0x07, 0x20};

  const uint8_t *cmds[] = {le_read_buf, read_buf, wr_ssp, wr_cod};
  const size_t lens[] = {sizeof(le_read_buf), sizeof(read_buf), sizeof(wr_ssp), sizeof(wr_cod)};
  for (size_t i = 0; i < ARRAY_LENGTH(cmds); i++) {
    s_tx_len = 0;
    cl_assert_equal_i(hci_router_classic_cmd(&s_router, cmds[i], lens[i]), 0);
    cl_assert_equal_i(s_tx_len, 1 + lens[i]);
    cl_assert(memcmp(&s_tx[1], cmds[i], lens[i]) == 0);
    // The controller's answer goes back to the classic host.
    const uint8_t cc[] = {0x04, 0x0E, 0x04, 0x01, cmds[i][0], cmds[i][1], 0x00};
    prv_rx(cc, sizeof(cc));
    cl_assert_equal_i(s_classic_evt.n, (int)i + 1);
  }
  cl_assert_equal_i(s_router.stats.classic_cmd_passed, ARRAY_LENGTH(cmds));
  cl_assert_equal_i(s_nimble_evt.n, 0);
}

void test_hci_router__classic_unknown_le_and_vendor_rejected(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t le_set_adv_enable[] = {0x0A, 0x20, 0x01, 0x01};
  const uint8_t vendor[] = {0x01, 0xFC, 0x00};
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, le_set_adv_enable, sizeof(le_set_adv_enable)),
                    0);
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, vendor, sizeof(vendor)), 0);
  prv_expect_local_cc(0, 0x200A, 0x0C);
  prv_expect_local_cc(1, 0xFC01, 0x01);
  cl_assert_equal_i(s_tx_len, 0);
  cl_assert_equal_i(s_router.stats.classic_cmd_rejected, 2);
}

void test_hci_router__classic_event_mask_merged_with_nimble(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  // Before NimBLE has set its mask, the classic mask is kept and acked locally.
  const uint8_t classic_mask[] = {0x01, 0x0C, 0x08, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, classic_mask, sizeof(classic_mask)), 0);
  cl_assert_equal_i(s_tx_len, 0);
  prv_expect_local_cc(0, 0x0C01, 0x00);

  // NimBLE's own mask then carries the classic bits.
  cl_assert_equal_i(hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, s_nimble_mask_cmd,
                                      sizeof(s_nimble_mask_cmd)),
                    0);
  cl_assert_equal_i(s_tx[1 + 3], 0x90 | 0x1F);
  const uint8_t cc[] = {0x04, 0x0E, 0x04, 0x01, 0x01, 0x0C, 0x00};
  prv_rx(cc, sizeof(cc));
  cl_assert_equal_i(s_nimble_evt.n, 1);

  // Afterwards a classic mask goes out merged, and its ack is the classic host's.
  s_tx_len = 0;
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, classic_mask, sizeof(classic_mask)), 0);
  cl_assert_equal_i(s_tx[1 + 3], 0x90 | 0x1F);
  cl_assert_equal_i(s_tx[1 + 3 + 7], 0x20);
  prv_rx(cc, sizeof(cc));
  cl_assert_equal_i(s_classic_evt.n, 2);
  cl_assert_equal_i(s_router.stats.classic_cmd_merged, 1);
}

void test_hci_router__event_mask_page2_merged(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t nimble_p2[] = {0x63, 0x0C, 0x08, 0x00, 0x00, 0x80, 0x00, 0, 0, 0, 0};
  const uint8_t classic_p2[] = {0x63, 0x0C, 0x08, 0x04, 0x00, 0x00, 0x00, 0, 0, 0, 0};
  cl_assert_equal_i(
      hci_router_tx_cmd(&s_router, HCI_ROUTER_OWNER_NIMBLE, nimble_p2, sizeof(nimble_p2)), 0);
  const uint8_t cc[] = {0x04, 0x0E, 0x04, 0x01, 0x63, 0x0C, 0x00};
  prv_rx(cc, sizeof(cc));

  s_tx_len = 0;
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, classic_p2, sizeof(classic_p2)), 0);
  cl_assert_equal_i(s_tx[1 + 3], 0x04);
  cl_assert_equal_i(s_tx[1 + 5], 0x80);
}

// --- Classic HCI log and LE pairing record -----------------------------------------

void test_hci_router__log_records_classic_cmds_and_events(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t auth_req[] = {0x11, 0x04, 0x02, 0x81, 0x00};
  const uint8_t vendor[] = {0x01, 0xFC, 0x00};
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, auth_req, sizeof(auth_req)), 0);
  cl_assert_equal_i(hci_router_classic_cmd(&s_router, vendor, sizeof(vendor)), 0);
  prv_classic_connect(0x0081);
  // Authentication Complete, status 0x05, on 0x0081.
  const uint8_t auth_done[] = {0x04, 0x06, 0x03, 0x05, 0x81, 0x00};
  prv_rx(auth_done, sizeof(auth_done));

  struct hci_router_log_entry log[HCI_ROUTER_LOG_LEN];
  cl_assert_equal_i(hci_router_log_copy(&s_router, log), 5);
  cl_assert_equal_i(log[0].kind, HCI_ROUTER_LOG_CMD);
  cl_assert_equal_i(log[0].action, HCI_ROUTER_LOG_PASS);
  cl_assert_equal_i(log[0].code, 0x0411);
  cl_assert_equal_i(log[0].params[0], 0x81);
  cl_assert_equal_i(log[1].action, HCI_ROUTER_LOG_REJECT);
  cl_assert_equal_i(log[1].code, 0xFC01);
  // The local Command Complete for the rejected command.
  cl_assert_equal_i(log[2].kind, HCI_ROUTER_LOG_EVT);
  cl_assert_equal_i(log[2].code, 0x0E);
  cl_assert_equal_i(log[3].code, 0x03);
  cl_assert_equal_i(log[4].code, 0x06);
  cl_assert_equal_i(log[4].params[0], 0x05);
  cl_assert_equal_i(log[4].params[1], 0x81);
  cl_assert_equal_i(log[4].seq, 4);
}

void test_hci_router__log_keeps_the_last_entries(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  const uint8_t vendor[] = {0x01, 0xFC, 0x00};
  // Each command logs itself and its local Command Complete.
  for (int i = 0; i < 20; i++) {
    s_classic_evt.n = 0;
    cl_assert_equal_i(hci_router_classic_cmd(&s_router, vendor, sizeof(vendor)), 0);
  }
  struct hci_router_log_entry log[HCI_ROUTER_LOG_LEN];
  cl_assert_equal_i(hci_router_log_copy(&s_router, log), HCI_ROUTER_LOG_LEN);
  cl_assert_equal_i(log[0].seq, 40 - HCI_ROUTER_LOG_LEN);
  cl_assert_equal_i(log[HCI_ROUTER_LOG_LEN - 1].seq, 39);
}

void test_hci_router__smp_pairing_request_and_response_recorded(void) {
  // Phone's Pairing Request on LE handle 0x0040.
  const uint8_t req[] = {0x02, 0x40, 0x20, 0x0B, 0x00, 0x07, 0x00, 0x06,
                         0x00, 0x01, 0x04, 0x00, 0x2D, 0x10, 0x0F, 0x0F};
  prv_rx(req, sizeof(req));
  cl_assert_equal_i(s_nimble_acl.n, 1);
  cl_assert(s_router.smp_req.valid);
  cl_assert(s_router.smp_req.rx);
  cl_assert_equal_i(s_router.smp_req.authreq, 0x2D);
  cl_assert_equal_i(s_router.smp_req.init_key_dist, 0x0F);
  cl_assert_equal_i(s_router.smp_req.resp_key_dist, 0x0F);
  cl_assert(!s_router.smp_rsp.valid);

  // Watch's Pairing Response, split across fragments as NimBLE's mbufs can be.
  const uint8_t hdr[] = {0x40, 0x00, 0x0B, 0x00, 0x07, 0x00};
  const uint8_t body[] = {0x06, 0x00, 0x02, 0x01, 0x00, 0x0D, 0x10, 0x0B, 0x09};
  struct frags f = {.parts = {hdr, body}, .lens = {sizeof(hdr), sizeof(body)}, .n = 2};
  cl_assert_equal_i(hci_router_tx_data(&s_router, HCI_ROUTER_H4_ACL, prv_next, &f), 0);
  cl_assert(s_router.smp_rsp.valid);
  cl_assert(!s_router.smp_rsp.rx);
  cl_assert_equal_i(s_router.smp_rsp.io_cap, 0x01);
  cl_assert_equal_i(s_router.smp_rsp.authreq, 0x0D);
  cl_assert_equal_i(s_router.smp_rsp.init_key_dist, 0x0B);
  cl_assert_equal_i(s_router.smp_rsp.resp_key_dist, 0x09);
}

void test_hci_router__smp_ignores_other_channels_and_fragments(void) {
  // ATT (CID 4) with the same bytes, and a continuation fragment.
  const uint8_t att[] = {0x02, 0x40, 0x20, 0x0B, 0x00, 0x07, 0x00, 0x04,
                         0x00, 0x01, 0x04, 0x00, 0x2D, 0x10, 0x0F, 0x0F};
  const uint8_t cont[] = {0x02, 0x40, 0x10, 0x0B, 0x00, 0x07, 0x00, 0x06,
                          0x00, 0x01, 0x04, 0x00, 0x2D, 0x10, 0x0F, 0x0F};
  prv_rx(att, sizeof(att));
  prv_rx(cont, sizeof(cont));
  cl_assert(!s_router.smp_req.valid);
}

void test_hci_router__sync_conn_complete_kept(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  cl_assert(!s_router.sync_conn_valid);
  // Synchronous Connection Complete: handle 0x0101, eSCO, CVSD.
  const uint8_t scc[] = {0x04, 0x2C, 17,   0x00, 0x01, 0x01, 1, 2,  3, 4,
                         5,    6,    0x02, 0x0C, 0x02, 60,   0, 60, 0, 0x02};
  prv_rx(scc, sizeof(scc));
  cl_assert_equal_i(s_classic_evt.n, 1);
  cl_assert(s_router.sync_conn_valid);
  cl_assert(memcmp(s_router.sync_conn, &scc[3], HCI_ROUTER_SYNC_CONN_LEN) == 0);
}

void test_hci_router__log_leaves_completed_packets_out(void) {
  hci_router_set_classic(&s_router, prv_classic_evt, prv_classic_acl, NULL);
  prv_classic_connect(0x0081);
  const uint8_t nocp[] = {0x04, 0x13, 0x05, 0x01, 0x81, 0x00, 0x01, 0x00};
  prv_rx(nocp, sizeof(nocp));
  struct hci_router_log_entry log[HCI_ROUTER_LOG_LEN];
  cl_assert_equal_i(hci_router_log_copy(&s_router, log), 1);
  cl_assert_equal_i(log[0].code, 0x03);
}
