/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/display.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/panel.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/reset.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_display_sf32lb_lcdc SF32LB LCD controller
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the SF32LB LCD controller scanning out to a panel.
 *
 * Drives a @ref drivers_panel on the JDI memory-in-pixel parallel interface, with the panel's
 * timing. Takes @ref PBL_DISPLAY_FORMAT_ARGB2222 rows, converted in place to the controller's
 * RGB332 while an update runs. Updates complete from the controller's interrupt, connected by the
 * board.
 *
 * @code{.c}
 * PBL_DISPLAY_SF32LB_LCDC_DEFINE(s_display, "display", SF32LB52_LCDC1_BASE, PBL_IRQN(LCDC1),
 *                                PBL_CLK(SF32LB_HPSYS_RCC, SF32LB52_HPSYS_CLK_LCDC1),
 *                                PBL_RESET(SF32LB_HPSYS_RCC_RESET, SF32LB52_HPSYS_RST_LCDC1),
 *                                &s_panel.panel, 200, 228, false,
 *                                PBL_PINCTRL(&s_lcdc_default, NULL, 0), s_discharge,
 *                                ARRAY_LENGTH(s_discharge));
 * PBL_IRQ_CONNECT(LCDC1, 5, pbl_display_sf32lb_lcdc_irq_handler, &s_display, 0);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_display_sf32lb_lcdc_state {
  uint8_t *fb;
  uint32_t pitch;
  uint16_t y0;
  uint16_t y1;
  uint16_t max_line;
  bool rotated;
};
/** @endcond */

/** @brief SF32LB LCD controller. */
struct pbl_display_sf32lb_lcdc {
  /** Display. */
  struct pbl_display disp;
  /** Driver runtime state. */
  struct pbl_display_sf32lb_lcdc_state *state;
  /** Controller registers. */
  uintptr_t base;
  /** Controller interrupt. */
  pbl_irq_t irqn;
  /** Controller clock. */
  struct pbl_clk clk;
  /** Controller reset. */
  struct pbl_reset rst;
  /** Panel. */
  const struct pbl_panel *panel;
  /** Panel interface pins. */
  struct pbl_pinctrl pinctrl;
  /** Panel inputs driven low, as GPIOs, while the panel is power cycled at init. */
  const struct pbl_gpio *discharge;
  /** Number of @ref discharge pins. */
  uint8_t num_discharge;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_sf32lb_lcdc_ops;
/** @endcond */

/**
 * @brief Define an SF32LB LCD controller display.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _base Controller registers.
 * @param _irqn Controller interrupt, from PBL_IRQN().
 * @param _clk Controller clock, from PBL_CLK().
 * @param _rst Controller reset, from PBL_RESET().
 * @param _panel Panel.
 * @param _width Width, in pixels, the panel's visible columns.
 * @param _height Height, in pixels, the panel's visible lines.
 * @param _rotated Panel mounted upside down.
 * @param _pinctrl Panel interface pins, from PBL_PINCTRL().
 * @param _discharge Pins driven low while the panel is power cycled, or NULL.
 * @param _num_discharge Number of @p _discharge pins.
 */
#define PBL_DISPLAY_SF32LB_LCDC_DEFINE(sym, _name, _base, _irqn, _clk, _rst, _panel, _width,     \
                                       _height, _rotated, _pinctrl, _discharge, _num_discharge)  \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                 \
  static struct pbl_display_sf32lb_lcdc_state sym##_lcdc_state;                                  \
  const struct pbl_display_sf32lb_lcdc sym = {                                                   \
    .disp = PBL_DISPLAY_INIT(sym, _name, &pbl_display_sf32lb_lcdc_ops, nullptr, _width, _height, \
                             PBL_DISPLAY_FORMAT_ARGB2222, _rotated),                             \
    .state = &sym##_lcdc_state,                                                                  \
    .base = (_base),                                                                             \
    .irqn = (_irqn),                                                                             \
    .clk = _clk,                                                                                 \
    .rst = _rst,                                                                                 \
    .panel = (_panel),                                                                           \
    .pinctrl = _pinctrl,                                                                         \
    .discharge = (_discharge),                                                                   \
    .num_discharge = (_num_discharge),                                                           \
  };                                                                                             \
  PBL_DEVICE_REGISTER(sym, &sym.disp.dev)

/**
 * @brief Controller interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param lcdc Display.
 */
void pbl_display_sf32lb_lcdc_irq_handler(const struct pbl_display_sf32lb_lcdc *lcdc);

#ifndef CONFIG_RELEASE
/**
 * @brief Drop the next transfer-complete interrupt.
 *
 * Test hook simulating a lost completion. The silent-loss timer then fires about 500 ms later
 * and crashes the system, producing a coredump. Not available in release builds.
 */
void pbl_display_sf32lb_lcdc_test_drop_next_complete(void);
#endif

/** @} */
