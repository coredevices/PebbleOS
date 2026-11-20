/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/backlight.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/mipi_dbi.h>
#include <pbl/drivers/panel.h>

/**
 * @defgroup drivers_panel_co5300 CO5300
 * @ingroup drivers_panel
 * @brief @ref drivers_panel driver for AMOLED panels on the ChipOne CO5300 driver IC.
 *
 * The panel has its own frame memory, written in RGB565 over quad SPI by its @ref drivers_mipi_dbi
 * host, in pairs of rows. Being an AMOLED, it has no backlight: its brightness is a DCS command,
 * which the driver gives as a @ref drivers_backlight of the panel, @c sym_backlight. The panel
 * starts at its brightest, and a brightness set while it is off is sent when it turns on.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_panel_co5300_state {
  uint8_t brightness;
  bool awake;
  bool on;
  bool h_flip;
};
/** @endcond */

/** @brief CO5300 panel. */
struct pbl_panel_co5300 {
  /** Panel, child of its MIPI DBI host. */
  struct pbl_panel panel;
  /** Driver runtime state. */
  struct pbl_panel_co5300_state *state;
  /** MIPI DBI host. */
  const struct pbl_mipi_dbi *dbi;
  /** Reset line, active while the IC is held in reset. */
  struct pbl_gpio reset;
};

/** @brief Brightness of a CO5300 panel. */
struct pbl_panel_co5300_backlight {
  /** Backlight, child of the panel. */
  struct pbl_backlight backlight;
  /** Panel. */
  const struct pbl_panel_co5300 *co5300;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_panel_ops pbl_panel_co5300_ops;
extern const struct pbl_backlight_ops pbl_panel_co5300_backlight_ops;
int pbl_panel_co5300_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define a CO5300 panel and its brightness.
 *
 * @param sym Symbol of the panel.
 * @param _name Name.
 * @param _dbi MIPI DBI host.
 * @param _reset Reset line, from PBL_GPIO().
 * @param _bus_max_hz Fastest clock of the quad SPI bus, in Hz.
 * @param _width Visible columns.
 * @param _height Visible lines.
 * @param _x First visible column in the frame memory.
 * @param _y First visible line in the frame memory.
 */
#define PBL_PANEL_CO5300_DEFINE(sym, _name, _dbi, _reset, _bus_max_hz, _width, _height, _x, _y) \
  extern const struct pbl_backlight sym##_backlight;                                            \
  PBL_DEVICE_STATE_DEFINE(sym);                                                                 \
  static struct pbl_panel_co5300_state sym##_co5300_state = {                                   \
    .brightness = PBL_BACKLIGHT_BRIGHTNESS_MAX,                                                 \
  };                                                                                            \
  static const struct pbl_panel_timing sym##_timing = {                                         \
    .hactive = (_width),                                                                        \
    .hback_porch = (_x),                                                                        \
    .vactive = (_height),                                                                       \
    .vback_porch = (_y),                                                                        \
  };                                                                                            \
  PBL_DEVICE_INSTANCE(sym, struct pbl_panel_co5300, struct pbl_panel, panel){                   \
    .panel = PBL_PANEL_INIT(sym, _name, pbl_panel_co5300_init, nullptr, &pbl_panel_co5300_ops,  \
                            &sym##_timing, PBL_PANEL_BUS_FORMAT_QSPI_RGB565,                    \
                            .bus_max_hz = (_bus_max_hz), .row_multiple = 2U),                   \
    .state = &sym##_co5300_state,                                                               \
    .dbi = (_dbi),                                                                              \
    .reset = _reset,                                                                            \
  };                                                                                            \
  PBL_BACKLIGHT_STATE_DEFINE(sym##_backlight);                                                  \
  PBL_DEVICE_INSTANCE(sym##_backlight, struct pbl_panel_co5300_backlight, struct pbl_backlight, \
                      backlight) {                                                              \
    .backlight = PBL_BACKLIGHT_INIT(sym##_backlight, _name "_backlight", &sym##_inst.panel.dev, \
                                    &pbl_panel_co5300_backlight_ops),                           \
    .co5300 = &sym##_inst,                                                                      \
  }

/** @} */
