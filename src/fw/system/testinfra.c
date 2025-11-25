/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "testinfra.h"

#include "console/pulse_internal.h"
#include "kernel/core_dump.h"
#include "services/common/new_timer/new_timer.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/passert.h"

void notify_system_ready_for_communication(void) {
#if !UNITTEST
  pbl_log(LOG_LEVEL_DEBUG, __FILE_NAME__, __LINE__, "Ready for communication.");
#if PULSE_EVERYWHERE
  static bool s_pulse_started = false;
  if (!s_pulse_started) {
    pulse_start();
    s_pulse_started = true;
  }
#endif
#endif
}

#if IS_BIGBOARD
NORETURN test_infra_quarantine_board(const char *quarantine_reason) {
  PBL_LOG(LOG_LEVEL_INFO, "Quarantine Board: %s", quarantine_reason);
  boot_bit_set(BOOT_BIT_FORCE_PRF);
  core_dump_reset(true /* is_forced */);
}
#endif /* IS_BIGBOARD */
