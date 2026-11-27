/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
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
 * with frame memory over quad SPI, or scanning out to a JDI memory-in-pixel panel.
 *
 * The interface is the panel's: a panel child of the controller is on quad SPI, a panel at the
 * other end of its port on the JDI parallel interface.
 *
 * On quad SPI, the controller is the @ref drivers_mipi_dbi host of its panel, @c sym_dbi, through
 * which the panel driver sends its commands: on one line, at the speed the panel reads at. Frames
 * go out on four lines in RGB565, at the fastest clock the panel takes. Updates cover the
 * multiples of rows the panel takes.
 *
 * On the JDI parallel interface, frames go out with the panel's timing, in RGB222. The panel is
 * power cycled at init with the pins in their sleep state, if any, e.g. driven low. The MIPI DBI
 * host takes no commands.
 *
 * The framebuffer format is @c CONFIG_DISPLAY_SF32LB57_LCDC_FORMAT, @ref
 * PBL_DISPLAY_FORMAT_ARGB2222 for a JDI panel: @ref PBL_DISPLAY_FORMAT_RGB565 rows are sent as they
 * are, and
 * @ref PBL_DISPLAY_FORMAT_ARGB2222 ones are converted in place to the controller's RGB332 while an
 * update runs, for a framebuffer half the size. A panel turned by 180 degrees is read from its
 * last line up and flipped left to right by the panel, or by mirroring the rows in place while an
 * update runs if the panel cannot. Framebuffers are at addresses the controller reaches: the RAM
 * of the SoC, or external RAM. Updates complete from the controller's interrupt, which an instance
 * connects itself, with priority @c CONFIG_DISPLAY_SF32LB57_LCDC_IRQ_PRIO, or from the system work
 * queue when rows have to be put back as they were.
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

// The panel interface: quad SPI or JDI parallel
struct pbl_display_sf32lb57_lcdc__intf {
  enum pbl_panel_bus_format bus_format;
  int (*hw_init)(const struct pbl_display_sf32lb57_lcdc *lcdc);
  int (*update_start)(const struct pbl_display_sf32lb57_lcdc *lcdc);
  // Handles its interrupts, returns false while the frame is not done
  bool (*irq)(const struct pbl_display_sf32lb57_lcdc *lcdc, uint32_t irq);
  // MIPI DBI commands to the panel, if it takes any
  int (*command_write)(const struct pbl_display_sf32lb57_lcdc *lcdc, uint8_t cmd,
                       const uint8_t *data, size_t len);
  int (*command_read)(const struct pbl_display_sf32lb57_lcdc *lcdc, uint8_t cmd, uint8_t *data,
                      size_t len);
};

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
  uint16_t max_line;
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
  /** Panel interface. */
  const struct pbl_display_sf32lb57_lcdc__intf *intf;
  /** Panel interface pins, with a sleep state for the panel power cycle at init. */
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
extern const struct pbl_display_sf32lb57_lcdc__intf pbl_display_sf32lb57_lcdc__qspi;
extern const struct pbl_display_sf32lb57_lcdc__intf pbl_display_sf32lb57_lcdc__jdi;

// Only the interfaces of the panels the build has drivers for are built
#ifdef CONFIG_DISPLAY_SF32LB57_LCDC_QSPI
#define PBL_DISPLAY_SF32LB57_LCDC__HAS_QSPI 1
#define PBL_DISPLAY_SF32LB57_LCDC__QSPI     &pbl_display_sf32lb57_lcdc__qspi
#else
#define PBL_DISPLAY_SF32LB57_LCDC__HAS_QSPI 0
#define PBL_DISPLAY_SF32LB57_LCDC__QSPI     nullptr
#endif

#ifdef CONFIG_DISPLAY_SF32LB57_LCDC_JDI
#define PBL_DISPLAY_SF32LB57_LCDC__HAS_JDI 1
#define PBL_DISPLAY_SF32LB57_LCDC__JDI     &pbl_display_sf32lb57_lcdc__jdi
#else
#define PBL_DISPLAY_SF32LB57_LCDC__HAS_JDI 0
#define PBL_DISPLAY_SF32LB57_LCDC__JDI     nullptr
#endif
/** @endcond */

/**
 * @brief Define an SF32LB57 LCD controller display and its MIPI DBI host.
 *
 * The panel is either on quad SPI or on the JDI parallel interface: the arguments of the other
 * interface are NULL and 0.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _base Controller registers.
 * @param _irq Controller interrupt.
 * @param _clk Controller clock, from PBL_CLK().
 * @param _rst Controller reset, from PBL_RESET().
 * @param _qspi_panel Panel on quad SPI, or NULL.
 * @param _qspi_width Its visible columns, or 0.
 * @param _qspi_height Its visible lines, or 0.
 * @param _qspi_rotation Its rotation, 0 or 180 degrees.
 * @param _jdi_panel Panel on the JDI parallel interface, or NULL.
 * @param _jdi_width Its visible columns, or 0.
 * @param _jdi_height Its visible lines, or 0.
 * @param _jdi_rotation Its rotation, 0 or 180 degrees.
 * @param _round_mask Round mask of the panel, or NULL.
 * @param _fb Memory reserved for the framebuffer, or 0.
 * @param _fb_size Its size, in bytes, at least a frame.
 * @param _fb_ram RAM device the memory is in, or NULL.
 * @param _pinctrl Default state of the panel interface pins.
 * @param _pinctrl_sleep Their state while the panel is power cycled, or NULL.
 */
