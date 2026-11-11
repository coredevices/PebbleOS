/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_pwm_sf32lb_lptim SF32LB LPTIM PWM
 * @ingroup drivers_pwm
 * @brief @ref drivers_pwm controller driver for the SF32LB low-power timers.
 *
 * One channel, counting clk_rtc, so it keeps running in deep sleep. The period and duty cycle
 * resolution is one clk_rtc cycle, about 100 us on the LRC10; the frequency is the
 * calibrated one when the output is applied. The pin state can route the inverted output too.
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_lptim2_default, SF32LB52_PINCTRL,
 *                          SF32LB52_PA24_PBR_LPTIM2_OUT | PBL_PINCTRL_SF32LB52_INPUT,
 *                          SF32LB52_PA25_PBR_LPTIM2_INV_OUT | PBL_PINCTRL_SF32LB52_INPUT);
 * PBL_PWM_SF32LB_LPTIM_DEFINE(s_lptim2, "lptim2", 0x500c2000,
 *                             PBL_CLK(SF32LB52_RTC_CLK, SF32LB52_CLK_RTC), &s_lptim2_default);
 * @endcode
 * @{
 */

/** @brief SF32LB LPTIM PWM controller. */
struct pbl_pwm_sf32lb_lptim {
  /** PWM controller. */
  struct pbl_pwm_controller ctrl;
  /** Output pins. */
  struct pbl_pinctrl pinctrl;
  /** Timer registers. */
  uintptr_t base;
  /** Counter clock, the low-power oscillator. */
  struct pbl_clk clk;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pwm_controller_ops pbl_pwm_sf32lb_lptim_ops;
int pbl_pwm_sf32lb_lptim_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an SF32LB LPTIM PWM controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Timer registers.
 * @param _clk Counter clock, from PBL_CLK().
 * @param _pinctrl Pin state with the outputs, from PBL_PINCTRL_STATE_DEFINE().
 */
#define PBL_PWM_SF32LB_LPTIM_DEFINE(sym, _name, _base, _clk, _pinctrl)                     \
  PBL_PWM_CONTROLLER_STATE_DEFINE(sym, 1);                                                 \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pwm_sf32lb_lptim, struct pbl_pwm_controller, ctrl) { \
    .ctrl = PBL_PWM_CONTROLLER_INIT(sym, _name, pbl_pwm_sf32lb_lptim_init, nullptr,        \
                                    &pbl_pwm_sf32lb_lptim_ops),                            \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0), .base = (_base), .clk = _clk,            \
  }

/** @} */
