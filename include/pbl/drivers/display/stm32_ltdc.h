/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/display.h>
#include <pbl/drivers/display_bridge.h>
#include <pbl/drivers/panel.h>

/**
 * @defgroup drivers_display_stm32_ltdc STM32 LTDC
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the STM32 LTDC, sending frames to a panel with frame
 * memory.
 *
 * The LTDC sends the rows of an update through its @ref drivers_display_bridge, a DSI host in
 * adapted command mode, to the frame memory of the panel, straight from the buffer given, as ST
 * does on its boards: in two halves, left and right, the first from the tearing effect of the
 * panel and the second once the first is out, each written where the scan of the panel is not.
 * The panel takes the frame rotated by the devicetree @c rotation of the panel, turned by a
 * further 180 degrees on request. The framebuffer the board reserves holds the frame the panel
 * gets first.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
enum pbl_display_stm32_ltdc_stage : uint8_t {
  PBL_DISPLAY_STM32_LTDC_IDLE,
  PBL_DISPLAY_STM32_LTDC_TEAR,
  PBL_DISPLAY_STM32_LTDC_FIRST,
  PBL_DISPLAY_STM32_LTDC_SECOND,
};

struct pbl_display_stm32_ltdc_state {
  // The frame and rows being sent, how far, and whether an update waits for them
  uint8_t *frame;
  uint16_t y0;
  uint16_t y1;
  enum pbl_display_stm32_ltdc_stage stage;
  bool notify;
  // The right half goes first
  bool right_first;
  // An update asked for meanwhile
  uint8_t *queued;
  uint16_t queued_y0;
  uint16_t queued_y1;
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
  /** Pixel clock. */
  struct pbl_clk clk;
  /** Panel. */
  const struct pbl_panel *panel;
  /** Bridge to the panel. */
  const struct pbl_display_bridge *bridge;
  /** Rotation of the panel, in degrees counterclockwise. */
  uint16_t rotation;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_stm32_ltdc_ops;
/** @endcond */

/**
 * @brief Define the LTDC.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _base Registers.
 * @param _clk Pixel clock, from PBL_CLK().
 * @param _panel Panel.
 * @param _bridge Bridge to the panel.
 * @param _rotation Rotation of the panel: 0, 90, 180 or 270 degrees.
 * @param _width Width of the framebuffer, in pixels.
 * @param _height Height of the framebuffer, in pixels.
 * @param _format Format of the framebuffer, a @ref pbl_display_format.
 * @param _fb Address of the framebuffer.
 * @param _fb_size Its size, in bytes.
 * @param _fb_ram RAM device the framebuffer is in, or NULL.
 */
#define PBL_DISPLAY_STM32_LTDC_DEFINE(sym, _name, _base, _clk, _panel, _bridge, _rotation, _width, \
                                      _height, _format, _fb, _fb_size, _fb_ram)                    \
  static_assert((_rotation) % 90 == 0 && (_rotation) < 360, "rotation of 0, 90, 180 or 270");      \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                   \
  static struct pbl_display_stm32_ltdc_state sym##_ltdc_state;                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_display_stm32_ltdc, struct pbl_display, disp) {              \
    .disp = PBL_DISPLAY_INIT_FRAMEBUFFER(sym, _name, nullptr, &pbl_display_stm32_ltdc_ops, _width, \
                                         _height, _format, false, _fb, _fb_size, _fb_ram),         \
    .state = &sym##_ltdc_state, .base = (_base), .clk = _clk, .panel = (_panel),                   \
    .bridge = (_bridge), .rotation = (_rotation),                                                  \
  }

/** @} */
