/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// HCI driver and host side of the Zephyr host: its packets go through the
// classic port of the PebbleOS HCI router, which shares the controller with
// NimBLE.
//
// Packets for the Zephyr host arrive on the router's receive thread, which
// copies them into host buffers and queues them to the ZephyrRX thread; that
// thread hands them to bt_recv(), as an HCI driver's RX thread would.
//
// Everything here lives in the worker RAM and only between zbt_start() and
// zbt_stop(); ctl/zbt_ctl.c starts and stops it.

#include "zbt_hfp.h"
#include "zbt_pbap.h"
#include "zbt_port.h"
#include "zbt_power.h"

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/hci_router.h>
#include <pbl/kernel/thread.h>

#include <host/keys.h>
#include <zbt/zbt.h>
#include <zbt_link.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/buf.h>
#include <zephyr/bluetooth/classic/classic.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/bluetooth.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/iterable_sections.h>

// Disconnecting at stop waits at most this long.
#define STOP_DISCONNECT_MS 3000
// The router's receive thread waits this long for a host buffer.
#define RX_BUF_WAIT_MS 50

static struct bt_hci_driver_data s_drv_data;
static bool s_halted;

/* Fatal errors ---------------------------------------------------------------*/

void zbt_fatal(const char *file, int line) {
  s_halted = true;
  zbt_ctl_fatal(file, line);
  // End the host's own threads here; zbt_stop() then releases the RAM. On the
  // router's receive thread, return: the gate drops input from here on.
  if (zbt_executor_is_current() || zbt_rx_thread_is_current()) {
    pbl_thread_abort(NULL);
  }
}

/* Receive: router classic port -> ZephyrRX -> bt_recv --------------------------*/

void zbt_host_rx(const uint8_t *pkt, size_t len, bool evt) {
  struct zbt_status *st = zbt_ctl_status();
  if (s_drv_data.recv == NULL || s_halted || len == 0) {
    st->rx_drop++;
    return;
  }

  struct net_buf *buf = evt ? bt_buf_get_evt(pkt[0], false, K_MSEC(RX_BUF_WAIT_MS))
                            : bt_buf_get_rx(BT_BUF_ACL_IN, K_MSEC(RX_BUF_WAIT_MS));
  if (buf == NULL || net_buf_tailroom(buf) < len) {
    if (buf != NULL) {
      net_buf_unref(buf);
    }
    st->rx_drop++;
    return;
  }

  net_buf_add_mem(buf, pkt, len);
  if (evt) {
    st->rx_evt++;
    if ((pkt[0] == BT_HCI_EVT_CMD_COMPLETE && len >= 5) ||
        (pkt[0] == BT_HCI_EVT_CMD_STATUS && len >= 6)) {
      const uint8_t *op = &pkt[pkt[0] == BT_HCI_EVT_CMD_COMPLETE ? 3 : 4];
      st->last_cmd_done = op[0] | (op[1] << 8);
    }
  } else {
    st->rx_acl++;
  }
  zbt_rx_thread_put(buf);
}

static void prv_rx_handler(void *item) {
  bt_hci_recv(&zbt_hci_dev, item);
}

/* Driver ---------------------------------------------------------------------*/

// HCI fields are little endian; the host only skips the swap with
// CONFIG_LITTLE_ENDIAN, which comes from Zephyr's arch Kconfig, not Bluetooth's.
BUILD_ASSERT(sys_cpu_to_le16(0x0102) == 0x0102, "Zephyr host built big endian");

static int prv_open(const struct device *dev) {
  return zbt_ctl_hci_open();
}

