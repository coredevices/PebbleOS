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

#include <FreeRTOS.h>
#include <bluetooth/init.h>
#include <comm/bt_lock.h>
#include <host/ble_hs.h>
#include <host/ble_hs_stop.h>
#include <host/util/util.h>
#include <kernel/pebble_tasks.h>
#include <nimble/nimble_port.h>
#include <os/tick.h>
#include <semphr.h>
#include <services/dis/ble_svc_dis.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <stdlib.h>
#include <system/logging.h>
#include <system/passert.h>

#include "nimble_store.h"

static const uint32_t s_bt_stack_start_stop_timeout_ms = 2000;

extern void pebble_pairing_service_init(void);

#if NIMBLE_CFG_CONTROLLER
static TaskHandle_t s_ll_task_handle;
#endif
static TaskHandle_t s_host_task_handle;
static SemaphoreHandle_t s_host_started;
static SemaphoreHandle_t s_host_stopped;
static DisInfo s_dis_info;
static struct ble_hs_stop_listener s_listener;

static void prv_sync_cb(void) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "NimBLE host synchronized");
  xSemaphoreGive(s_host_started);
}

static void prv_reset_cb(int reason) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_WARNING, "NimBLE reset (reason: %d)", reason);
}

static void prv_host_task_main(void *unused) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_DEBUG, "NimBLE host task started");

  ble_hs_cfg.sync_cb = prv_sync_cb;
  ble_hs_cfg.reset_cb = prv_reset_cb;

  nimble_port_run();
}

static void prv_ble_hs_stop_cb(int status, void *arg) { xSemaphoreGive(s_host_stopped); }

// ----------------------------------------------------------------------------------------
void bt_driver_init(void) {
  bt_lock_init();

  s_host_started = xSemaphoreCreateBinary();
  s_host_stopped = xSemaphoreCreateBinary();

  nimble_port_init();
  nimble_store_init();

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
      .usStackDepth = (configMINIMAL_STACK_SIZE + 600) / sizeof(StackType_t),
      .uxPriority = (configMAX_PRIORITIES - 1) | portPRIVILEGE_BIT,
      .puxStackBuffer = NULL,
  };

  pebble_task_create(PebbleTask_BTController, &ll_task_params, &s_ll_task_handle);
  PBL_ASSERTN(s_ll_task_handle);
#endif
}

bool bt_driver_start(BTDriverConfig *config) {
  int rc;
  BaseType_t f_rc;

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
  f_rc = xSemaphoreTake(s_host_started, milliseconds_to_ticks(s_bt_stack_start_stop_timeout_ms));
  if (f_rc != pdTRUE) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Host synchronization timed out");
    return false;
  }

  rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Failed to ensure address: %d", rc);
    return false;
  }

  return true;
}

void bt_driver_stop(void) {
  BaseType_t f_rc;

  ble_hs_stop(&s_listener, prv_ble_hs_stop_cb, NULL);
  f_rc = xSemaphoreTake(s_host_stopped, milliseconds_to_ticks(s_bt_stack_start_stop_timeout_ms));
  PBL_ASSERT(f_rc == pdTRUE, "NimBLE host stop timed out");

  ble_gatts_reset();

  nimble_store_unload();
}

void bt_driver_power_down_controller_on_boot(void) {}
