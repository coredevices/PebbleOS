/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gatt_service_changed.h"

#include "gap_le_connection.h"

#include <pbl/bluetooth/gatt.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

#include <comm/ble/kernel_le_client/kernel_le_client.h>
#include <comm/bt_lock.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// Server -- Pebble serving up the "Service Changed" characteristic to the remote

// Work-around for iOS issue where sending the indication immediately when iOS subscribes to the
// characteristic is causing problems:
// BTServer: ATT Failed to locate GAP primary service on device ...
#define GATT_SERVICE_CHANGED_INDICATION_DELAY_MS (10000)

#define GATT_SERVICE_CHANGED_INDICATION_MAX_TIMES (5)

static uint32_t s_service_changed_indications_left;

// For unit testing
void gatt_service_changed_server_init(void) {
  s_service_changed_indications_left = 0;
}

void gatt_service_changed_server_handle_fw_update(void) {
  // Once set, just keep it set until the next "normal" reboot.
  // It will cause Pebble to send the "Service Change" indication to be sent every time the other
  // end subscribes to it, causing the remote cache to be invalidated each time and force the
  // remote to do discover services again. However, cap the total number of times we send the
  // "Service Change" indication:
  s_service_changed_indications_left = GATT_SERVICE_CHANGED_INDICATION_MAX_TIMES;
}

void pbl_bt_cb_gatt_service_changed_server_confirmation(
    const struct pbl_bt_gatt_server_changed_confirmation_event *event) {
  if (event->status_code != PBL_BT_HCI_STATUS_SUCCESS) {
    PBL_LOG_ERR("Service Changed indication confirmation failure (timed out?) %" PRIu32,
                (uint32_t)event->status_code);
  }
}

void gatt_service_changed_server_cleanup_by_connection(GAPLEConnection *connection) {
  // Initialized along with the flag. No need to wait for a running function: it never dereferences
  // the connection.
  if (connection->has_sent_gatt_service_changed_indication) {
    pbl_delayed_work_cancel(&connection->gatt_service_changed_indication_work);
  }
}

static void prv_send_service_changed_indication(void *ctx) {
  GAPLEConnection *connection = (GAPLEConnection *)ctx;

  struct pbl_bt_device_internal device;
  bt_lock();
  {
    // The connection may have been torn down between the timer firing and this
    // callback running, leaving a dangling pointer. Bail if it is no longer live.
    if (!connection || !gap_le_connection_is_valid(connection)) {
      bt_unlock();
      return;
    }
    // Copy the device address while holding bt_lock — the driver resolves the
    // NimBLE connection handle through its own connection table, so no bt_lock
    // is needed in the driver path.
    device = connection->device;
  }
  bt_unlock();

  // Invalidate the remote's entire attribute cache so it rediscovers all of our
  // services (see "2.5.2 Attribute Caching" in the BT Core Specification). This
  // mirrors what the client side treats as a "rediscover everything" request.
  const struct pbl_bt_att_handle_range range = {
    .start = 0x0001,
    .end = 0xFFFF,
  };
  pbl_bt_gatt_send_changed_indication(&device, &range);
}

static void prv_send_indication_fn(struct pbl_work *work) {
  pbl_sys_work_call(
      prv_send_service_changed_indication,
      PBL_CONTAINER_OF(work, GAPLEConnection, gatt_service_changed_indication_work.work));
}

void pbl_bt_cb_gatt_service_changed_server_subscribe(
    const struct pbl_bt_gatt_server_subscribe_event *event) {
  bt_lock();
  {
    if (event->is_subscribing) {
      PBL_LOG_DBG("Remote subscribed to Service Changed characteristic");

      GAPLEConnection *connection = gap_le_connection_by_addr(&event->dev_address);
      if (!connection || connection->has_sent_gatt_service_changed_indication) {
        // Already sent indication once during the lifetime of this connection, don't send again.
        goto unlock;
      }

      // PRF will always send a "Service Changed" indication:
#if !defined(CONFIG_RECOVERY_FW)
      if (s_service_changed_indications_left <= 0) {
        goto unlock;
      }
#endif

      PBL_LOG_INFO("Indicating Service Changed to remote device");

      // Work-around for iOS issue (see comment above), send the indication after a short delay:
      pbl_delayed_work_init(&connection->gatt_service_changed_indication_work,
                            prv_send_indication_fn);
      pbl_delayed_work_reschedule(&pbl_sys_workq_high,
                                  &connection->gatt_service_changed_indication_work,
                                  PBL_MSEC(GATT_SERVICE_CHANGED_INDICATION_DELAY_MS));
      --s_service_changed_indications_left;
      // Don't send again for this connection:
      connection->has_sent_gatt_service_changed_indication = true;
    }
  }
unlock:
  bt_unlock();
}

void pbl_bt_cb_gatt_service_changed_server_read_subscription(
    const struct pbl_bt_gatt_server_read_subscription_event *event) {
  bt_lock();
  {
    pbl_bt_gatt_respond_read_subscription(event->transaction_id, 0 /* not subscribed */);
  }
  bt_unlock();
}

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_svc_changed(const struct pbl_shell *sh, size_t argc, char **argv) {
  prv_send_service_changed_indication(gap_le_connection_any());
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, svc_changed, nullptr, "Send a Service Changed indication",
                     prv_cmd_svc_changed, 0, 0);

static int prv_cmd_rediscover(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (!gap_le_connection_any()) {
    pbl_shell_error(sh, "not connected");
    return -ENOTCONN;
  }

  kernel_le_client_rediscover();
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, rediscover, nullptr, "Rediscover the remote GATT database",
                     prv_cmd_rediscover, 0, 0);
#endif
