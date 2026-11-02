/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/assert.h>
#include <pbl/kernel/reboot.h>
#include <pbl/logging/logging.h>
#include <pbl/services/runlevel.h>
#include <pbl/services/services.h>
#include <pbl/services/services_common.h>
#include <pbl/services/services_normal.h>

void services_early_init(void) {
#ifndef CONFIG_RECOVERY_FW
  services_normal_early_init();
#endif
}

void services_init(void) {
  services_common_init();

#ifndef CONFIG_RECOVERY_FW
  services_normal_init();
#endif
}

void services_set_runlevel(RunLevel runlevel) {
  PBL_ASSERT(runlevel < RunLevel_COUNT, "Unknown runlevel %d", runlevel);
  PBL_LOG_INFO("Setting runlevel to %d", runlevel);
  services_common_set_runlevel(runlevel);
#ifndef CONFIG_RECOVERY_FW
  services_normal_set_runlevel(runlevel);
#endif
}

static void prv_sys_off_prepare(enum pbl_sys_off_mode mode, void *user_data) {
  // Tear down Bluetooth, to avoid confusing the phone
  services_set_runlevel(RunLevel_BareMinimum);
}

PBL_SYS_OFF_HANDLER_DEFINE(services_restart, PBL_SYS_OFF_RESTART_PREPARE, PBL_SYS_OFF_PRIO_HIGH,
                           prv_sys_off_prepare, nullptr);
PBL_SYS_OFF_HANDLER_DEFINE(services_power_off, PBL_SYS_OFF_POWER_OFF_PREPARE, PBL_SYS_OFF_PRIO_HIGH,
                           prv_sys_off_prepare, nullptr);
