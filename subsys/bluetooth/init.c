/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "adv/adv.h"
#include "clients/op_queue.h"
#include "responsiveness.h"
#include "shell.h"
#include "store.h"

#include <errno.h>

#include <pbl/bluetooth/gatts.h>
#include <pbl/bluetooth/id_addr.h>
#include <pbl/bluetooth/init.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>

#include <host/ble_hs.h>
#include <host/ble_hs_stop.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

PBL_LOG_MODULE_DEFINE(bt, CONFIG_BT_LOG_LEVEL);

#define START_STOP_TIMEOUT_MS 10000U

static PBL_SEM_DEFINE(s_host_started, 0, 1);
static PBL_SEM_DEFINE(s_host_stopped, 0, 1);
static struct ble_hs_stop_listener s_listener;

enum state {
  STATE_STOPPED,
  STATE_STARTING,
  STATE_STARTED,
  STATE_STOPPING,
};

// Starting and stopping run to completion or stop the system, so this follows the host's own
// state.
static enum state s_state = STATE_STOPPED;
// What setting the identity address up gave, when the host last synchronized.
static int s_addr_rc;

#define LL_MAX_TX_OCTETS  251
#define LL_MAX_TX_TIME_US 2120

static int prv_ensure_addr(void) {
#ifdef CONFIG_NIMBLE_ID_ADDR
  ble_addr_t addr;
  int rc;

  rc = pbl_bt_id_addr_get(&addr);
  if (rc == -ENOENT) {
    return ble_hs_util_ensure_addr(0);
  }
  if (rc != 0) {
    PBL_LOG_ERR("No identity address (%d)", rc);
    return BLE_HS_ENOADDR;
  }

  // A public one is programmed into the controller by its transport.
  if (addr.type == BLE_ADDR_RANDOM) {
    rc = ble_hs_id_set_rnd(addr.val);
    if (rc != 0) {
      return rc;
    }
  }
#endif

  return ble_hs_util_ensure_addr(0);
}

static void prv_sync_cb(void) {
  int rc;

  PBL_LOG_DBG("NimBLE host synchronized");

  // Controllers default to 27-byte LL PDUs when transmitting.
  rc = ble_gap_write_sugg_def_data_len(LL_MAX_TX_OCTETS, LL_MAX_TX_TIME_US);
  if (rc != 0) {
    PBL_LOG_WRN("Failed to set the default data length (0x%04x)", (uint16_t)rc);
  }

  // The host forgets a random address when it resets, so it is set each time.
  s_addr_rc = prv_ensure_addr();
  if (s_addr_rc != 0) {
    PBL_LOG_ERR("Failed to ensure address: 0x%04x", (uint16_t)s_addr_rc);
  }

  pbl_sem_give(&s_host_started);
#ifdef CONFIG_BT_ADV
  pbl_bt__adv_handle_sync();
#endif
}

static void prv_reset_cb(int reason) {
  PBL_LOG_WRN("NimBLE host reset (reason: 0x%04x)", (uint16_t)reason);
#ifdef CONFIG_NIMBLE_HCI_SF32LB
  // Controller stopped answering HCI. Crash so the coredump captures LCPU RAM
  // (core_dump wakes the LCPU itself); the reboot cold-recovers the controller.
  pbl_panic("NimBLE host reset 0x%04x; captured LCPU RAM", (uint16_t)reason);
#endif
}

static void prv_ble_hs_stop_cb(int status, void *arg) {
  pbl_sem_give(&s_host_stopped);
}

// ----------------------------------------------------------------------------------------
void pbl_bt_init(void) {
  pbl_bt__gattc_op_queue_init();

  nimble_port_init();
  pbl_bt__store_init();
  pbl_bt__responsiveness_init();
#ifdef CONFIG_SHELL
  pbl_bt__shell_init();
#endif

  ble_hs_cfg.sync_cb = prv_sync_cb;
  ble_hs_cfg.reset_cb = prv_reset_cb;

  nimble_port_pebble_init();
}

// Stops the host and waits for it, unless it is stopped already.
static void prv_stop_host(void) {
  bool stopped;
  int rc;

  s_state = STATE_STOPPING;
  (void)pbl_sem_take(&s_host_stopped, PBL_NO_WAIT);

  // No callback follows a refusal.
  rc = ble_hs_stop(&s_listener, prv_ble_hs_stop_cb, nullptr);
  if (rc == 0) {
    stopped = (pbl_sem_take(&s_host_stopped, PBL_MSEC(START_STOP_TIMEOUT_MS)) == 0);
    PBL_ASSERT(stopped, "NimBLE host stop timed out");
  } else if (rc != BLE_HS_EALREADY) {
    PBL_LOG_ERR("Failed to stop the NimBLE host: 0x%04x", (uint16_t)rc);
  }

  s_state = STATE_STOPPED;
  (void)ble_gatts_reset();
}

int pbl_bt_start(const struct pbl_bt_sm_key *irk) {
  int rc;

  if (s_state == STATE_STARTED) {
    return 0;
  }

  if (s_state != STATE_STOPPED) {
    return -EBUSY;
  }

  s_state = STATE_STARTING;
  // One left by a host that synchronized again on its own.
  (void)pbl_sem_take(&s_host_started, PBL_NO_WAIT);

  pbl_bt__store_set_local_irk(irk);

  ble_svc_gap_init();
  ble_svc_gatt_init();
  rc = pbl_bt_gatts_add_services();
  if (rc != 0) {
    (void)ble_gatts_reset();
    s_state = STATE_STOPPED;
    return -EIO;
  }

  ble_hs_sched_start();
  if (pbl_sem_take(&s_host_started, PBL_MSEC(START_STOP_TIMEOUT_MS)) != 0) {
    // core_dump wakes the LCPU itself, so its RAM is captured here too.
    pbl_panic("NimBLE host start timed out");
  }

  if (s_addr_rc != 0) {
    prv_stop_host();
    return -EADDRNOTAVAIL;
  }

  s_state = STATE_STARTED;

  return 0;
}

void pbl_bt_stop(void) {
  if (s_state != STATE_STARTED) {
    return;
  }

  prv_stop_host();
}
