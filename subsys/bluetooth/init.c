/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/services.h"
#include "store.h"

#include <errno.h>

#include <pbl/bluetooth/id_addr.h>
#include <pbl/bluetooth/init.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>

#include <comm/bt_lock.h>
#include <host/ble_hs.h>
#include <host/ble_hs_stop.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

PBL_LOG_MODULE_DEFINE(bt, CONFIG_BT_LOG_LEVEL);

static const uint32_t s_bt_stack_start_stop_timeout_ms = 10000;

extern void nimble_discover_init(void);
extern void nimble_gattc_op_queue_init(void);

static PBL_SEM_DEFINE(s_host_started, 0, 1);
static PBL_SEM_DEFINE(s_host_stopped, 0, 1);
static struct ble_hs_stop_listener s_listener;

typedef enum {
  DriverStateStopped,
  DriverStateStarting,
  DriverStateStarted,
  DriverStateStopping,
} DriverState;

// Host start/stop run to completion or crash on timeout, so this stays in sync
// with the vendor's ble_hs_enabled_state (Stopped<->OFF, Started<->ON).
static DriverState s_driver_state = DriverStateStopped;

#define LL_MAX_TX_OCTETS  251
#define LL_MAX_TX_TIME_US 2120

static void prv_sync_cb(void) {
  PBL_LOG_DBG("NimBLE host synchronized");

  // Controllers default to 27-byte LL PDUs when transmitting.
  int rc = ble_gap_write_sugg_def_data_len(LL_MAX_TX_OCTETS, LL_MAX_TX_TIME_US);
  if (rc != 0) {
    PBL_LOG_WRN("Failed to set the default data length (0x%04x)", (uint16_t)rc);
  }

  pbl_sem_give(&s_host_started);
  pbl_bt_handle_host_resynced();
}

static void prv_reset_cb(int reason) {
  PBL_LOG_WRN("NimBLE host reset (reason: 0x%04x)", (uint16_t)reason);
#ifdef CONFIG_NIMBLE_HCI_SF32LB
  // Controller stopped answering HCI. Crash so the coredump captures LCPU RAM
  // (core_dump wakes the LCPU itself); the reboot cold-recovers the controller.
  pbl_panic("NimBLE host reset 0x%04x; captured LCPU RAM", (uint16_t)reason);
#endif
}

static int prv_ensure_addr(void) {
#ifdef CONFIG_NIMBLE_ID_ADDR
  struct pbl_bt_addr addr;
  enum pbl_bt_id_addr_type type;
  int rc;

  rc = pbl_bt_id_addr_get(&addr, &type);
  if (rc == -ENOENT) {
    return ble_hs_util_ensure_addr(0);
  }
  if (rc != 0) {
    PBL_LOG_ERR("No identity address (%d)", rc);
    return BLE_HS_ENOADDR;
  }

  // A public one is programmed into the controller by its transport.
  if (type == PBL_BT_ID_ADDR_RANDOM_STATIC) {
    rc = ble_hs_id_set_rnd(addr.octets);
    if (rc != 0) {
      return rc;
    }
  }
#endif

  return ble_hs_util_ensure_addr(0);
}

static void prv_ble_hs_stop_cb(int status, void *arg) {
  pbl_sem_give(&s_host_stopped);
}

// ----------------------------------------------------------------------------------------
void pbl_bt_init(void) {
  bt_lock_init();

  nimble_discover_init();
  nimble_gattc_op_queue_init();

  nimble_port_init();
  pbl_bt__store_init();

  ble_hs_cfg.sync_cb = prv_sync_cb;
  ble_hs_cfg.reset_cb = prv_reset_cb;

  nimble_port_pebble_init();
}

bool pbl_bt_start(struct pbl_bt_config *config) {
  int rc;
  bool f_rc;

  if (s_driver_state == DriverStateStarted) {
    PBL_LOG_WRN("Driver already started; skipping start");
    return true;
  }

  if (s_driver_state != DriverStateStopped) {
    PBL_LOG_ERR("Unexpected driver state %u; refusing to start", (unsigned)s_driver_state);
    return false;
  }

  s_driver_state = DriverStateStarting;
  // Drain a stale host_started signal (e.g. from an autonomous host re-sync)
  // so we wait for *this* start to sync.
  (void)(pbl_sem_take(&s_host_started, PBL_NO_WAIT) == 0);

  pbl_bt__store_set_local_irk(&config->root_keys[PBL_BT_SM_ROOT_KEY_TYPE_IDENTITY]);

  ble_svc_gap_init();
  ble_svc_gatt_init();
  rc = pbl_bt__services_init(&config->dis_info);
  if (rc != 0) {
    (void)ble_gatts_reset();
    s_driver_state = DriverStateStopped;
    return false;
  }

  ble_hs_sched_start();
  f_rc = (pbl_sem_take(&s_host_started, PBL_MSEC(s_bt_stack_start_stop_timeout_ms)) == 0);
  if (!f_rc) {
    // core_dump wakes the LCPU itself, so its RAM is captured here too.
    pbl_panic("NimBLE host start timed out");
  }

  rc = prv_ensure_addr();
  if (rc != 0) {
    PBL_LOG_ERR("Failed to ensure address: 0x%04x", (uint16_t)rc);
    goto err;
  }

  s_driver_state = DriverStateStarted;
  return true;

err:
  s_driver_state = DriverStateStopping;
  (void)(pbl_sem_take(&s_host_stopped, PBL_NO_WAIT) == 0);
  rc = ble_hs_stop(&s_listener, prv_ble_hs_stop_cb, nullptr);
  if (rc == BLE_HS_EALREADY) {
    s_driver_state = DriverStateStopped;
    return false;
  } else if (rc != 0) {
    PBL_LOG_ERR("Failed to stop NimBLE host after start failure: 0x%04x", (uint16_t)rc);
    return false;
  }

  f_rc = (pbl_sem_take(&s_host_stopped, PBL_MSEC(s_bt_stack_start_stop_timeout_ms)) == 0);
  PBL_ASSERT(f_rc, "NimBLE host stop timed out after start failure");

  s_driver_state = DriverStateStopped;
  (void)ble_gatts_reset();

  return false;
}

void pbl_bt_stop(void) {
  bool f_rc;

  s_driver_state = DriverStateStopping;
  (void)(pbl_sem_take(&s_host_stopped, PBL_NO_WAIT) == 0);
  ble_hs_stop(&s_listener, prv_ble_hs_stop_cb, nullptr);
  f_rc = (pbl_sem_take(&s_host_stopped, PBL_MSEC(s_bt_stack_start_stop_timeout_ms)) == 0);
  PBL_ASSERT(f_rc, "NimBLE host stop timed out");
  s_driver_state = DriverStateStopped;

  ble_gatts_reset();
}

void pbl_bt_power_down_controller_on_boot(void) {
}