static int prv_send(const struct device *dev, struct net_buf *buf) {
  struct zbt_status *st = zbt_ctl_status();
  // The host leaves the H4 type in front of the packet.
  const uint8_t type = net_buf_pull_u8(buf);
  int rc;

  switch (type) {
    case BT_HCI_H4_CMD:
      if (buf->len >= 2) {
        st->last_cmd_sent = buf->data[0] | (buf->data[1] << 8);
      }
      st->tx_cmd++;
      rc = hci_router_classic_send_cmd(buf->data, buf->len);
      break;
    case BT_HCI_H4_ACL:
      st->tx_acl++;
      rc = hci_router_classic_send_acl(buf->data, buf->len);
      break;
    default:
      rc = -EINVAL;
      break;
  }

  net_buf_unref(buf);
  return rc;
}

static const struct bt_hci_driver_api s_driver_api = {
  .open = prv_open,
  .send = prv_send,
};

const struct device zbt_hci_dev = {
  .name = "zbt_hci",
  .api = &s_driver_api,
  .data = &s_drv_data,
};

/* Host operations, run on the Zephyr thread ------------------------------------*/

static struct k_work s_start_work;
static struct k_work s_discoverable_work;
static struct k_work s_stop_work;
static struct k_work_delayable s_stop_poll;
static char s_name[CONFIG_BT_DEVICE_NAME_MAX + 1];
static uint32_t s_cod;
static bool s_discoverable;
static bool s_quiesced;
static int64_t s_stop_deadline;

/* Link key, kept by the control side */

BUILD_ASSERT(BT_KEYS_LINK_KEY_STORAGE_LEN <= ZBT_LINK_KEY_MAX, "Link key does not fit");

// The key the host holds, to tell a new one by (given, derived or last saved).
static struct zbt_link_key s_key;
// The key file's key (given at start, or last saved), and the one derived from the LE pairing
// at start. The key file's goes first: it is there because a link got encrypted with it (and a
// pairing made after the LE one must not lose to the LE one's key). A link refused before
// encryption takes the other one, for the next try.
static struct zbt_link_key s_saved;
static struct zbt_derived_key s_derived;
// The link got encrypted (since it came up); it took the other key once already; the watch
// set it up, on its own (zbt_host_connect()'s unattended).
static bool s_link_encrypted;
static bool s_link_swapped;
static bool s_link_outgoing;
static bool s_link_unattended;
static struct k_work s_unpair_work;

static void prv_install_derived_key(void) {
  struct zbt_status *st = zbt_ctl_status();
  bt_addr_t addr;
  memcpy(addr.val, s_derived.addr, sizeof(addr.val));
  struct bt_keys_link_key *key = bt_keys_get_link_key(&addr);
  if (key == NULL) {
    return;
  }
  // Authenticated combination key from P-256 (type 0x08), when the LE pairing was authenticated.
  key->flags = BT_LINK_KEY_SC | (s_derived.authenticated ? BT_LINK_KEY_AUTHENTICATED : 0);
  memcpy(key->val, s_derived.val, sizeof(key->val));
  st->key_loaded = true;
  // Taken as saved, so the key file is only written when the link gets another key.
  s_key = (struct zbt_link_key){.valid = true, .len = BT_KEYS_LINK_KEY_STORAGE_LEN};
  memcpy(s_key.addr, addr.val, sizeof(s_key.addr));
  memcpy(s_key.data, key->storage_start, s_key.len);
}

static bool prv_install_saved_key(void) {
  struct zbt_status *st = zbt_ctl_status();
  if (!s_saved.valid || s_saved.len != BT_KEYS_LINK_KEY_STORAGE_LEN) {
    return false;
  }
  bt_addr_t addr;
  memcpy(addr.val, s_saved.addr, sizeof(addr.val));
  struct bt_keys_link_key *key = bt_keys_get_link_key(&addr);
  if (key == NULL) {
    return false;
  }
  memcpy(key->storage_start, s_saved.data, s_saved.len);
  st->key_loaded = true;
  s_key = s_saved;
  return true;
}

static void prv_install_key(void) {
  struct zbt_status *st = zbt_ctl_status();
  st->key_source = '-';
  if (prv_install_saved_key()) {
    st->key_source = 's';
  } else if (s_derived.valid) {
    prv_install_derived_key();
    st->key_source = 'd';
  }
}

