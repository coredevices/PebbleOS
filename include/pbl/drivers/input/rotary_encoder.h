/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/gpio.h>

/**
 * @defgroup drivers_input_rotary_encoder Rotary encoder
 * @ingroup drivers
 * @brief Gray-coded quadrature encoder on two GPIOs, reporting @ref PBL_INPUT_EV_REL steps.
 *
 * Every edge of either line is decoded in its interrupt, which wakes the system from sleep.
 * Transitions skipping a state are ignored, so contact bounce cancels out.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_rotary_encoder_state {
  uint8_t last;
  int8_t acc;
};
/** @endcond */

/** @brief Rotary encoder. */
struct pbl_rotary_encoder {
  /** Device. */
  struct pbl_device dev;
  /** Line A, read active when its contact is closed. */
  struct pbl_gpio a;
  /** Line B, read active when its contact is closed. */
  struct pbl_gpio b;
  /** @ref PBL_INPUT_EV_REL code, @c linux,axis. */
  uint16_t code;
  /** Steps per Gray code period (1, 2 or 4), @c rotary-encoder,steps-per-period. */
  uint8_t steps_per_period;
  /** Whether turning it wakes the system from power-off, @c wakeup-source. */
  bool wakeup_source;
  /** Runtime state. */
  struct pbl_rotary_encoder_state *state;
};

/** @cond INTERNAL_HIDDEN */
int pbl_rotary_encoder_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define and register a rotary encoder.
 *
 * @param sym Symbol of the struct pbl_rotary_encoder.
 * @param _name Device name.
 * @param _a Line A, a @ref PBL_GPIO.
 * @param _b Line B, a @ref PBL_GPIO.
 * @param _code @ref PBL_INPUT_EV_REL code.
 * @param _steps_per_period Steps per Gray code period.
 * @param _wakeup_source Whether turning it wakes the system from power-off.
 */
#define PBL_ROTARY_ENCODER_DEFINE(sym, _name, _a, _b, _code, _steps_per_period, _wakeup_source)  \
  static struct pbl_rotary_encoder_state sym##_rotary_encoder_state;                             \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  PBL_DEVICE_INSTANCE_DEV(sym, struct pbl_rotary_encoder) {                                      \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_rotary_encoder_init, nullptr), .a = _a, .b = _b,      \
    .code = (_code), .steps_per_period = (_steps_per_period), .wakeup_source = (_wakeup_source), \
    .state = &sym##_rotary_encoder_state,                                                        \
  }

/** @} */
