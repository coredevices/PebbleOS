/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_pwm_sf32lb_lptim SF32LB LPTIM PWM
 * @ingroup drivers_pwm
 * @brief @ref drivers_pwm controller driver for the SF32LB low-power timers.
 *
 * One channel, counting the RC10K low-power clock, so it keeps running in deep sleep. The
 * period and duty cycle resolution is one RC10K cycle, about 100 us; the frequency is the
 * calibrated one when the output is applied. The pin state can route the inverted output too.
 *
 * @code{.c}
 * PBL_PINCTRL_STATE_DEFINE(s_lptim2_default, SF32LB52_PINCTRL,
 *                          SF32LB52_PA24_PBR_LPTIM2_OUT | PBL_PINCTRL_SF32LB52_INPUT,
 *                          SF32LB52_PA25_PBR_LPTIM2_INV_OUT | PBL_PINCTRL_SF32LB52_INPUT);
 * PBL_PWM_SF32LB_LPTIM_DEFINE(s_lptim2, "lptim2", 2, &s_lptim2_default, NULL);
 * @endcode
 * @{
 */

/** @brief SF32LB LPTIM PWM controller. */
struct pbl_pwm_sf32lb_lptim {
  /** PWM controller. */
  struct pbl_pwm_controller ctrl;
  /** Output pins. */
  struct pbl_pinctrl pinctrl;
  /** Timer instance, 1 or 2. */
  uint8_t index;
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
 * @param _idx Timer instance, 1 or 2.
 * @param _pinctrl Pin state with the outputs, from PBL_PINCTRL_STATE_DEFINE().
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_PWM_SF32LB_LPTIM_DEFINE(sym, _name, _idx, _pinctrl, _deps)            \
  PBL_PWM_CONTROLLER_STATE_DEFINE(sym, 1);                                        \
  const struct pbl_pwm_sf32lb_lptim sym = {                                       \
    .ctrl = PBL_PWM_CONTROLLER_INIT(sym, _name, pbl_pwm_sf32lb_lptim_init, _deps, \
                                    &pbl_pwm_sf32lb_lptim_ops),                   \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                 \
    .index = (_idx),                                                              \
  };                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.ctrl.dev)

/** @} */