// A link refused before it got encrypted (the phone has another key for the watch): the other
// key, if there is one and the link did not try it already. Returns whether it took it.
static bool prv_swap_key(bool refused) {
  struct zbt_status *st = zbt_ctl_status();
  if (s_link_encrypted || s_link_swapped || !refused || !s_saved.valid || !s_derived.valid) {
    return false;
  }
  s_link_swapped = true;
  if (st->key_source == 's') {
    prv_install_derived_key();
    st->key_source = 'd';
  } else if (prv_install_saved_key()) {
    st->key_source = 's';
  }
  st->key_swaps++;
  return true;
}

// After a link got encrypted: hand a new or changed key to the control side.
static void prv_check_key(struct bt_conn *conn) {
  const bt_addr_t *addr = bt_conn_get_dst_br(conn);
  struct bt_keys_link_key *key = addr ? bt_keys_find_link_key(addr) : NULL;
  if (key == NULL) {
    return;
  }
  struct zbt_link_key now = {.valid = true, .len = BT_KEYS_LINK_KEY_STORAGE_LEN};
  memcpy(now.addr, addr->val, sizeof(now.addr));
  memcpy(now.data, key->storage_start, now.len);
  // Not the key file's (a new pairing, or the derived key taking a stale one's place): saved.
  if (s_saved.valid && memcmp(&s_saved, &now, sizeof(now)) == 0) {
    return;
  }
  s_key = now;
  s_saved = now;
  zbt_ctl_status()->key_source = 's';
  zbt_ctl_save_key(&now);
}

static void prv_unpair_work(struct k_work *work) {
  bt_keys_link_key_clear_addr(NULL);
  s_key = (struct zbt_link_key){};
  s_saved = (struct zbt_link_key){};
  s_derived = (struct zbt_derived_key){};
  zbt_ctl_status()->key_source = '-';
}

/* BR/EDR link and pairing */

static struct k_work s_connect_work;
static struct k_work s_reply_work;
static bt_addr_t s_peer;
static bool s_reply_accept;
// The link to the phone; one reference held.
static struct bt_conn *s_conn;

static bool prv_is_br(struct bt_conn *conn) {
  struct bt_conn_info info;
  return bt_conn_get_info(conn, &info) == 0 && info.type == BT_CONN_TYPE_BR;
}

static void prv_drop_conn(struct bt_conn *conn) {
  if (conn == s_conn) {
    bt_conn_unref(s_conn);
    s_conn = NULL;
  }
}

static void prv_connected(struct bt_conn *conn, uint8_t err) {
  struct zbt_status *st = zbt_ctl_status();
  if (!prv_is_br(conn)) {
    return;
  }
  st->link_err = err;
  if (err != 0) {
    st->link = ZBT_LINK_DISCONNECTED;
    prv_drop_conn(conn);
    return;
  }
  st->link = ZBT_LINK_CONNECTED;
  s_link_encrypted = false;
  zbt_ctl_link_changed(true);
  if (s_conn == NULL) {
    s_conn = bt_conn_ref(conn);
  }
  // Authenticated, so a new pairing is a numeric comparison.
  st->security_err = bt_conn_set_security(conn, BT_SECURITY_L3);
}

static void prv_disconnected(struct bt_conn *conn, uint8_t reason) {
  struct zbt_status *st = zbt_ctl_status();
  if (!prv_is_br(conn)) {
    return;
  }
  st->link = ZBT_LINK_DISCONNECTED;
  st->link_err = reason;
  st->encrypted = false;
  // Refused (the phone cuts the link, Android does): with the other key, again at once if the
  // watch had set the link up.
  const bool retry =
      prv_swap_key(reason == BT_HCI_ERR_AUTH_FAIL || reason == BT_HCI_ERR_PIN_OR_KEY_MISSING ||
                   reason == BT_HCI_ERR_REMOTE_USER_TERM_CONN) &&
      s_link_outgoing;
  if (!retry && (reason == BT_HCI_ERR_AUTH_FAIL || reason == BT_HCI_ERR_PIN_OR_KEY_MISSING)) {
    st->link_refused = true;
  }
  zbt_ctl_link_changed(false);
  zbt_hfp_link_down(conn);
  zbt_power_link_down();
  prv_drop_conn(conn);
  if (retry) {
    k_work_submit(&s_connect_work);
  } else {
    // The next link starts over: either key, once.
    s_link_outgoing = false;
    s_link_unattended = false;
    s_link_swapped = false;
  }
}

