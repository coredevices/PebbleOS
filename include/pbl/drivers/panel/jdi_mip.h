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
 * Each supported part has its timing and size here, for the JDI LPM015M135A and the Sharp
 * LS013B7DD02. The panel has two supplies: VLCD, raised first, and VDDP. Its VCOM is inverted by a
 * PWM output at the VCOM frequency, 50% duty cycle, while the image is on. Once powered down, the
 * panel is kept off for 500 ms before it is powered again.
 *
 * @code{.c}
 * PBL_PANEL_JDI_LPM015M135A_DEFINE(s_panel, "panel", &s_vddp, &s_vlcd,
 *                                  PBL_PWM(&s_lptim2, 0, PBL_PWM_HZ(60), 0));
 * @endcode
 * @{
 */

/** @name JDI LPM015M135A, 200 x 228
 * @{
 */
#define PBL_PANEL_JDI_LPM015M135A_WIDTH      200U
#define PBL_PANEL_JDI_LPM015M135A_HEIGHT     228U
#define PBL_PANEL_JDI_LPM015M135A_ROUND_MASK nullptr
#define PBL_PANEL_JDI_LPM015M135A_TIMING        \
  {.clock_hz = 746268,                          \
   .hactive = PBL_PANEL_JDI_LPM015M135A_WIDTH,  \
   .hback_porch = 2,                            \
   .hfront_porch = 6,                           \
   .vactive = PBL_PANEL_JDI_LPM015M135A_HEIGHT, \
   .vback_porch = 0,                            \
   .vfront_porch = 6,                           \
   .enable_start = 3,                           \
   .enable_end = 99}
/** @} */

/** @name Sharp LS013B7DD02, 260 x 260 round
 * @{
 */
#define PBL_PANEL_SHARP_LS013B7DD02_WIDTH      260U
#define PBL_PANEL_SHARP_LS013B7DD02_HEIGHT     260U
#define PBL_PANEL_SHARP_LS013B7DD02_ROUND_MASK pbl_panel_ls013b7dd02_round_mask
#define PBL_PANEL_SHARP_LS013B7DD02_TIMING        \
  {.clock_hz = 746268,                            \
   .hactive = PBL_PANEL_SHARP_LS013B7DD02_WIDTH,  \
   .hback_porch = 0,                              \
   .hfront_porch = 8,                             \
   .vactive = PBL_PANEL_SHARP_LS013B7DD02_HEIGHT, \
   .vback_porch = 0,                              \
   .vfront_porch = 4,                             \
   .enable_start = 6,                             \
   .enable_end = 128}
/** @} */

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

#define PBL_PANEL_JDI_MIP_DEFINE_(sym, _name, _part, _vddp, _vlcd, ...)                          \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                  \
  static struct pbl_panel_jdi_mip_state sym##_jdi_state;                                         \
  static const struct pbl_panel_timing sym##_timing = _part##_TIMING;                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_panel_jdi_mip, struct pbl_panel, panel) {                  \
    .panel = PBL_PANEL_INIT(sym, _name, pbl_panel_jdi_mip_init, &pbl_panel_jdi_mip_ops, nullptr, \
                            &sym##_timing, PBL_PANEL_BUS_FORMAT_JDI_RGB222),                     \
    .state = &sym##_jdi_state, .vddp = (_vddp), .vlcd = (_vlcd), .vcom = __VA_ARGS__,            \
  }
/** @endcond */

/**
 * @brief Define a JDI LPM015M135A panel.
 *
 * @param sym Symbol of the panel.
 * @param _name Name.
 * @param _vddp VDDP supply.
 * @param _vlcd VLCD supply.
 * @param ... VCOM inversion signal, from PBL_PWM().
 */
#define PBL_PANEL_JDI_LPM015M135A_DEFINE(sym, _name, _vddp, _vlcd, ...) \
  PBL_PANEL_JDI_MIP_DEFINE_(sym, _name, PBL_PANEL_JDI_LPM015M135A, _vddp, _vlcd, __VA_ARGS__)

/**
 * @brief Define a Sharp LS013B7DD02 panel.
 *
 * @param sym Symbol of the panel.
 * @param _name Name.
 * @param _vddp VDDP supply.
 * @param _vlcd VLCD supply.
 * @param ... VCOM inversion signal, from PBL_PWM().
 */
#define PBL_PANEL_SHARP_LS013B7DD02_DEFINE(sym, _name, _vddp, _vlcd, ...) \
  PBL_PANEL_JDI_MIP_DEFINE_(sym, _name, PBL_PANEL_SHARP_LS013B7DD02, _vddp, _vlcd, __VA_ARGS__)

/** @} */
