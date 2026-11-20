/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/display.h>
#include <pbl/drivers/mipi_dbi.h>
#include <pbl/drivers/panel.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/reset.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>

/**
 * @defgroup drivers_display_sf32lb57_lcdc SF32LB57 LCD controller
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the SF32LB57 LCD controller, sending frames to a panel
 * with frame memory over quad SPI.
 *
 * The controller is the @ref drivers_mipi_dbi host of its panel, @c sym_dbi, through which the
 * panel driver sends its commands: on one line, at the speed the panel reads at. Frames go out on
 * four lines in RGB565, at the fastest clock the panel takes. The framebuffer format is
 * @c CONFIG_DISPLAY_SF32LB57_LCDC_FORMAT: @ref PBL_DISPLAY_FORMAT_RGB565 rows are sent as they
 * are, and @ref PBL_DISPLAY_FORMAT_ARGB2222 ones are converted in place to the controller's RGB332
 * while an update runs, for a framebuffer half the size. A panel turned by 180 degrees is read
 * from its last line up and flipped left to right by the panel, or by mirroring the rows in
 * place while an update runs if the panel cannot. Framebuffers are at addresses the
 * controller reaches: the RAM of the SoC, or external RAM. Updates cover the multiples of rows the
 * panel takes. They complete from the controller's interrupt, which an instance connects itself,
 * with priority @c CONFIG_DISPLAY_SF32LB57_LCDC_IRQ_PRIO, or from the system work queue when rows
 * have to be put back as they were.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#ifdef CONFIG_DISPLAY_SF32LB57_LCDC_FORMAT_RGB565
#define PBL_DISPLAY_SF32LB57_LCDC_FORMAT PBL_DISPLAY_FORMAT_RGB565
#define PBL_DISPLAY_SF32LB57_LCDC_BPP    2
#else
#define PBL_DISPLAY_SF32LB57_LCDC_FORMAT PBL_DISPLAY_FORMAT_ARGB2222
#define PBL_DISPLAY_SF32LB57_LCDC_BPP    1
#endif
/** @endcond */

/** @cond INTERNAL_HIDDEN */
struct pbl_display_sf32lb57_lcdc;

struct pbl_display_sf32lb57_lcdc_state {
  struct pbl_mutex lock;
  struct pbl_work work;
  const struct pbl_display_sf32lb57_lcdc *lcdc;
  int status;
  uint8_t *fb;
  uint32_t pitch;
  uint32_t write_div;
  uint16_t y0;
  uint16_t y1;
  volatile bool frame;
  bool rotated;
  bool frame_rotated;
  bool mirrored;
  bool frame_mirrored;
};
/** @endcond */

/** @brief SF32LB57 LCD controller. */
struct pbl_display_sf32lb57_lcdc {
  /** Display. */
  struct pbl_display disp;
  /** Driver runtime state. */
  struct pbl_display_sf32lb57_lcdc_state *state;
  /** Controller registers. */
  uintptr_t base;
  /** Controller interrupt. */
  pbl_irq_t irq;
  /** Controller clock. */
  struct pbl_clk clk;
  /** Controller reset. */
  struct pbl_reset rst;
  /** Panel. */
  const struct pbl_panel *panel;
  /** Panel interface pins. */
  struct pbl_pinctrl pinctrl;
};

/** @brief MIPI DBI host of an SF32LB57 LCD controller. */
struct pbl_display_sf32lb57_lcdc_dbi {
  /** Host. */
  struct pbl_mipi_dbi dbi;
  /** Controller. */
  const struct pbl_display_sf32lb57_lcdc *lcdc;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_sf32lb57_lcdc_ops;
extern const struct pbl_mipi_dbi_ops pbl_display_sf32lb57_lcdc_dbi_ops;
/** @endcond */

/**
 * @brief Define an SF32LB57 LCD controller display and its MIPI DBI host.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _base Controller registers.
 * @param _irq Controller interrupt.
 * @param _clk Controller clock, from PBL_CLK().
 * @param _rst Controller reset, from PBL_RESET().
 * @param _panel Panel.
 * @param _width Width, in pixels, the panel's visible columns.
 * @param _height Height, in pixels, the panel's visible lines.
 * @param _rotation Panel rotation, 0 or 180 degrees.
 * @param _fb Memory reserved for the framebuffer, or 0.
 * @param _fb_size Its size, in bytes, at least a frame.
 * @param _fb_ram RAM device the memory is in, or NULL.
 * @param _pinctrl Default state of the panel interface pins.
 */
#define PBL_DISPLAY_SF32LB57_LCDC_DEFINE(sym, _name, _base, _irq, _clk, _rst, _panel, _width,     \
                                         _height, _rotation, _fb, _fb_size, _fb_ram, _pinctrl)    \
  static_assert((_rotation) == 0 || (_rotation) == 180, "rotation of 0 or 180 degrees");          \
  static_assert(                                                                                  \
      (_fb_size) == 0 || (_fb_size) >= (_width) * (_height) * PBL_DISPLAY_SF32LB57_LCDC_BPP,      \
      "reserved framebuffer smaller than a frame");                                               \
  extern const struct pbl_mipi_dbi sym##_dbi;                                                     \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                  \
  static struct pbl_display_sf32lb57_lcdc_state sym##_lcdc_state = {                              \
    .lock = PBL_MUTEX_INITIALIZER,                                                                \
  };                                                                                              \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_sf32lb57_lcdc, struct pbl_display, disp){           \
    .disp = PBL_DISPLAY_INIT_FRAMEBUFFER(sym, _name, nullptr, &pbl_display_sf32lb57_lcdc_ops,     \
                                         _width, _height, PBL_DISPLAY_SF32LB57_LCDC_FORMAT,       \
                                         (_rotation) == 180, _fb, _fb_size, _fb_ram),             \
    .state = &sym##_lcdc_state,                                                                   \
    .base = (_base),                                                                              \
    .irq = (_irq),                                                                                \
    .clk = _clk,                                                                                  \
    .rst = _rst,                                                                                  \
    .panel = (_panel),                                                                            \
    .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0),                                                 \
  };                                                                                              \
  PBL_DEVICE_STATE_DEFINE(sym##_dbi);                                                             \
  PBL_DEVICE_INSTANCE(sym##_dbi, struct pbl_display_sf32lb57_lcdc_dbi, struct pbl_mipi_dbi, dbi){ \
    .dbi = PBL_MIPI_DBI_INIT(sym##_dbi, _name "_dbi", &sym##_inst.disp.dev,                       \
                             &pbl_display_sf32lb57_lcdc_dbi_ops),                                 \
    .lcdc = &sym##_inst,                                                                          \
  };                                                                                              \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_DISPLAY_SF32LB57_LCDC_IRQ_PRIO,                                \
                      pbl_display_sf32lb57_lcdc_irq_handler, &sym##_inst, 0)

/**
 * @brief Controller interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param lcdc Display.
 */
void pbl_display_sf32lb57_lcdc_irq_handler(const struct pbl_display_sf32lb57_lcdc *lcdc);

/** @} */
