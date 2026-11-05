/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_pwm_nrf5 nRF5 PWM
 * @ingroup drivers_pwm
 * @brief @ref drivers_pwm controller driver for the nRF5 PWM peripheral.
 *
 * Four channels share one counter, so channels running at the same time have the same period;
 * applying another period fails with @c -EBUSY. The counter runs from the 16 MHz clock with the
 * smallest prescaler that fits the period, at most 32767 counts. The outputs are GPIOs driven
 * to their inactive level while the PWM is stopped.
 *
 * @code{.c}
 * PBL_PWM_NRF5_DEFINE(s_pwm0, "pwm0", 0, NULL, PBL_GPIO(NRF5_GPIO_P0, 26, 0));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_pwm_nrf5_state {
  uint16_t seq[4];
  uint32_t prescaler;
  uint32_t top;
  uint8_t running;
  bool stopping;
};
/** @endcond */

/** @brief nRF5 PWM controller. */
struct pbl_pwm_nrf5 {
  /** PWM controller. */
  struct pbl_pwm_controller ctrl;
  /** Driver runtime state. */
  struct pbl_pwm_nrf5_state *state;
  /** Output of each channel; unconnected channels cannot be used. */
  struct pbl_gpio out[4];
  /** Peripheral instance, from 0. */
  uint8_t index;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pwm_controller_ops pbl_pwm_nrf5_ops;
int pbl_pwm_nrf5_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define an nRF5 PWM controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _idx Peripheral instance, 0 to 3.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param ... Outputs of channels 0 to 3, from PBL_GPIO().
 */
#define PBL_PWM_NRF5_DEFINE(sym, _name, _idx, _deps, ...)                                     \
  PBL_PWM_CONTROLLER_STATE_DEFINE(sym, 4);                                                    \
  static struct pbl_pwm_nrf5_state sym##_nrf5_state;                                          \
  const struct pbl_pwm_nrf5 sym = {                                                           \
    .ctrl = PBL_PWM_CONTROLLER_INIT(sym, _name, pbl_pwm_nrf5_init, _deps, &pbl_pwm_nrf5_ops), \
    .state = &sym##_nrf5_state,                                                               \
    .out = {__VA_ARGS__},                                                                     \
    .index = (_idx),                                                                          \
  };                                                                                          \
  PBL_DEVICE_REGISTER(sym, &sym.ctrl.dev)

/** @} */
