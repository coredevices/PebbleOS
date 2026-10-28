/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/pmic.h>
#include <pbl/logging/logging.h>

#include <kernel/util/standby.h>
#include <system/bootbits.h>
#include <system/reset.h>

int enter_standby(RebootReasonCode reason) {
#ifdef CONFIG_PMIC
  // Check before tearing anything down, so the caller can keep running
  if (pmic_is_usb_connected()) {
    PBL_LOG_WRN("Not entering standby, USB is connected");
    return -EBUSY;
  }
#endif

  PBL_LOG_ALWAYS("Preparing to enter standby mode (reason %u).", (unsigned)reason);

  RebootReason reboot_reason = {reason, 0};
  reboot_reason_set(&reboot_reason);

  (void)pbl_display_clear(DISPLAY);
  (void)pbl_display_disable(DISPLAY);

  system_reset_prepare();
  reboot_reason_set_restarted_safely();

#ifdef CONFIG_PMIC
  int ret = pmic_power_off();
  // Everything is torn down already, so resetting is the only way out
  PBL_LOG_ERR("Could not power off (%d), resetting", ret);
#else
  boot_bit_set(BOOT_BIT_STANDBY_MODE_REQUESTED);
#endif
  system_hard_reset();
}
