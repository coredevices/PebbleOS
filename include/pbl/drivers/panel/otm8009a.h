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
 * The panel takes a 24-bit video stream on two lanes, in burst mode, at 60 Hz from a 27.5 MHz
 * pixel clock; its controller sets the stream up from the timing of the panel.
 * @{
 */

/** @brief Panel. */
struct pbl_panel_otm8009a {
  /** Panel, child of its MIPI DSI host. */
  struct pbl_panel panel;
  /** The panel on its host. */
  struct pbl_mipi_dsi_device dsi;
  /** Reset line, active while the IC is held in reset. */
  struct pbl_gpio reset;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_panel_ops pbl_panel_otm8009a_ops;
extern const struct pbl_panel_timing pbl_panel_otm8009a_timing;
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
  PBL_DEVICE_INSTANCE(sym, struct pbl_panel_otm8009a, struct pbl_panel, panel) {                   \
    .panel = PBL_PANEL_INIT(sym, _name, pbl_panel_otm8009a_init, nullptr, &pbl_panel_otm8009a_ops, \
                            &pbl_panel_otm8009a_timing, PBL_PANEL_BUS_FORMAT_MIPI_DSI_RGB888),     \
    .dsi =                                                                                         \
        {                                                                                          \
          .host = (_host),                                                                         \
          .channel = (_channel),                                                                   \
          .lanes = 2U,                                                                             \
          .format = PBL_MIPI_DSI_FMT_RGB888,                                                       \
          .mode_flags =                                                                            \
              PBL_MIPI_DSI_MODE_VIDEO | PBL_MIPI_DSI_MODE_VIDEO_BURST | PBL_MIPI_DSI_MODE_LPM,     \
          .timing = &pbl_panel_otm8009a_timing,                                                    \
        },                                                                                         \
    .reset = _reset,                                                                               \
  }

/** @} */
