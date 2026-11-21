/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/mipi_dsi.h>
#include <pbl/drivers/panel.h>

/**
 * @defgroup drivers_panel_otm8009a Orise Tech OTM8009A
 * @ingroup drivers_panel
 * @brief @ref drivers_panel driver for a 480x800 panel on an OTM8009A, over MIPI DSI.
 *
 * The panel keeps the image in its frame memory, written in command mode on two lanes, from a
 * 42 MHz pixel clock, in windows timed by its tearing effect. It takes a frame turned a quarter
 * turn, 800x480, through its address mode.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_panel_otm8009a_state {
  bool turned;
  struct pbl_panel_timing timing;
  // Commands go in low power only while the panel is set up
  struct pbl_mipi_dsi_device dsi;
};
/** @endcond */

/** @brief Panel. */
struct pbl_panel_otm8009a {
  /** Panel, child of its MIPI DSI host. */
  struct pbl_panel panel;
  /** Reset line, active while the IC is held in reset. */
  struct pbl_gpio reset;
  /** Driver runtime state: the panel on its host, and the timing of the orientation of the frame.
   */
  struct pbl_panel_otm8009a_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_panel_ops pbl_panel_otm8009a_ops;
int pbl_panel_otm8009a_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the panel.
 *
 * @param sym Symbol of the panel.
 * @param _name Name.
 * @param _host MIPI DSI host.
 * @param _channel Virtual channel.
 * @param _reset Reset line, from PBL_GPIO().
 */
#define PBL_PANEL_OTM8009A_DEFINE(sym, _name, _host, _channel, _reset)                             \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                    \
  static struct pbl_panel_otm8009a_state sym##_otm_state = {                                       \
    .dsi = {                                                                                       \
      .host = (_host),                                                                             \
      .channel = (_channel),                                                                       \
      .lanes = 2U,                                                                                 \
      .format = PBL_MIPI_DSI_FMT_RGB888,                                                           \
      .timing = &sym##_otm_state.timing,                                                           \
    },                                                                                             \
  };                                                                                               \
  PBL_DEVICE_INSTANCE(sym, struct pbl_panel_otm8009a, struct pbl_panel, panel) {                   \
    .panel = PBL_PANEL_INIT(sym, _name, pbl_panel_otm8009a_init, nullptr, &pbl_panel_otm8009a_ops, \
                            &sym##_otm_state.timing, PBL_PANEL_BUS_FORMAT_MIPI_DSI_CMD_RGB888),    \
    .reset = _reset, .state = &sym##_otm_state,                                                    \
  }

/** @} */
