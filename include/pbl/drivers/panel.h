/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/util/bits.h>

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
  /**
   * MIPI DSI in command mode: RGB888 frames of a display controller into the frame memory of
   * the panel, each on a refresh of the @ref drivers_mipi_dsi host, written in windows timed by
   * the tearing effect of the panel.
   */
  PBL_PANEL_BUS_FORMAT_MIPI_DSI_CMD_RGB888,
};

/**
 * @brief Rotations and reflections of the frame a panel takes, as a mask of one rotation and any
 * reflections.
 *
 * As the plane rotations of Linux: rotations are counterclockwise, and a reflection mirrors the
 * frame before it is rotated.
 */
enum pbl_panel_rotation {
  /** Not rotated. */
  PBL_PANEL_ROTATE_0 = PBL_BIT(0),
  /** Rotated by 90 degrees. */
  PBL_PANEL_ROTATE_90 = PBL_BIT(1),
  /** Rotated by 180 degrees. */
  PBL_PANEL_ROTATE_180 = PBL_BIT(2),
  /** Rotated by 270 degrees. */
  PBL_PANEL_ROTATE_270 = PBL_BIT(3),
  /** Mirrored left to right. */
  PBL_PANEL_REFLECT_X = PBL_BIT(4),
  /** Mirrored top to bottom. */
  PBL_PANEL_REFLECT_Y = PBL_BIT(5),
};

/**
 * @brief Rotation of a panel from degrees, as the devicetree @c rotation of a panel gives them.
 *
 * @param deg 0, 90, 180 or 270.
 */
#define PBL_PANEL_ROTATION(deg) PBL_BIT((deg) / 90U)

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
   * Take the frame rotated and reflected, a mask of @ref pbl_panel_rotation, from the next frame
   * and through power cycles. Optional. Returns 0, @c -ENOTSUP for one the panel cannot take, or
   * another negative errno.
   */
  int (*set_rotation)(const struct pbl_panel *panel, uint32_t rotation);
  /**
   * Write the next pixels from column @p x0 to @p x1 and row @p y0 to @p y1 of the frame, both
   * included. Optional. Returns 0 or a negative errno.
   */
  int (*set_window)(const struct pbl_panel *panel, uint16_t x0, uint16_t y0, uint16_t x1,
                    uint16_t y1);
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
 * @brief Take the frame rotated and reflected, if the panel can.
 *
 * Takes effect from the next frame written, and is kept through power cycles of the panel. A
 * panel turned by 90 or 270 degrees has the active width and height of its timing exchanged, for
 * a frame wider than tall on a panel taller than wide, or the other way around.
 *
 * @param panel Panel.
 * @param rotation Mask of one rotation and any reflections, from @ref pbl_panel_rotation.
 * @return 0, @c -ENOTSUP if the panel cannot, or another negative errno.
 */
int pbl_panel_set_rotation(const struct pbl_panel *panel, uint32_t rotation);

/**
 * @brief Write the next pixels of a panel with frame memory to a window of the frame.
 *
 * @param panel Panel.
 * @param x0 First column.
 * @param y0 First row.
 * @param x1 Last column.
 * @param y1 Last row.
 * @return 0, @c -ENOTSUP if the panel cannot, or another negative errno.
 */
int pbl_panel_set_window(const struct pbl_panel *panel, uint16_t x0, uint16_t y0, uint16_t x1,
                         uint16_t y1);

/** @} */