static void prv_security_changed(struct bt_conn *conn, bt_security_t level,
                                 enum bt_security_err err) {
  struct zbt_status *st = zbt_ctl_status();
  if (!prv_is_br(conn)) {
    return;
  }
  st->security = level;
  st->security_err = err;
  st->encrypted = err == BT_SECURITY_ERR_SUCCESS && level >= BT_SECURITY_L2;
  // Refused with the link up (the key does not match): the other key, and ask again.
  const bool key_refused =
      err == BT_SECURITY_ERR_AUTH_FAIL || err == BT_SECURITY_ERR_PIN_OR_KEY_MISSING;
  if (!st->encrypted && prv_swap_key(key_refused)) {
    st->security_err = bt_conn_set_security(conn, BT_SECURITY_L3);
    return;
  }
  if (!st->encrypted && key_refused) {
    st->link_refused = true;
  }
  if (st->encrypted) {
    s_link_encrypted = true;
    st->link_refused = false;
    prv_check_key(conn);
    zbt_hfp_link_encrypted(conn);
    zbt_power_link_up();
  }
}

static struct bt_conn_cb s_conn_cb = {
  .connected = prv_connected,
  .disconnected = prv_disconnected,
  .security_changed = prv_security_changed,
  .br.mode_changed = zbt_power_mode_changed,
};

static void prv_passkey_display(struct bt_conn *conn, unsigned int passkey) {
  zbt_ctl_status()->passkey = passkey;
}

// A pairing on a link the watch set up on its own: no one to confirm it, and the phone's user
// should not be asked again and again. Cancelled, and taken as refused.
static bool prv_unattended_pairing(struct bt_conn *conn) {
  if (!s_link_outgoing || !s_link_unattended) {
    return false;
  }
  struct zbt_status *st = zbt_ctl_status();
  (void)bt_conn_auth_cancel(conn);
  st->pairing = ZBT_PAIRING_CANCELLED;
  st->link_refused = true;
  return true;
}

static void prv_passkey_confirm(struct bt_conn *conn, unsigned int passkey) {
  struct zbt_status *st = zbt_ctl_status();
  if (prv_unattended_pairing(conn)) {
    return;
  }
  st->passkey = passkey;
  st->pairing = ZBT_PAIRING_CONFIRM;
}

static void prv_pairing_confirm(struct bt_conn *conn) {
  if (prv_unattended_pairing(conn)) {
    return;
  }
  zbt_ctl_status()->pairing = ZBT_PAIRING_CONSENT;
}

static void prv_auth_cancel(struct bt_conn *conn) {
  zbt_ctl_status()->pairing = ZBT_PAIRING_CANCELLED;
}

// Display and yes/no: DisplayYesNo, so numeric comparison with a phone.
static struct bt_conn_auth_cb s_auth_cb = {
  .passkey_display = prv_passkey_display,
  .passkey_confirm = prv_passkey_confirm,
  .pairing_confirm = prv_pairing_confirm,
  .cancel = prv_auth_cancel,
};

static void prv_pairing_complete(struct bt_conn *conn, bool bonded) {
  struct zbt_status *st = zbt_ctl_status();
  st->pairing = ZBT_PAIRING_DONE;
  st->pairing_err = 0;
  st->bonded = bonded;
  // A pairing the phone started: only the side that asked for authentication encrypts after it
  // (ssp.c, bt_hci_auth_complete()), and a phone that then connects no profile leaves the link
  // as it is. Ask for it with the new key.
  if (prv_is_br(conn) && !st->encrypted) {
    st->security_err = bt_conn_set_security(conn, BT_SECURITY_L3);
  }
}

