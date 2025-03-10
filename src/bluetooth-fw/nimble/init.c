/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <bluetooth/init.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "comm/bt_lock.h"
#include "host/ble_hs.h"
#include "host/ble_hs_stop.h"
#include "host/util/util.h"
#include "kernel/event_loop.h"
#include "nimble/nimble_port.h"
#include "os/tick.h"
#include "pebble_errors.h"
#include "semphr.h"
#include "services/ans/ble_svc_ans.h"
#include "services/dis/ble_svc_dis.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "system/logging.h"
#include "system/passert.h"

static const uint32_t s_bt_stack_start_stop_timeout_ms = 2000;

extern void pebble_pairing_service_init(void);

void ble_store_ram_init(void);

#if NIMBLE_CFG_CONTROLLER
static TaskHandle_t s_ll_task_handle;
#endif
static TaskHandle_t s_host_task_handle;
static SemaphoreHandle_t s_host_started;
static SemaphoreHandle_t s_host_stopped;
static DisInfo s_dis_info;

static void sync_cb(void) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "BT sync_cb");
  xSemaphoreGive(s_host_started);
}

static void reset_cb(int reason) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "BT reset_cb, reason: %d", reason);
}

static void prv_host_task_main(void *unused) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "BT host task started");

  ble_hs_cfg.sync_cb = sync_cb;
  ble_hs_cfg.reset_cb = reset_cb;
  ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_sc = 1;

  nimble_port_run();
}

// ----------------------------------------------------------------------------------------
void bt_driver_init(void) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "bt_driver_init");
  bt_lock_init();

  s_host_started = xSemaphoreCreateBinary();
  s_host_stopped = xSemaphoreCreateBinary();

  nimble_port_init();
  ble_store_ram_init();

  TaskParameters_t host_task_params = {
      .pvTaskCode = prv_host_task_main,
      .pcName = "NimbleHost",
      .usStackDepth = 4000 / sizeof(StackType_t),  // TODO: probably reduce this
      .uxPriority = (configMAX_PRIORITIES - 2) | portPRIVILEGE_BIT,
      .puxStackBuffer = NULL,
  };

  pebble_task_create(PebbleTask_BTHost, &host_task_params, &s_host_task_handle);
  PBL_ASSERTN(s_host_task_handle);

#if NIMBLE_CFG_CONTROLLER
  TaskParameters_t ll_task_params = {
    .pvTaskCode = nimble_port_ll_task_func,
    .pcName = "NimbleLL",
    .usStackDepth = (configMINIMAL_STACK_SIZE + 400) / sizeof(StackType_t),
    .uxPriority = (configMAX_PRIORITIES - 1) | portPRIVILEGE_BIT,
    .puxStackBuffer = NULL,
  };

  pebble_task_create(PebbleTask_BTController, &ll_task_params, &s_ll_task_handle);
  PBL_ASSERTN(s_ll_task_handle);
#endif
}

bool bt_driver_start(BTDriverConfig *config) {
  int rc;

  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "bt_driver_start");

  s_dis_info = config->dis_info;
  ble_svc_dis_model_number_set(s_dis_info.model_number);
  ble_svc_dis_serial_number_set(s_dis_info.serial_number);
  ble_svc_dis_firmware_revision_set(s_dis_info.fw_revision);
  ble_svc_dis_software_revision_set(s_dis_info.sw_revision);
  ble_svc_dis_manufacturer_name_set(s_dis_info.manufacturer);

  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_svc_dis_init();
  pebble_pairing_service_init();

  ble_hs_sched_start();
  bool started = xSemaphoreTake(s_host_started,
                                milliseconds_to_ticks(s_bt_stack_start_stop_timeout_ms)) == pdTRUE;
  if (!started) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "bt_driver_start timeout");
    return false;
  }

  rc = ble_hs_util_ensure_addr(0);
  PBL_ASSERTN(rc == 0);

  return true;
}

static void prv_ble_hs_stop_cb(int status, void *arg) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, "stop_cb, status: %d", status);
  xSemaphoreGive(s_host_stopped);
}

void bt_driver_stop(void) {
  static struct ble_hs_stop_listener listener;
  ble_hs_stop(&listener, prv_ble_hs_stop_cb, NULL);
  if (xSemaphoreTake(s_host_stopped, milliseconds_to_ticks(s_bt_stack_start_stop_timeout_ms)) ==
      pdFALSE) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "bt_driver_stop timeout");
  }
  ble_gatts_reset();
}

void bt_driver_power_down_controller_on_boot(void) {
  // no-op
}
