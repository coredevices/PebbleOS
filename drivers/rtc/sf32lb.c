/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/rtc.h>
#include <pbl/drivers/rtc/sf32lb.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/services/time.h>
#include <pbl/util/bits.h>

#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_rtc.h>

// The 1 Hz calendar clock is CLK_RTC / (DIVA_INT + DIVA_FRAC / 2^14) / DIVB, CLK_RTC being the
// RC10K.
#define DIVB           256U
#define DIVA_FRAC_BITS 14U

#define TIMEOUT_US 1000000U

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(SF32LB52_RTC_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, SF32LB52_RTC_BASE + offset);
}

static inline void prv_modreg(uint32_t offset, uint32_t clear, uint32_t set) {
  pbl_modifyl(clear, set, SF32LB52_RTC_BASE + offset);
}

static void prv_rate_changed(struct pbl_clk_notifier *nb, uint64_t rate_mhz);

static struct pbl_clk_notifier s_rc10k_nb = {
  .clk = PBL_CLK(SF32LB_RC10K, SF32LB52_CLK_RC10K),
  .fn = prv_rate_changed,
};

static uint8_t prv_bcd2bin(uint32_t tens, uint32_t units) {
  return (uint8_t)(tens * 10U + units);
}

static int prv_enter_init(void) {
  prv_modreg(SF32LB52_RTC_ISR_OFFSET, 0U, RTC_ISR_INIT);

  return PBL_POLL_BUSY_US((prv_getreg(SF32LB52_RTC_ISR_OFFSET) & RTC_ISR_INITF) != 0U, 0U,
                          TIMEOUT_US);
}

// The prescaler can be written while the calendar runs.
static void prv_set_prescaler(uint64_t rate_mhz) {
  uint64_t diva = (rate_mhz << DIVA_FRAC_BITS) / (1000U * DIVB);

  prv_putreg(SF32LB52_RTC_PSCLR_OFFSET,
             PBL_FIELD_PREP(RTC_PSCLR_DIVA_INT_MASK, diva >> DIVA_FRAC_BITS) |
                 PBL_FIELD_PREP(RTC_PSCLR_DIVA_FRAC_MASK, diva) |
                 PBL_FIELD_PREP(RTC_PSCLR_DIVB_MASK, DIVB));
}

static void prv_rate_changed(struct pbl_clk_notifier *nb, uint64_t rate_mhz) {
  prv_set_prescaler(rate_mhz);
}

// Leaves initialization mode and waits for the calendar shadow registers to catch up.
static int prv_exit_init(void) {
  prv_modreg(SF32LB52_RTC_ISR_OFFSET, RTC_ISR_INIT, 0U);
  prv_modreg(SF32LB52_RTC_ISR_OFFSET, RTC_ISR_RSF | RTC_ISR_TSOVF, 0U);

  return PBL_POLL_BUSY_US((prv_getreg(SF32LB52_RTC_ISR_OFFSET) & RTC_ISR_RSF) != 0U, 0U,
                          TIMEOUT_US);
}

