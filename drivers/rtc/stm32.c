/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/rtc/stm32.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/time.h>

#include <hardware/stm32_rtc.h>

// The asynchronous prescaler at its largest, for the least power
#define PREDIV_A 127U

#define INIT_TIMEOUT_US 100000U

// Clock rates are in mHz
#define MHZ_PER_HZ 1000ULL

static const struct pbl_rtc_stm32 *prv_rtc(const struct pbl_rtc *rtc) {
  return PBL_CONTAINER_OF(rtc, const struct pbl_rtc_stm32, rtc);
}

static inline uint32_t prv_getreg(const struct pbl_rtc_stm32 *rtc, uint32_t offset) {
  return pbl_readl(rtc->base + offset);
}

static inline void prv_putreg(const struct pbl_rtc_stm32 *rtc, uint32_t offset, uint32_t value) {
  pbl_writel(value, rtc->base + offset);
}

static uint8_t prv_bcd(uint32_t tens, uint32_t units) {
  return (uint8_t)(tens * 10U + units);
}

// The registers take writes in the backup domain, which the clock controller unlocked for the
// LSE, once their own protection is lifted
static void prv_unlock(const struct pbl_rtc_stm32 *rtc) {
  prv_putreg(rtc, STM32_RTC_WPR_OFFSET, RTC_WPR_KEY1);
  prv_putreg(rtc, STM32_RTC_WPR_OFFSET, RTC_WPR_KEY2);
}

static void prv_lock(const struct pbl_rtc_stm32 *rtc) {
  prv_putreg(rtc, STM32_RTC_WPR_OFFSET, RTC_WPR_LOCK);
}

static int prv_enter_init(const struct pbl_rtc_stm32 *rtc) {
  int ret;

  prv_unlock(rtc);
  pbl_modifyl(0U, RTC_ISR_INIT, rtc->base + STM32_RTC_ISR_OFFSET);
  ret = PBL_POLL_BUSY_US((prv_getreg(rtc, STM32_RTC_ISR_OFFSET) & RTC_ISR_INITF) != 0U, 10U,
                         INIT_TIMEOUT_US);
  if (ret != 0) {
    prv_lock(rtc);
  }

  return ret;
}

static void prv_exit_init(const struct pbl_rtc_stm32 *rtc) {
  pbl_modifyl(RTC_ISR_INIT, 0U, rtc->base + STM32_RTC_ISR_OFFSET);
  prv_lock(rtc);
}

// The calendar is read straight, bypassing the shadow registers that Stop leaves stale: the time
// again until it did not change while the date was read
static int prv_get_time(const struct pbl_rtc *dev, time_t *t) {
  const struct pbl_rtc_stm32 *rtc = prv_rtc(dev);
  uint32_t tr;
  uint32_t dr;

  do {
    tr = prv_getreg(rtc, STM32_RTC_TR_OFFSET);
    dr = prv_getreg(rtc, STM32_RTC_DR_OFFSET);
  } while (tr != prv_getreg(rtc, STM32_RTC_TR_OFFSET));

  struct pbl_tm tm = {
    .tm_sec = prv_bcd(PBL_FIELD_GET(RTC_TR_ST_MASK, tr), PBL_FIELD_GET(RTC_TR_SU_MASK, tr)),
    .tm_min = prv_bcd(PBL_FIELD_GET(RTC_TR_MNT_MASK, tr), PBL_FIELD_GET(RTC_TR_MNU_MASK, tr)),
    .tm_hour = prv_bcd(PBL_FIELD_GET(RTC_TR_HT_MASK, tr), PBL_FIELD_GET(RTC_TR_HU_MASK, tr)),
    .tm_mday = prv_bcd(PBL_FIELD_GET(RTC_DR_DT_MASK, dr), PBL_FIELD_GET(RTC_DR_DU_MASK, dr)),
    .tm_mon = prv_bcd(PBL_FIELD_GET(RTC_DR_MT, dr), PBL_FIELD_GET(RTC_DR_MU_MASK, dr)) - 1,
    .tm_year = prv_bcd(PBL_FIELD_GET(RTC_DR_YT_MASK, dr), PBL_FIELD_GET(RTC_DR_YU_MASK, dr)) + 100,
  };

  *t = pbl_mktime(&tm);

  return 0;
}

