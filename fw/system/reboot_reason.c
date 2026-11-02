/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "reboot_reason.h"

#include <stddef.h>

#include <pbl/drivers/retained.h>
#include <pbl/kernel/sched.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/interrupts.h>
#include <pbl/util/size.h>

#ifdef CONFIG_SOC_POSIX
// The host has no interrupt priority masking.
#define prv_get_basepri() (0U)
#else
#define prv_get_basepri() __get_BASEPRI()
#endif

static_assert(sizeof(RebootReason) == sizeof(uint32_t[4]), "RebootReason is a funny size");

static const enum pbl_retained_reg s_regs[] = {
  PBL_RETAINED_REBOOT_REASON_0,
  PBL_RETAINED_REBOOT_REASON_1,
  PBL_RETAINED_REBOOT_REASON_2,
  PBL_RETAINED_REBOOT_REASON_3,
};

void reboot_reason_set(RebootReason *reason) {
  uint32_t *raw = (uint32_t *)reason;

  if (pbl_retained_read(PBL_RETAINED_REBOOT_REASON_0)) {
    // It's not safe to log if we're called from an ISR or from a FreeRTOS critical section (basepri
    // != 0)
    if (!mcu_state_is_isr() && prv_get_basepri() == 0 && pbl_kernel_is_running()) {
      PBL_LOG_WRN("Reboot reason is already set");
    }
    return;
  }

  for (size_t i = 0; i < ARRAY_LENGTH(s_regs); i++) {
    pbl_retained_write(s_regs[i], raw[i]);
  }
}

void reboot_reason_set_restarted_safely(void) {
  RebootReason reason;
  reboot_reason_get(&reason);
  reason.restarted_safely = true;

  uint32_t *raw = (uint32_t *)&reason;
  pbl_retained_write(PBL_RETAINED_REBOOT_REASON_0, *raw);
}

void reboot_reason_get(RebootReason *reason) {
  uint32_t *raw = (uint32_t *)reason;

  for (size_t i = 0; i < ARRAY_LENGTH(s_regs); i++) {
    raw[i] = pbl_retained_read(s_regs[i]);
  }
}

void reboot_reason_clear(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_regs); i++) {
    pbl_retained_write(s_regs[i], 0);
  }
}

uint32_t reboot_get_slot_of_last_launched_app(void) {
  return pbl_retained_read(PBL_RETAINED_LAST_APP_SLOT);
}

void reboot_set_slot_of_last_launched_app(uint32_t app_slot) {
  pbl_retained_write(PBL_RETAINED_LAST_APP_SLOT, app_slot);
}
