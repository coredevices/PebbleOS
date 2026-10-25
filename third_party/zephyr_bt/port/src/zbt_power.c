/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Sniff mode for the phone's link. The controller's default link policy keeps sniff off, so
// a link stayed active and was polled every few slots for as long as it was up. Sniff is
// allowed on every link, and once the link is idle (no call, no voice link, no phone book
// reading) for IDLE_MS, the watch asks for it; whatever needs the link at full speed leaves
// it. All on the Zephyr thread.

#include "zbt_power.h"

#include <errno.h>

#include <zbt/zbt.h>
#include <zbt_link.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/byteorder.h>

// clang-format off
#include <host/hci_core.h>
#include <host/conn_internal.h>
// clang-format on

#include "zbt_hfp.h"

#define IDLE_MS 5000
// Sniff interval, in 0.625 ms slots: first 1.2 to 1.28 s, asked for with the HCI command itself
// since bt_conn_br_enter_sniff_mode() takes at most 0x0540; a ring reaches the watch that much
// later at most. A link that does not take it within SNIFF_CHECK_MS gets 720 to 840 ms.
#define SNIFF_LONG_MIN 0x0780
#define SNIFF_LONG_MAX 0x0800
#define SNIFF_MIN      0x0480
#define SNIFF_MAX      0x0540
#define SNIFF_ATTEMPT  4
#define SNIFF_TIMEOUT  1
#define SNIFF_CHECK_MS 3000

static struct k_work_delayable s_idle_work;
// The long interval asked for and not yet taken; refused on this link.
static bool s_long_pending;
static bool s_long_refused;
static struct k_work s_busy_work;
static struct k_work s_policy_work;

static int prv_policy(uint16_t opcode, const struct bt_conn *conn) {
  // Read, set the sniff bit, write back: the role switch bit is Zephyr's.
  struct net_buf *rsp;
  int err;
  uint16_t policy;
  if (conn == NULL) {
    err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_DEFAULT_LINK_POLICY_SETTINGS, NULL, &rsp);
    if (err != 0) {
      return err;
    }
    policy = ((struct bt_hci_rp_read_default_link_policy_settings *)rsp->data)
                 ->default_link_policy_settings;
  } else {
    struct net_buf *buf = bt_hci_cmd_alloc(K_FOREVER);
    if (buf == NULL) {
      return -ENOBUFS;
    }
    struct bt_hci_cp_read_link_policy_settings *cp = net_buf_add(buf, sizeof(*cp));
    cp->handle = sys_cpu_to_le16(conn->handle);
    err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_LINK_POLICY_SETTINGS, buf, &rsp);
    if (err != 0) {
      return err;
    }
    policy = sys_le16_to_cpu(
        ((struct bt_hci_rp_read_link_policy_settings *)rsp->data)->link_policy_settings);
  }
  net_buf_unref(rsp);
  policy |= BT_HCI_LINK_POLICY_SETTINGS_ENABLE_SNIFF_SWITCH;
  zbt_ctl_status()->link_policy = policy;

  struct net_buf *buf = bt_hci_cmd_alloc(K_FOREVER);
  if (buf == NULL) {
    return -ENOBUFS;
  }
  if (conn == NULL) {
    struct bt_hci_cp_write_default_link_policy_settings *cp = net_buf_add(buf, sizeof(*cp));
    cp->default_link_policy_settings = sys_cpu_to_le16(policy);
  } else {
    struct bt_hci_cp_write_link_policy_settings *cp = net_buf_add(buf, sizeof(*cp));
    cp->handle = sys_cpu_to_le16(conn->handle);
    cp->link_policy_settings = sys_cpu_to_le16(policy);
  }
  return bt_hci_cmd_send_sync(opcode, buf, NULL);
}

// Something needs the link: no sniff for now.
static bool prv_link_needed(const struct zbt_status *st) {
  return st->sco || (st->call != ZBT_CALL_NONE && st->call != ZBT_CALL_ENDED) ||
         st->pbap == ZBT_PBAP_SEARCHING || st->pbap == ZBT_PBAP_CONNECTING ||
         st->pbap == ZBT_PBAP_PULLING;
}

