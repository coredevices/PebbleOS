/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file
 * @brief H4 HCI router between the controller and two hosts.
 *
 * Sits between the controller transport and NimBLE so that a BR/EDR host can
 * share the controller. It reassembles the H4 stream itself, sends LE traffic
 * and anything it does not recognise to NimBLE, BR/EDR-only traffic to the
 * classic port, and Command Complete/Status to whichever host sent the
 * command. Transmission holds one lock per H4 packet and lets only one
 * command be outstanding at a time.
 *
 * The core (@ref hci_router_rx and friends) has no OS or NimBLE dependency;
 * the transport supplies them through @ref hci_router_ops.
 */

#define HCI_ROUTER_H4_CMD 0x01U
#define HCI_ROUTER_H4_ACL 0x02U
#define HCI_ROUTER_H4_SCO 0x03U
#define HCI_ROUTER_H4_EVT 0x04U
#define HCI_ROUTER_H4_ISO 0x05U

/** Largest event: 2-byte header and 255 bytes of parameters. */
#define HCI_ROUTER_MAX_EVT_LEN (2U + 255U)
/** Largest ACL payload reassembled (the BR/EDR buffer size of the SF32LB52). */
#define HCI_ROUTER_MAX_ACL_DATA 1021U
#define HCI_ROUTER_RX_BUF_SIZE  (4U + HCI_ROUTER_MAX_ACL_DATA)

#define HCI_ROUTER_MAX_CLASSIC_HANDLES 8U

enum hci_router_owner {
  HCI_ROUTER_OWNER_NIMBLE = 0,
  HCI_ROUTER_OWNER_CLASSIC,
  /** Commands the router sends on its own, e.g. a merged event mask. */
  HCI_ROUTER_OWNER_ROUTER,
};

struct hci_router_stats {
  uint32_t nimble_evt;
  uint32_t nimble_acl;
  uint32_t classic_evt;
  uint32_t classic_acl;
  /** Classic packets dropped because no classic host is attached. */
  uint32_t drop_classic;
  /** NimBLE-bound events longer than its event buffers. */
  uint32_t drop_evt_too_long;
  /** Bytes skipped while looking for a valid H4 packet type. */
  uint32_t drop_unknown_type;
  uint32_t drop_sco;
  uint32_t drop_iso;
  /** Packets longer than the reassembly buffer. */
  uint32_t drop_oversize;
  /** Packets with a header that does not fit their type, e.g. a short NOCP. */
  uint32_t drop_malformed;
  /** NimBLE buffer allocation failed. */
  uint32_t drop_alloc;
  /** SF32LB52 vendor "BLE ready" Command Complete, never given to NimBLE. */
  uint32_t drop_vendor_ready;
  /** Command Complete/Status for a command not in flight; sent to NimBLE. */
  uint32_t cc_unmatched;
  /** Acks for commands the router sent on its own. */
  uint32_t router_acks;
  /** Number of Completed Packets events split between the two hosts. */
  uint32_t nocp_split;
  uint32_t tx_cmd;
  uint32_t tx_data;
  uint32_t tx_errors;
  /** Times a sender found the TX lock held. */
  uint32_t tx_lock_waits;
  /** Times a command had to wait for the one in flight. */
  uint32_t gate_waits;
  /** Times that wait gave up and sent anyway. */
  uint32_t gate_timeouts;
  /** Classic host commands sent to the controller unchanged. */
  uint32_t classic_cmd_passed;
  /** Classic host event masks merged with NimBLE's before sending. */
  uint32_t classic_cmd_merged;
  /** Classic host commands answered locally with success (would change LE state). */
  uint32_t classic_cmd_faked;
  /** Classic host commands refused locally (unknown LE or vendor commands). */
  uint32_t classic_cmd_rejected;
};

/** Parameters of a Synchronous Connection Complete event. */
#define HCI_ROUTER_SYNC_CONN_LEN 17U

/** Entries in the classic HCI log. */
#define HCI_ROUTER_LOG_LEN 32U

enum hci_router_log_kind {
  /** A classic host command, and what the router did with it (enum below). */
  HCI_ROUTER_LOG_CMD = 1,
  /** An event handed to the classic host. */
  HCI_ROUTER_LOG_EVT,
};

enum hci_router_log_action {
  HCI_ROUTER_LOG_PASS = 0,
  HCI_ROUTER_LOG_MERGE,
  HCI_ROUTER_LOG_FAKE,
  HCI_ROUTER_LOG_REJECT,
};