static void prv_pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
  struct zbt_status *st = zbt_ctl_status();
  st->pairing = ZBT_PAIRING_FAILED;
  st->pairing_err = reason;
  if (s_link_outgoing && s_link_unattended) {
    st->link_refused = true;
  }
}

static struct bt_conn_auth_info_cb s_auth_info_cb = {
  .pairing_complete = prv_pairing_complete,
  .pairing_failed = prv_pairing_failed,
};

static void prv_connect_work(struct k_work *work) {
  struct zbt_status *st = zbt_ctl_status();
  if (s_conn != NULL) {
    st->link_err = -EALREADY;
    return;
  }
  st->link = ZBT_LINK_CONNECTING;
  st->link_err = 0;
  s_link_outgoing = true;
  st->pairing = ZBT_PAIRING_NONE;
  st->encrypted = false;
  s_conn = bt_conn_create_br(&s_peer, BT_BR_CONN_PARAM_DEFAULT);
  if (s_conn == NULL) {
    st->link = ZBT_LINK_DISCONNECTED;
    st->link_err = -EIO;
  }
}

static void prv_reply_work(struct k_work *work) {
  struct zbt_status *st = zbt_ctl_status();
  if (s_conn == NULL) {
    return;
  }
  int rc = -EINVAL;
  if (!s_reply_accept) {
    rc = bt_conn_auth_cancel(s_conn);
  } else if (st->pairing == ZBT_PAIRING_CONFIRM) {
    rc = bt_conn_auth_passkey_confirm(s_conn);
  } else if (st->pairing == ZBT_PAIRING_CONSENT) {
    rc = bt_conn_auth_pairing_confirm(s_conn);
  }
  if (rc != 0) {
    st->pairing_err = rc;
  }
}

/* Start */

static void prv_start_work(struct k_work *work) {
  STRUCT_SECTION_FOREACH(zbt_init_entry, entry) {
    (void)entry->init_fn();
  }
  int rc = bt_enable(NULL);
  if (rc == 0) {
    bt_conn_cb_register(&s_conn_cb);
    rc = bt_conn_auth_cb_register(&s_auth_cb);
  }
  if (rc == 0) {
    rc = bt_conn_auth_info_cb_register(&s_auth_info_cb);
  }
  if (rc == 0 && s_name[0] != '\0') {
    // As LE advertises it, so the phone shows one name for the watch.
    rc = bt_set_name(s_name);
  }
  if (rc == 0) {
    prv_install_key();
    rc = zbt_hfp_init();
  }
  if (rc == 0) {
    zbt_pbap_init();
    zbt_ctl_status()->sniff_err = zbt_power_init();
  }
  zbt_ctl_enabled(rc);
}

static int prv_set_discoverable(void) {
  int rc = bt_set_name(s_name);
  if (rc == 0) {
    rc = bt_br_set_class_of_device(s_cod);
  }
  if (rc == 0) {
    const struct bt_data eir[] = {
      BT_DATA(BT_DATA_NAME_COMPLETE, s_name, strlen(s_name)),
    };
    rc = bt_br_write_eir(eir, ARRAY_SIZE(eir), true);
  }
  if (rc == 0) {
    rc = bt_br_set_connectable(true, NULL);
  }
  if (rc == 0) {
    rc = bt_br_set_discoverable(true, false);
  }
  return rc;
}

static int prv_clear_discoverable(void) {
  int rc = bt_br_set_discoverable(false, false);
  if (rc == 0) {
    rc = bt_br_set_connectable(false, NULL);
  }
  return rc;
}

static void prv_discoverable_work(struct k_work *work) {
  struct zbt_status *st = zbt_ctl_status();
  st->discoverable_err = s_discoverable ? prv_set_discoverable() : prv_clear_discoverable();
  st->discoverable = s_discoverable && st->discoverable_err == 0;
}