static int prv_enter_long_sniff(const struct bt_conn *acl) {
  struct net_buf *buf = bt_hci_cmd_alloc(K_FOREVER);
  if (buf == NULL) {
    return -ENOBUFS;
  }
  struct bt_hci_cp_sniff_mode *cp = net_buf_add(buf, sizeof(*cp));
  cp->handle = sys_cpu_to_le16(acl->handle);
  cp->max_interval = sys_cpu_to_le16(SNIFF_LONG_MAX);
  cp->min_interval = sys_cpu_to_le16(SNIFF_LONG_MIN);
  cp->attempt = sys_cpu_to_le16(SNIFF_ATTEMPT);
  cp->timeout = sys_cpu_to_le16(SNIFF_TIMEOUT);
  return bt_hci_cmd_send_sync(BT_HCI_OP_SNIFF_MODE, buf, NULL);
}

static void prv_idle_work(struct k_work *work) {
  struct bt_conn *acl = zbt_hfp_acl();
  struct zbt_status *st = zbt_ctl_status();
  if (acl == NULL || st->link_mode == BT_SNIFF_MODE) {
    return;
  }
  if (s_long_pending) {
    // Asked for, and the link is still active: the phone or the controller refused it.
    s_long_pending = false;
    s_long_refused = true;
  }
  if (prv_link_needed(st)) {
    (void)k_work_reschedule(&s_idle_work, K_MSEC(IDLE_MS));
    return;
  }
  if (!s_long_refused) {
    st->sniff_err = prv_enter_long_sniff(acl);
    if (st->sniff_err == 0) {
      s_long_pending = true;
      (void)k_work_reschedule(&s_idle_work, K_MSEC(SNIFF_CHECK_MS));
      return;
    }
    s_long_refused = true;
  }
  st->sniff_err =
      bt_conn_br_enter_sniff_mode(acl, SNIFF_MIN, SNIFF_MAX, SNIFF_ATTEMPT, SNIFF_TIMEOUT);
}

static void prv_busy_work(struct k_work *work) {
  struct bt_conn *acl = zbt_hfp_acl();
  struct zbt_status *st = zbt_ctl_status();
  s_long_pending = false;
  if (acl != NULL && st->link_mode == BT_SNIFF_MODE) {
    st->sniff_err = bt_conn_br_exit_sniff_mode(acl);
  }
  (void)k_work_reschedule(&s_idle_work, K_MSEC(IDLE_MS));
}

static void prv_policy_work(struct k_work *work) {
  struct bt_conn *acl = zbt_hfp_acl();
  s_long_pending = false;
  if (acl != NULL) {
    zbt_ctl_status()->sniff_err = prv_policy(BT_HCI_OP_WRITE_LINK_POLICY_SETTINGS, acl);
  }
  (void)k_work_reschedule(&s_idle_work, K_MSEC(IDLE_MS));
}

void zbt_power_busy(void) {
  k_work_submit(&s_busy_work);
}

void zbt_power_link_up(void) {
  k_work_submit(&s_policy_work);
}

void zbt_power_mode_changed(struct bt_conn *conn, uint8_t mode, uint16_t interval) {
  struct zbt_status *st = zbt_ctl_status();
  st->link_mode = mode;
  st->sniff_interval = mode == BT_SNIFF_MODE ? interval : 0;
  if (mode == BT_SNIFF_MODE) {
    st->sniff_count++;
    s_long_pending = false;
  } else {
    // The phone or the watch woke the link: back to sniff once idle again.
    (void)k_work_reschedule(&s_idle_work, K_MSEC(IDLE_MS));
  }
}

void zbt_power_link_down(void) {
  struct zbt_status *st = zbt_ctl_status();
  st->link_mode = BT_ACTIVE_MODE;
  st->sniff_interval = 0;
  // A new link may take the long interval.
  s_long_pending = false;
  s_long_refused = false;
  (void)k_work_cancel_delayable(&s_idle_work);
}

int zbt_power_init(void) {
  k_work_init_delayable(&s_idle_work, prv_idle_work);
  k_work_init(&s_busy_work, prv_busy_work);
  k_work_init(&s_policy_work, prv_policy_work);
  struct zbt_status *st = zbt_ctl_status();
  st->link_mode = BT_ACTIVE_MODE;
  st->sniff_interval = 0;
  st->sniff_count = 0;
  // Links made from now on allow sniff.
  return prv_policy(BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY_SETTINGS, NULL);
}