/** One classic HCI exchange: enough to tell where a BR/EDR procedure stopped. */
struct hci_router_log_entry {
  /** Running number, so a wrap shows. */
  uint16_t seq;
  uint8_t kind;
  /** For a command, enum hci_router_log_action. */
  uint8_t action;
  /** Opcode, or event code. */
  uint16_t code;
  /** First parameter bytes (status, handle, reason, ...). */
  uint8_t params[6];
  uint8_t len;
  uint8_t reserved[3];
};

/** An SMP Pairing Request or Response seen on an LE link. */
struct hci_router_smp_pdu {
  bool valid;
  /** True if it came from the controller (the peer sent it). */
  bool rx;
  uint8_t io_cap;
  uint8_t oob;
  uint8_t authreq;
  uint8_t max_key_size;
  uint8_t init_key_dist;
  uint8_t resp_key_dist;
};

struct hci_router_ops {
  /** Hand an event to NimBLE. Returns 0, or a negative value if no buffer was free. */
  int (*nimble_evt)(void *ctx, const uint8_t *evt, size_t len, bool discardable);
  /** Hand an ACL packet (with its 4-byte header) to NimBLE. Same return as above. */
  int (*nimble_acl)(void *ctx, const uint8_t *acl, size_t len);
  /** Take the TX lock; with @p wait false only try. Returns whether it is held. */
  bool (*tx_lock)(void *ctx, bool wait);
  void (*tx_unlock)(void *ctx);
  /** Write bytes to the controller. Returns 0 on success. */
  int (*tx_write)(void *ctx, const uint8_t *data, size_t len);
  /** Take the command gate; with @p wait false only try, else wait a bounded time. */
  bool (*gate_take)(void *ctx, bool wait);
  void (*gate_give)(void *ctx);
  void *ctx;
};

typedef void (*hci_router_classic_cb_t)(void *ctx, const uint8_t *pkt, size_t len);

/** Yields the fragments of an outgoing packet; returns false when there are no more. */
typedef bool (*hci_router_frag_next_t)(void *it, const uint8_t **data, size_t *len);

struct hci_router {
  const struct hci_router_ops *ops;
  struct hci_router_stats stats;
  size_t nimble_evt_max;

  /* RX reassembly */
  uint8_t rx_state;
  uint8_t rx_type;
  uint8_t rx_hdr_len;
  bool rx_discard;
  uint32_t rx_pos;
  uint32_t rx_total;
  uint8_t rx_buf[HCI_ROUTER_RX_BUF_SIZE];
  uint8_t scratch[HCI_ROUTER_MAX_EVT_LEN];

  /* Command in flight */
  volatile bool cmd_pending;
  volatile uint16_t cmd_opcode;
  volatile uint8_t cmd_owner;

  /* Connection handles owned by the classic host */
  uint16_t classic_handles[HCI_ROUTER_MAX_CLASSIC_HANDLES];
  uint8_t num_classic_handles;

  /* Event masks (page 1 and 2), merged into NimBLE's */
  uint64_t nimble_evt_mask;
  uint64_t classic_evt_mask;
  bool nimble_evt_mask_valid;
  uint64_t nimble_evt_mask2;
  uint64_t classic_evt_mask2;
  bool nimble_evt_mask2_valid;

  hci_router_classic_cb_t classic_evt;
  hci_router_classic_cb_t classic_acl;
  void *classic_ctx;

  /* Classic HCI log (ring) and the last LE pairing request and response */
  struct hci_router_log_entry log[HCI_ROUTER_LOG_LEN];
  uint16_t log_seq;
  struct hci_router_smp_pdu smp_req;
  struct hci_router_smp_pdu smp_rsp;
  /* Parameters of the last Synchronous Connection Complete for the classic host */
  uint8_t sync_conn[HCI_ROUTER_SYNC_CONN_LEN];
  bool sync_conn_valid;
};

/**
 * @brief Set up a router.
 *
 * @param r Router.
 * @param ops Transport hooks.
 * @param nimble_evt_max Size of NimBLE's event buffers; longer events are dropped.
 */
void hci_router_init(struct hci_router *r, const struct hci_router_ops *ops, size_t nimble_evt_max);

/**
 * @brief Forget the stream and the command in flight, e.g. when the controller restarts.
 *
 * Statistics and the classic host registration are kept.
 */
void hci_router_reset(struct hci_router *r);

/** @brief Feed bytes received from the controller; may span or split packets. */
void hci_router_rx(struct hci_router *r, const uint8_t *data, size_t len);

/**
 * @brief Send a command.
 *
 * @param r Router.
 * @param owner Host that gets the Command Complete/Status.
 * @param cmd Opcode, parameter length and parameters, without the H4 type.
 * @param len Length of @p cmd.
 * @return 0 on success, negative if it could not be written.
 */
