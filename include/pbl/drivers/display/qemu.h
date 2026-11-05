/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/display.h>

/**
 * @defgroup drivers_display_qemu QEMU display
 * @ingroup drivers_display
 * @brief @ref drivers_display driver for the QEMU framebuffer display peripheral.
 *
 * Rows are copied to the peripheral's framebuffer and shown at once; updates complete before
 * pbl_display_update() returns. The image is not rotated.
 * @{
 */

/** @brief QEMU display. */
struct pbl_display_qemu {
  /** Display. */
  struct pbl_display disp;
  /** Register base address. */
  uintptr_t base;
  /** Framebuffer base address. */
  uintptr_t fb;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_display_ops pbl_display_qemu_ops;
/** @endcond */

/**
 * @brief Define a QEMU display.
 *
 * @param sym Symbol of the display.
 * @param _name Name.
 * @param _base Register base address.
 * @param _fb Framebuffer base address.
 * @param _width Width, in pixels.
 * @param _height Height, in pixels.
 * @param _format Framebuffer format.
 */
#define PBL_DISPLAY_QEMU_DEFINE(sym, _name, _base, _fb, _width, _height, _format)                  \
  PBL_DISPLAY_STATE_DEFINE(sym);                                                                   \
  const struct pbl_display_qemu sym = {                                                            \
    .disp = PBL_DISPLAY_INIT(sym, _name, &pbl_display_qemu_ops, nullptr, _width, _height, _format, \
                             false),                                                               \
    .base = (_base),                                                                               \
    .fb = (_fb),                                                                                   \
  };                                                                                               \
  PBL_DEVICE_REGISTER(sym, &sym.disp.dev)

/** @} */
