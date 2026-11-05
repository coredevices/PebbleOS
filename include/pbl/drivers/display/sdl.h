/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/display.h>

/**
 * @defgroup drivers_display_sdl SDL display
 * @ingroup drivers_display
 * @brief @ref drivers_display driver showing the framebuffer in an SDL window on the host.
 *
 * Updates complete before pbl_display_update() returns. The image is not rotated.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_sdl_ops;
/** @endcond */

/**
 * @brief Define an SDL display.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _width Width, in pixels.
 * @param _height Height, in pixels.
 * @param _format Framebuffer format.
 */
#define PBL_DISPLAY_SDL_DEFINE(sym, _name, _width, _height, _format)                         \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                             \
  const struct pbl_display sym = PBL_DISPLAY_INIT(sym, _name, &pbl_display_sdl_ops, nullptr, \
                                                  _width, _height, _format, false);          \
  PBL_DEVICE_REGISTER(sym, &sym.dev)

/** @} */
