/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_rtc_stm32 STM32 RTC
 * @ingroup drivers_rtc
 * @brief @ref drivers_rtc driver for the calendar RTC of the STM32H7, in the backup domain: it
 * keeps counting across resets and in Standby.
 * @{
 */

/** @brief STM32 RTC state. */
struct pbl_rtc_stm32_state {
  /** Alarm callback, or @c nullptr. */
  pbl_rtc_alarm_callback_t alarm_cb;
  /** Data passed to @ref alarm_cb. */
  void *alarm_user_data;
};

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
  /** EXTI registers. */
  uintptr_t exti;
  /** EXTI line of the alarms. */
  uint8_t exti_line;
  /** Alarm interrupt. */
  pbl_irq_t irq;
  /** State. */
  struct pbl_rtc_stm32_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rtc_ops pbl_rtc_stm32_ops;
int pbl_rtc_stm32_init(const struct pbl_device *dev);
void pbl_rtc_stm32_isr(const struct pbl_rtc_stm32 *rtc);
/** @endcond */

/**
 * @brief Define the RTC.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 * @param _pclk Bus clock, from PBL_CLK().
 * @param _clk Kernel clock, from PBL_CLK().
 * @param _exti EXTI registers.
 * @param _exti_line EXTI line of the alarms.
 * @param _irq Alarm interrupt.
 */
#define PBL_RTC_STM32_DEFINE(sym, _name, _base, _pclk, _clk, _exti, _exti_line, _irq) \
  PBL_DEVICE_STATE_DEFINE(sym);                                                       \
  static struct pbl_rtc_stm32_state sym##_rtc_state;                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rtc_stm32, struct pbl_rtc, rtc){                \
    .rtc = PBL_RTC_INIT(sym, _name, pbl_rtc_stm32_init, nullptr, &pbl_rtc_stm32_ops), \
    .base = (_base),                                                                  \
    .pclk = _pclk,                                                                    \
    .clk = _clk,                                                                      \
    .exti = (_exti),                                                                  \
    .exti_line = (_exti_line),                                                        \
    .irq = (_irq),                                                                    \
    .state = &sym##_rtc_state,                                                        \
  };                                                                                  \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_RTC_STM32_IRQ_PRIO, pbl_rtc_stm32_isr, &sym##_inst, 0)

/** @} */
