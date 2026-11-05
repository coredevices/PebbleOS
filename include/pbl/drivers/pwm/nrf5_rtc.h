/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_pwm_nrf5_rtc nRF5 RTC PWM
 * @ingroup drivers_pwm
 * @brief @ref drivers_pwm controller driver generating a PWM output with an nRF5 RTC.
 *
 * One channel, generated without the CPU or the high-frequency clock: the RTC counts the 32768 Hz
 * low-frequency clock, and PPI connects its compare events to GPIOTE tasks driving the output,
 * one at the end of the period, which also clears the counter, and one at the end of the duty
 * cycle. The period and duty cycle resolution is one RTC tick, about 30.5 us. A 0% or 100% duty
 * cycle stops the RTC and holds the output.
 *
 * @code{.c}
 * PBL_PWM_NRF5_RTC_DEFINE(s_extcomin, "extcomin", 2, PBL_GPIO(NRF5_GPIO_P1, 15, 0), NULL);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_pwm_nrf5_rtc_state {
  uint8_t gpiote_ch;
  uint8_t ppi_ch[2];
};
/** @endcond */

/** @brief nRF5 RTC PWM controller. */
struct pbl_pwm_nrf5_rtc {
  /** PWM controller. */
  struct pbl_pwm_controller ctrl;
  /** Driver runtime state. */
  struct pbl_pwm_nrf5_rtc_state *state;
  /** Output. */
  struct pbl_gpio out;
  /** RTC instance, from 0, not used by anything else. */
  uint8_t rtc;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pwm_controller_ops pbl_pwm_nrf5_rtc_ops;
int pbl_pwm_nrf5_rtc_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an nRF5 RTC PWM controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _rtc RTC instance generating the output, 0 to 2.
 * @param _out Output, from PBL_GPIO().
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_PWM_NRF5_RTC_DEFINE(sym, _name, _rtc, _out, _deps)                                    \
  PBL_PWM_CONTROLLER_STATE_DEFINE(sym, 1);                                                        \
  static struct pbl_pwm_nrf5_rtc_state sym##_nrf5_rtc_state;                                      \
  const struct pbl_pwm_nrf5_rtc sym = {                                                           \
    .ctrl =                                                                                       \
        PBL_PWM_CONTROLLER_INIT(sym, _name, pbl_pwm_nrf5_rtc_init, _deps, &pbl_pwm_nrf5_rtc_ops), \
    .state = &sym##_nrf5_rtc_state,                                                               \
    .out = _out,                                                                                  \
    .rtc = (_rtc),                                                                                \
  };                                                                                              \
  PBL_DEVICE_REGISTER(sym, &sym.ctrl.dev)

/** @} */
