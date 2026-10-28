/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/pwm.h>

#include <bf0_hal.h>
#include <bf0_hal_lcdc.h>

/**
 * @defgroup drivers_display_sf32lb SF32LB JDI display
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for JDI memory-in-pixel panels driven by the SF32LB LCD
 * controller.
 *
 * Takes @ref PBL_DISPLAY_FORMAT_ARGB2222 rows, converted in place to the controller's RGB332
 * while an update runs. Updates complete from the LCD controller's interrupt, connected by the
 * board.
 * @{
 */

/** @brief Driver runtime state, with the LCD controller configuration set by the board. */
struct pbl_display_jdi_sf32lb_state {
  /** LCD controller handle; the board sets its instance and configuration. */
  LCDC_HandleTypeDef hlcdc;
  /** @cond INTERNAL_HIDDEN */
  uint8_t *fb;
  uint32_t pitch;
  uint16_t y0;
  uint16_t y1;
  bool rotated;
  /** @endcond */
};

/** @brief SF32LB JDI display. */
struct pbl_display_jdi_sf32lb {
  /** Display. */
  struct pbl_display disp;
  /** Driver runtime state. */
  struct pbl_display_jdi_sf32lb_state *state;
  /** LCD controller interrupt. */
  IRQn_Type irqn;
  /** VCOM inversion signal, run at its period with a 50% duty cycle while the panel is on. */
  struct pbl_pwm vcom;
  /** Panel interface pins. */
  struct pbl_pinctrl pinctrl;
  /** Panel inputs driven low while the panel is power cycled, as GPIOs. */
  const struct pbl_gpio *discharge;
  /** Number of @ref discharge pins. */
  uint8_t num_discharge;
  /** Panel VDDP supply enable. */
  struct pbl_gpio vddp;
  /** Panel VLCD supply enable. */
  struct pbl_gpio vlcd;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_jdi_sf32lb_ops;
/** @endcond */

/**
 * @brief Define an SF32LB JDI display.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _state Driver runtime state, with the LCD controller configuration.
 * @param _width Width, in pixels.
 * @param _height Height, in pixels.
 * @param _rotated Panel mounted upside down.
 * @param ... Remaining struct pbl_display_jdi_sf32lb fields, as designated initializers.
 */
#define PBL_DISPLAY_JDI_SF32LB_DEFINE(sym, _name, _state, _width, _height, _rotated, ...)       \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                \
  const struct pbl_display_jdi_sf32lb sym = {                                                   \
    .disp = PBL_DISPLAY_INIT(sym, _name, &pbl_display_jdi_sf32lb_ops, nullptr, _width, _height, \
                             PBL_DISPLAY_FORMAT_ARGB2222, _rotated),                            \
    .state = (_state),                                                                          \
    __VA_ARGS__,                                                                                \
  };                                                                                            \
  PBL_DEVICE_REGISTER(sym, &sym.disp.dev)

/**
 * @brief LCD controller interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param jdi Display.
 */
void pbl_display_jdi_sf32lb_irq_handler(const struct pbl_display_jdi_sf32lb *jdi);

#ifndef CONFIG_RELEASE
/**
 * @brief Drop the next LCD transfer-complete interrupt.
 *
 * Test hook simulating a lost completion. The silent-loss timer then fires about 500 ms later
 * and crashes the system, producing a coredump. Not available in release builds.
 */
void display_jdi_test_drop_next_complete(void);
#endif

/** @} */
