/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/rtc.h>

/**
 * @defgroup drivers_rtc_sf32lb SF32LB
 * @ingroup drivers_rtc
 * @brief SiFli SF32LB calendar RTC, counting clk_rtc at its calibrated rate.
 * @{
 */

/** @brief SF32LB calendar RTC. */
struct pbl_rtc_sf32lb {
  /** RTC. */
  struct pbl_rtc rtc;
  /** Registers. */
  uintptr_t base;
  /** HPSYS configuration registers, holding the shadow time and date. */
  uintptr_t cfg;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rtc_ops pbl_rtc_sf32lb_ops;
int pbl_rtc_sf32lb_init(const struct pbl_device *dev);
extern const struct pbl_rtc pbl_rtc_sf32lb;
/** @endcond */

/**
 * @brief Define the RTC.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 * @param _cfg HPSYS configuration registers.
 */
#define PBL_RTC_SF32LB_DEFINE(sym, _name, _base, _cfg)                                          \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                 \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rtc_sf32lb, struct pbl_rtc, rtc) {                        \
    .rtc = PBL_RTC_INIT(sym, _name, pbl_rtc_sf32lb_init, &pbl_rtc_sf32lb_ops), .base = (_base), \
    .cfg = (_cfg),                                                                              \
  }

/** @brief The SF32LB RTC. */
#define SF32LB_RTC (&pbl_rtc_sf32lb)

/** @} */
