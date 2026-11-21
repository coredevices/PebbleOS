/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/rtc.h>

/**
 * @defgroup drivers_rtc_stm32 STM32 RTC
 * @ingroup drivers_rtc
 * @brief @ref drivers_rtc driver for the calendar RTC of the STM32H7, in the backup domain: it
 * keeps counting across resets and in Standby.
 * @{
 */

/** @brief STM32 RTC. */
struct pbl_rtc_stm32 {
  /** RTC. */
  struct pbl_rtc rtc;
  /** Registers. */
  uintptr_t base;
  /** Bus clock, from PBL_CLK(). */
  struct pbl_clk pclk;
  /** Kernel clock, from PBL_CLK(). */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rtc_ops pbl_rtc_stm32_ops;
int pbl_rtc_stm32_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the RTC.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 * @param _pclk Bus clock, from PBL_CLK().
 * @param _clk Kernel clock, from PBL_CLK().
 */
#define PBL_RTC_STM32_DEFINE(sym, _name, _base, _pclk, _clk)                          \
  PBL_DEVICE_STATE_DEFINE(sym);                                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rtc_stm32, struct pbl_rtc, rtc) {               \
    .rtc = PBL_RTC_INIT(sym, _name, pbl_rtc_stm32_init, nullptr, &pbl_rtc_stm32_ops), \
    .base = (_base), .pclk = _pclk, .clk = _clk,                                      \
  }

/** @} */