int hci_router_tx_cmd(struct hci_router *r, enum hci_router_owner owner, const uint8_t *cmd,
                      size_t len);

/**
 * @brief Send an ACL or ISO packet made of fragments, under one TX lock.
 *
 * @return 0 on success, negative if it could not be written.
 */
int hci_router_tx_data(struct hci_router *r, uint8_t h4_type, hci_router_frag_next_t next,
                       void *it);

/** @brief Attach the classic host's receive callbacks; NULL detaches. */
void hci_router_set_classic(struct hci_router *r, hci_router_classic_cb_t evt,
                            hci_router_classic_cb_t acl, void *ctx);

/**
 * @brief Send a command for the classic host.
 *
 * Commands are passed, merged or answered locally so that NimBLE's controller
 * state is never changed:
 * - BR/EDR commands and LE reads go to the controller unchanged;
 * - Set Event Mask (pages 1 and 2) is merged with NimBLE's mask;
 * - HCI_Reset, host flow control, Write LE Host Supported and the LE setters
 *   the host uses at start-up get a local successful Command Complete;
 * - any other LE command gets Command Disallowed, a vendor command Unknown
 *   HCI Command.
 */
int hci_router_classic_cmd(struct hci_router *r, const uint8_t *cmd, size_t len);

/** @brief Set the events the classic host wants; merged with NimBLE's mask. */
int hci_router_classic_set_event_mask(struct hci_router *r, uint64_t mask);

/**
 * @brief Copy the classic HCI log, oldest first.
 * @return Number of entries copied, at most HCI_ROUTER_LOG_LEN.
 */
size_t hci_router_log_copy(const struct hci_router *r, struct hci_router_log_entry *out);

/** @brief Whether a connection handle belongs to the classic host. */
bool hci_router_handle_is_classic(const struct hci_router *r, uint16_t handle);

/* System-wide router, implemented by the HCI transport. */

/** @brief Whether this boot routes HCI through the router. */
bool hci_router_is_active(void);

/** @brief Copy the router statistics; zeroes them when the router is inactive. */
void hci_router_get_stats(struct hci_router_stats *out);

/** @brief System router's classic HCI log, as hci_router_log_copy(); 0 when inactive. */
size_t hci_router_get_log(struct hci_router_log_entry *out);

/**
 * @brief Parameters of the last Synchronous Connection Complete (status, handle, address, link
 * type, intervals, packet lengths, air mode) the system router handed to the classic host.
 * @return false if there was none this boot.
 */
bool hci_router_get_sync_conn(uint8_t out[HCI_ROUTER_SYNC_CONN_LEN]);

/** @brief Last LE SMP Pairing Request and Response the system router saw. */
void hci_router_get_smp(struct hci_router_smp_pdu *req, struct hci_router_smp_pdu *rsp);

/** @brief Stack size and never-used bytes of the HCI receive thread (classic host callbacks). */
void hci_router_rx_stack(uint32_t *size, uint32_t *unused);

/**
 * @brief Attach the classic host to the system router.
 *
 * The callbacks run on the HCI receive thread.
 *
 * @return 0, or -ENODEV when this boot does not route through the router.
 */
int hci_router_classic_attach(hci_router_classic_cb_t evt, hci_router_classic_cb_t acl, void *ctx);

/** @brief Send a classic host command (opcode, length, parameters) through the system router. */
int hci_router_classic_send_cmd(const uint8_t *cmd, size_t len);

/** @brief Send a classic host ACL packet (with its 4-byte header) through the system router. */
int hci_router_classic_send_acl(const uint8_t *pkt, size_t len);

/* One-shot enable, see hci_router_boot.c. */

/**
 * @brief Ask for the next boot, and only that one, to route through the router.
 *
 * @return 0 on success, negative on a filesystem error.
 */
int hci_router_boot_reserve(void);

/**
 * @brief Read and delete the reservation.
 *
 * Called once per boot before the controller is powered, so a boot that dies
 * in Bluetooth start-up falls back to the plain transport on the next one.
 *
 * @return Whether a reservation was present.
 */
bool hci_router_boot_consume(void);

#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
/**
 * @brief The routed boot came up (the classic host started): the next one routes too.
 *
 * With CONFIG_BT_ZEPHYR_CLASSIC_AUTO every boot routes, without a reservation, unless the last
 * routed one never confirmed: then routing stays off until hci_router_boot_reserve().
 */
void hci_router_boot_confirm(void);
#endif