static int prv_set_time(const struct pbl_rtc *dev, time_t t) {
  const struct pbl_rtc_stm32 *rtc = prv_rtc(dev);
  struct pbl_tm tm;
  int ret;

  pbl_gmtime_r(&t, &tm);
  // The calendar holds years 2000 to 2099
  if (tm.tm_year < 100 || tm.tm_year > 199) {
    return -EINVAL;
  }

  ret = prv_enter_init(rtc);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(rtc, STM32_RTC_TR_OFFSET,
             PBL_FIELD_PREP(RTC_TR_ST_MASK, tm.tm_sec / 10) |
                 PBL_FIELD_PREP(RTC_TR_SU_MASK, tm.tm_sec % 10) |
                 PBL_FIELD_PREP(RTC_TR_MNT_MASK, tm.tm_min / 10) |
                 PBL_FIELD_PREP(RTC_TR_MNU_MASK, tm.tm_min % 10) |
                 PBL_FIELD_PREP(RTC_TR_HT_MASK, tm.tm_hour / 10) |
                 PBL_FIELD_PREP(RTC_TR_HU_MASK, tm.tm_hour % 10));
  prv_putreg(rtc, STM32_RTC_DR_OFFSET,
             PBL_FIELD_PREP(RTC_DR_DT_MASK, tm.tm_mday / 10) |
                 PBL_FIELD_PREP(RTC_DR_DU_MASK, tm.tm_mday % 10) |
                 PBL_FIELD_PREP(RTC_DR_MT, (tm.tm_mon + 1) / 10) |
                 PBL_FIELD_PREP(RTC_DR_MU_MASK, (tm.tm_mon + 1) % 10) |
                 PBL_FIELD_PREP(RTC_DR_WDU_MASK, tm.tm_wday == 0 ? 7 : tm.tm_wday) |
                 PBL_FIELD_PREP(RTC_DR_YT_MASK, (tm.tm_year % 100) / 10) |
                 PBL_FIELD_PREP(RTC_DR_YU_MASK, tm.tm_year % 10));
  prv_exit_init(rtc);

  return 0;
}

// The calendar keeps counting across resets: it is only set up when its prescaler does not match
// its clock, as on the first boot, or when a reset left it stopped in initialization mode
int pbl_rtc_stm32_init(const struct pbl_device *dev) {
  const struct pbl_rtc_stm32 *rtc = PBL_CONTAINER_OF(dev, const struct pbl_rtc_stm32, rtc.dev);
  uint64_t rate_mhz;
  uint32_t prer;
  int ret;

  ret = pbl_device_init(&rtc->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&rtc->pclk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&rtc->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_get_rate(&rtc->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  prer = PBL_FIELD_PREP(RTC_PRER_PREDIV_A_MASK, PREDIV_A) |
         PBL_FIELD_PREP(RTC_PRER_PREDIV_S_MASK,
                        (uint32_t)(rate_mhz / MHZ_PER_HZ / (PREDIV_A + 1U)) - 1U);
  if (prv_getreg(rtc, STM32_RTC_PRER_OFFSET) == prer &&
      (prv_getreg(rtc, STM32_RTC_CR_OFFSET) & RTC_CR_BYPSHAD) != 0U &&
      (prv_getreg(rtc, STM32_RTC_ISR_OFFSET) & RTC_ISR_INIT) == 0U) {
    return 0;
  }

  ret = prv_enter_init(rtc);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(rtc, STM32_RTC_PRER_OFFSET, prer);
  pbl_modifyl(RTC_CR_FMT, RTC_CR_BYPSHAD, rtc->base + STM32_RTC_CR_OFFSET);
  prv_exit_init(rtc);

  return 0;
}

const struct pbl_rtc_ops pbl_rtc_stm32_ops = {
  .get_time = prv_get_time,
  .set_time = prv_set_time,
};

#include PBL_DT_INSTANCES(st_stm32h7_rtc)