static int prv_get_time(const struct pbl_rtc *rtc, time_t *t) {
  uint32_t tr;
  uint32_t dr;
  int ret;

  // The shadow registers can lag the calendar by over a second, RSF set or not: clear RSF and
  // wait for the next update. RSF stays clear in initialization mode; the registers are read as
  // they are then.
  prv_modreg(SF32LB52_RTC_ISR_OFFSET, RTC_ISR_RSF, 0U);
  ret = PBL_POLL_BUSY_US(
      (prv_getreg(SF32LB52_RTC_ISR_OFFSET) & (RTC_ISR_RSF | RTC_ISR_INITF)) != 0U, 0U, TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  // The date reads with ERR set when it rolled over since the time was read.
  do {
    tr = pbl_readl(SF32LB52_HPSYS_CFG_BASE + SF32LB52_HPSYS_CFG_RTC_TR_OFFSET);
    dr = pbl_readl(SF32LB52_HPSYS_CFG_BASE + SF32LB52_HPSYS_CFG_RTC_DR_OFFSET);
  } while ((dr & RTC_DR_ERR) != 0U);

  struct pbl_tm tm = {
    .tm_sec = prv_bcd2bin(PBL_FIELD_GET(RTC_TR_ST_MASK, tr), PBL_FIELD_GET(RTC_TR_SU_MASK, tr)),
    .tm_min = prv_bcd2bin(PBL_FIELD_GET(RTC_TR_MNT_MASK, tr), PBL_FIELD_GET(RTC_TR_MNU_MASK, tr)),
    .tm_hour = prv_bcd2bin(PBL_FIELD_GET(RTC_TR_HT_MASK, tr), PBL_FIELD_GET(RTC_TR_HU_MASK, tr)),
    .tm_mday = prv_bcd2bin(PBL_FIELD_GET(RTC_DR_DT_MASK, dr), PBL_FIELD_GET(RTC_DR_DU_MASK, dr)),
    .tm_mon = prv_bcd2bin(PBL_FIELD_GET(RTC_DR_MT, dr), PBL_FIELD_GET(RTC_DR_MU_MASK, dr)) - 1,
    .tm_year =
        prv_bcd2bin(PBL_FIELD_GET(RTC_DR_YT_MASK, dr), PBL_FIELD_GET(RTC_DR_YU_MASK, dr)) + 100,
  };

  *t = pbl_mktime(&tm);

  return 0;
}

static int prv_set_time(const struct pbl_rtc *rtc, time_t t) {
  struct pbl_tm tm;
  uint32_t tr;
  uint32_t dr;
  int ret;

  pbl_gmtime_r(&t, &tm);
  // The calendar holds years 2000 to 2099.
  if (tm.tm_year < 100 || tm.tm_year > 199) {
    return -EINVAL;
  }

  tr = PBL_FIELD_PREP(RTC_TR_ST_MASK, tm.tm_sec / 10) |
       PBL_FIELD_PREP(RTC_TR_SU_MASK, tm.tm_sec % 10) |
       PBL_FIELD_PREP(RTC_TR_MNT_MASK, tm.tm_min / 10) |
       PBL_FIELD_PREP(RTC_TR_MNU_MASK, tm.tm_min % 10) |
       PBL_FIELD_PREP(RTC_TR_HT_MASK, tm.tm_hour / 10) |
       PBL_FIELD_PREP(RTC_TR_HU_MASK, tm.tm_hour % 10);
  dr = PBL_FIELD_PREP(RTC_DR_DT_MASK, tm.tm_mday / 10) |
       PBL_FIELD_PREP(RTC_DR_DU_MASK, tm.tm_mday % 10) |
       PBL_FIELD_PREP(RTC_DR_MT, (tm.tm_mon + 1) / 10) |
       PBL_FIELD_PREP(RTC_DR_MU_MASK, (tm.tm_mon + 1) % 10) |
       PBL_FIELD_PREP(RTC_DR_WD_MASK, (tm.tm_wday == 0) ? 7 : tm.tm_wday) |
       PBL_FIELD_PREP(RTC_DR_YT_MASK, (tm.tm_year % 100) / 10) |
       PBL_FIELD_PREP(RTC_DR_YU_MASK, tm.tm_year % 10) | RTC_DR_CB;

  ret = prv_enter_init();
  if (ret != 0) {
    return ret;
  }

  prv_putreg(SF32LB52_RTC_TR_OFFSET, tr);
  prv_putreg(SF32LB52_RTC_DR_OFFSET, dr);

  return prv_exit_init();
}

static int prv_init(const struct pbl_device *dev) {
  uint64_t rate_mhz;
  int ret;

  ret = pbl_device_init(&SF32LB_RC10K->dev);
  if (ret != 0) {
    return ret;
  }

  ret = prv_enter_init();
  if (ret != 0) {
    return ret;
  }

  // 24-hour format, calibration output off.
  prv_modreg(SF32LB52_RTC_CR_OFFSET, RTC_CR_FMT | RTC_CR_OSEL_MASK | RTC_CR_POL, 0U);
  prv_modreg(SF32LB52_RTC_ISR_OFFSET, RTC_ISR_INIT, 0U);

  ret = pbl_clk_get_rate(&s_rc10k_nb.clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }
  prv_set_prescaler(rate_mhz);
  pbl_clk_notifier_register(&s_rc10k_nb);

  return prv_exit_init();
}

static const struct pbl_rtc_ops s_ops = {
  .get_time = prv_get_time,
  .set_time = prv_set_time,
};

PBL_DEVICE_STATE_DEFINE(pbl_rtc_sf32lb);
const struct pbl_rtc pbl_rtc_sf32lb = PBL_RTC_INIT(pbl_rtc_sf32lb, "rtc", prv_init, &s_ops);
PBL_DEVICE_REGISTER(pbl_rtc_sf32lb, &pbl_rtc_sf32lb.dev);
