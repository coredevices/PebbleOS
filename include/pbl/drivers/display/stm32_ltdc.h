/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/display.h>
#include <pbl/drivers/mipi_dsi.h>
#include <pbl/drivers/panel.h>
#include <pbl/kernel/irq.h>

/**
 * @defgroup drivers_display_stm32_ltdc STM32 LTDC
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the STM32 LTDC, scanning a framebuffer to a panel.
 *
 * The LTDC scans the framebuffer the board reserves, with one layer, at the timing of the panel
 * and the rate of its clock, which the board sets to the panel's. It starts before the panel is
 * prepared: a panel on a MIPI DSI host needs the video stream running to take its commands.
 * Updates flush the rows from the data cache, after copying them when they come from another
 * buffer, and complete at once. The sync signals are those a DSI host takes.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
enum pbl_display_stm32_ltdc_stage : uint8_t {
  PBL_DISPLAY_STM32_LTDC_IDLE,
  PBL_DISPLAY_STM32_LTDC_TEAR,
  PBL_DISPLAY_STM32_LTDC_LEFT,
  PBL_DISPLAY_STM32_LTDC_RIGHT,
};

struct pbl_display_stm32_ltdc_state {
  bool enabled;
  uint8_t *scanout;
  uint8_t *next;
  // Command mode: the frame being sent, how far, and whether an update waits for it
  uint8_t *frame;
  enum pbl_display_stm32_ltdc_stage stage;
  bool notify;
  // A frame asked for meanwhile
  uint8_t *queued;
  bool queued_notify;
};
/** @endcond */

/** @brief LTDC. */
struct pbl_display_stm32_ltdc {
  /** Display. */
  struct pbl_display disp;
  /** Driver runtime state. */
  struct pbl_display_stm32_ltdc_state *state;
  /** Registers. */
  uintptr_t base;
  /** Interrupt. */
  pbl_irq_t irq;
  /** Pixel clock. */
  struct pbl_clk clk;
  /** Panel. */
  const struct pbl_panel *panel;
  /** MIPI DSI host between the controller and a panel in command mode, or NULL. */
  const struct pbl_mipi_dsi_host *host;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_stm32_ltdc_ops;
void pbl_display_stm32_ltdc_irq_handler(const struct pbl_display_stm32_ltdc *ltdc);
/** @endcond */

/**
 * @brief Define the LTDC.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _base Registers.
 * @param _irq Interrupt.
 * @param _clk Pixel clock, from PBL_CLK().
 * @param _panel Panel.
 * @param _host MIPI DSI host of a panel in command mode, or NULL.
 * @param _width Width of the framebuffer, in pixels.
 * @param _height Height of the framebuffer, in pixels.
 * @param _format Format of the framebuffer, a @ref pbl_display_format.
 * @param _fb Address of the framebuffer.
 * @param _fb_size Its size, in bytes.
 * @param _fb_ram RAM device the framebuffer is in, or NULL.
 */
#define PBL_DISPLAY_STM32_LTDC_DEFINE(sym, _name, _base, _irq, _clk, _panel, _host, _width,        \
                                      _height, _format, _fb, _fb_size, _fb_ram)                    \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                   \
  static struct pbl_display_stm32_ltdc_state sym##_ltdc_state;                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_stm32_ltdc, struct pbl_display, disp){               \
    .disp = PBL_DISPLAY_INIT_FRAMEBUFFER(sym, _name, nullptr, &pbl_display_stm32_ltdc_ops, _width, \
                                         _height, _format, false, _fb, _fb_size, _fb_ram),         \
    .state = &sym##_ltdc_state,                                                                    \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .clk = _clk,                                                                                   \
    .panel = (_panel),                                                                             \
    .host = (_host),                                                                               \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_DISPLAY_STM32_LTDC_IRQ_PRIO,                                    \
                      pbl_display_stm32_ltdc_irq_handler, &sym##_inst, 0)

/** @} */
