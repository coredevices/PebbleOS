/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/util/standby.h"

#include "drivers/display/display.h"
#include "drivers/pmic.h"
#include "drivers/pwr.h"
#include "drivers/periph_config.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "system/reset.h"
#include "system/passert.h"

#ifdef CONFIG_PMIC
static NORETURN prv_enter_standby(void) {
  pmic_power_off();

  PBL_CROAK("We were not shut down!");
}
#else
static NORETURN prv_enter_standby(void) {
  boot_bit_set(BOOT_BIT_STANDBY_MODE_REQUESTED);
  system_hard_reset();
}
#endif

NORETURN enter_standby(RebootReasonCode reason) {
  PBL_LOG_ALWAYS("Preparing to enter standby mode.");

  RebootReason reboot_reason = { reason, 0 };
  reboot_reason_set(&reboot_reason);

  display_clear();
  display_set_enabled(false);

  system_reset_prepare();
  reboot_reason_set_restarted_safely();

  prv_enter_standby();
}
