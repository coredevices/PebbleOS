/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/clients/service_changed.h>
#include <pbl/bluetooth/responsiveness.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/transports/gatt.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/gateway.h>
#if defined(CONFIG_BT_AMS_CLIENT)
#include <pbl/services/music_ams.h>
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
#include <pbl/services/notifications/ancs/ancs.h>
#endif

#include <kernel/event_loop.h>
#include <mfg/mfg_serials.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

//! How long the connection is kept fast for the clients to find their services.
#define DISCOVERY_RESPONSE_TIME_SECS 30

#if defined(CONFIG_PP_TRANSPORT_GATT)
_Static_assert(PBL_PP_GATT_SERIAL_NUMBER_SIZE == MFG_SERIAL_NUMBER_SIZE);
#endif

PBL_BT_SERVICE_CHANGED_CLIENT_DEFINE(s_service_changed);

//! Tells the indications of a gateway that is gone from those of the current one.
static uint32_t s_generation;
static struct pbl_bt_responsiveness_req s_response_time_req;
static bool s_is_connected;

static void prv_services_changed_kernel_main_cb(void *data) {
  if (!s_is_connected || (uintptr_t)data != s_generation) {
    return;
  }

  PBL_LOG_INFO("Service Changed: looking for the services again");

#if defined(CONFIG_PP_TRANSPORT_GATT)
  pbl_pp_gatt_handle_services_changed();
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
  ancs_handle_services_changed();
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
  ams_handle_services_changed();
#endif
}

void bt_gateway_rediscover(void) {
  launcher_task_add_callback(prv_services_changed_kernel_main_cb, (void *)(uintptr_t)s_generation);
}

// On the host task. Whatever the handles affected, each client looks for its service again.
static void prv_services_changed(struct pbl_bt_service_changed_client *client,
                                 uint16_t start_handle, uint16_t end_handle) {
  bt_gateway_rediscover();
}

// -------------------------------------------------------------------------------------------------

static void prv_connected_kernel_main_cb(void *data) {
  const uint16_t conn_handle = (uint16_t)(uintptr_t)data;

  PBL_LOG_DBG("Connected to Gateway!");

  pbl_bt_responsiveness_request(conn_handle, &s_response_time_req, PBL_BT_RESPONSE_TIME_MIN,
                                DISCOVERY_RESPONSE_TIME_SECS);

  ++s_generation;
  s_is_connected = true;

  // Each looks for its service, in this order: the session with the phone app comes first.
#if defined(CONFIG_PP_TRANSPORT_GATT)
  pbl_pp_gatt_create();
  pbl_pp_gatt_discover(conn_handle);
#endif
  const int rc =
      pbl_bt_service_changed_client_start(&s_service_changed, conn_handle, prv_services_changed);
  if (rc != 0) {
    PBL_LOG_ERR("Could not look for Service Changed (%d)", rc);
  }
#if defined(CONFIG_BT_ANCS_CLIENT)
  ancs_create(conn_handle);
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
  ams_create(conn_handle);
#endif
}

static void prv_disconnected_kernel_main_cb(void *data) {
  PBL_LOG_DBG("Disconnected from Gateway!");

  s_is_connected = false;
  pbl_bt_service_changed_client_stop(&s_service_changed);
#if defined(CONFIG_PP_TRANSPORT_GATT)
  pbl_pp_gatt_destroy();
#endif
#if defined(CONFIG_BT_AMS_CLIENT)
  ams_destroy();
#endif
#if defined(CONFIG_BT_ANCS_CLIENT)
  ancs_destroy();
#endif
}

void bt_gateway_connected(uint16_t conn_handle) {
  launcher_task_add_callback(prv_connected_kernel_main_cb, (void *)(uintptr_t)conn_handle);
}

void bt_gateway_disconnected(void) {
  launcher_task_add_callback(prv_disconnected_kernel_main_cb, nullptr);
}

// -------------------------------------------------------------------------------------------------
void bt_gateway_init(void) {
#if defined(CONFIG_PP_TRANSPORT_GATT)
  const struct pbl_pp_gatt_config config = {
    .serial_number = mfg_get_serial_number(),
    .set_responsiveness = pbl_bt_responsiveness_request,
  };

  pbl_pp_gatt_configure(&config);
  pbl_pp_gatt_reset_disconnect_counter();
#endif

  bt_advertising_set_reconnectable(pbl_bt_bond_count() > 0);
}

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_rediscover(const struct pbl_shell *sh, size_t argc, char **argv) {
  bt_gateway_rediscover();
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, rediscover, nullptr, "Look for the services of the phone again",
                     prv_cmd_rediscover, 0, 0);
#endif
