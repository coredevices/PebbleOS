/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/timer.h>
#include <pbl/kernel/mmio.h>

#include <hardware/qemu_memorymap.h>

#define RTC_TICKS_OFFSET 0x10U
#define RTC_TICKS_HZ     1000U

static uint32_t prv_read(const struct pbl_clocksource *cs) {
  return pbl_readl(QEMU_RTC_BASE + RTC_TICKS_OFFSET);
}

static const struct pbl_clocksource_ops s_ops = {
  .read = prv_read,
};

static int prv_init(const struct pbl_device *dev);

PBL_DEVICE_STATE_DEFINE(s_rtc_counter);
static const struct pbl_clocksource s_rtc_counter =
    PBL_CLOCKSOURCE_INIT(s_rtc_counter, "rtc-counter", prv_init, &s_ops, 32);

const struct pbl_clocksource *const pbl_system_clocksource = &s_rtc_counter;

static int prv_init(const struct pbl_device *dev) {
  pbl_clocksource_set_hz(&s_rtc_counter, RTC_TICKS_HZ);

  return 0;
}
