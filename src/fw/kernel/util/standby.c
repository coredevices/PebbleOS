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

#ifdef TARGET_QEMU
#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#include <mcu.h>
#endif

#if CAPABILITY_HAS_PMIC
static NORETURN prv_enter_standby(void) {
#ifdef TARGET_QEMU
  periph_config_enable(PWR, RCC_APB1Periph_PWR);
  pwr_enable_wakeup(true);
  PWR_EnterSTANDBYMode();
#else
  pmic_power_off();
#endif

  PBL_CROAK("We were not shut down!");
}
#else
static NORETURN prv_enter_standby(void) {
  boot_bit_set(BOOT_BIT_STANDBY_MODE_REQUESTED);
  system_hard_reset();
}
#endif

NORETURN enter_standby(RebootReasonCode reason) {
  PBL_LOG(LOG_LEVEL_ALWAYS, "Preparing to enter standby mode.");

  RebootReason reboot_reason = { reason, 0 };
  reboot_reason_set(&reboot_reason);

  display_clear();
  display_set_enabled(false);

  system_reset_prepare(false);
  reboot_reason_set_restarted_safely();

  prv_enter_standby();
}
