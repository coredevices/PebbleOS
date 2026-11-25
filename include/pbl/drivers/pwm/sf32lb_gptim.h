/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_pwm_sf32lb_gptim SF32LB GPTIM PWM
 * @ingroup drivers_pwm
 * @brief @ref drivers_pwm controller driver for the SF32LB general-purpose timers.
 *
 * Four channels counting PCLK, sharing the period: a channel cannot change it while another
 * one runs. Duty cycle changes take effect at the end of the running period, a period change
 * restarts it. A disabled channel drives its inactive level. PCLK stops in deep sleep, so the
 * system stays out of it while a channel runs.
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_gptim1_default, SF32LB57_PINCTRL, SF32LB57_PA30_GPTIM1_CH1);
 * PBL_PWM_SF32LB_GPTIM_DEFINE(s_gptim1, "gptim1", 0x50090000,
 *                             PBL_CLK(&dt_hpsys_rcc, SF32LB57_HPSYS_CLK_GPTIM1),
 *                             &s_gptim1_default);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_pwm_sf32lb_gptim_state {
  uint32_t psc;
  uint32_t arr;
  uint8_t running;
};
/** @endcond */

/** @brief SF32LB GPTIM PWM controller. */
struct pbl_pwm_sf32lb_gptim {
  /** PWM controller. */
  struct pbl_pwm_controller ctrl;
  /** Output pins. */
  struct pbl_pinctrl pinctrl;
  /** Timer registers. */
  uintptr_t base;
  /** Counter clock, PCLK. */
  struct pbl_clk clk;
  /** Runtime state. */
  struct pbl_pwm_sf32lb_gptim_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pwm_controller_ops pbl_pwm_sf32lb_gptim_ops;
int pbl_pwm_sf32lb_gptim_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an SF32LB GPTIM PWM controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _base Timer registers.
 * @param _clk Counter clock, from PBL_CLK().
 * @param _pinctrl Pin state with the outputs, from PBL_PINCTRL_STATE_DEFINE().
 */
#define PBL_PWM_SF32LB_GPTIM_DEFINE(sym, _name, _base, _clk, _pinctrl)                     \
  PBL_PWM_CONTROLLER_STATE_DEFINE(sym, 4);                                                 \
  static struct pbl_pwm_sf32lb_gptim_state sym##_gptim_state;                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_pwm_sf32lb_gptim, struct pbl_pwm_controller, ctrl) { \
    .ctrl = PBL_PWM_CONTROLLER_INIT(sym, _name, pbl_pwm_sf32lb_gptim_init, nullptr,        \
                                    &pbl_pwm_sf32lb_gptim_ops),                            \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0), .base = (_base), .clk = _clk,            \
    .state = &sym##_gptim_state,                                                           \
  }

/** @} */
