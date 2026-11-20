/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_panel Panel
 * @ingroup drivers
 * @brief Panel class.
 *
 * A panel is the glass behind a display controller: a struct pbl_panel device owning the panel's
 * supplies and drive signals, and describing what the controller has to generate, its timing and
 * bus format. As in Linux, the controller brings the panel up and down in steps around its own:
 *
 * - pbl_panel_prepare() powers the panel, before the controller starts scanning out;
 * - pbl_panel_enable() turns the image on, once the controller drives the panel;
 * - pbl_panel_disable() and pbl_panel_unprepare() undo them, in reverse order.
 *
 * Calls are made by the controller, from task context, and return 0 or a negative errno,
 * @c -ENODEV if the panel is not ready.
 * @{
 */

/** @brief Bus formats. */
enum pbl_panel_bus_format {
  /**
   * JDI memory-in-pixel parallel interface: 2 bits per color on the R1/R2, G1/G2 and B1/B2
   * lines, with HST/HCK, VST/VCK, ENB and XRST.
   */
  PBL_PANEL_BUS_FORMAT_JDI_RGB222,
  /**
   * Quad SPI to a panel with frame memory, on a @ref drivers_mipi_dbi host: commands on one line
   * with opcode 0x02 and RGB565 pixels on four with opcode 0x32, each with the command in the
   * middle byte of a 24-bit address, and reads with opcode 0x03.
   */
  PBL_PANEL_BUS_FORMAT_QSPI_RGB565,
};

/**
 * @brief Timing of a panel scanned by its controller.
 *
 * Horizontal values are in clock periods, vertical values in lines. A panel with frame memory is
 * not scanned: the porches before the visible columns and lines are where they start in its
 * memory.
 */
struct pbl_panel_timing {
  /** Horizontal clock frequency (HCK), in Hz. */
  uint32_t clock_hz;
  /** Visible columns. */
  uint16_t hactive;
  /** Columns before the visible ones. */
  uint16_t hback_porch;
  /** Columns after the visible ones. */
  uint16_t hfront_porch;
  /** Visible lines. */
  uint16_t vactive;
  /** Lines before the visible ones. */
  uint16_t vback_porch;
  /** Lines after the visible ones. */
  uint16_t vfront_porch;
  /** First column of the write enable (ENB) pulse. */
  uint16_t enable_start;
  /** Last column of the write enable (ENB) pulse. */
  uint16_t enable_end;
};

/** @brief Class of the panels. */
extern const struct pbl_device_class pbl_panel_class;

struct pbl_panel;

/** @brief Panel driver operations. */
struct pbl_panel_ops {
  /** Power the panel up. Returns 0 or a negative errno. */
  int (*prepare)(const struct pbl_panel *panel);
  /** Turn the image on. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_panel *panel);
  /** Turn the image off. Returns 0 or a negative errno. */
  int (*disable)(const struct pbl_panel *panel);
  /** Power the panel down. Returns 0 or a negative errno. */
  int (*unprepare)(const struct pbl_panel *panel);
  /**
   * Flip what is written to the panel left to right, from the next frame and through power
   * cycles. Optional. Returns 0 or a negative errno.
   */
  int (*set_h_flip)(const struct pbl_panel *panel, bool flip);
};

/** @brief A panel. */
struct pbl_panel {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_panel_ops *ops;
  /** Timing the controller generates. */
  const struct pbl_panel_timing *timing;
  /** Bus format. */
  enum pbl_panel_bus_format bus_format;
  /** Fastest clock of the bus a panel with frame memory takes its pixels on, in Hz. */
  uint32_t bus_max_hz;
  /** Rows a panel with frame memory is written in multiples of, 0 for any number. */
  uint8_t row_multiple;
};

/**
 * @brief Initializer for the struct pbl_panel of panel @p sym, for the @c PBL_*_DEFINE() macro of
 * a driver, with its device state defined by PBL_DEVICE_STATE_DEFINE().
 *
 * @param sym Symbol of the panel instance.
 * @param _name Name.
 * @param _init Driver device init.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 * @param _timing Timing.
 * @param _bus_format Bus format.
 * @param ... Other fields, designated initializers of struct pbl_panel.
 */
#define PBL_PANEL_INIT(sym, _name, _init, _parent, _ops, _timing, _bus_format, ...) \
  {.dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, &pbl_panel_class),      \
   .ops = (_ops),                                                                   \
   .timing = (_timing),                                                             \
   .bus_format = (_bus_format),                                                     \
   __VA_ARGS__}

/**
 * @brief Power a panel up.
 *
 * @param panel Panel.
 * @return 0 or a negative errno.
 */
int pbl_panel_prepare(const struct pbl_panel *panel);

/**
 * @brief Turn a panel's image on.
 *
 * @param panel Panel.
 * @return 0 or a negative errno.
 */
int pbl_panel_enable(const struct pbl_panel *panel);

/**
 * @brief Turn a panel's image off.
 *
 * @param panel Panel.
 * @return 0 or a negative errno.
 */
int pbl_panel_disable(const struct pbl_panel *panel);

/**
 * @brief Power a panel down.
 *
 * @param panel Panel.
 * @return 0 or a negative errno.
 */
int pbl_panel_unprepare(const struct pbl_panel *panel);

/**
 * @brief Flip what is written to a panel left to right, if the panel can.
 *
 * Takes effect from the next frame written, and is kept through power cycles of the panel.
 *
 * @param panel Panel.
 * @param flip Flip the image.
 * @return 0, @c -ENOTSUP if the panel cannot, or another negative errno.
 */
int pbl_panel_set_h_flip(const struct pbl_panel *panel, bool flip);

/** @} */
