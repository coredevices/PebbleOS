/*
 * Copyright 2024 Google LLC
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

//! @file services.c
//!
//! This file should control the initialization of the various services in the right order.
//! I'll slowly move initialization routines into here as we continue to refactor services.
//! For now this will just be woefully incomplete.

#include "services_common.h"

#include "mfg/mfg_info.h"

#ifdef MICRO_FAMILY_STM32F2
#include "services/common/legacy/factory_registry.h"
#endif
#include "drivers/accel.h"
#include "services/common/accel_manager.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/common/comm_session/app_session_capabilities.h"
#include "services/common/comm_session/default_kernel_sender.h"
#include "services/common/comm_session/session.h"
#include "services/common/cron.h"
#include "services/common/firmware_update.h"
#include "services/common/hrm/hrm_manager.h"
#include "services/common/light.h"
#include "services/common/poll_remote.h"
#include "services/common/put_bytes/put_bytes.h"
#include "services/common/shared_prf_storage/shared_prf_storage.h"
#include "services/common/touch/touch.h"
#include "services/common/vibe_pattern.h"
#include "services/runlevel_impl.h"
#include "system/logging.h"
#include "util/size.h"

#define LOG_DOMAIN "services_common"

void services_common_init(void) {
  firmware_update_init();
  put_bytes_init();
  poll_remote_init();

  // Only initialize the accelerometer manager if the accelerometer driver
  // successfully initialized. We test this by attempting to peek at a sample.
  AccelDriverSample probe_sample;
  if (accel_peek(&probe_sample) == 0) {
    accel_manager_init();
  } else {
    PBL_LOG(LOG_LEVEL_WARNING, "Accelerometer driver not initialized, skipping accel_manager_init");
  }

  light_init();

  cron_service_init();

  shared_prf_storage_init();
  bt_persistent_storage_init();

  comm_default_kernel_sender_init();
  comm_session_app_session_capabilities_init();
  comm_session_init();

  bt_ctl_init();

#if CAPABILITY_HAS_TOUCHSCREEN
  touch_init();
#endif

#if CAPABILITY_HAS_BUILTIN_HRM
  hrm_manager_init();
#endif

  // We only use the factory registry on tintins and biancas
#ifdef MICRO_FAMILY_STM32F2
  factory_registry_init();
#endif
}

static struct ServiceRunLevelSetting s_runlevel_settings[] = {
  {
    .set_enable_fn = accel_manager_enable,
    .enable_mask = R_Stationary | R_FirmwareUpdate | R_Normal,
  },
  {
    .set_enable_fn = light_allow,
    .enable_mask = R_LowPower | R_FirmwareUpdate | R_Normal,
  },
  {
    .set_enable_fn = vibe_service_set_enabled,
    .enable_mask = R_LowPower | R_FirmwareUpdate | R_Normal
  },
  {
    .set_enable_fn = bt_ctl_set_enabled,
    .enable_mask = R_FirmwareUpdate | R_Normal,
  },
#if CAPABILITY_HAS_BUILTIN_HRM
  {
    .set_enable_fn = hrm_manager_enable,
    .enable_mask = R_Normal,
  },
#endif
};

void services_common_set_runlevel(RunLevel runlevel) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_runlevel_settings); ++i) {
    struct ServiceRunLevelSetting *service = &s_runlevel_settings[i];
    service->set_enable_fn(((1 << runlevel) & service->enable_mask) != 0);
  }
}

