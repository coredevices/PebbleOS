/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "debug.h"

#include "debug_reboot_reason.h"

#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/services/process_management/app_storage.h>
#include <pbl/util/build_id.h>

#include <kernel/pebble_tasks.h>
#include <mfg/mfg_serials.h>
#include <process_management/app_manager.h>
#include <system/reboot_reason.h>
#include <system/version.h>

void debug_init(McuRebootReason mcu_reboot_reason) {
  PBL_LOG_ALWAYS("Platform: %u, hw: %s, sn: %s", TINTIN_METADATA.hw_platform, mfg_get_hw_version(),
                 mfg_get_serial_number());

  // Log the firmware build id to flash:
  char build_id_string[64];
  version_copy_current_build_id_hex_string(build_id_string, 64);
  PBL_LOG_ALWAYS("BUILD ID: %s", build_id_string);

  debug_reboot_reason_print(mcu_reboot_reason);
}

void debug_print_last_launched_app(void) {
  // Get the slot of the last launched app
  // so we know what was running when we rebooted
  uint32_t last_launched_app_slot = reboot_get_slot_of_last_launched_app();

  // check if last app launched was a system app
  if (last_launched_app_slot == (uint32_t)SYSTEM_APP_BANK_ID) {
    PBL_LOG_INFO("Last launched app: <System_App>");
  } else if ((last_launched_app_slot != (uint32_t)INVALID_BANK_ID)) {
    PebbleProcessInfo last_launched_app;
    uint8_t build_id[BUILD_ID_EXPECTED_LEN];
    AppStorageGetAppInfoResult result = app_storage_get_process_info(
        &last_launched_app, build_id, (AppInstallId)last_launched_app_slot, PebbleTask_App);

    if (result == GET_APP_INFO_SUCCESS) {
      PBL_LOG_INFO("Last launched app: %s", last_launched_app.name);
      PBL_LOG_HEXDUMP_INFO(build_id, sizeof(build_id));
    }
  }
}