static void prv_count_conn(struct bt_conn *conn, void *data) {
  (*(int *)data)++;
}

static void prv_disconnect(struct bt_conn *conn, void *data) {
  (void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void prv_stop_poll(struct k_work *work) {
  int conns = 0;
  bt_conn_foreach(BT_CONN_TYPE_BR, prv_count_conn, &conns);
  if (conns == 0 || k_uptime_get() >= s_stop_deadline) {
    s_quiesced = true;
    return;
  }
  k_work_schedule(&s_stop_poll, K_MSEC(100));
}

static void prv_stop_work(struct k_work *work) {
  if (zbt_ctl_status()->discoverable) {
    (void)prv_clear_discoverable();
    zbt_ctl_status()->discoverable = false;
  }
  bt_conn_foreach(BT_CONN_TYPE_BR, prv_disconnect, NULL);
  s_stop_deadline = k_uptime_get() + STOP_DISCONNECT_MS;
  prv_stop_poll(&s_stop_poll.work);
}

/* Called by the control side -------------------------------------------------*/

int zbt_host_start(const struct zbt_link_key *key, const struct zbt_derived_key *derived,
                   const char *name, const MpuRegion *const regions[PBL_THREAD_MAX_MEM_REGIONS],
                   struct zbt_host_threads *threads) {
  strncpy(s_name, name, sizeof(s_name) - 1);
  if (key != NULL) {
    s_key = *key;
    s_saved = *key;
  }
  if (derived != NULL) {
    s_derived = *derived;
  }
  k_work_init(&s_unpair_work, prv_unpair_work);
  k_work_init(&s_start_work, prv_start_work);
  k_work_init(&s_discoverable_work, prv_discoverable_work);
  k_work_init(&s_stop_work, prv_stop_work);
  k_work_init_delayable(&s_stop_poll, prv_stop_poll);
  k_work_init(&s_connect_work, prv_connect_work);
  k_work_init(&s_reply_work, prv_reply_work);

  zbt_rx_thread_start(prv_rx_handler, threads, regions);
  zbt_executor_start(threads, regions);
  if (zbt_threads_stopped()) {
    return -ENOMEM;
  }
  k_work_submit(&s_start_work);
  return 0;
}

void zbt_host_stop(void) {
  if (s_halted) {
    // The Zephyr thread may be gone; nothing to disconnect through.
    s_quiesced = true;
    return;
  }
  k_work_submit(&s_stop_work);
}

bool zbt_host_quiesced(void) {
  return s_quiesced;
}

void zbt_host_quit(void) {
  zbt_executor_quit();
  zbt_rx_thread_quit();
}

bool zbt_host_stopped(void) {
  return zbt_threads_stopped();
}

int zbt_host_set_discoverable(bool on, const char *name, uint32_t cod) {
  if (on) {
    if (name != NULL) {
      strncpy(s_name, name, sizeof(s_name) - 1);
    }
    s_cod = cod;
  }
  s_discoverable = on;
  k_work_submit(&s_discoverable_work);
  return 0;
}

int zbt_host_connect(const uint8_t addr[6], bool unattended) {
  memcpy(s_peer.val, addr, sizeof(s_peer.val));
  s_link_unattended = unattended;
  k_work_submit(&s_connect_work);
  return 0;
}

int zbt_host_pairing_reply(bool accept) {
  s_reply_accept = accept;
  k_work_submit(&s_reply_work);
  return 0;
}

int zbt_host_unpair(void) {
  k_work_submit(&s_unpair_work);
  return 0;
}

void zbt_host_counts(uint32_t *exec_loops, uint32_t *rx_items) {
  zbt_threads_counts(exec_loops, rx_items);
}

void zbt_host_stacks(struct zbt_stack *exec, struct zbt_stack *rx) {
  zbt_threads_stacks(exec, rx);
}
