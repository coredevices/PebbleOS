/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rtc.h>
#include <pbl/drivers/rtc/qemu.h>
#include <pbl/kernel/mmio.h>

#include <board/board.h>

#define RTC_TIME_LO_OFFSET 0x00U
#define RTC_CTRL_OFFSET    0x0CU
#define RTC_BACKUP_OFFSET  0x40U

#define RTC_CTRL_ALARM_IE (1U << 0)

#define RTC_BACKUP_REGS 16U

void RTC_WriteBackupRegister(uint32_t reg_id, uint32_t value) {
  if (reg_id < RTC_BACKUP_REGS) {
    pbl_writel(value, QEMU_RTC_BASE + RTC_BACKUP_OFFSET + reg_id * 4U);
  }
}

uint32_t RTC_ReadBackupRegister(uint32_t reg_id) {
  if (reg_id < RTC_BACKUP_REGS) {
    return pbl_readl(QEMU_RTC_BASE + RTC_BACKUP_OFFSET + reg_id * 4U);
  }
  return 0U;
}

static int prv_get_time(const struct pbl_rtc *rtc, time_t *t) {
  *t = (time_t)pbl_readl(QEMU_RTC_BASE + RTC_TIME_LO_OFFSET);

  return 0;
}

static int prv_set_time(const struct pbl_rtc *rtc, time_t t) {
  pbl_writel((uint32_t)t, QEMU_RTC_BASE + RTC_TIME_LO_OFFSET);

  return 0;
}

static int prv_init(const struct pbl_device *dev) {
  // Clear a pending alarm, then disable it.
  pbl_writel(RTC_CTRL_ALARM_IE, QEMU_RTC_BASE + RTC_CTRL_OFFSET);
  pbl_writel(0U, QEMU_RTC_BASE + RTC_CTRL_OFFSET);

  return 0;
}

static const struct pbl_rtc_ops s_ops = {
  .get_time = prv_get_time,
  .set_time = prv_set_time,
};

PBL_DEVICE_STATE_DEFINE(pbl_rtc_qemu);
const struct pbl_rtc pbl_rtc_qemu = PBL_RTC_INIT(pbl_rtc_qemu, "rtc", prv_init, &s_ops);
PBL_DEVICE_REGISTER(pbl_rtc_qemu, &pbl_rtc_qemu.dev);
