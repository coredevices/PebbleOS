/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/logging/logging.h>
#include <pbl/services/runlevel.h>
#include <pbl/services/services.h>
#include <pbl/services/services_common.h>
#include <pbl/services/services_normal.h>

#include <system/passert.h>

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
