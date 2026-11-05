/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/panel.h>
#include <pbl/drivers/pwm.h>

/**
 * @defgroup drivers_panel_jdi_mip JDI memory-in-pixel panel
 * @ingroup drivers_panel
 * @brief @ref drivers_panel driver for JDI memory-in-pixel panels on the parallel interface.
 *
 * The panel takes its timing from the board, and has two supplies switched by GPIOs: VLCD, raised
 * first, and VDDP. Its VCOM is inverted by a PWM output at the VCOM frequency, 50% duty cycle,
 * while the image is on. Once powered down, the panel is kept off for 500 ms before it is powered
 * again. The panel is powered down when it is set up.
 *
 * @code{.c}
 * static const struct pbl_panel_timing s_timing = {
 *   .clock_hz = 746268, .hactive = 200, .hback_porch = 2, .hfront_porch = 6,
 *   .vactive = 228, .vfront_porch = 6, .enable_start = 3, .enable_end = 99,
 * };
 *
 * PBL_PANEL_JDI_MIP_DEFINE(s_panel, "panel", &s_timing, PBL_GPIO(SF32LB_GPIO1, 28, 0),
 *                          PBL_GPIO(SF32LB_GPIO1, 29, PBL_GPIO_ACTIVE_LOW),
 *                          PBL_PWM(&s_lptim2.ctrl, 0, PBL_PWM_HZ(60), 0));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_panel_jdi_mip_state {
  uint64_t off_ms;
};
/** @endcond */

/** @brief JDI memory-in-pixel panel. */
struct pbl_panel_jdi_mip {
  /** Panel. */
  struct pbl_panel panel;
  /** Driver runtime state. */
  struct pbl_panel_jdi_mip_state *state;
  /** VDDP supply enable. */
  struct pbl_gpio vddp;
  /** VLCD supply enable. */
  struct pbl_gpio vlcd;
  /** VCOM inversion signal. */
  struct pbl_pwm vcom;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_panel_ops pbl_panel_jdi_mip_ops;
int pbl_panel_jdi_mip_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a JDI memory-in-pixel panel.
 *
 * @param sym Symbol of the panel.
 * @param _name Name.
 * @param _timing Timing, a struct pbl_panel_timing.
 * @param _vddp VDDP supply enable, from PBL_GPIO().
 * @param _vlcd VLCD supply enable, from PBL_GPIO().
 * @param _vcom VCOM inversion signal, from PBL_PWM(), at the VCOM frequency.
 */
#define PBL_PANEL_JDI_MIP_DEFINE(sym, _name, _timing, _vddp, _vlcd, _vcom)                       \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  static struct pbl_panel_jdi_mip_state sym##_jdi_state;                                         \
  const struct pbl_panel_jdi_mip sym = {                                                         \
    .panel = PBL_PANEL_INIT(sym, _name, pbl_panel_jdi_mip_init, &pbl_panel_jdi_mip_ops, nullptr, \
                            _timing, PBL_PANEL_BUS_FORMAT_JDI_RGB222),                           \
    .state = &sym##_jdi_state,                                                                   \
    .vddp = _vddp,                                                                               \
    .vlcd = _vlcd,                                                                               \
    .vcom = _vcom,                                                                               \
  };                                                                                             \
  PBL_DEVICE_REGISTER(sym, &sym.panel.dev)

/** @} */
