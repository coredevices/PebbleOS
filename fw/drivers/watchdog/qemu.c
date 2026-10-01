/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/watchdog.h>

#include "board/board.h"

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// ARM CMSDK APB watchdog, clocked from the 1 MHz reference clock. The first
// expiry raises the timeout line, the second resets the machine, so the load
// value is half the timeout.
#define WDOG_LOAD    (QEMU_WDOG_BASE + 0x000)
#define WDOG_CONTROL (QEMU_WDOG_BASE + 0x008)
#define WDOG_INTCLR  (QEMU_WDOG_BASE + 0x00C)
#define WDOG_LOCK    (QEMU_WDOG_BASE + 0xC00)

#define WDOG_CONTROL_INTEN (1 << 0)
#define WDOG_CONTROL_RESEN (1 << 1)
#define WDOG_UNLOCK        0x1ACCE551

#define WDOG_CLK_HZ    1000000
#define WDOG_TIMEOUT_S 8

// Reset reason latch in the system controller (write 1 to clear).
#define SYSCTRL_RESET_REASON (QEMU_SYSCTRL_BASE + 0x14)

#define RESET_REASON_POR  (1 << 0)
#define RESET_REASON_SOFT (1 << 1)
#define RESET_REASON_WDOG (1 << 2)

static McuRebootReason s_cached_reset_flag;

void watchdog_init(void) {
  REG32(WDOG_LOCK) = WDOG_UNLOCK;
  REG32(WDOG_LOAD) = (WDOG_CLK_HZ * WDOG_TIMEOUT_S) / 2;
}

void watchdog_start(void) {
  REG32(WDOG_CONTROL) = WDOG_CONTROL_INTEN | WDOG_CONTROL_RESEN;
}

void watchdog_stop(void) {
  REG32(WDOG_CONTROL) = 0;
}

void watchdog_feed(void) {
  REG32(WDOG_INTCLR) = 1;
}

bool watchdog_check_reset_flag(void) {
  return (REG32(SYSCTRL_RESET_REASON) & RESET_REASON_WDOG) != 0;
}

McuRebootReason watchdog_clear_reset_flag(void) {
  uint32_t reason = REG32(SYSCTRL_RESET_REASON);
  REG32(SYSCTRL_RESET_REASON) = reason;

  s_cached_reset_flag = (McuRebootReason){
    .power_on_reset = (reason & RESET_REASON_POR) != 0,
    .software_reset = (reason & RESET_REASON_SOFT) != 0,
    .independent_watchdog_reset = (reason & RESET_REASON_WDOG) != 0,
  };

  return s_cached_reset_flag;
}

McuRebootReason watchdog_get_reset_flag(void) {
  return s_cached_reset_flag;
}