#define PBL_DISPLAY_SF32LB57_LCDC_DEFINE(sym, _name, _base, _irq, _clk, _rst, _qspi_panel,         \
                                         _qspi_width, _qspi_height, _qspi_rotation, _jdi_panel,    \
                                         _jdi_width, _jdi_height, _jdi_rotation, _round_mask, _fb, \
                                         _fb_size, _fb_ram, _pinctrl, _pinctrl_sleep)              \
  static_assert(((_qspi_width) == 0) != ((_jdi_width) == 0), "a panel on one interface");          \
  static_assert((_qspi_width) == 0 || PBL_DISPLAY_SF32LB57_LCDC__HAS_QSPI,                         \
                "quad SPI built, with a quad SPI panel driver");                                   \
  static_assert((_jdi_width) == 0 || PBL_DISPLAY_SF32LB57_LCDC__HAS_JDI,                           \
                "JDI built, with a JDI panel driver");                                             \
  static_assert((_jdi_width) == 0 || PBL_DISPLAY_SF32LB57_LCDC_BPP == 1,                           \
                "ARGB2222 framebuffers for a JDI panel");                                          \
  static_assert(                                                                                   \
      (_qspi_rotation) + (_jdi_rotation) == 0 || (_qspi_rotation) + (_jdi_rotation) == 180,        \
      "rotation of 0 or 180 degrees");                                                             \
  static_assert((_fb_size) == 0 || (_fb_size) >= ((_qspi_width) + (_jdi_width)) *                  \
                                                     ((_qspi_height) + (_jdi_height)) *            \
                                                     PBL_DISPLAY_SF32LB57_LCDC_BPP,                \
                "reserved framebuffer smaller than a frame");                                      \
  extern const struct pbl_mipi_dbi sym##_dbi;                                                      \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                   \
  static struct pbl_display_sf32lb57_lcdc_state sym##_lcdc_state = {                               \
    .lock = PBL_MUTEX_INITIALIZER,                                                                 \
  };                                                                                               \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_sf32lb57_lcdc, struct pbl_display, disp){            \
    .disp = PBL_DISPLAY_INIT_FRAMEBUFFER(                                                          \
        sym, _name, nullptr, &pbl_display_sf32lb57_lcdc_ops, (_qspi_width) + (_jdi_width),         \
        (_qspi_height) + (_jdi_height), PBL_DISPLAY_SF32LB57_LCDC_FORMAT,                          \
        (_qspi_rotation) + (_jdi_rotation) == 180, _fb, _fb_size, _fb_ram,                         \
        .round_mask = (_round_mask)),                                                              \
    .state = &sym##_lcdc_state,                                                                    \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .clk = _clk,                                                                                   \
    .rst = _rst,                                                                                   \
    .panel = (_jdi_width) != 0 ? (_jdi_panel) : (_qspi_panel),                                     \
    .intf = (_jdi_width) != 0 ? PBL_DISPLAY_SF32LB57_LCDC__JDI : PBL_DISPLAY_SF32LB57_LCDC__QSPI,  \
    .pinctrl = PBL_PINCTRL(_pinctrl, _pinctrl_sleep, 0),                                           \
  };                                                                                               \
  PBL_DEVICE_STATE_DEFINE(sym##_dbi);                                                              \
  PBL_DEVICE_INSTANCE(sym##_dbi, struct pbl_display_sf32lb57_lcdc_dbi, struct pbl_mipi_dbi, dbi){  \
    .dbi = PBL_MIPI_DBI_INIT(sym##_dbi, _name "_dbi", &sym##_inst.disp.dev,                        \
                             &pbl_display_sf32lb57_lcdc_dbi_ops),                                  \
    .lcdc = &sym##_inst,                                                                           \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_DISPLAY_SF32LB57_LCDC_IRQ_PRIO,                                 \
                      pbl_display_sf32lb57_lcdc_irq_handler, &sym##_inst, 0)

/**
 * @brief Controller interrupt handler, to connect with PBL_IRQ_CONNECT().
 *
 * @param lcdc Display.
 */
void pbl_display_sf32lb57_lcdc_irq_handler(const struct pbl_display_sf32lb57_lcdc *lcdc);

/** @} */
