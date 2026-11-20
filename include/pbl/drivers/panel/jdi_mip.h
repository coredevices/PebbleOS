/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/panel.h>
#include <pbl/drivers/panel/ls013b7dd02.h>
#include <pbl/drivers/pwm.h>
#include <pbl/drivers/regulator.h>

/**
 * @defgroup drivers_panel_jdi_mip Memory-in-pixel panels
 * @ingroup drivers_panel
 * @brief @ref drivers_panel driver for memory-in-pixel panels on the JDI parallel interface.
 *
 * Drives the JDI LPM015M135A and the Sharp LS013B7DD02, with their timing from devicetree. The
 * panel has two supplies: VLCD, raised first, and VDDP. Its VCOM is inverted by a PWM output at the
 * VCOM frequency, 50% duty cycle, while the image is on. Once powered down, the panel is kept off
 * for 500 ms before it is powered again.
 *
 * @code{.c}
 * PBL_PANEL_JDI_MIP_DEFINE(s_panel, "panel", &s_vddp, &s_vlcd, 746268, 200, 2, 6, 228, 0, 6, 3, 99,
 *                          PBL_PWM(&s_lptim2, 0, PBL_PWM_HZ(60), 0));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_panel_jdi_mip_state {
  uint64_t off_ms;
  bool powered;
};
/** @endcond */

/** @brief Memory-in-pixel panel. */
struct pbl_panel_jdi_mip {
  /** Panel. */
  struct pbl_panel panel;
  /** Driver runtime state. */
  struct pbl_panel_jdi_mip_state *state;
  /** VDDP supply. */
  const struct pbl_regulator *vddp;
  /** VLCD supply. */
  const struct pbl_regulator *vlcd;
  /** VCOM inversion signal. */
  struct pbl_pwm vcom;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_panel_ops pbl_panel_jdi_mip_ops;
int pbl_panel_jdi_mip_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a memory-in-pixel panel.
 *
 * @param sym Symbol of the panel.
 * @param _name Name.
 * @param _vddp VDDP supply.
 * @param _vlcd VLCD supply.
 * @param _clock_hz HCK frequency, in Hz.
 * @param _hactive Visible columns.
 * @param _hback_porch Columns before the visible ones.
 * @param _hfront_porch Columns after the visible ones.
 * @param _vactive Visible lines.
 * @param _vback_porch Lines before the visible ones.
 * @param _vfront_porch Lines after the visible ones.
 * @param _enable_start First column of the ENB pulse.
 * @param _enable_end Last column of the ENB pulse.
 * @param ... VCOM inversion signal, from PBL_PWM().
 */
#define PBL_PANEL_JDI_MIP_DEFINE(sym, _name, _vddp, _vlcd, _clock_hz, _hactive, _hback_porch,    \
                                 _hfront_porch, _vactive, _vback_porch, _vfront_porch,           \
                                 _enable_start, _enable_end, ...)                                \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  static struct pbl_panel_jdi_mip_state sym##_jdi_state;                                         \
  static const struct pbl_panel_timing sym##_timing = {                                          \
    .clock_hz = (_clock_hz),                                                                     \
    .hactive = (_hactive),                                                                       \
    .hback_porch = (_hback_porch),                                                               \
    .hfront_porch = (_hfront_porch),                                                             \
    .vactive = (_vactive),                                                                       \
    .vback_porch = (_vback_porch),                                                               \
    .vfront_porch = (_vfront_porch),                                                             \
    .enable_start = (_enable_start),                                                             \
    .enable_end = (_enable_end),                                                                 \
  };                                                                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_panel_jdi_mip, struct pbl_panel, panel) {                  \
    .panel = PBL_PANEL_INIT(sym, _name, pbl_panel_jdi_mip_init, nullptr, &pbl_panel_jdi_mip_ops, \
                            &sym##_timing, PBL_PANEL_BUS_FORMAT_JDI_RGB222),                     \
    .state = &sym##_jdi_state, .vddp = (_vddp), .vlcd = (_vlcd), .vcom = __VA_ARGS__,            \
  }

/** @} */
